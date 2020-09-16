// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Ernesto A. Fernández <ernesto.mnd.fernandez@gmail.com>
 */

#include <linux/slab.h>
#include <linux/buffer_head.h>
#include "apfs.h"

/**
 * apfs_node_is_valid - Check basic sanity of the node index
 * @sb:		filesystem superblock
 * @node:	node to check
 *
 * Verifies that the node index fits in a single block, and that the number
 * of records fits in the index. Without this check a crafted filesystem could
 * pretend to have too many records, and calls to apfs_node_locate_key() and
 * apfs_node_locate_data() would read beyond the limits of the node.
 */
static bool apfs_node_is_valid(struct super_block *sb,
			       struct apfs_node *node)
{
	int records = node->records;
	int index_size = node->key - sizeof(struct apfs_btree_node_phys);
	int entry_size;

	if (node->key > sb->s_blocksize)
		return false;

	entry_size = (apfs_node_has_fixed_kv_size(node)) ?
		sizeof(struct apfs_kvoff) : sizeof(struct apfs_kvloc);

	return records * entry_size <= index_size;
}

static void apfs_node_release(struct kref *kref)
{
	struct apfs_node *node =
		container_of(kref, struct apfs_node, refcount);

	brelse(node->object.bh);
	kfree(node);
}

void apfs_node_get(struct apfs_node *node)
{
	kref_get(&node->refcount);
}

void apfs_node_put(struct apfs_node *node)
{
	kref_put(&node->refcount, apfs_node_release);
}

/**
 * apfs_read_node - Read a node header from disk
 * @sb:		filesystem superblock
 * @oid:	object id for the node
 * @storage:	storage type for the node object
 * @write:	request write access?
 *
 * Returns ERR_PTR in case of failure, otherwise return a pointer to the
 * resulting apfs_node structure with the initial reference taken.
 *
 * For now we assume the node has not been read before.
 */
struct apfs_node *apfs_read_node(struct super_block *sb, u64 oid, u32 storage,
				 bool write)
{
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct buffer_head *bh = NULL;
	struct apfs_btree_node_phys *raw;
	struct apfs_node *node;
	struct apfs_nloc *free_head;
	u64 bno;
	int err;

	switch (storage) {
	case APFS_OBJ_VIRTUAL:
		/* All virtual nodes are inside a volume, at least for now */
		err = apfs_omap_lookup_block(sb, sbi->s_omap_root, oid,
					     &bno, write);
		if (err)
			return ERR_PTR(err);
		bh = apfs_read_object_block(sb, bno, write);
		if (IS_ERR(bh))
			return (void *)bh;
		break;
	case APFS_OBJ_PHYSICAL:
		bh = apfs_read_object_block(sb, oid, write);
		if (IS_ERR(bh))
			return (void *)bh;
		oid = bh->b_blocknr;
		break;
	case APFS_OBJ_EPHEMERAL:
		/* Ephemeral objects are checkpoint data, so ignore 'write' */
		bh = apfs_read_ephemeral_object(sb, oid);
		if (IS_ERR(bh))
			return (void *)bh;
		break;
	}
	raw = (struct apfs_btree_node_phys *) bh->b_data;

	node = kmalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		brelse(bh);
		return ERR_PTR(-ENOMEM);
	}

	node->flags = le16_to_cpu(raw->btn_flags);
	node->records = le32_to_cpu(raw->btn_nkeys);
	node->key = sizeof(*raw) + le16_to_cpu(raw->btn_table_space.off)
				+ le16_to_cpu(raw->btn_table_space.len);
	node->free = node->key + le16_to_cpu(raw->btn_free_space.off);
	node->data = node->free + le16_to_cpu(raw->btn_free_space.len);

	free_head = &raw->btn_key_free_list;
	node->key_free_list_len = le16_to_cpu(free_head->len);
	free_head = &raw->btn_val_free_list;
	node->val_free_list_len = le16_to_cpu(free_head->len);

	node->object.sb = sb;
	node->object.block_nr = bh->b_blocknr;
	node->object.oid = oid;
	node->object.bh = bh;

	kref_init(&node->refcount);

	if (sbi->s_flags & APFS_CHECK_NODES &&
	    !apfs_obj_verify_csum(sb, &raw->btn_o)) {
		/* TODO: don't check this twice for virtual/physical objects */
		apfs_alert(sb, "bad checksum for node in block 0x%llx", bno);
		apfs_node_put(node);
		return ERR_PTR(-EFSBADCRC);
	}
	if (!apfs_node_is_valid(sb, node)) {
		apfs_alert(sb, "bad node in block 0x%llx", bno);
		apfs_node_put(node);
		return ERR_PTR(-EFSCORRUPTED);
	}

	return node;
}

