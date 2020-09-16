// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Ernesto A. Fernández <ernesto.mnd.fernandez@gmail.com>
 */

#include <linux/slab.h>
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include "apfs.h"

/**
 * apfs_drec_from_query - Read the directory record found by a successful query
 * @query:	the query that found the record
 * @drec:	Return parameter.  The directory record found.
 *
 * Reads the directory record into @drec and performs some basic sanity checks
 * as a protection against crafted filesystems.  Returns 0 on success or
 * -EFSCORRUPTED otherwise.
 *
 * The caller must not free @query while @drec is in use, because @drec->name
 * points to data on disk.
 */
static int apfs_drec_from_query(struct apfs_query *query,
				struct apfs_drec *drec)
{
	char *raw = query->node->object.bh->b_data;
	struct apfs_drec_hashed_key *de_key;
	struct apfs_drec_val *de;
	int namelen = query->key_len - sizeof(*de_key);
	char *xval = NULL;
	int xlen;

	if (namelen < 1)
		return -EFSCORRUPTED;
	if (query->len < sizeof(*de))
		return -EFSCORRUPTED;

	de = (struct apfs_drec_val *)(raw + query->off);
	de_key = (struct apfs_drec_hashed_key *)(raw + query->key_off);

	if (namelen != (le32_to_cpu(de_key->name_len_and_hash) &
			APFS_DREC_LEN_MASK))
		return -EFSCORRUPTED;

	/* Filename must be NULL-terminated */
	if (de_key->name[namelen - 1] != 0)
		return -EFSCORRUPTED;

	/* The dentry may have at most one xfield: the sibling id */
	drec->sibling_id = 0;
	xlen = apfs_find_xfield(de->xfields, query->len - sizeof(*de),
				APFS_DREC_EXT_TYPE_SIBLING_ID, &xval);
	if (xlen >= sizeof(__le64)) {
		__le64 *sib_id = (__le64 *)xval;

		drec->sibling_id = le64_to_cpup(sib_id);
	}

	drec->name = de_key->name;
	drec->name_len = namelen - 1; /* Don't count the NULL termination */
	drec->ino = le64_to_cpu(de->file_id);

	drec->type = le16_to_cpu(de->flags) & APFS_DREC_TYPE_MASK;
	if (drec->type != DT_FIFO && drec->type & 1) /* Invalid file type */
		drec->type = DT_UNKNOWN;
	return 0;
}

/**
 * apfs_dentry_lookup - Lookup a dentry record in the catalog b-tree
 * @dir:	parent directory
 * @child:	filename
 * @drec:	on return, the directory record found
 *
 * Runs a catalog query for @name in the @dir directory.  On success, sets
 * @drec and returns a pointer to the query structure.  On failure, returns
 * an appropriate error pointer.
 */
static struct apfs_query *apfs_dentry_lookup(struct inode *dir,
					     const struct qstr *child,
					     struct apfs_drec *drec)
{
	struct super_block *sb = dir->i_sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct apfs_key key;
	struct apfs_query *query;
	u64 cnid = apfs_ino(dir);
	int err;

	apfs_init_drec_hashed_key(sb, cnid, child->name, &key);

	query = apfs_alloc_query(sbi->s_cat_root, NULL /* parent */);
	if (!query)
		return ERR_PTR(-ENOMEM);
	query->key = &key;

	/*
	 * Distinct filenames in the same directory may (rarely) share the same
	 * hash.  The query code cannot handle that because their order in the
	 * b-tree would	depend on their unnormalized original names.  Just get
	 * all the candidates and check them one by one.
	 */
	query->flags |= APFS_QUERY_CAT | APFS_QUERY_ANY_NAME | APFS_QUERY_EXACT;
	do {
		err = apfs_btree_query(sb, &query);
		if (err)
			goto fail;
		err = apfs_drec_from_query(query, drec);
		if (err)
			goto fail;
	} while (unlikely(apfs_filename_cmp(sb, child->name, drec->name)));

	return query;

fail:
	apfs_free_query(sb, query);
	return ERR_PTR(err);
}

/**
 * apfs_inode_by_name - Find the cnid for a given filename
 * @dir:	parent directory
 * @child:	filename
 * @ino:	on return, the inode number found
 *
 * Returns 0 and the inode number (which is the cnid of the file
 * record); otherwise, return the appropriate error code.
 */
