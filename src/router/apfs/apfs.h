/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 Ernesto A. Fernández <ernesto.mnd.fernandez@gmail.com>
 */

#ifndef _APFS_H
#define _APFS_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/version.h>
#include "apfs_raw.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0) /* SB_RDONLY came in 4.14 */
#define SB_RDONLY MS_RDONLY
static inline bool sb_rdonly(const struct super_block *sb) { return sb->s_flags & MS_RDONLY; }
#endif

#define APFS_SUPER_MAGIC			0x4253584E

#define APFS_MODULE_ID_STRING	"linux-apfs by EA Fernández"

#define EFSBADCRC	EBADMSG		/* Bad CRC detected */
#define EFSCORRUPTED	EUCLEAN		/* Filesystem is corrupted */

/*
 * In-memory representation of an APFS object
 */
struct apfs_object {
	struct super_block *sb;
	u64 block_nr;
	u64 oid;		/* Often the same as the block number */

	/*
	 * Buffer head containing the one block of the object.  TODO: support
	 * objects with more than one block.
	 */
	struct buffer_head *bh;
};

/* Constants used in managing the size of a node's table of contents */
#define APFS_BTREE_TOC_ENTRY_INCREMENT	8
#define APFS_BTREE_TOC_ENTRY_MAX_UNUSED	(2 * BTREE_TOC_ENTRY_INCREMENT)

/*
 * In-memory representation of an APFS node
 */
struct apfs_node {
	u16 flags;		/* Node flags */
	u32 records;		/* Number of records in the node */

	int key;		/* Offset of the key area in the block */
	int free;		/* Offset of the free area in the block */
	int data;		/* Offset of the data area in the block */

	int key_free_list_len;	/* Length of the fragmented free key space */
	int val_free_list_len;	/* Length of the fragmented free value space */

	struct apfs_object object; /* Object holding the node */

	struct kref refcount;
};

/**
 * apfs_node_is_leaf - Check if a b-tree node is a leaf
 * @node: the node to check
 */
static inline bool apfs_node_is_leaf(struct apfs_node *node)
{
	return (node->flags & APFS_BTNODE_LEAF) != 0;
}

/**
 * apfs_node_is_root - Check if a b-tree node is the root
 * @node: the node to check
 */
static inline bool apfs_node_is_root(struct apfs_node *node)
{
	return (node->flags & APFS_BTNODE_ROOT) != 0;
}

/**
 * apfs_node_has_fixed_kv_size - Check if a b-tree node has fixed key/value
 * sizes
 * @node: the node to check
 */
static inline bool apfs_node_has_fixed_kv_size(struct apfs_node *node)
{
	return (node->flags & APFS_BTNODE_FIXED_KV_SIZE) != 0;
}

/*
 * Space manager data in memory.
 */
struct apfs_spaceman {
	struct apfs_spaceman_phys *sm_raw; /* On-disk spaceman structure */
	struct buffer_head	  *sm_bh;  /* Buffer head for @sm_raw */

	int sm_struct_size;		/* Actual size of @sm_raw */
	u32 sm_blocks_per_chunk;	/* Blocks covered by a bitmap block */
	u32 sm_chunks_per_cib;		/* Chunk count in a chunk-info block */
	u64 sm_block_count;		/* Block count for the container */
	u64 sm_chunk_count;		/* Number of bitmap blocks */
	u32 sm_cib_count;		/* Number of chunk-info blocks */
	u64 sm_free_count;		/* Number of free blocks */
	u32 sm_addr_offset;		/* Offset of cib addresses in @sm_raw */
};

/*
 * Structure that keeps track of a transaction.
 */
struct apfs_transaction {
	struct buffer_head *t_old_msb;  /* Main superblock being replaced */
	struct buffer_head *t_old_vsb;  /* Volume superblock being replaced */

	struct apfs_node t_old_omap_root; /* Omap root node being replaced */
	struct apfs_node t_old_cat_root;  /* Catalog root node being replaced */

	struct list_head t_buffers;	/* List of buffers in the transaction */
};

/* State bits for buffer heads in a transaction */
#define BH_TRANS	BH_PrivateStart		/* Attached to a transaction */
#define BH_CSUM		(BH_PrivateStart + 1)	/* Requires checksum update */
BUFFER_FNS(TRANS, trans);
BUFFER_FNS(CSUM, csum);

/*
 * Additional information for a buffer in a transaction.
 */
struct apfs_bh_info {
	struct buffer_head	*bh;	/* The buffer head */
	struct list_head	list;	/* List of buffers in the transaction */
};

/* Mount option flags */
#define APFS_CHECK_NODES	1