/**
 * apfs_create_node - Allocates a new nonroot b-tree node on disk
 * @sb:		filesystem superblock
 * @storage:	storage type for the node object
 *
 * On success returns a pointer to the new in-memory node structure; the object
 * header is initialized, and the node fields are given reasonable defaults.
 * On failure, returns an error pointer.
 */
static struct apfs_node *apfs_create_node(struct super_block *sb, u32 storage)
{
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct apfs_nx_superblock *msb_raw = sbi->s_msb_raw;
	struct apfs_superblock *vsb_raw = sbi->s_vsb_raw;
	struct apfs_node *node;
	struct buffer_head *bh;
	struct apfs_btree_node_phys *raw;
	u64 bno, oid;
	u32 subtype;
	int err;

	switch (storage) {
	case APFS_OBJ_VIRTUAL:
		err = apfs_spaceman_allocate_block(sb, &bno);
		if (err)
			return ERR_PTR(err);
		le64_add_cpu(&vsb_raw->apfs_fs_alloc_count, 1);

		oid = le64_to_cpu(msb_raw->nx_next_oid);
		le64_add_cpu(&msb_raw->nx_next_oid, 1);
		err = apfs_create_omap_rec(sb, oid, bno);
		if (err)
			return ERR_PTR(err);

		subtype = APFS_OBJECT_TYPE_FSTREE;
		break;
	case APFS_OBJ_PHYSICAL:
		err = apfs_spaceman_allocate_block(sb, &bno);
		if (err)
			return ERR_PTR(err);
		/* We don't write to the container's omap */
		le64_add_cpu(&vsb_raw->apfs_fs_alloc_count, 1);

		oid = bno;
		subtype = APFS_OBJECT_TYPE_OMAP;
		break;
	case APFS_OBJ_EPHEMERAL:
		apfs_cpoint_data_allocate(sb, &bno);
		oid = le64_to_cpu(msb_raw->nx_next_oid);
		le64_add_cpu(&msb_raw->nx_next_oid, 1);

		err = apfs_create_cpoint_map(sb, oid, bno);
		if (err)
			return ERR_PTR(err);

		subtype = APFS_OBJECT_TYPE_SPACEMAN_FREE_QUEUE;
		break;
	default:
		ASSERT(false);
	}

	bh = sb_bread(sb, bno);
	if (!bh)
		return ERR_PTR(-EIO);
	raw = (void *)bh->b_data;
	err = apfs_transaction_join(sb, bh);
	if (err)
		goto fail;
	set_buffer_csum(bh);

	/* The object header is entirely set here; the caller can ignore it */
	raw->btn_o.o_oid = cpu_to_le64(oid);
	raw->btn_o.o_xid = cpu_to_le64(sbi->s_xid);
	raw->btn_o.o_type = cpu_to_le32(storage | APFS_OBJECT_TYPE_BTREE_NODE);
	raw->btn_o.o_subtype = cpu_to_le32(subtype);

	/* The caller is expected to change most node fields */
	raw->btn_flags = 0;
	raw->btn_level = 0;
	raw->btn_nkeys = 0;
	raw->btn_table_space.off = 0; /* Put the toc right after the header */
	raw->btn_table_space.len = 0;
	raw->btn_free_space.off = 0;
	raw->btn_free_space.len = cpu_to_le16(sb->s_blocksize - sizeof(*raw));
	raw->btn_key_free_list.off = cpu_to_le16(APFS_BTOFF_INVALID);
	raw->btn_key_free_list.len = 0;
	raw->btn_val_free_list.off = cpu_to_le16(APFS_BTOFF_INVALID);
	raw->btn_val_free_list.len = 0;

	node = kmalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		err = -ENOMEM;
		goto fail;
	}

	node->object.sb = sb;
	node->object.block_nr = bh->b_blocknr;
	node->object.oid = oid;
	node->object.bh = bh;
	kref_init(&node->refcount);
	return node;

fail:
	brelse(bh);
	return ERR_PTR(err);
}

/**
 * apfs_delete_node - Deletes a nonroot node from disk
 * @query: query pointing to the node
 *
 * Does nothing to the in-memory node structure.  Returns 0 on success, or a
 * negative error code in case of failure.
 */