int apfs_inode_by_name(struct inode *dir, const struct qstr *child, u64 *ino)
{
	struct super_block *sb = dir->i_sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct apfs_query *query;
	struct apfs_drec drec;
	int err = 0;

	down_read(&sbi->s_big_sem);
	query = apfs_dentry_lookup(dir, child, &drec);
	if (IS_ERR(query)) {
		err = PTR_ERR(query);
		goto out;
	}
	*ino = drec.ino;
	apfs_free_query(sb, query);
out:
	up_read(&sbi->s_big_sem);
	return err;
}

static int apfs_readdir(struct file *file, struct dir_context *ctx)
{
	struct inode *inode = file_inode(file);
	struct super_block *sb = inode->i_sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct apfs_key key;
	struct apfs_query *query;
	u64 cnid = apfs_ino(inode);
	loff_t pos;
	int err = 0;

	down_read(&sbi->s_big_sem);

	/* Inode numbers might overflow here; follow btrfs in ignoring that */
	if (!dir_emit_dots(file, ctx))
		goto out;

	query = apfs_alloc_query(sbi->s_cat_root, NULL /* parent */);
	if (!query) {
		err = -ENOMEM;
		goto out;
	}

	/* We want all the children for the cnid, regardless of the name */
	apfs_init_drec_hashed_key(sb, cnid, NULL /* name */, &key);
	query->key = &key;
	query->flags = APFS_QUERY_CAT | APFS_QUERY_MULTIPLE | APFS_QUERY_EXACT;

	pos = ctx->pos - 2;
	while (1) {
		struct apfs_drec drec;
		/*
		 * We query for the matching records, one by one. After we
		 * pass ctx->pos we begin to emit them.
		 *
		 * TODO: Faster approach for large directories?
		 */

		err = apfs_btree_query(sb, &query);
		if (err == -ENODATA) { /* Got all the records */
			err = 0;
			break;
		}
		if (err)
			break;

		err = apfs_drec_from_query(query, &drec);
		if (err) {
			apfs_alert(sb, "bad dentry record in directory 0x%llx",
				   cnid);
			break;
		}

		err = 0;
		if (pos <= 0) {
			if (!dir_emit(ctx, drec.name, drec.name_len,
				      drec.ino, drec.type))
				break;
		}
		pos--;
	}

	if (pos < 0)
		ctx->pos -= pos;
	apfs_free_query(sb, query);

out:
	up_read(&sbi->s_big_sem);
	return err;
}

const struct file_operations apfs_dir_operations = {
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
	.iterate_shared	= apfs_readdir,
};

/**
 * apfs_build_dentry_key - Allocate and initialize the key for a dentry record
 * @qname:	filename
 * @hash:	filename hash
 * @parent_id:	inode number for the parent of the dentry
 * @key_p:	on return, a pointer to the new on-disk key structure
 *
 * Returns the length of the key, or a negative error code in case of failure.
 */
static int apfs_build_dentry_key(struct qstr *qname, u64 hash, u64 parent_id,
				 struct apfs_drec_hashed_key **key_p)
{
	struct apfs_drec_hashed_key *key;
	u16 namelen = qname->len + 1; /* We count the null-termination */
	int key_len;

	key_len = sizeof(*key) + namelen;
	key = kmalloc(key_len, GFP_KERNEL);
	if (!key)
		return -ENOMEM;

	apfs_key_set_hdr(APFS_TYPE_DIR_REC, parent_id, key);
	key->name_len_and_hash = cpu_to_le32(namelen | hash);
	strcpy(key->name, qname->name);

	*key_p = key;
	return key_len;
}

/**
 * apfs_build_dentry_val - Allocate and initialize the value for a dentry record
 * @inode:	vfs inode for the dentry
 * @sibling_id:	sibling id for this hardlink (0 for none)
 * @val_p:	on return, a pointer to the new on-disk value structure
 *
 * Returns the length of the value, or a negative error code in case of failure.
 */
static int apfs_build_dentry_val(struct inode *inode, u64 sibling_id,
				 struct apfs_drec_val **val_p)
{
	struct apfs_drec_val *val;
	struct apfs_x_field xkey;
	int total_xlen = 0, val_len;
	__le64 raw_sibling_id = cpu_to_le64(sibling_id);
	struct timespec now = current_time(inode);

	/* The dentry record may have one xfield: the sibling id */
	if (sibling_id)
		total_xlen += sizeof(struct apfs_xf_blob) +
			      sizeof(xkey) + sizeof(raw_sibling_id);

	val_len = sizeof(*val) + total_xlen;
	val = kmalloc(val_len, GFP_KERNEL);
	if (!val)
		return -ENOMEM;
	*val_p = val;

	val->file_id = cpu_to_le64(apfs_ino(inode));
	val->date_added = cpu_to_le64(timespec_to_ns(&now));
	val->flags = cpu_to_le16((inode->i_mode >> 12) & 15); /* File type */