/*
 * Superblock data in memory, both from the main superblock and the volume
 * checkpoint superblock.
 */
struct apfs_sb_info {
	struct apfs_nx_superblock *s_msb_raw;		/* On-disk main sb */
	struct apfs_superblock *s_vsb_raw;		/* On-disk volume sb */

	u64 s_xid;			/* Latest transaction id */
	struct apfs_node *s_cat_root;	/* Root of the catalog tree */
	struct apfs_node *s_omap_root;	/* Root of the object map tree */

	struct apfs_object s_mobject;	/* Main superblock object */
	struct apfs_object s_vobject;	/* Volume superblock object */

	/* Mount options */
	unsigned int s_flags;
	unsigned int s_vol_nr;		/* Index of the volume in the sb list */
	kuid_t s_uid;			/* uid to override on-disk uid */
	kgid_t s_gid;			/* gid to override on-disk gid */

	/* TODO: handle block sizes above the maximum of PAGE_SIZE? */
	unsigned long s_blocksize;
	unsigned char s_blocksize_bits;

	struct inode *s_private_dir;	/* Inode for the private directory */

	struct apfs_spaceman s_spaceman;
	struct apfs_transaction s_transaction;

	/* For now, a single semaphore for every operation */
	struct rw_semaphore s_big_sem;
};

static inline struct apfs_sb_info *APFS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline bool apfs_is_case_insensitive(struct super_block *sb)
{
	return (APFS_SB(sb)->s_vsb_raw->apfs_incompatible_features &
	       cpu_to_le64(APFS_INCOMPAT_CASE_INSENSITIVE)) != 0;
}

/**
 * apfs_max_maps_per_block - Find the maximum map count for a mapping block
 * @sb: superblock structure
 */
static inline int apfs_max_maps_per_block(struct super_block *sb)
{
	unsigned long maps_size;

	maps_size = (sb->s_blocksize - sizeof(struct apfs_checkpoint_map_phys));
	return maps_size / sizeof(struct apfs_checkpoint_mapping);
}

/*
 * In-memory representation of a key, as relevant for a b-tree query.
 */
struct apfs_key {
	u64		id;
	u64		number;	/* Extent offset, name hash or transaction id */
	const char	*name;	/* On-disk name string */
	u8		type;	/* Record type (0 for the omap) */
};

/**
 * apfs_init_free_queue_key - Initialize an in-memory key for a free queue query
 * @xid:	transaction id
 * @paddr:	block number
 * @key:	apfs_key structure to initialize
 */
static inline void apfs_init_free_queue_key(u64 xid, u64 paddr,
					    struct apfs_key *key)
{
	key->id = xid;
	key->type = 0;
	key->number = paddr;
	key->name = NULL;
}

/**
 * apfs_init_omap_key - Initialize an in-memory key for an omap query
 * @oid:	object id
 * @xid:	latest transaction id
 * @key:	apfs_key structure to initialize
 */
static inline void apfs_init_omap_key(u64 oid, u64 xid, struct apfs_key *key)
{
	key->id = oid;
	key->type = 0;
	key->number = xid;
	key->name = NULL;
}

/**
 * apfs_init_inode_key - Initialize an in-memory key for an inode query
 * @ino:	inode number
 * @key:	apfs_key structure to initialize
 */
static inline void apfs_init_inode_key(u64 ino, struct apfs_key *key)
{
	key->id = ino;
	key->type = APFS_TYPE_INODE;
	key->number = 0;
	key->name = NULL;
}

/**
 * apfs_init_file_extent_key - Initialize an in-memory key for an extent query
 * @id:		extent id
 * @offset:	logical address (0 for a multiple query)
 * @key:	apfs_key structure to initialize
 */
static inline void apfs_init_file_extent_key(u64 id, u64 offset,
					     struct apfs_key *key)
{
	key->id = id;
	key->type = APFS_TYPE_FILE_EXTENT;
	key->number = offset;
	key->name = NULL;
}

/**
 * apfs_init_sibling_link_key - Initialize an in-memory key for a sibling query
 * @ino:	inode number
 * @id:		sibling id
 * @key:	apfs_key structure to initialize
 */
static inline void apfs_init_sibling_link_key(u64 ino, u64 id,
					      struct apfs_key *key)
{
	key->id = ino;
	key->type = APFS_TYPE_SIBLING_LINK;
	key->number = id; /* Only guessing */
	key->name = NULL;
}

/**
 * apfs_init_sibling_map_key - Initialize in-memory key for a sibling map query
 * @id:		sibling id
 * @key:	apfs_key structure to initialize
 */
