/*
 * Broadcom SiliconBackplane chipcommon serial flash interface
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id$
 */

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
#include <linux/config.h>
#endif
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
#include <linux/mtd/compatmac.h>
#endif
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include "../../mtdcore.h"
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <asm/io.h>

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <bcmnvram.h>
#include <siutils.h>
#include <hndpci.h>
#include <pcicfg.h>
#include <hndsoc.h>
#include <hndsflash.h>

#ifdef CONFIG_MIPS_BRCM
extern struct mtd_partition cfe_nvrampart;
extern struct mtd_partition *
init_mtd_partitions(struct mtd_info *mtd, size_t size);
#else
extern struct mtd_partition *
init_mtd_partitions(hndsflash_t *sfl_info,struct mtd_info *mtd, size_t size);
#endif
extern void *partitions_lock_init(void);
#define	BCMSFLASH_LOCK(lock)		if (lock) spin_lock(lock)
#define	BCMSFLASH_UNLOCK(lock)	if (lock) spin_unlock(lock)

struct bcmsflash_mtd {
	si_t *sih;
	hndsflash_t *sfl;
	struct mtd_info mtd;
	struct mtd_erase_region_info region;
};

/* Private global state */
static struct bcmsflash_mtd bcmsflash;
#ifdef CONFIG_MIPS
extern struct mtd_partition cfe_boardpart;

void add_netgear_boarddata_sflash(void)
{
	if (bcmsflash.mtd.name && cfe_boardpart.name)
		add_mtd_partitions(&bcmsflash.mtd, &cfe_boardpart, 1);
}
void add_cfenvram(void)
{
	if (bcmsflash.mtd.name && cfe_nvrampart.name)
		add_mtd_partitions(&bcmsflash.mtd, &cfe_nvrampart, 1);
}
#endif
static int
bcmsflash_mtd_poll(hndsflash_t *sfl, unsigned int offset, int timeout)
{
	int now = jiffies;
	int ret = 0;

	for (;;) {
		if (!hndsflash_poll(sfl, offset))
			break;

		if (time_after((unsigned long)jiffies, (unsigned long)now + timeout)) {
			if (!hndsflash_poll(sfl, offset))
				break;

			printk(KERN_ERR "sflash: timeout\n");
			ret = -ETIMEDOUT;
			break;
		}
		udelay(1);
	}

	return ret;
}

static int
bcmsflash_mtd_read(struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, u_char *buf)
{
	hndsflash_t *sfl = ((struct bcmsflash_mtd *)mtd->priv)->sfl;
	int bytes, ret = 0;
	/* Check address range */
	if (!len)
		return 0;
	if ((from + len) > mtd->size)
		return -EINVAL;
	BCMSFLASH_LOCK(mtd->mlock);

	*retlen = 0;
	while (len) {
		if ((bytes = hndsflash_read(sfl, (uint)from, len, buf))
			< 0) {
			ret = bytes;
			break;
		}
		from += (loff_t) bytes;
		len -= bytes;
		buf += bytes;
		*retlen += bytes;
	}

	BCMSFLASH_UNLOCK(mtd->mlock);
	return ret;
}

static int
bcmsflash_mtd_write(struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen, const u_char *buf)
{
	hndsflash_t *sfl = ((struct bcmsflash_mtd *)mtd->priv)->sfl;
	int bytes, ret = 0;

	/* Check address range */
	if (!len)
		return 0;
	if ((to + len) > mtd->size)
		return -EINVAL;

	BCMSFLASH_LOCK(mtd->mlock);
	*retlen = 0;
	while (len) {
		if ((bytes = hndsflash_write(sfl, (uint)to, len, (u_char *)buf))
			< 0) {
			ret = bytes;
			break;
		}
		if ((ret = bcmsflash_mtd_poll(sfl, (unsigned int)to, HZ)))
			break;
		to += (loff_t) bytes;
		len -= bytes;
		buf += bytes;
		*retlen += bytes;
	}

	BCMSFLASH_UNLOCK(mtd->mlock);
	return ret;
}