	if (!sibling_id)
		return val_len;

	/* The buffer was just allocated: none of these functions should fail */
	apfs_init_xfields(val->xfields, val_len - sizeof(*val));
	xkey.x_type = APFS_DREC_EXT_TYPE_SIBLING_ID;
	xkey.x_flags = 0; /* TODO: proper flags here? */
	xkey.x_size = cpu_to_le16(sizeof(raw_sibling_id));
	apfs_insert_xfield(val->xfields, total_xlen, &xkey, &raw_sibling_id);
	return val_len;
}

/**
 * apfs_create_dentry_rec - Create a dentry record in the catalog b-tree
 * @inode:	vfs inode for the dentry
 * @qname:	filename
 * @parent_id:	inode number for the parent of the dentry
 * @sibling_id:	sibling id for this hardlink (0 for none)
 *
 * Returns 0 on success or a negative error code in case of failure.
 */
static int apfs_create_dentry_rec(struct inode *inode, struct qstr *qname,
				  u64 parent_id, u64 sibling_id)
{
	struct super_block *sb = inode->i_sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct apfs_key key;
	struct apfs_query *query;
	struct apfs_drec_hashed_key *raw_key = NULL;
	struct apfs_drec_val *raw_val = NULL;
	int key_len, val_len;
	int ret;

	apfs_init_drec_hashed_key(sb, parent_id, qname->name, &key);
	query = apfs_alloc_query(sbi->s_cat_root, NULL /* parent */);
	if (!query)
		return -ENOMEM;
	query->key = &key;
	query->flags |= APFS_QUERY_CAT;

	ret = apfs_btree_query(sb, &query);
	if (ret && ret != -ENODATA)
		goto fail;

	key_len = apfs_build_dentry_key(qname, key.number, parent_id, &raw_key);
	if (key_len < 0) {
		ret = key_len;
		goto fail;
	}
	val_len = apfs_build_dentry_val(inode, sibling_id, &raw_val);
	if (val_len < 0) {
		ret = val_len;
		goto fail;
	}
	/* TODO: deal with hash collisions */
	ret = apfs_btree_insert(query, raw_key, key_len, raw_val, val_len);

fail:
	kfree(raw_val);
	kfree(raw_key);
	apfs_free_query(sb, query);
	return ret;
}

/**
 * apfs_build_sibling_val - Allocate and initialize a sibling link's value
 * @dentry:	in-memory dentry for this hardlink
 * @val_p:	on return, a pointer to the new on-disk value structure
 *
 * Returns the length of the value, or a negative error code in case of failure.
 */
static int apfs_build_sibling_val(struct dentry *dentry,
				  struct apfs_sibling_val **val_p)
{
	struct apfs_sibling_val *val;
	struct qstr *qname = &dentry->d_name;
	u16 namelen = qname->len + 1; /* We count the null-termination */
	struct inode *parent = d_inode(dentry->d_parent);
	int val_len;

	val_len = sizeof(*val) + namelen;
	val = kmalloc(val_len, GFP_KERNEL);
	if (!val)
		return -ENOMEM;

	val->parent_id = cpu_to_le64(apfs_ino(parent));
	val->name_len = cpu_to_le16(namelen);
	strcpy(val->name, qname->name);

	*val_p = val;
	return val_len;
}

/**
 * apfs_create_sibling_link_rec - Create a sibling link record for a dentry
 * @dentry:	the in-memory dentry
 * @inode:	vfs inode for the dentry
 * @sibling_id:	sibling id for this hardlink
 *
 * Returns 0 on success or a negative error code in case of failure.
 */
static int apfs_create_sibling_link_rec(struct dentry *dentry,
					struct inode *inode, u64 sibling_id)
{
	struct super_block *sb = dentry->d_sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct apfs_key key;
	struct apfs_query *query = NULL;
	struct apfs_sibling_link_key raw_key;
	struct apfs_sibling_val *raw_val;
	int val_len;
	int ret;

	apfs_init_sibling_link_key(apfs_ino(inode), sibling_id, &key);
	query = apfs_alloc_query(sbi->s_cat_root, NULL /* parent */);
	if (!query)
		return -ENOMEM;
	query->key = &key;
	query->flags |= APFS_QUERY_CAT;

	ret = apfs_btree_query(sb, &query);
	if (ret && ret != -ENODATA)
		goto fail;