static inline void apfs_init_sibling_map_key(u64 id, struct apfs_key *key)
{
	key->id = id;
	key->type = APFS_TYPE_SIBLING_MAP;
	key->number = 0;
	key->name = NULL;
}

extern void apfs_init_drec_hashed_key(struct super_block *sb, u64 ino,
				      const char *name, struct apfs_key *key);

/**
 * apfs_init_xattr_key - Initialize an in-memory key for a xattr query
 * @ino:	inode number of the parent file
 * @name:	xattr name (NULL for a multiple query)
 * @key:	apfs_key structure to initialize
 */
static inline void apfs_init_xattr_key(u64 ino, const char *name,
				       struct apfs_key *key)
{
	key->id = ino;
	key->type = APFS_TYPE_XATTR;
	key->number = 0;
	key->name = name;
}

/**
 * apfs_key_set_hdr - Set the header for a raw catalog key
 * @type:	record type
 * @id:		record id
 * @key:	the key to initialize
 */
static inline void apfs_key_set_hdr(u64 type, u64 id, void *key)
{
	struct apfs_key_header *hdr = key;

	hdr->obj_id_and_type = cpu_to_le64(id | type << APFS_OBJ_TYPE_SHIFT);
}

/* Flags for the query structure */
#define APFS_QUERY_TREE_MASK	0007	/* Which b-tree we query */
#define APFS_QUERY_OMAP		0001	/* This is a b-tree object map query */
#define APFS_QUERY_CAT		0002	/* This is a catalog tree query */
#define APFS_QUERY_FREE_QUEUE	0004	/* This is a free queue query */
#define APFS_QUERY_NEXT		0010	/* Find next of multiple matches */
#define APFS_QUERY_EXACT	0020	/* Search for an exact match */
#define APFS_QUERY_DONE		0040	/* The search at this level is over */
#define APFS_QUERY_ANY_NAME	0100	/* Multiple search for any name */
#define APFS_QUERY_ANY_NUMBER	0200	/* Multiple search for any number */
#define APFS_QUERY_MULTIPLE	(APFS_QUERY_ANY_NAME | APFS_QUERY_ANY_NUMBER)

/*
 * Structure used to retrieve data from an APFS B-Tree. For now only used
 * on the calalog and the object map.
 */
struct apfs_query {
	struct apfs_node *node;		/* Node being searched */
	struct apfs_key *key;		/* What the query is looking for */

	struct apfs_query *parent;	/* Query for parent node */
	unsigned int flags;

	/* Set by the query on success */
	int index;			/* Index of the entry in the node */
	int key_off;			/* Offset of the key in the node */
	int key_len;			/* Length of the key */
	int off;			/* Offset of the data in the node */
	int len;			/* Length of the data */

	int depth;			/* Put a limit on recursion */
};

/**
 * apfs_query_storage - Get the storage type for a query's btree
 * @query: the query structure
 */
static inline u32 apfs_query_storage(struct apfs_query *query)
{
	if (query->flags & APFS_QUERY_OMAP)
		return APFS_OBJ_PHYSICAL;
	if (query->flags & APFS_QUERY_CAT)
		return APFS_OBJ_VIRTUAL;
	if (query->flags & APFS_QUERY_FREE_QUEUE)
		return APFS_OBJ_EPHEMERAL;
	BUG();
}

/*
 * Extent record data in memory
 */
struct apfs_file_extent {
	u64 logical_addr;
	u64 phys_block_num;
	u64 len;
};

/*
 * APFS inode data in memory
 */
struct apfs_inode_info {
	u64			i_ino64;	 /* 32-bit-safe inode number */
	u64			i_parent_id;	 /* ID of primary parent */
	u64			i_extent_id;	 /* ID of the extent records */
	struct apfs_file_extent	i_cached_extent; /* Latest extent record */
	spinlock_t		i_extent_lock;	 /* Protects i_cached_extent */
	struct timespec		i_crtime;	 /* Time of creation */
	u32			i_nchildren;	 /* Child count for directory */
	uid_t			i_saved_uid;	 /* User ID on disk */
	gid_t			i_saved_gid;	 /* Group ID on disk */

	struct inode vfs_inode;
};

static inline struct apfs_inode_info *APFS_I(const struct inode *inode)
{
	return container_of(inode, struct apfs_inode_info, vfs_inode);
}

/**
 * apfs_ino - Get the 64-bit id of an inode
 * @inode: the vfs inode
 *
 * Returns all 64 bits of @inode's id, even on 32-bit architectures.
 */
static inline u64 apfs_ino(const struct inode *inode)
{
	return APFS_I(inode)->i_ino64;
}