int apfs_delete_node(struct apfs_query *query)
{
	struct super_block *sb = query->node->object.sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct apfs_superblock *vsb_raw = sbi->s_vsb_raw;
	struct apfs_node *node = query->node;
	u64 oid = node->object.oid;
	u64 bno = node->object.block_nr;
	int err;

	ASSERT(sbi->s_xid == le64_to_cpu(vsb_raw->apfs_o.o_xid));

	switch (query->flags & APFS_QUERY_TREE_MASK) {
	case APFS_QUERY_CAT:
		err = apfs_free_queue_insert(sb, bno);
		if (err)
			return err;
		err = apfs_delete_omap_rec(sb, oid);
		if (err)
			return err;
		le64_add_cpu(&vsb_raw->apfs_fs_alloc_count, -1);
		break;
	case APFS_QUERY_OMAP:
		err = apfs_free_queue_insert(sb, bno);
		if (err)
			return err;
		/* We don't write to the container's omap */
		le64_add_cpu(&vsb_raw->apfs_fs_alloc_count, -1);
		break;
	default:
		/* TODO: ephemeral nodes */
		return -EOPNOTSUPP;
	}
	return apfs_btree_remove(query->parent);
}

/**
 * apfs_update_node - Update an existing node header
 * @node: the modified in-memory node
 */
void apfs_update_node(struct apfs_node *node)
{
	struct super_block *sb = node->object.sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct buffer_head *bh = node->object.bh;
	struct apfs_btree_node_phys *raw = (void *)bh->b_data;
	struct apfs_nloc *free_head;
	u32 tflags, type;
	int toc_off;

	ASSERT(sbi->s_xid == le64_to_cpu(raw->btn_o.o_xid));

	raw->btn_o.o_oid = cpu_to_le64(node->object.oid);

	/* The node may no longer be a root, so update the object type */
	tflags = le32_to_cpu(raw->btn_o.o_type) & APFS_OBJECT_TYPE_FLAGS_MASK;
	type = (node->flags & APFS_BTNODE_ROOT) ? APFS_OBJECT_TYPE_BTREE :
						  APFS_OBJECT_TYPE_BTREE_NODE;
	raw->btn_o.o_type = cpu_to_le32(type | tflags);

	raw->btn_flags = cpu_to_le16(node->flags);
	raw->btn_nkeys = cpu_to_le32(node->records);

	toc_off = sizeof(*raw) + le16_to_cpu(raw->btn_table_space.off);
	raw->btn_table_space.len = cpu_to_le16(node->key - toc_off);
	raw->btn_free_space.off = cpu_to_le16(node->free - node->key);
	raw->btn_free_space.len = cpu_to_le16(node->data - node->free);

	free_head = &raw->btn_key_free_list;
	free_head->len = cpu_to_le16(node->key_free_list_len);
	free_head = &raw->btn_val_free_list;
	free_head->len = cpu_to_le16(node->val_free_list_len);

	apfs_obj_set_csum(sb, &raw->btn_o);
}

/**
 * apfs_node_locate_key - Locate the key of a node record
 * @node:	node to be searched
 * @index:	number of the entry to locate
 * @off:	on return will hold the offset in the block
 *
 * Returns the length of the key, or 0 in case of failure. The function checks
 * that this length fits within the block; callers must use the returned value
 * to make sure they never operate outside its bounds.
 */
int apfs_node_locate_key(struct apfs_node *node, int index, int *off)
{
	struct super_block *sb = node->object.sb;
	struct apfs_btree_node_phys *raw;
	int len;

	if (index >= node->records)
		return 0;

	raw = (struct apfs_btree_node_phys *)node->object.bh->b_data;
	if (apfs_node_has_fixed_kv_size(node)) {
		struct apfs_kvoff *entry;

		entry = (struct apfs_kvoff *)raw->btn_data + index;
		len = 16;
		/* Translate offset in key area to offset in block */
		*off = node->key + le16_to_cpu(entry->k);
	} else {
		/* These node types have variable length keys and data */
		struct apfs_kvloc *entry;

		entry = (struct apfs_kvloc *)raw->btn_data + index;
		len = le16_to_cpu(entry->k.len);
		/* Translate offset in key area to offset in block */
		*off = node->key + le16_to_cpu(entry->k.off);
	}

	if (*off + len > sb->s_blocksize) {
		/* Avoid out-of-bounds read if corrupted */
		return 0;
	}
	return len;
}