	apfs_key_set_hdr(APFS_TYPE_SIBLING_LINK, apfs_ino(inode), &raw_key);
	raw_key.sibling_id = cpu_to_le64(sibling_id);
	val_len = apfs_build_sibling_val(dentry, &raw_val);
	if (val_len < 0)
		goto fail;

	ret = apfs_btree_insert(query, &raw_key, sizeof(raw_key),
				raw_val, val_len);
	kfree(raw_val);

fail:
	apfs_free_query(sb, query);
	return ret;
}

/**
 * apfs_create_sibling_map_rec - Create a sibling map record for a dentry
 * @dentry:	the in-memory dentry
 * @inode:	vfs inode for the dentry
 * @sibling_id:	sibling id for this hardlink
 *
 * Returns 0 on success or a negative error code in case of failure.
 */
static int apfs_create_sibling_map_rec(struct dentry *dentry,
				       struct inode *inode, u64 sibling_id)
{
	struct super_block *sb = dentry->d_sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct apfs_key key;
	struct apfs_query *query = NULL;
	struct apfs_sibling_map_key raw_key;
	struct apfs_sibling_map_val raw_val;
	int ret;

	apfs_init_sibling_map_key(sibling_id, &key);
	query = apfs_alloc_query(sbi->s_cat_root, NULL /* parent */);
	if (!query)
		return -ENOMEM;
	query->key = &key;
	query->flags |= APFS_QUERY_CAT;

	ret = apfs_btree_query(sb, &query);
	if (ret && ret != -ENODATA)
		goto fail;

	apfs_key_set_hdr(APFS_TYPE_SIBLING_MAP, sibling_id, &raw_key);
	raw_val.file_id = cpu_to_le64(apfs_ino(inode));

	ret = apfs_btree_insert(query, &raw_key, sizeof(raw_key),
				&raw_val, sizeof(raw_val));

fail:
	apfs_free_query(sb, query);
	return ret;
}

/**
 * apfs_create_sibling_recs - Create sibling link and map records for a dentry
 * @dentry:	the in-memory dentry
 * @inode:	vfs inode for the dentry
 * @sibling_id:	on return, the sibling id for this hardlink
 *
 * Returns 0 on success or a negative error code in case of failure.
 */
static int apfs_create_sibling_recs(struct dentry *dentry,
				    struct inode *inode, u64 *sibling_id)
{
	struct super_block *sb = dentry->d_sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct apfs_superblock *vsb_raw = sbi->s_vsb_raw;
	u64 cnid;
	int ret;

	/* Sibling ids come from the same pool as the inode numbers */
	ASSERT(sbi->s_xid == le64_to_cpu(vsb_raw->apfs_o.o_xid));
	cnid = le64_to_cpu(vsb_raw->apfs_next_obj_id);
	le64_add_cpu(&vsb_raw->apfs_next_obj_id, 1);

	ret = apfs_create_sibling_link_rec(dentry, inode, cnid);
	if (ret)
		return ret;
	ret = apfs_create_sibling_map_rec(dentry, inode, cnid);
	if (ret)
		return ret;

	*sibling_id = cnid;
	return 0;
}

/**
 * apfs_create_dentry - Create all records for a new dentry
 * @dentry:	the in-memory dentry
 * @inode:	vfs inode for the dentry
 *
 * Creates the dentry record itself, as well as the sibling records if needed;
 * also updates the child count for the parent inode.  Returns 0 on success or
 * a negative error code in case of failure.
 */
static int apfs_create_dentry(struct dentry *dentry, struct inode *inode)
{
	struct inode *parent = d_inode(dentry->d_parent);
	u64 sibling_id = 0;
	int err;

	if (inode->i_nlink > 1) {
		/* This is optional for a single link, so don't waste space */
		err = apfs_create_sibling_recs(dentry, inode, &sibling_id);
		if (err)
			return err;
	}

	err = apfs_create_dentry_rec(inode, &dentry->d_name,
				     apfs_ino(parent), sibling_id);
	if (err)
		return err;

	/* Now update the parent inode */
	parent->i_mtime = parent->i_ctime = current_time(inode);
	++APFS_I(parent)->i_nchildren;
	err = apfs_update_inode(parent, NULL /* new_name */);
	if (err)
		--APFS_I(parent)->i_nchildren;
	return err;
}

/**
 * apfs_undo_create_dentry - Clean up apfs_create_dentry()
 * @dentry: the in-memory dentry
 */
static void apfs_undo_create_dentry(struct dentry *dentry)
{
	struct inode *parent = d_inode(dentry->d_parent);

	--APFS_I(parent)->i_nchildren;
}

int apfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode,
	       dev_t rdev)
{
	struct super_block *sb = dir->i_sb;
	struct inode *inode;
	int err;

	err = apfs_transaction_start(sb);
	if (err)
		return err;

	inode = apfs_new_inode(dir, mode, rdev);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_abort;
	}

	err = apfs_create_inode_rec(sb, inode, dentry);
	if (err)
		goto out_discard_inode;

	err = apfs_create_dentry(dentry, inode);
	if (err)
		goto out_discard_inode;

	err = apfs_transaction_commit(sb);
	if (err)
		goto out_undo_create;

	d_instantiate_new(dentry, inode);
	return 0;

out_undo_create:
	apfs_undo_create_dentry(dentry);
out_discard_inode:
	/* Don't reset nlink: on-disk cleanup is unneeded and would deadlock */
	unlock_new_inode(inode);
out_abort:
	apfs_transaction_abort(sb);
	return err;
}

int apfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	return apfs_mknod(dir, dentry, mode | S_IFDIR, 0 /* rdev */);
}

int apfs_create(struct inode *dir, struct dentry *dentry, umode_t mode,
		bool excl)
{
	return apfs_mknod(dir, dentry, mode, 0 /* rdev */);
}

/**
 * apfs_prepare_dentry_for_link - Assign a sibling id and records to a dentry
 * @dentry: the in-memory dentry (should be for a primary link)
 *
 * Returns 0 on success, or a negative error code in case of failure.
 */
static int apfs_prepare_dentry_for_link(struct dentry *dentry)
{
	struct super_block *sb = dentry->d_sb;
	struct inode *parent = d_inode(dentry->d_parent);
	struct apfs_query *query;
	struct apfs_drec drec;
	u64 sibling_id;
	int ret;

	query = apfs_dentry_lookup(parent, &dentry->d_name, &drec);
	if (IS_ERR(query))
		return PTR_ERR(query);
	if (drec.sibling_id) {
		/* This dentry already has a sibling id xfield */
		apfs_free_query(sb, query);
		return 0;
	}

	/* Don't modify the dentry record, just delete it to make a new one */
	ret = apfs_btree_remove(query);
	apfs_free_query(sb, query);
	if (ret)
		return ret;

	ret = apfs_create_sibling_recs(dentry, d_inode(dentry), &sibling_id);
	if (ret)
		return ret;
	return apfs_create_dentry_rec(d_inode(dentry), &dentry->d_name,
				      apfs_ino(parent), sibling_id);
}

/**
 * __apfs_undo_link - Clean up __apfs_link()
 * @dentry: the in-memory dentry
 * @inode:  target inode
 */
static void __apfs_undo_link(struct dentry *dentry, struct inode *inode)
{
	apfs_undo_create_dentry(dentry);
	drop_nlink(inode);
}

/**
 * __apfs_link - Link a dentry
 * @old_dentry: dentry for the old link
 * @dir:	parent directory for new dentry
 * @dentry:	new dentry to link
 *
 * Does the same as apfs_link(), but without starting a transaction, taking a
 * new reference to @old_dentry->d_inode, or instantiating @dentry.
 */
static int __apfs_link(struct dentry *old_dentry, struct inode *dir,
		       struct dentry *dentry)
{
	struct inode *inode = d_inode(old_dentry);
	int err;

	/* First update the inode's link count */
	inc_nlink(inode);
	inode->i_ctime = current_time(inode);
	err = apfs_update_inode(inode, NULL /* new_name */);
	if (err)
		goto fail;

	if (inode->i_nlink == 2) {
		/* A single link may lack sibling records, so create them now */
		err = apfs_prepare_dentry_for_link(old_dentry);
		if (err)
			goto fail;
	}

	err = apfs_create_dentry(dentry, inode);
	if (err)
		goto fail;
	return 0;

fail:
	drop_nlink(inode);
	return err;
}

int apfs_link(struct dentry *old_dentry, struct inode *dir,
	      struct dentry *dentry)
{
	struct super_block *sb = dir->i_sb;
	struct inode *inode = d_inode(old_dentry);
	int err;

	err = apfs_transaction_start(sb);
	if (err)
		return err;

	err = __apfs_link(old_dentry, dir, dentry);
	if (err)
		goto out_abort;
	ihold(inode);

	err = apfs_transaction_commit(sb);
	if (err)
		goto out_undo_link;

	d_instantiate(dentry, inode);
	return 0;

out_undo_link:
	iput(inode);
	__apfs_undo_link(dentry, inode);
out_abort:
	apfs_transaction_abort(sb);
	return err;
}