/**
 * apfs_set_ino - Set a 64-bit id on an inode
 * @inode: the vfs inode
 * @id:	   id to set
 *
 * Sets both the vfs inode number and the actual 32-bit-safe id.
 */
static inline void apfs_set_ino(struct inode *inode, u64 id)
{
	inode->i_ino = id; /* Higher bits may be lost, but it doesn't matter */
	APFS_I(inode)->i_ino64 = id;
}

/* Make the compiler complain if we ever access i_ino directly by mistake */
#define i_ino	DONT_USE_I_INO

/*
 * Directory entry record in memory
 */
struct apfs_drec {
	u8 *name;
	u64 ino;
	u64 sibling_id; /* The sibling id; 0 if none */
	int name_len;
	unsigned int type;
};

/*
 * Xattr record data in memory
 */
struct apfs_xattr {
	u8 *name;
	u8 *xdata;
	int name_len;
	int xdata_len;
	bool has_dstream;
};

#define apfs_emerg(sb, fmt, ...) apfs_msg(sb, KERN_EMERG, fmt, ##__VA_ARGS__)
#define apfs_alert(sb, fmt, ...) apfs_msg(sb, KERN_ALERT, fmt, ##__VA_ARGS__)
#define apfs_crit(sb, fmt, ...) apfs_msg(sb, KERN_CRIT, fmt, ##__VA_ARGS__)
#define apfs_err(sb, fmt, ...) apfs_msg(sb, KERN_ERR, fmt, ##__VA_ARGS__)
#define apfs_warn(sb, fmt, ...) apfs_msg(sb, KERN_WARNING, fmt, ##__VA_ARGS__)
#define apfs_notice(sb, fmt, ...) apfs_msg(sb, KERN_NOTICE, fmt, ##__VA_ARGS__)
#define apfs_info(sb, fmt, ...) apfs_msg(sb, KERN_INFO, fmt, ##__VA_ARGS__)

#ifdef CONFIG_APFS_DEBUG
#define ASSERT(expr)	BUG_ON(!(expr))
#define apfs_debug(sb, fmt, ...) apfs_msg(sb, KERN_DEBUG, fmt, ##__VA_ARGS__)
#else
#define ASSERT(expr)	((void)0)
#define apfs_debug(sb, fmt, ...) no_printk(fmt, ##__VA_ARGS__)
#endif /* CONFIG_APFS_DEBUG */

/* btree.c */
extern struct apfs_query *apfs_alloc_query(struct apfs_node *node,
					   struct apfs_query *parent);
extern void apfs_free_query(struct super_block *sb, struct apfs_query *query);
extern int apfs_btree_query(struct super_block *sb, struct apfs_query **query);
extern struct apfs_node *apfs_omap_read_node(struct super_block *sb, u64 id);
extern int apfs_omap_lookup_block(struct super_block *sb, struct apfs_node *tbl,
				  u64 id, u64 *block, bool write);
extern int apfs_create_omap_rec(struct super_block *sb, u64 oid, u64 bno);
extern int apfs_delete_omap_rec(struct super_block *sb, u64 oid);
extern int apfs_query_join_transaction(struct apfs_query *query);
extern int apfs_btree_insert(struct apfs_query *query, void *key, int key_len,
			     void *val, int val_len);
extern int apfs_btree_remove(struct apfs_query *query);
extern void apfs_btree_change_node_count(struct apfs_query *query, int change);
extern int apfs_btree_replace(struct apfs_query *query, void *key, int key_len,
			      void *val, int val_len);

/* dir.c */
extern int apfs_inode_by_name(struct inode *dir, const struct qstr *child,
			      u64 *ino);
extern int apfs_mknod(struct inode *dir, struct dentry *dentry,
		      umode_t mode, dev_t rdev);
extern int apfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
extern int apfs_create(struct inode *dir, struct dentry *dentry, umode_t mode,
		       bool excl);
extern int apfs_link(struct dentry *old_dentry, struct inode *dir,
		     struct dentry *dentry);
extern int apfs_unlink(struct inode *dir, struct dentry *dentry);
extern int apfs_rmdir(struct inode *dir, struct dentry *dentry);
extern int apfs_rename(struct inode *old_dir, struct dentry *old_dentry,
		       struct inode *new_dir, struct dentry *new_dentry,
		       unsigned int flags);
extern int apfs_delete_orphan_link(struct inode *inode);

/* extents.c */
extern int apfs_extent_from_query(struct apfs_query *query,
				  struct apfs_file_extent *extent);
extern int apfs_get_block(struct inode *inode, sector_t iblock,
			  struct buffer_head *bh_result, int create);