/**
 * apfs_node_locate_data - Locate the data of a node record
 * @node:	node to be searched
 * @index:	number of the entry to locate
 * @off:	on return will hold the offset in the block
 *
 * Returns the length of the data, which may be 0 in case of corruption or if
 * the record is a ghost. The function checks that this length fits within the
 * block; callers must use the returned value to make sure they never operate
 * outside its bounds.
 */
static int apfs_node_locate_data(struct apfs_node *node, int index, int *off)
{
	struct super_block *sb = node->object.sb;
	struct apfs_btree_node_phys *raw;
	int len;

	if (index >= node->records)
		return 0;

	raw = (struct apfs_btree_node_phys *)node->object.bh->b_data;
	if (apfs_node_has_fixed_kv_size(node)) {
		/* These node types have fixed length keys and data */
		struct apfs_kvoff *entry;
		u32 subtype = le32_to_cpu(raw->btn_o.o_subtype);

		entry = (struct apfs_kvoff *)raw->btn_data + index;
		if (subtype == APFS_OBJECT_TYPE_SPACEMAN_FREE_QUEUE) {
			/* A free-space queue record may have no value */
			if (le16_to_cpu(entry->v) == APFS_BTOFF_INVALID)
				return 0;
			len = 8;
		} else {
			/* This is an object-map node */
			len = apfs_node_is_leaf(node) ? 16 : 8;
		}
		/*
		 * Data offsets are counted backwards from the end of the
		 * block, or from the beginning of the footer when it exists
		 */
		if (apfs_node_is_root(node)) /* has footer */
			*off = sb->s_blocksize - sizeof(struct apfs_btree_info)
					- le16_to_cpu(entry->v);
		else
			*off = sb->s_blocksize - le16_to_cpu(entry->v);
	} else {
		/* These node types have variable length keys and data */
		struct apfs_kvloc *entry;

		entry = (struct apfs_kvloc *)raw->btn_data + index;
		len = le16_to_cpu(entry->v.len);
		/*
		 * Data offsets are counted backwards from the end of the
		 * block, or from the beginning of the footer when it exists
		 */
		if (apfs_node_is_root(node)) /* has footer */
			*off = sb->s_blocksize - sizeof(struct apfs_btree_info)
					- le16_to_cpu(entry->v.off);
		else
			*off = sb->s_blocksize - le16_to_cpu(entry->v.off);
	}

	if (*off < 0 || *off + len > sb->s_blocksize) {
		/* Avoid out-of-bounds read if corrupted */
		return 0;
	}
	return len;
}

/**
 * apfs_create_toc_entry - Create the table-of-contents entry for a record
 * @query: query pointing to the record
 *
 * Creates a toc entry for the record at index @query->index and increases
 * @node->records.  The caller must ensure enough space in the table.
 */
void apfs_create_toc_entry(struct apfs_query *query)
{
	struct apfs_node *node = query->node;
	struct super_block *sb = node->object.sb;
	struct apfs_btree_node_phys *raw = (void *)node->object.bh->b_data;
	int value_end;
	int recs = node->records;
	int index = query->index;

	value_end = sb->s_blocksize;
	if (apfs_node_is_root(node))
		value_end -= sizeof(struct apfs_btree_info);

	if (apfs_node_has_fixed_kv_size(node)) {
		struct apfs_kvoff *kvoff;

		kvoff = (struct apfs_kvoff *)raw->btn_data + query->index;
		memmove(kvoff + 1, kvoff, (recs - index) * sizeof(*kvoff));

		if (!query->len) /* Ghost record */
			kvoff->v = cpu_to_le16(APFS_BTOFF_INVALID);
		else
			kvoff->v = cpu_to_le16(value_end - query->off);
		kvoff->k = cpu_to_le16(query->key_off - node->key);
	} else {
		struct apfs_kvloc *kvloc;

		kvloc = (struct apfs_kvloc *)raw->btn_data + query->index;
		memmove(kvloc + 1, kvloc, (recs - index) * sizeof(*kvloc));

		kvloc->v.off = cpu_to_le16(value_end - query->off);
		kvloc->v.len = cpu_to_le16(query->len);
		kvloc->k.off = cpu_to_le16(query->key_off - node->key);
		kvloc->k.len = cpu_to_le16(query->key_len);
	}
	node->records++;
}