/**
 * apfs_delete_sibling_link_rec - Delete the sibling link record for a dentry
 * @dentry:	the in-memory dentry
 * @sibling_id:	sibling id for this hardlink
 *
 * Returns 0 on success or a negative error code in case of failure.
 */
static int apfs_delete_sibling_link_rec(struct dentry *dentry, u64 sibling_id)
{
	struct super_block *sb = dentry->d_sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct inode *inode = d_inode(dentry);
	struct apfs_key key;
	struct apfs_query *query = NULL;
	int ret;

	ASSERT(sibling_id);

	apfs_init_sibling_link_key(apfs_ino(inode), sibling_id, &key);
	query = apfs_alloc_query(sbi->s_cat_root, NULL /* parent */);
	if (!query)
		return -ENOMEM;
	query->key = &key;
	query->flags |= APFS_QUERY_CAT | APFS_QUERY_EXACT;

	ret = apfs_btree_query(sb, &query);
	if (ret == -ENODATA) {
		/* A dentry with a sibling id must have sibling records */
		ret = -EFSCORRUPTED;
	}
	if (ret)
		goto fail;
	ret = apfs_btree_remove(query);

fail:
	apfs_free_query(sb, query);
	return ret;
}

/**
 * apfs_delete_sibling_map_rec - Delete the sibling map record for a dentry
 * @dentry:	the in-memory dentry
 * @sibling_id:	sibling id for this hardlink
 *
 * Returns 0 on success or a negative error code in case of failure.
 */
static int apfs_delete_sibling_map_rec(struct dentry *dentry, u64 sibling_id)
{
	struct super_block *sb = dentry->d_sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct apfs_key key;
	struct apfs_query *query = NULL;
	int ret;

	ASSERT(sibling_id);

	apfs_init_sibling_map_key(sibling_id, &key);
	query = apfs_alloc_query(sbi->s_cat_root, NULL /* parent */);
	if (!query)
		return -ENOMEM;
	query->key = &key;
	query->flags |= APFS_QUERY_CAT | APFS_QUERY_EXACT;

	ret = apfs_btree_query(sb, &query);
	if (ret == -ENODATA) {
		/* A dentry with a sibling id must have sibling records */
		ret = -EFSCORRUPTED;
	}
	if (ret)
		goto fail;
	ret = apfs_btree_remove(query);

fail:
	apfs_free_query(sb, query);
	return ret;
}

/**
 * apfs_delete_dentry - Delete all records for a dentry
 * @dentry: the in-memory dentry
 *
 * Deletes the dentry record itself, as well as the sibling records if they
 * exist; also updates the child count for the parent inode.  Returns 0 on
 * success or a negative error code in case of failure.
 */
static int apfs_delete_dentry(struct dentry *dentry)
{
	struct super_block *sb = dentry->d_sb;
	struct inode *parent = d_inode(dentry->d_parent);
	struct apfs_query *query;
	struct apfs_drec drec;
	int err;

	query = apfs_dentry_lookup(parent, &dentry->d_name, &drec);
	if (IS_ERR(query))
		return PTR_ERR(query);
	err = apfs_btree_remove(query);
	apfs_free_query(sb, query);
	if (err)
		return err;

	if (drec.sibling_id) {
		err = apfs_delete_sibling_link_rec(dentry, drec.sibling_id);
		if (err)
			return err;
		err = apfs_delete_sibling_map_rec(dentry, drec.sibling_id);
		if (err)
			return err;
	}

	/* Now update the parent inode */
	parent->i_mtime = parent->i_ctime = current_time(parent);
	--APFS_I(parent)->i_nchildren;
	err = apfs_update_inode(parent, NULL /* new_name */);
	if (err)
		++APFS_I(parent)->i_nchildren;
	return err;
}

/**
 * apfs_undo_delete_dentry - Clean up apfs_delete_dentry()
 * @dentry: the in-memory dentry
 */
static inline void apfs_undo_delete_dentry(struct dentry *dentry)
{
	struct inode *parent = d_inode(dentry->d_parent);

	/* Cleanup for the on-disk changes will happen on transaction abort */
	++APFS_I(parent)->i_nchildren;
}

/**
 * apfs_sibling_link_from_query - Read the sibling link record found by a query
 * @query:	the query that found the record
 * @name:	on return, the name of link
 * @parent:	on return, the inode number for the link's parent
 *
 * Reads the sibling link information into @parent and @name, and performs some
 * basic sanity checks as a protection against crafted filesystems.  The caller
 * must free @name after use.  Returns 0 on success or a negative error code in
 * case of failure.
 */
