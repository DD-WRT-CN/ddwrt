/*
 * Broadcom SiliconBackplane chipcommon serial flash interface
 *
 * Copyright 2006, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: sflash.c,v 1.1.1.13 2006/02/27 03:43:16 honor Exp $
 */

#include <osl.h>
#include <typedefs.h>
#include <sbconfig.h>
#include <sbchipc.h>
#include <mipsinc.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <sflash.h>

/* Private global state */
static struct sflash sflash;

/* Issue a serial flash command */
static INLINE void
sflash_cmd(chipcregs_t *cc, uint opcode)
{
	W_REG(NULL, &cc->flashcontrol, SFLASH_START | opcode);
	while (R_REG(NULL, &cc->flashcontrol) & SFLASH_BUSY);
}

/* Initialize serial flash access */
struct sflash *
sflash_init(chipcregs_t *cc)
{
	uint32 id, id2;

	bzero(&sflash, sizeof(sflash));

	sflash.type = R_REG(NULL, &cc->capabilities) & CAP_FLASH_MASK;

	switch (sflash.type) {
	case SFLASH_ST:
		/* Probe for ST chips */
		sflash_cmd(cc, SFLASH_ST_DP);
		sflash_cmd(cc, SFLASH_ST_RES);
		id = R_REG(NULL, &cc->flashdata);
		switch (id) {
		case 0x11:
			/* ST M25P20 2 Mbit Serial Flash */
			sflash.blocksize = 64 * 1024;
			sflash.numblocks = 4;
			break;
		case 0x12:
			/* ST M25P40 4 Mbit Serial Flash */
			sflash.blocksize = 64 * 1024;
			sflash.numblocks = 8;
			break;
		case 0x13:
			/* ST M25P80 8 Mbit Serial Flash */
			sflash.blocksize = 64 * 1024;
			sflash.numblocks = 16;
			break;
		case 0x14:
			/* ST M25P16 16 Mbit Serial Flash */
			sflash.blocksize = 64 * 1024;
			sflash.numblocks = 32;
			break;
		case 0x15:
			/* ST M25P32 32 Mbit Serial Flash */
			sflash.blocksize = 64 * 1024;
			sflash.numblocks = 64;
			break;
		case 0x16:
			/* ST M25P64 64 Mbit Serial Flash */
			sflash.blocksize = 64 * 1024;
			sflash.numblocks = 128;
			break;
		case 0xbf:
			W_REG(NULL, &cc->flashaddress, 1);
			sflash_cmd(cc, SFLASH_ST_RES);
			id2 = R_REG(NULL, &cc->flashdata);
			if (id2 == 0x44) {
				/* SST M25VF80 4 Mbit Serial Flash */
				sflash.blocksize = 64 * 1024;
				sflash.numblocks = 8;
			}
			break;
		}
		break;

	case SFLASH_AT:
		/* Probe for Atmel chips */
		sflash_cmd(cc, SFLASH_AT_STATUS);
		id = R_REG(NULL, &cc->flashdata) & 0x3c;
		switch (id) {
		case 0xc:
			/* Atmel AT45DB011 1Mbit Serial Flash */
			sflash.blocksize = 256;
			sflash.numblocks = 512;
			break;
		case 0x14:
			/* Atmel AT45DB021 2Mbit Serial Flash */
			sflash.blocksize = 256;
			sflash.numblocks = 1024;
			break;
		case 0x1c:
			/* Atmel AT45DB041 4Mbit Serial Flash */
			sflash.blocksize = 256;
			sflash.numblocks = 2048;
			break;
		case 0x24:
			/* Atmel AT45DB081 8Mbit Serial Flash */
			sflash.blocksize = 256;
			sflash.numblocks = 4096;
			break;
		case 0x2c:
			/* Atmel AT45DB161 16Mbit Serial Flash */
			sflash.blocksize = 512;
			sflash.numblocks = 4096;
			break;
		case 0x34:
			/* Atmel AT45DB321 32Mbit Serial Flash */
			sflash.blocksize = 512;
			sflash.numblocks = 8192;
			break;
		case 0x3c:
			/* Atmel AT45DB642 64Mbit Serial Flash */
			sflash.blocksize = 1024;
			sflash.numblocks = 8192;
			break;
		}
		break;
	}

	sflash.size = sflash.blocksize * sflash.numblocks;
	return sflash.size ? &sflash : NULL;
}