/**
 * apfs_key_from_query - Read the current key from a query structure
 * @query:	the query, with @query->key_off and @query->key_len already set
 * @key:	return parameter for the key
 *
 * Reads the key into @key and performs some basic sanity checks as a
 * protection against crafted filesystems.  Returns 0 on success or a
 * negative error code otherwise.
 */
static int apfs_key_from_query(struct apfs_query *query, struct apfs_key *key)
{
	struct super_block *sb = query->node->object.sb;
	char *raw = query->node->object.bh->b_data;
	void *raw_key = (void *)(raw + query->key_off);
	int err = 0;

	switch (query->flags & APFS_QUERY_TREE_MASK) {
	case APFS_QUERY_CAT:
		err = apfs_read_cat_key(raw_key, query->key_len, key);
		break;
	case APFS_QUERY_OMAP:
		err = apfs_read_omap_key(raw_key, query->key_len, key);
		break;
	case APFS_QUERY_FREE_QUEUE:
		err = apfs_read_free_queue_key(raw_key, query->key_len, key);
		break;
	default:
		/* Not implemented yet */
		err = -EINVAL;
		break;
	}
	if (err) {
		apfs_alert(sb, "bad node key in block 0x%llx",
			   query->node->object.block_nr);
	}

	/* A multiple query must ignore some of these fields */
	if (query->flags & APFS_QUERY_ANY_NAME)
		key->name = NULL;
	if (query->flags & APFS_QUERY_ANY_NUMBER)
		key->number = 0;

	return err;
}

/**
 * apfs_node_next - Find the next matching record in the current node
 * @sb:		filesystem superblock
 * @query:	multiple query in execution
 *
 * Returns 0 on success, -EAGAIN if the next record is in another node,
 * -ENODATA if no more matching records exist, or another negative error
 * code in case of failure.
 */
static int apfs_node_next(struct super_block *sb, struct apfs_query *query)
{
	struct apfs_node *node = query->node;
	struct apfs_key curr_key;
	int cmp, err;

	if (query->flags & APFS_QUERY_DONE)
		/* Nothing left to search; the query failed */
		return -ENODATA;

	if (!query->index) /* The next record may be in another node */
		return -EAGAIN;
	--query->index;

	query->key_len = apfs_node_locate_key(node, query->index,
					      &query->key_off);
	err = apfs_key_from_query(query, &curr_key);
	if (err)
		return err;

	cmp = apfs_keycmp(sb, &curr_key, query->key);

	if (cmp > 0) /* Records are out of order */
		return -EFSCORRUPTED;

	if (cmp != 0 && apfs_node_is_leaf(node) &&
	    query->flags & APFS_QUERY_EXACT)
		return -ENODATA;

	query->len = apfs_node_locate_data(node, query->index, &query->off);
	if (query->len == 0)
		return -EFSCORRUPTED;

	if (cmp != 0) {
		/*
		 * This is the last entry that can be relevant in this node.
		 * Keep searching the children, but don't return to this level.
		 */
		query->flags |= APFS_QUERY_DONE;
	}

	return 0;
}

/**
 * apfs_node_query - Execute a query on a single node
 * @sb:		filesystem superblock
 * @query:	the query to execute
 *
 * The search will start at index @query->index, looking for the key that comes
 * right before @query->key, according to the order given by apfs_keycmp().
 *
 * The @query->index will be updated to the last index checked. This is
 * important when searching for multiple entries, since the query may need
 * to remember where it was on this level. If we are done with this node, the
 * query will be flagged as APFS_QUERY_DONE, and the search will end in failure
 * as soon as we return to this level. The function may also return -EAGAIN,
 * to signal that the search should go on in a different branch.
 *
 * On success returns 0; the offset of the data within the block will be saved
 * in @query->off, and its length in @query->len. The function checks that this
 * length fits within the block; callers must use the returned value to make
 * sure they never operate outside its bounds.
 *
 * -ENODATA will be returned if no appropriate entry was found, -EFSCORRUPTED
 * in case of corruption.
 */