static int apfs_sibling_link_from_query(struct apfs_query *query,
					char **name, u64 *parent)
{
	char *raw = query->node->object.bh->b_data;
	struct apfs_sibling_val *siblink;
	int namelen = query->len - sizeof(*siblink);

	if (namelen < 1)
		return -EFSCORRUPTED;
	siblink = (struct apfs_sibling_val *)(raw + query->off);

	if (namelen != le16_to_cpu(siblink->name_len))
		return -EFSCORRUPTED;
	/* Filename must be NULL-terminated */
	if (siblink->name[namelen - 1] != 0)
		return -EFSCORRUPTED;

	*name = kmalloc(namelen, GFP_KERNEL);
	if (!*name)
		return -ENOMEM;
	strcpy(*name, siblink->name);
	*parent = le64_to_cpu(siblink->parent_id);
	return 0;
}

/**
 * apfs_find_primary_link - Find the primary link for an inode
 * @inode:	the vfs inode
 * @name:	on return, the name of the primary link
 * @parent:	on return, the inode number for the primary parent
 *
 * On success, returns 0 and sets @parent and @name; the second must be freed
 * by the caller after use.  Returns a negative error code in case of failure.
 */
static int apfs_find_primary_link(struct inode *inode, char **name, u64 *parent)
{
	struct super_block *sb = inode->i_sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct apfs_key key;
	struct apfs_query *query;
	int err;

	query = apfs_alloc_query(sbi->s_cat_root, NULL /* parent */);
	if (!query)
		return -ENOMEM;

	apfs_init_sibling_link_key(apfs_ino(inode), 0 /* sibling_id */, &key);
	query->key = &key;
	query->flags |= APFS_QUERY_CAT | APFS_QUERY_ANY_NUMBER |
			APFS_QUERY_EXACT;

	/* The primary link is the one with the lowest sibling id */
	*name = NULL;
	while (1) {
		err = apfs_btree_query(sb, &query);
		if (err == -ENODATA) /* No more link records */
			break;
		kfree(*name);
		if (err)
			goto fail;

		err = apfs_sibling_link_from_query(query, name, parent);
		if (err)
			goto fail;
	}
	err = *name ? 0 : -EFSCORRUPTED; /* Sibling records must exist */

fail:
	apfs_free_query(sb, query);
	return err;
}

/**
 * apfs_orphan_name - Get the name for an orphan inode's invisible link
 * @inode: the vfs inode
 * @qname: on return, the name assigned to the link
 *
 * Returns 0 on success; the caller must remember to free @qname->name after
 * use.  Returns a negative error code in case of failure.
 */
static int apfs_orphan_name(struct inode *inode, struct qstr *qname)
{
	int max_len;
	char *name;

	/* The name is the inode number in hex, with 'linux' prefix */
	max_len = 5 + 16 + 1;
	name = kmalloc(max_len, GFP_KERNEL);
	if (!name)
		return -ENOMEM;
	qname->len = snprintf(name, max_len, "linux%llx", apfs_ino(inode));
	qname->name = name;
	return 0;
}

/**
 * apfs_create_orphan_link - Create a link for an orphan inode under private-dir
 * @inode:	the vfs inode
 * @name:	on return, the name of the new link
 * @parent:	on return, the inode number for the new parent (private-dir)
 *
 * The official reference does not speak of orphan inodes; we are allowed to
 * use private-dir, so this function makes a new dentry there.  TODO: it might
 * be better to use a subdirectory.
 *
 * On success, returns 0 and sets @parent and @name; the second must be freed
 * by the caller after use.  Returns a negative error code in case of failure.
 */
static int apfs_create_orphan_link(struct inode *inode, char **name,
				   u64 *parent)
{
	struct super_block *sb = inode->i_sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct inode *priv_dir = sbi->s_private_dir;
	struct qstr qname;
	int err;

	err = apfs_orphan_name(inode, &qname);
	if (err)
		return err;
	err = apfs_create_dentry_rec(inode, &qname, apfs_ino(priv_dir),
				     0 /* sibling_id */);
	if (err)
		goto fail;

	/* Now update the child count for private-dir */
	priv_dir->i_mtime = priv_dir->i_ctime = current_time(priv_dir);
	++APFS_I(priv_dir)->i_nchildren;
	err = apfs_update_inode(priv_dir, NULL /* new_name */);
	if (err) {
		--APFS_I(priv_dir)->i_nchildren;
		goto fail;
	}

	*name = (char *)qname.name;
	*parent = apfs_ino(priv_dir);
	return 0;

fail:
	kfree(qname.name);
	return err;
}