/* Read len bytes starting at offset into buf. Returns number of bytes read. */
int
sflash_read(chipcregs_t *cc, uint offset, uint len, uchar *buf)
{
	int cnt;
	uint32 *from, *to;

	if (!len)
		return 0;

	if ((offset + len) > sflash.size)
		return -22;

	if ((len >= 4) && (offset & 3))
		cnt = 4 - (offset & 3);
	else if ((len >= 4) && ((uint32)buf & 3))
		cnt = 4 - ((uint32)buf & 3);
	else
		cnt = len;

	from = (uint32 *)KSEG1ADDR(SB_FLASH2 + offset);
	to = (uint32 *)buf;

	if (cnt < 4) {
		bcopy(from, to, cnt);
		return cnt;
	}

	while (cnt >= 4) {
		*to++ = *from++;
		cnt -= 4;
	}

	return (len - cnt);
}

/* Poll for command completion. Returns zero when complete. */
int
sflash_poll(chipcregs_t *cc, uint offset)
{
	if (offset >= sflash.size)
		return -22;

	switch (sflash.type) {
	case SFLASH_ST:
		/* Check for ST Write In Progress bit */
		sflash_cmd(cc, SFLASH_ST_RDSR);
		return R_REG(NULL, &cc->flashdata) & SFLASH_ST_WIP;
	case SFLASH_AT:
		/* Check for Atmel Ready bit */
		sflash_cmd(cc, SFLASH_AT_STATUS);
		return !(R_REG(NULL, &cc->flashdata) & SFLASH_AT_READY);
	}

	return 0;
}

/* Write len bytes starting at offset into buf. Returns number of bytes
 * written. Caller should poll for completion.
 */
int
sflash_write(chipcregs_t *cc, uint offset, uint len, const uchar *buf)
{
	struct sflash *sfl;
	int ret = 0;
	bool is4712b0;
	uint32 page, byte, mask;

	if (!len)
		return 0;

	if ((offset + len) > sflash.size)
		return -22;

	sfl = &sflash;
	switch (sfl->type) {
	case SFLASH_ST:
		mask = R_REG(NULL, &cc->chipid);
		is4712b0 = (((mask & CID_ID_MASK) == BCM4712_CHIP_ID) &&
		            ((mask & CID_REV_MASK) == (3 << CID_REV_SHIFT)));
		/* Enable writes */
		sflash_cmd(cc, SFLASH_ST_WREN);
		if (is4712b0) {
			mask = 1 << 14;
			W_REG(NULL, &cc->flashaddress, offset);
			W_REG(NULL, &cc->flashdata, *buf++);
			/* Set chip select */
			OR_REG(NULL, &cc->gpioout, mask);
			/* Issue a page program with the first byte */
			sflash_cmd(cc, SFLASH_ST_PP);
			ret = 1;
			offset++;
			len--;
			while (len > 0) {
				if ((offset & 255) == 0) {
					/* Page boundary, drop cs and return */
					AND_REG(NULL, &cc->gpioout, ~mask);
					if (!sflash_poll(cc, offset)) {
						/* Flash rejected command */
						return -11;
					}
					return ret;
				} else {
					/* Write single byte */
					sflash_cmd(cc, *buf++);
				}
				ret++;
				offset++;
				len--;
			}
			/* All done, drop cs if needed */
			if ((offset & 255) != 1) {
				/* Drop cs */
				AND_REG(NULL, &cc->gpioout, ~mask);
				if (!sflash_poll(cc, offset)) {
					/* Flash rejected command */
					return -12;
				}
			}
		} else {
			ret = 1;
			W_REG(NULL, &cc->flashaddress, offset);
			W_REG(NULL, &cc->flashdata, *buf);
			/* Page program */
			sflash_cmd(cc, SFLASH_ST_PP);
		}
		break;
	case SFLASH_AT:
		mask = sfl->blocksize - 1;
		page = (offset & ~mask) << 1;
		byte = offset & mask;
		/* Read main memory page into buffer 1 */
		if (byte || (len < sfl->blocksize)) {
			W_REG(NULL, &cc->flashaddress, page);
			sflash_cmd(cc, SFLASH_AT_BUF1_LOAD);
			/* 250 us for AT45DB321B */
			SPINWAIT(sflash_poll(cc, offset), 1000);
			ASSERT(!sflash_poll(cc, offset));
		}
		/* Write into buffer 1 */
		for (ret = 0; (ret < (int)len) && (byte < sfl->blocksize); ret++) {
			W_REG(NULL, &cc->flashaddress, byte++);
			W_REG(NULL, &cc->flashdata, *buf++);
			sflash_cmd(cc, SFLASH_AT_BUF1_WRITE);
		}
		/* Write buffer 1 into main memory page */
		W_REG(NULL, &cc->flashaddress, page);
		sflash_cmd(cc, SFLASH_AT_BUF1_PROGRAM);
		break;
	}

	return ret;
}

