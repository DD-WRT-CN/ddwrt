/*
 * cfg80211 debugfs
 *
 * Copyright 2009	Luis R. Rodriguez <lrodriguez@atheros.com>
 * Copyright 2007	Johannes Berg <johannes@sipsolutions.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/ctype.h>
#include "core.h"
#include "debugfs.h"

#define DEBUGFS_READONLY_FILE(name, buflen, fmt, value...)		\
static ssize_t name## _read(struct file *file, char __user *userbuf,	\
			    size_t count, loff_t *ppos)			\
{									\
	struct wiphy *wiphy = file->private_data;			\
	char buf[buflen];						\
	int res;							\
									\
	res = scnprintf(buf, buflen, fmt "\n", ##value);		\
	return simple_read_from_buffer(userbuf, count, ppos, buf, res);	\
}									\
									\
static const struct file_operations name## _ops = {			\
	.read = name## _read,						\
	.open = simple_open,						\
	.llseek = generic_file_llseek,					\
}

DEBUGFS_READONLY_FILE(rts_threshold, 20, "%d",
		      wiphy->rts_threshold);
DEBUGFS_READONLY_FILE(fragmentation_threshold, 20, "%d",
		      wiphy->frag_threshold);
DEBUGFS_READONLY_FILE(short_retry_limit, 20, "%d",
		      wiphy->retry_short);
DEBUGFS_READONLY_FILE(long_retry_limit, 20, "%d",
		      wiphy->retry_long);

static int ht_print_chan(struct ieee80211_channel *chan,
			 char *buf, int buf_size, int offset)
{
	if (WARN_ON(offset > buf_size))
		return 0;

	if (chan->flags & IEEE80211_CHAN_DISABLED)
		return scnprintf(buf + offset,
				 buf_size - offset,
				 "%d Disabled\n",
				 chan->center_freq);

	return scnprintf(buf + offset,
			 buf_size - offset,
			 "%d HT40 %c%c\n",
			 chan->center_freq,
			 (chan->flags & IEEE80211_CHAN_NO_HT40MINUS) ?
				' ' : '-',
			 (chan->flags & IEEE80211_CHAN_NO_HT40PLUS) ?
				' ' : '+');
}

static ssize_t ht40allow_map_read(struct file *file,
				  char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	struct wiphy *wiphy = file->private_data;
	char *buf;
	unsigned int offset = 0, buf_size = PAGE_SIZE, i, r;
	enum nl80211_band band;
	struct ieee80211_supported_band *sband;

	buf = kzalloc(buf_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	rtnl_lock();

	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		sband = wiphy->bands[band];
		if (!sband)
			continue;
		for (i = 0; i < sband->n_channels; i++)
			offset += ht_print_chan(&sband->channels[i],
						buf, buf_size, offset);
	}

	rtnl_unlock();

	r = simple_read_from_buffer(user_buf, count, ppos, buf, offset);

	kfree(buf);

	return r;
}

static const struct file_operations ht40allow_map_ops = {
	.read = ht40allow_map_read,
	.open = simple_open,
	.llseek = default_llseek,
};

static ssize_t chandata_write(struct file *file, const char __user *user_buf,
			      size_t count, loff_t *ppos)
{
	struct wiphy *wiphy = file->private_data;
	char *buf, *p, *next, *end;
	ssize_t ret = -EINVAL;
	int disabled;
	static bool no_enable=false;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf[count] = '\0';
	if (copy_from_user(buf, user_buf, count)) {
		ret = -EFAULT;
		goto out;
	}

	if (buf[count - 1] == '\n')
		buf[count - 1] = 0;

	ret = count;
	switch (buf[0]) {
	case '?':
		no_enable = false;
		goto out;
		break;
	case '@':
		no_enable = true;
		goto out;
		break;
	case '-':
		disabled = 1;
		break;
	case '+':
#ifndef HAVE_SUPERCHANNEL
		goto out;
#endif
		if (no_enable)
			goto out;
		disabled = 0;
		break;
	default:
		goto out;
	}

	rtnl_lock();

	for (p = buf + 1; *p; p = next) {
		struct ieee80211_supported_band *sband;
		enum nl80211_band band;
		struct ieee80211_channel *chan;
		int freq_start;
		int freq_end;
		int txpower = 0;
		int i;

		if (!isdigit(*p))
			goto out_unlock;

		end = strchr(p, ',');
		if (end)
			next = end + 1;
		else
			next = p + strlen(p);

		freq_start = simple_strtoul(p, &end, 10);
		freq_end = freq_start;

		if (*end == '-') {
			end++;

			if (!isdigit(*end))
				goto out_unlock;

			freq_end = simple_strtoul(end, &end, 10);
		}

		if (*end == ':') {
			end++;

			if (!isdigit(*end))
				goto out_unlock;

			txpower = simple_strtoul(end, &end, 10);
		}

		if (*end)
			end++;

		if (end != next)
			goto out_unlock;

		for (band = 0; band < NUM_NL80211_BANDS; band++) {
			sband = wiphy->bands[band];
			if (!sband)
				continue;

			for (i = 0; i < sband->n_channels; i++) {
				chan = &sband->channels[i];

				if (chan->center_freq - 20 >= freq_start &&
					chan->center_freq - 20 <= freq_end) {
					chan->flags &= ~IEEE80211_CHAN_NO_HT40MINUS;
					chan->flags |= disabled * IEEE80211_CHAN_NO_HT40MINUS;
				}

				if (chan->center_freq + 20 >= freq_start &&
					chan->center_freq + 20 <= freq_end) {
					chan->flags &= ~IEEE80211_CHAN_NO_HT40PLUS;
					chan->flags |= disabled * IEEE80211_CHAN_NO_HT40PLUS;
				}

				if (chan->center_freq >= freq_start &&
				    chan->center_freq <= freq_end) {
					chan->flags &= ~IEEE80211_CHAN_DISABLED;
					chan->flags |= disabled * IEEE80211_CHAN_DISABLED;
					if (txpower)
						chan->max_power = txpower;
				}

				if (disabled || !txpower)
					continue;
			}
		}
	}

out_unlock:
	rtnl_unlock();
out:
	kfree(buf);
	return ret;
}

static const struct file_operations chandata_ops = {
	.write = chandata_write,
	.open = simple_open,
	.llseek = default_llseek,
};



#define DEBUGFS_ADD(name)						\
	debugfs_create_file(#name, 0444, phyd, &rdev->wiphy, &name## _ops)

void cfg80211_debugfs_rdev_add(struct cfg80211_registered_device *rdev)
{
	struct dentry *phyd = rdev->wiphy.debugfsdir;

	DEBUGFS_ADD(rts_threshold);
	DEBUGFS_ADD(fragmentation_threshold);
	DEBUGFS_ADD(short_retry_limit);
	DEBUGFS_ADD(long_retry_limit);
	DEBUGFS_ADD(ht40allow_map);
	DEBUGFS_ADD(chandata);
}