static int
bcmsflash_mtd_erase(struct mtd_info *mtd, struct erase_info *erase)
{
	hndsflash_t *sfl = ((struct bcmsflash_mtd *)mtd->priv)->sfl;
	int i, j, ret = 0;
	unsigned int addr, len;

	/* Check address range */
	if (!erase->len)
		return 0;
	if ((erase->addr + erase->len) > mtd->size)
		return -EINVAL;

	BCMSFLASH_LOCK(mtd->mlock);
	addr = erase->addr;
	len = erase->len;

	/* Ensure that requested region is aligned */
	for (i = 0; i < mtd->numeraseregions; i++) {
		for (j = 0; j < mtd->eraseregions[i].numblocks; j++) {
			if (addr == mtd->eraseregions[i].offset +
			    mtd->eraseregions[i].erasesize * j &&
			    len >= mtd->eraseregions[i].erasesize) {
				if ((ret = hndsflash_erase(sfl, addr)) < 0)
					break;
				if ((ret = bcmsflash_mtd_poll(sfl, addr, 10 * HZ)))
					break;
				addr += mtd->eraseregions[i].erasesize;
				len -= mtd->eraseregions[i].erasesize;
			}
		}
		if (ret)
			break;
	}

	/* Set erase status */
	if (ret < 0)
		erase->state = MTD_ERASE_FAILED;
	else {
		erase->state = MTD_ERASE_DONE;
		ret = 0;
	}

	BCMSFLASH_UNLOCK(mtd->mlock);

	/* Call erase callback */
	if (erase->callback)
		erase->callback(erase);

	return ret;
}

extern void add_cfenvram(void);
static int __init
bcmsflash_mtd_init(void)
{
	int ret = 0;
	hndsflash_t *info;
	struct mtd_partition *parts;
	int i;
	uint boardnum = bcm_strtoul(nvram_safe_get("boardnum"), NULL, 0);

	memset(&bcmsflash, 0, sizeof(struct bcmsflash_mtd));

	/* attach to the backplane */
	if (!(bcmsflash.sih = si_kattach(SI_OSH))) {
		printk(KERN_ERR "bcmsflash: error attaching to backplane\n");
		ret = -EIO;
		goto fail;
	}

	/* Initialize serial flash access */
	if (!(info = hndsflash_init(bcmsflash.sih))) {
		printk(KERN_ERR "bcmsflash: found no supported devices\n");
		ret = -ENODEV;
		goto fail;
	}
	if (boardnum == 1234 && nvram_match("boardrev","0x1100"))
	    info->size = 8*1024*1024;  // truncate to 8 MB

	bcmsflash.sfl = info;

	/* Setup region info */
	bcmsflash.region.offset = 0;
	bcmsflash.region.erasesize = info->blocksize;
	bcmsflash.region.numblocks = info->numblocks;
	if (bcmsflash.region.erasesize > bcmsflash.mtd.erasesize)
		bcmsflash.mtd.erasesize = bcmsflash.region.erasesize;
	bcmsflash.mtd.size = info->size;
	bcmsflash.mtd.numeraseregions = 1;

	/* Register with MTD */
	bcmsflash.mtd.name = "bcmsflash";
	bcmsflash.mtd.type = MTD_NORFLASH;
	bcmsflash.mtd.flags = MTD_CAP_NORFLASH;
	bcmsflash.mtd.eraseregions = &bcmsflash.region;
	bcmsflash.mtd._erase = bcmsflash_mtd_erase;
	bcmsflash.mtd._read = bcmsflash_mtd_read;
	bcmsflash.mtd._write = bcmsflash_mtd_write;
	bcmsflash.mtd.writesize = 1;
	bcmsflash.mtd.priv = &bcmsflash;
	bcmsflash.mtd.owner = THIS_MODULE;
	bcmsflash.mtd.mlock = partitions_lock_init();
	if (!bcmsflash.mtd.mlock)
		return -ENOMEM;

#ifdef CONFIG_MIPS_BRCM
	parts = init_mtd_partitions(&bcmsflash.mtd, bcmsflash.mtd.size);
#else
	parts = init_mtd_partitions(info, &bcmsflash.mtd, bcmsflash.mtd.size);
#endif

	if (parts) {
		for (i = 0; parts[i].name; i++);
		ret = add_mtd_partitions(&bcmsflash.mtd, parts, i);
		if (ret) {
			printk(KERN_ERR "bcmsflash: add_mtd failed\n");
			goto fail;
		}
	}

#ifdef CONFIG_MIPS_BRCM
#ifndef CONFIG_MTD_NFLASH	
	add_cfenvram();
#endif
#endif

	return 0;

fail:
	return ret;
}

static void __exit
bcmsflash_mtd_exit(void)
{
#ifdef CONFIG_MTD_PARTITIONS
	del_mtd_partitions(&bcmsflash.mtd);
#else
	del_mtd_device(&bcmsflash.mtd);
#endif
}

module_init(bcmsflash_mtd_init);
module_exit(bcmsflash_mtd_exit);