/* Erase a region. Returns number of bytes scheduled for erasure.
 * Caller should poll for completion.
 */
int
sflash_erase(chipcregs_t *cc, uint offset)
{
	struct sflash *sfl;

	if (offset >= sflash.size)
		return -22;

	sfl = &sflash;
	switch (sfl->type) {
	case SFLASH_ST:
		sflash_cmd(cc, SFLASH_ST_WREN);
		W_REG(NULL, &cc->flashaddress, offset);
		sflash_cmd(cc, SFLASH_ST_SE);
		return sfl->blocksize;
	case SFLASH_AT:
		W_REG(NULL, &cc->flashaddress, offset << 1);
		sflash_cmd(cc, SFLASH_AT_PAGE_ERASE);
		return sfl->blocksize;
	}

	return 0;
}

/*
 * writes the appropriate range of flash, a NULL buf simply erases
 * the region of flash
 */
int
sflash_commit(chipcregs_t *cc, uint offset, uint len, const uchar *buf)
{
	struct sflash *sfl;
	uchar *block = NULL, *cur_ptr, *blk_ptr;
	uint blocksize = 0, mask, cur_offset, cur_length, cur_retlen, remainder;
	uint blk_offset, blk_len, copied;
	int bytes, ret = 0;

	/* Check address range */
	if (len <= 0)
		return 0;

	sfl = &sflash;
	if ((offset + len) > sfl->size)
		return -1;

	blocksize = sfl->blocksize;
	mask = blocksize - 1;

	/* Allocate a block of mem */
	if (!(block = MALLOC(NULL, blocksize)))
		return -1;

	while (len) {
		/* Align offset */
		cur_offset = offset & ~mask;
		cur_length = blocksize;
		cur_ptr = block;

		remainder = blocksize - (offset & mask);
		if (len < remainder)
			cur_retlen = len;
		else
			cur_retlen = remainder;

		/* buf == NULL means erase only */
		if (buf) {
			/* Copy existing data into holding block if necessary */
			if ((offset & mask) || (len < blocksize)) {
				blk_offset = cur_offset;
				blk_len = cur_length;
				blk_ptr = cur_ptr;

				/* Copy entire block */
				while (blk_len) {
					copied = sflash_read(cc, blk_offset, blk_len, blk_ptr);
					blk_offset += copied;
					blk_len -= copied;
					blk_ptr += copied;
				}
			}

			/* Copy input data into holding block */
			memcpy(cur_ptr + (offset & mask), buf, cur_retlen);
		}

		/* Erase block */
		if ((ret = sflash_erase(cc, (uint) cur_offset)) < 0)
			goto done;
		while (sflash_poll(cc, (uint) cur_offset));

		/* buf == NULL means erase only */
		if (!buf) {
			offset += cur_retlen;
			len -= cur_retlen;
			continue;
		}

		/* Write holding block */
		while (cur_length > 0) {
			if ((bytes = sflash_write(cc,
			                          (uint) cur_offset,
			                          (uint) cur_length,
			                          (uchar *) cur_ptr)) < 0) {
				ret = bytes;
				goto done;
			}
			while (sflash_poll(cc, (uint) cur_offset));
			cur_offset += bytes;
			cur_length -= bytes;
			cur_ptr += bytes;
		}

		offset += cur_retlen;
		len -= cur_retlen;
		buf += cur_retlen;
	}

	ret = len;
done:
	if (block)
		MFREE(NULL, block, blocksize);
	return ret;
}