int apfs_node_query(struct super_block *sb, struct apfs_query *query)
{
	struct apfs_node *node = query->node;
	int left, right;
	int cmp;
	int err;

	if (query->flags & APFS_QUERY_NEXT)
		return apfs_node_next(sb, query);

	/* Search by bisection */
	cmp = 1;
	left = 0;
	do {
		struct apfs_key curr_key;
		if (cmp > 0) {
			right = query->index - 1;
			if (right < left) {
				query->index = -1;
				return -ENODATA;
			}
			query->index = (left + right) / 2;
		} else {
			left = query->index;
			query->index = DIV_ROUND_UP(left + right, 2);
		}

		query->key_len = apfs_node_locate_key(node, query->index,
						      &query->key_off);
		err = apfs_key_from_query(query, &curr_key);
		if (err)
			return err;

		cmp = apfs_keycmp(sb, &curr_key, query->key);
		if (cmp == 0 && !(query->flags & APFS_QUERY_MULTIPLE))
			break;
	} while (left != right);

	if (cmp > 0) {
		query->index = -1;
		return -ENODATA;
	}

	if (cmp != 0 && apfs_node_is_leaf(query->node) &&
	    query->flags & APFS_QUERY_EXACT)
		return -ENODATA;

	if (query->flags & APFS_QUERY_MULTIPLE) {
		if (cmp != 0) /* Last relevant entry in level */
			query->flags |= APFS_QUERY_DONE;
		query->flags |= APFS_QUERY_NEXT;
	}

	query->len = apfs_node_locate_data(node, query->index, &query->off);
	return 0;
}

/**
 * apfs_bno_from_query - Read the block number found by a successful omap query
 * @query:	the query that found the record
 * @bno:	Return parameter.  The block number found.
 *
 * Returns -EOPNOTSUPP if the object doesn't fit in one block, and -EFSCORRUPTED
 * if the filesystem appears to be malicious.  Otherwise, reads the block number
 * in the omap record into @bno and returns 0.
 */
int apfs_bno_from_query(struct apfs_query *query, u64 *bno)
{
	struct super_block *sb = query->node->object.sb;
	struct apfs_omap_val *omap_val;
	char *raw = query->node->object.bh->b_data;

	if (query->len != sizeof(*omap_val))
		return -EFSCORRUPTED;
	omap_val = (struct apfs_omap_val *)(raw + query->off);

	/* TODO: support objects with multiple blocks */
	if (le32_to_cpu(omap_val->ov_size) != sb->s_blocksize) {
		apfs_err(sb, "object size doesn't match block size");
		return -EOPNOTSUPP;
	}

	*bno = le64_to_cpu(omap_val->ov_paddr);
	return 0;
}

/**
 * apfs_btree_inc_height - Increase the height of a b-tree
 * @query: query pointing to the root node
 *
 * On success returns 0, and @query is left pointing to the same record.
 * Returns a negative error code in case of failure.
 */
static int apfs_btree_inc_height(struct apfs_query *query)
{
	struct apfs_query *root_query;
	struct apfs_node *root = query->node;
	struct apfs_node *new_node;
	struct super_block *sb = root->object.sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct apfs_btree_node_phys *root_raw;
	struct apfs_btree_node_phys *new_raw;
	struct apfs_btree_info *info;
	__le64 *raw_oid;
	u32 storage = apfs_query_storage(query);
	int toc_entry_size;

	root_raw = (void *)root->object.bh->b_data;
	ASSERT(sbi->s_xid == le64_to_cpu(root_raw->btn_o.o_xid));

	if (query->parent || query->depth)
		return -EFSCORRUPTED;

	if (apfs_node_has_fixed_kv_size(root))
		toc_entry_size = sizeof(struct apfs_kvoff);
	else
		toc_entry_size = sizeof(struct apfs_kvloc);

	/* Create a new child node */
	new_node = apfs_create_node(sb, storage);
	if (IS_ERR(new_node))
		return PTR_ERR(new_node);
	new_node->flags = root->flags & ~APFS_BTNODE_ROOT;

	/* Move all records into the child node; get rid of the info footer */
	new_node->records = root->records;
	new_node->key = root->key;
	new_node->free = root->free;
	new_node->data = root->data + sizeof(*info);
	new_node->key_free_list_len = root->key_free_list_len;
	new_node->val_free_list_len = root->val_free_list_len;
	new_raw = (void *)new_node->object.bh->b_data;
	/* Don't copy the object header, already set by apfs_create_node() */
	memcpy((void *)new_raw + sizeof(new_raw->btn_o),
	       (void *)root_raw + sizeof(root_raw->btn_o),
	       root->free - sizeof(new_raw->btn_o));
	memcpy((void *)new_raw + new_node->data,
	       (void *)root_raw + root->data,
	       sb->s_blocksize - new_node->data);
	query->off += sizeof(*info);
	apfs_update_node(new_node);

	/* Add a new level to the query chain */
	root_query = query->parent = apfs_alloc_query(root, NULL /* parent */);
	if (!query->parent) {
		apfs_node_put(new_node);
		return -ENOMEM;
	}
	root_query->key = query->key;
	root_query->flags = query->flags;
	apfs_node_put(query->node);
	query->node = new_node;

	/* Now assemble the new root with only the first key */
	root_query->key_len = apfs_node_locate_key(root, 0 /* index */,
						   &root_query->key_off);
	if (!root_query->key_len)
		return -EFSCORRUPTED;
	root->key = sizeof(*root_raw) +
		    APFS_BTREE_TOC_ENTRY_INCREMENT * toc_entry_size;
	memmove((void *)root_raw + root->key,
		(void *)root_raw + root_query->key_off, root_query->key_len);
	root_query->key_off = root->key;
	root->free = root->key + root_query->key_len;

	/* The new root is a nonleaf node; the record value is the child id */
	root->flags &= ~APFS_BTNODE_LEAF;
	root->data = sb->s_blocksize - sizeof(*info) - sizeof(*raw_oid);
	raw_oid = (void *)root_raw + root->data;
	*raw_oid = cpu_to_le64(new_node->object.oid);
	root_query->off = root->data;
	root_query->len = sizeof(*raw_oid);

	/* With the key and value in place, set the table-of-contents */
	root->records = 0;
	root_query->index = 0;
	apfs_create_toc_entry(root_query);

	/* There is no internal fragmentation */
	root->key_free_list_len = 0;
	root->val_free_list_len = 0;

	/* Finally, update the node count in the info footer */
	apfs_btree_change_node_count(root_query, 1 /* change */);

	le16_add_cpu(&root_raw->btn_level, 1); /* TODO: move to update_node() */
	apfs_update_node(root);
	return 0;
}