/**
 * apfs_delete_orphan_link - Delete the link for an orphan inode
 * @inode: the vfs inode
 *
 * Returns 0 on success or a negative error code in case of failure.
 */
int apfs_delete_orphan_link(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct apfs_sb_info *sbi = APFS_SB(sb);
	struct inode *priv_dir = sbi->s_private_dir;
	struct apfs_query *query;
	struct qstr qname;
	struct apfs_drec drec;
	int err;

	err = apfs_orphan_name(inode, &qname);
	if (err)
		return err;

	query = apfs_dentry_lookup(priv_dir, &qname, &drec);
	if (IS_ERR(query)) {
		err = PTR_ERR(query);
		query = NULL;
		goto fail;
	}
	err = apfs_btree_remove(query);
	if (err)
		goto fail;

	/* Now update the child count for private-dir */
	priv_dir->i_mtime = priv_dir->i_ctime = current_time(priv_dir);
	--APFS_I(priv_dir)->i_nchildren;
	err = apfs_update_inode(priv_dir, NULL /* new_name */);
	if (err)
		++APFS_I(priv_dir)->i_nchildren;

fail:
	apfs_free_query(sb, query);
	kfree(qname.name);
	return err;
}

/**
 * __apfs_undo_unlink - Clean up __apfs_unlink()
 * @dentry: dentry to unlink
 */
static void __apfs_undo_unlink(struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);

	inode->i_state |= I_LINKABLE; /* Silence warning about nlink 0->1 */
	inc_nlink(inode);
	inode->i_state &= ~I_LINKABLE;

	apfs_undo_delete_dentry(dentry);
}

/**
 * __apfs_unlink - Unlink a dentry
 * @dir:    parent directory
 * @dentry: dentry to unlink
 *
 * Does the same as apfs_unlink(), but without starting a transaction.
 */
static int __apfs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	struct apfs_inode_info *ai = APFS_I(inode);
	char *primary_name = NULL;
	int err;

	err = apfs_delete_dentry(dentry);
	if (err)
		return err;

	drop_nlink(inode);
	if (!inode->i_nlink) {
		/* Don't let the inode become orphaned */
		err = apfs_create_orphan_link(inode, &primary_name,
					      &ai->i_parent_id);
	} else {
		/* We may have deleted the primary link, so get the new one */
		err = apfs_find_primary_link(inode, &primary_name,
					     &ai->i_parent_id);
	}
	if (err)
		goto fail;

	inode->i_ctime = dir->i_ctime;
	err = apfs_update_inode(inode, primary_name);

fail:
	kfree(primary_name);
	primary_name = NULL;
	if (err)
		__apfs_undo_unlink(dentry);
	return err;
}

int apfs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct super_block *sb = dir->i_sb;
	int err;

	err = apfs_transaction_start(sb);
	if (err)
		return err;

	err = __apfs_unlink(dir, dentry);
	if (err)
		goto out_abort;

	err = apfs_transaction_commit(sb);
	if (err)
		goto out_undo_unlink;
	return 0;

out_undo_unlink:
	__apfs_undo_unlink(dentry);
out_abort:
	apfs_transaction_abort(sb);
	return err;
}

int apfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);

	if (APFS_I(inode)->i_nchildren)
		return -ENOTEMPTY;
	return apfs_unlink(dir, dentry);
}

int apfs_rename(struct inode *old_dir, struct dentry *old_dentry,
		struct inode *new_dir, struct dentry *new_dentry,
		unsigned int flags)
{
	struct super_block *sb = old_dir->i_sb;
	struct inode *old_inode = d_inode(old_dentry);
	struct inode *new_inode = d_inode(new_dentry);
	int err;

	if (flags & ~RENAME_NOREPLACE) /* TODO: support RENAME_EXCHANGE */
		return -EINVAL;

	err = apfs_transaction_start(sb);
	if (err)
		return err;

	if (new_inode) {
		err = __apfs_unlink(new_dir, new_dentry);
		if (err)
			goto out_abort;
	}

	err = __apfs_link(old_dentry, new_dir, new_dentry);
	if (err)
		goto out_undo_unlink_new;

	err = __apfs_unlink(old_dir, old_dentry);
	if (err)
		goto out_undo_link;

	err = apfs_transaction_commit(sb);
	if (err)
		goto out_undo_unlink_old;
	return 0;

out_undo_unlink_old:
	__apfs_undo_unlink(old_dentry);
out_undo_link:
	__apfs_undo_link(new_dentry, old_inode);
out_undo_unlink_new:
	if (new_inode)
		__apfs_undo_unlink(new_dentry);
out_abort:
	apfs_transaction_abort(sb);
	return err;
}