/* inode.c */
extern struct inode *apfs_iget(struct super_block *sb, u64 cnid);
extern int apfs_update_inode(struct inode *inode, char *new_name);
extern void apfs_evict_inode(struct inode *inode);
extern int apfs_getattr(struct vfsmount *mnt, struct dentry *dentry,
		   struct kstat *stat);
extern struct inode *apfs_new_inode(struct inode *dir, umode_t mode,
				    dev_t rdev);
extern int apfs_create_inode_rec(struct super_block *sb, struct inode *inode,
				 struct dentry *dentry);

/* key.c */
extern int apfs_filename_cmp(struct super_block *sb,
			     const char *name1, const char *name2);
extern int apfs_keycmp(struct super_block *sb,
		       struct apfs_key *k1, struct apfs_key *k2);
extern int apfs_read_cat_key(void *raw, int size, struct apfs_key *key);
extern int apfs_read_free_queue_key(void *raw, int size, struct apfs_key *key);
extern int apfs_read_omap_key(void *raw, int size, struct apfs_key *key);

/* message.c */
extern __printf(3, 4)
void apfs_msg(struct super_block *sb, const char *prefix, const char *fmt, ...);

/* node.c */
extern struct apfs_node *apfs_read_node(struct super_block *sb, u64 oid,
					u32 storage, bool write);
extern void apfs_update_node(struct apfs_node *node);
extern int apfs_delete_node(struct apfs_query *query);
extern int apfs_node_query(struct super_block *sb, struct apfs_query *query);
extern int apfs_bno_from_query(struct apfs_query *query, u64 *bno);
extern void apfs_create_toc_entry(struct apfs_query *query);
extern int apfs_node_split(struct apfs_query *query);
extern int apfs_node_locate_key(struct apfs_node *node, int index, int *off);
extern void apfs_node_get(struct apfs_node *node);
extern void apfs_node_put(struct apfs_node *node);

/* object.c */
extern int apfs_obj_verify_csum(struct super_block *sb,
				struct apfs_obj_phys *obj);
extern void apfs_obj_set_csum(struct super_block *sb,
			      struct apfs_obj_phys *obj);
extern int apfs_create_cpoint_map(struct super_block *sb, u64 oid, u64 bno);
extern struct buffer_head *apfs_read_ephemeral_object(struct super_block *sb,
						      u64 oid);
extern struct buffer_head *apfs_read_object_block(struct super_block *sb,
						  u64 bno, bool write);

/* spaceman.c */
extern int apfs_read_spaceman(struct super_block *sb);
extern int apfs_free_queue_insert(struct super_block *sb, u64 bno);
extern int apfs_spaceman_allocate_block(struct super_block *sb, u64 *bno);

/* super.c */
extern int apfs_map_volume_super(struct super_block *sb, bool write);
extern int apfs_read_omap(struct super_block *sb, bool write);
extern int apfs_read_catalog(struct super_block *sb, bool write);

/* transaction.c */
extern void apfs_cpoint_data_allocate(struct super_block *sb, u64 *bno);
extern int apfs_transaction_start(struct super_block *sb);
extern int apfs_transaction_commit(struct super_block *sb);
extern int apfs_transaction_join(struct super_block *sb,
				 struct buffer_head *bh);
void apfs_transaction_abort(struct super_block *sb);

/* xattr.c */
extern int __apfs_xattr_get(struct inode *inode, const char *name, void *buffer,
			    size_t size);
extern int apfs_xattr_get(struct inode *inode, const char *name, void *buffer,
			  size_t size);
extern ssize_t apfs_listxattr(struct dentry *dentry, char *buffer, size_t size);

/* xfield.c */
extern int apfs_find_xfield(u8 *xfields, int len, u8 xtype, char **xval);
extern int apfs_init_xfields(u8 *buffer, int buflen);
extern int apfs_insert_xfield(u8 *buffer, int buflen,
			      const struct apfs_x_field *xkey,
			      const void *xval);

/*
 * Inode and file operations
 */

/* dir.c */
extern const struct file_operations apfs_dir_operations;

/* file.c */
extern const struct file_operations apfs_file_operations;
extern const struct inode_operations apfs_file_inode_operations;

/* namei.c */
extern const struct inode_operations apfs_dir_inode_operations;
extern const struct inode_operations apfs_special_inode_operations;
extern const struct dentry_operations apfs_dentry_operations;

/* symlink.c */
extern const struct inode_operations apfs_symlink_inode_operations;

/* xattr.c */
extern const struct xattr_handler *apfs_xattr_handlers[];

#endif	/* _APFS_H */