/**
 * apfs_copy_record_range - Copy a range of records to an empty nonroot node
 * @dest_node:	destination node
 * @src_node:	source node
 * @start:	index of first record in range
 * @end:	index of first record after the range
 *
 * Returns 0 on success or a negative error code in case of failure.
 */
static int apfs_copy_record_range(struct apfs_node *dest_node,
				  struct apfs_node *src_node,
				  int start, int end)
{
	struct super_block *sb = dest_node->object.sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct apfs_btree_node_phys *dest_raw;
	struct apfs_btree_node_phys *src_raw;
	struct apfs_query *query = NULL;
	int toc_entry_size;
	int err;
	int i;

	dest_raw = (void *)dest_node->object.bh->b_data;
	src_raw = (void *)src_node->object.bh->b_data;

	ASSERT(!dest_node->records);
	ASSERT(!apfs_node_is_root(dest_node));
	ASSERT(sbi->s_xid == le64_to_cpu(dest_raw->btn_o.o_xid));

	/* Resize the table of contents so that all the records fit */
	if (apfs_node_has_fixed_kv_size(src_node))
		toc_entry_size = sizeof(struct apfs_kvoff);
	else
		toc_entry_size = sizeof(struct apfs_kvloc);
	dest_node->key = sizeof(*dest_raw) + toc_entry_size *
			 round_up(end - start, APFS_BTREE_TOC_ENTRY_INCREMENT);
	dest_node->free = dest_node->key;
	dest_node->data = sb->s_blocksize; /* Nonroot */

	/* We'll use a temporary query structure to move the records around */
	query = apfs_alloc_query(dest_node, NULL /* parent */);
	if (!query) {
		err = -ENOMEM;
		goto fail;
	}

	err = -EFSCORRUPTED;
	for (i = start; i < end; ++i) {
		int len, off;

		len = apfs_node_locate_key(src_node, i, &off);
		if (dest_node->free + len > sb->s_blocksize)
			goto fail;
		memcpy((char *)dest_raw + dest_node->free,
		       (char *)src_raw + off, len);
		query->key_off = dest_node->free;
		query->key_len = len;
		dest_node->free += len;

		len = apfs_node_locate_data(src_node, i, &off);
		dest_node->data -= len;
		if (dest_node->data < 0)
			goto fail;
		memcpy((char *)dest_raw + dest_node->data,
		       (char *)src_raw + off, len);
		query->off = dest_node->data;
		query->len = len;

		query->index = i - start;
		apfs_create_toc_entry(query);
	}
	err = 0;

fail:
	apfs_free_query(sb, query);
	return err;
}

