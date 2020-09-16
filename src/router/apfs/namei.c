// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Ernesto A. Fernández <ernesto.mnd.fernandez@gmail.com>
 */

#include "apfs.h"
#include "unicode.h"

static struct dentry *apfs_lookup(struct inode *dir, struct dentry *dentry,
				  unsigned int flags)
{
	struct inode *inode = NULL;
	u64 ino = 0;
	int err;

	if (dentry->d_name.len > APFS_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);

	err = apfs_inode_by_name(dir, &dentry->d_name, &ino);
	if (err && err != -ENODATA)
		return ERR_PTR(err);

	if (!err) {
		inode = apfs_iget(dir->i_sb, ino);
		if (IS_ERR(inode))
			return ERR_CAST(inode);
	}

	return d_splice_alias(inode, dentry);
}

const struct inode_operations apfs_dir_inode_operations = {
	.create		= apfs_create,
	.lookup		= apfs_lookup,
	.link		= apfs_link,
	.unlink		= apfs_unlink,
	.mkdir		= apfs_mkdir,
	.rmdir		= apfs_rmdir,
	.mknod		= apfs_mknod,
	.rename		= apfs_rename,
	.getattr	= apfs_getattr,
	.listxattr      = apfs_listxattr,
};

const struct inode_operations apfs_special_inode_operations = {
	.getattr	= apfs_getattr,
	.listxattr      = apfs_listxattr,
};

static int apfs_dentry_hash(const struct dentry *dir, struct qstr *child)
{
	struct apfs_unicursor cursor;
	unsigned long hash;
	bool case_fold = apfs_is_case_insensitive(dir->d_sb);

	apfs_init_unicursor(&cursor, child->name);
	hash = init_name_hash(dir);

	while (1) {
		int i;
		unicode_t utf32;

		utf32 = apfs_normalize_next(&cursor, case_fold);
		if (!utf32)
			break;

		/* Hash the unicode character one byte at a time */
		for (i = 0; i < 4; ++i) {
			hash = partial_name_hash((u8)utf32, hash);
			utf32 = utf32 >> 8;
		}
	}
	child->hash = end_name_hash(hash);

	/* TODO: return error instead of truncating invalid UTF-8? */
	return 0;
}

static int apfs_dentry_compare(const struct dentry *dentry, unsigned int len,
			       const char *str, const struct qstr *name)
{
	return apfs_filename_cmp(dentry->d_sb, name->name, str);
}

const struct dentry_operations apfs_dentry_operations = {
	.d_hash		= apfs_dentry_hash,
	.d_compare	= apfs_dentry_compare,
};
