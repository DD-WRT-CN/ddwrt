// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#ifndef __XFS_INODE_H__
#define __XFS_INODE_H__

/* These match kernel side includes */
#include "xfs_inode_buf.h"
#include "xfs_inode_fork.h"

struct xfs_trans;
struct xfs_mount;
struct xfs_inode_log_item;

/*
 * These are not actually used, they are only for userspace build
 * compatibility in code that looks at i_state
 */
#define I_DIRTY_TIME		0
#define I_DIRTY_TIME_EXPIRED	0

#define IS_I_VERSION(inode)			(0)
#define inode_maybe_inc_iversion(inode,flags)	(0)

/*
 * Inode interface. This fakes up a "VFS inode" to make the xfs_inode appear
 * similar to the kernel which now is used tohold certain parts of the on-disk
 * metadata.
 */
struct inode {
	mode_t		i_mode;
	uint32_t	i_uid;
	uint32_t	i_gid;
	uint32_t	i_nlink;
	xfs_dev_t	i_rdev;		/* This actually holds xfs_dev_t */
	unsigned long	i_state;	/* Not actually used in userspace */
	uint32_t	i_generation;
	uint64_t	i_version;
	struct timespec	i_atime;
	struct timespec	i_mtime;
	struct timespec	i_ctime;
};

static inline uint32_t i_uid_read(struct inode *inode)
{
	return inode->i_uid;
}
static inline uint32_t i_gid_read(struct inode *inode)
{
	return inode->i_gid;
}
static inline void i_uid_write(struct inode *inode, uint32_t uid)
{
	inode->i_uid = uid;
}
static inline void i_gid_write(struct inode *inode, uint32_t gid)
{
	inode->i_gid = gid;
}

typedef struct xfs_inode {
	struct cache_node	i_node;
	struct xfs_mount	*i_mount;	/* fs mount struct ptr */
	xfs_ino_t		i_ino;		/* inode number (agno/agino) */
	struct xfs_imap		i_imap;		/* location for xfs_imap() */
	struct xfs_buftarg	i_dev;		/* dev for this inode */
	struct xfs_ifork	*i_afp;		/* attribute fork pointer */
	struct xfs_ifork	*i_cowfp;	/* copy on write extents */
	struct xfs_ifork	i_df;		/* data fork */
	struct xfs_inode_log_item *i_itemp;	/* logging information */
	unsigned int		i_delayed_blks;	/* count of delay alloc blks */
	struct xfs_icdinode	i_d;		/* most of ondisk inode */

	xfs_extnum_t		i_cnextents;	/* # of extents in cow fork */
	unsigned int		i_cformat;	/* format of cow fork */

	xfs_fsize_t		i_size;		/* in-memory size */
	struct inode		i_vnode;
} xfs_inode_t;

/* Convert from vfs inode to xfs inode */
static inline struct xfs_inode *XFS_I(struct inode *inode)
{
	return container_of(inode, struct xfs_inode, i_vnode);
}

/* convert from xfs inode to vfs inode */
static inline struct inode *VFS_I(struct xfs_inode *ip)
{
	return &ip->i_vnode;
}

/* We only have i_size in the xfs inode in userspace */
static inline loff_t i_size_read(struct inode *inode)
{
	return XFS_I(inode)->i_size;
}

/*
 * wrappers around the mode checks to simplify code
 */
static inline bool XFS_ISREG(struct xfs_inode *ip)
{
	return S_ISREG(VFS_I(ip)->i_mode);
}

static inline bool XFS_ISDIR(struct xfs_inode *ip)
{
	return S_ISDIR(VFS_I(ip)->i_mode);
}

/*
 * For regular files we only update the on-disk filesize when actually
 * writing data back to disk.  Until then only the copy in the VFS inode
 * is uptodate.
 */
static inline xfs_fsize_t XFS_ISIZE(struct xfs_inode *ip)
{
	if (XFS_ISREG(ip))
		return ip->i_size;
	return ip->i_d.di_size;
}
#define XFS_IS_REALTIME_INODE(ip) ((ip)->i_d.di_flags & XFS_DIFLAG_REALTIME)

/* inode link counts */
static inline void set_nlink(struct inode *inode, uint32_t nlink)
{
	inode->i_nlink = nlink;
}
static inline void inc_nlink(struct inode *inode)
{
	inode->i_nlink++;
}

static inline bool xfs_is_reflink_inode(struct xfs_inode *ip)
{
	return ip->i_d.di_flags2 & XFS_DIFLAG2_REFLINK;
}

typedef struct cred {
	uid_t	cr_uid;
	gid_t	cr_gid;
} cred_t;

extern int	libxfs_inode_alloc (struct xfs_trans **, struct xfs_inode *,
				mode_t, nlink_t, xfs_dev_t, struct cred *,
				struct fsxattr *, struct xfs_inode **);
extern void	libxfs_trans_inode_alloc_buf (struct xfs_trans *,
				struct xfs_buf *);

extern void	libxfs_trans_ichgtime(struct xfs_trans *,
				struct xfs_inode *, int);
extern int	libxfs_iflush_int (struct xfs_inode *, struct xfs_buf *);

extern struct timespec64 current_time(struct inode *inode);

/* Inode Cache Interfaces */
extern int	libxfs_iget(struct xfs_mount *, struct xfs_trans *, xfs_ino_t,
				uint, struct xfs_inode **);
extern void	libxfs_irele(struct xfs_inode *ip);

#endif /* __XFS_INODE_H__ */