/**
 * apfs_attach_child - Attach a new node to its parent
 * @query:	query pointing to the previous record in the parent
 * @child:	the new child node to attach
 *
 * Returns 0 on success or a negative error code in case of failure; @query is
 * rendered invalid either way and must be freed by the caller.
 */
static int apfs_attach_child(struct apfs_query *query, struct apfs_node *child)
{
	struct apfs_object *object = &child->object;
	struct apfs_btree_node_phys *raw = (void *)object->bh->b_data;
	int key_len, key_off;
	__le64 raw_oid = cpu_to_le64(object->oid);

	key_len = apfs_node_locate_key(child, 0, &key_off);
	if (!key_len) /* This should never happen: @child was made by us */
		return -EFSCORRUPTED;

	return apfs_btree_insert(query, (void *)raw + key_off, key_len,
				 &raw_oid, sizeof(raw_oid));
}

/**
 * apfs_node_split - Split a b-tree node in two
 * @query: query pointing to the node
 *
 * On success returns 0, and @query is left pointing to the same record on the
 * leaf; to simplify the implementation, @query->parent is set to NULL.  Returns
 * a negative error code in case of failure.
 */
int apfs_node_split(struct apfs_query *query)
{
	struct super_block *sb = query->node->object.sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct apfs_node *old_node, *new_node;
	struct apfs_btree_node_phys *old_raw, *new_raw;
	char *buffer = NULL;
	struct apfs_node buf_node;
	struct buffer_head buf_bh;
	u32 storage = apfs_query_storage(query);
	int record_count;
	int err;

	if (apfs_node_is_root(query->node)) {
		err = apfs_btree_inc_height(query);
		if (err)
			return err;
	}
	old_node = query->node;

	/* Do this first, or node splits may cause @query->parent to be gone */
	apfs_btree_change_node_count(query->parent, 1 /* change */);

	old_raw = (void *)old_node->object.bh->b_data;
	ASSERT(sbi->s_xid == le64_to_cpu(old_raw->btn_o.o_xid));

	/*
	 * XXX: to defragment the original node, we put all records in a
	 * temporary buffer fake node before dealing them out.  This is absurd,
	 * and completely unacceptable because of the stack allocations.
	 */
	buffer = kmalloc(sb->s_blocksize, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;
	memcpy(buffer, old_raw, sb->s_blocksize);
	buf_bh = *old_node->object.bh;
	buf_bh.b_data = buffer;
	buf_node = *old_node;
	buf_node.object.bh = &buf_bh;

	/* The first half of the records go in the original node */
	record_count = old_node->records; /* TODO: what if record_count == 1? */
	old_node->records = 0;
	old_node->key_free_list_len = 0;
	old_node->val_free_list_len = 0;
	err = apfs_copy_record_range(old_node, &buf_node, 0, record_count / 2);
	if (err)
		goto out_free_buffer;
	apfs_update_node(old_node);

	/* The second half is copied to a new node */
	new_node = apfs_create_node(sb, storage);
	if (IS_ERR(new_node)) {
		err = PTR_ERR(new_node);
		goto out_free_buffer;
	}
	new_node->flags = old_node->flags;
	new_node->records = 0;
	new_node->key_free_list_len = 0;
	new_node->val_free_list_len = 0;
	err = apfs_copy_record_range(new_node, &buf_node, record_count / 2,
				     record_count);
	if (err)
		goto out_put_node;
	new_raw = (void *)new_node->object.bh->b_data;
	ASSERT(sbi->s_xid == le64_to_cpu(new_raw->btn_o.o_xid));
	new_raw->btn_level = old_raw->btn_level;
	apfs_update_node(new_node);

	err = apfs_attach_child(query->parent, new_node);
	apfs_free_query(sb, query->parent);
	query->parent = NULL; /* The caller only gets the leaf */
	if (err)
		goto out_put_node;

	/* Point the query back to the original record */
	if (query->index >= record_count / 2) {
		/* The record got moved to the new node */
		apfs_node_put(query->node);
		apfs_node_get(new_node);
		query->node = new_node;
		query->index -= record_count / 2;
	}
	/* Updating these fields isn't really necessary, but it's cleaner */
	query->len = apfs_node_locate_data(query->node, query->index,
					   &query->off);
	query->key_len = apfs_node_locate_key(query->node, query->index,
					      &query->key_off);

out_put_node:
	apfs_node_put(new_node);
out_free_buffer:
	kfree(buffer);
	return err;
}
