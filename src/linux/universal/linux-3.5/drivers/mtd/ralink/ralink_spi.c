/*
 * MTD SPI driver for ST M25Pxx flash chips
 *
 * Author: Mike Lavender, mike@steroidmicros.com
 *
 * Copyright (c) 2005, Intec Automation Inc.
 *
 * Some parts are based on lart.c by Abraham Van Der Merwe
 *
 * Cleaned up and generalized based on mtd_dataflash.c
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include "ralink_spi.h"
#include <linux/squashfs_fs.h>
#include "../mtdcore.h"
#include <linux/platform_device.h>



extern u32 get_surfboard_sysclk(void) ;

static struct mtd_partition rt2880_partitions[] = {
        {
                name:           "uboot",  /* mtdblock0 */
                size:           0x30000,  /* 192K */
                offset:         0,
        }, {
                name:           "uboot-config", /* mtdblock1 */
                size:           0x10000,  /* 64K */
                offset:         MTDPART_OFS_APPEND,
                mask_flags:     0  /* force read-only */
        }, {
                name:           "factory-defaults", /* mtdblock2 */
                size:           0x10000,  /* 64K */
                offset:         MTDPART_OFS_APPEND,
                mask_flags:     0  /* force read-only */
        }
	, {
                name:           "linux", /* mtdblock3 */
                size:           CONFIG_MTD_KERNEL_PART_SIZ,
                offset:         0x50000,
        }, 
    	    {
                name:           "rootfs", /* mtdblock4 */
                size:           MTDPART_SIZ_FULL,
                offset:         MTDPART_OFS_APPEND,
        }, {
                name:           "ddwrt", /* mtdblock4 */
                size:           0x10000,
                offset:         0x3f0000,
        }, {
                name:           "nvram", /* mtdblock4 */
                size:           0x10000,
                offset:         0x3f0000,
        }};



/******************************************************************************
 * SPI FLASH elementray definition and function
 ******************************************************************************/

#define FLASH_PAGESIZE		256
#define MX_4B_MODE			/* MXIC 4 Byte Mode */

/* Flash opcodes. */
#define OPCODE_WREN		6	/* Write enable */
#define OPCODE_RDSR		5	/* Read status register */
#define OPCODE_WRSR		1	/* Write status register */
#define OPCODE_READ		3	/* Read data bytes */
#define OPCODE_PP		2	/* Page program */
#define OPCODE_SE		0xD8	/* Sector erase */
#define OPCODE_RES		0xAB	/* Read Electronic Signature */
#define OPCODE_RDID		0x9F	/* Read JEDEC ID */

/* Status Register bits. */
#define SR_WIP			1	/* Write in progress */
#define SR_WEL			2	/* Write enable latch */
#define SR_BP0			4	/* Block protect 0 */
#define SR_BP1			8	/* Block protect 1 */
#define SR_BP2			0x10	/* Block protect 2 */
#define SR_EPE			0x20	/* Erase/Program error */
#define SR_SRWD			0x80	/* SR write protect */


static unsigned int spi_wait_nsec = 0;

//#define SPI_DEBUG
#if !defined (SPI_DEBUG)

#define ra_inl(addr)  (*(volatile unsigned int *)(addr))
#define ra_outl(addr, value)  (*(volatile unsigned int *)(addr) = (value))
#define ra_dbg(args...) do {} while(0)
//#define ra_dbg(args...) do { if (1) printk(args); } while(0)

#else

int ranfc_debug = 1;
#define ra_dbg(args...) do { if (ranfc_debug) printk(args); } while(0)
#define _ra_inl(addr)  (*(volatile unsigned int *)(addr))
#define _ra_outl(addr, value)  (*(volatile unsigned int *)(addr) = (value))

u32 ra_inl(u32 addr)
{	
	u32 retval = _ra_inl(addr);
	
	printk("%s(%x) => %x \n", __func__, addr, retval);

	return retval;	
}

u32 ra_outl(u32 addr, u32 val)
{
	_ra_outl(addr, val);

	printk("%s(%x, %x) \n", __func__, addr, val);

	return val;	
}

#endif // SPI_DEBUG

#define ra_aor(addr, a_mask, o_value)  ra_outl(addr, (ra_inl(addr) & (a_mask)) | (o_value))

#define ra_and(addr, a_mask)  ra_aor(addr, a_mask, 0)
#define ra_or(addr, o_value)  ra_aor(addr, -1, o_value)


static int spic_busy_wait(void)
{
	do {
		if ((ra_inl(RT2880_SPISTAT_REG) & 0x01) == 0)
			return 0;
	} while (spi_wait_nsec >> 1);

	printk("%s: fail \n", __func__);
	return -1;
}

#define SPIC_READ_BYTES (1<<0)
#define SPIC_WRITE_BYTES (1<<1)

/*
 * @cmd: command and address
 * @n_cmd: size of command, in bytes
 * @buf: buffer into which data will be read/written
 * @n_buf: size of buffer, in bytes
 * @flag: tag as READ/WRITE
 *
 * @return: if write_onlu, -1 means write fail, or return writing counter.
 * @return: if read, -1 means read fail, or return reading counter.
 */
static int spic_transfer(const u8 *cmd, int n_cmd, u8 *buf, int n_buf, int flag)
{
	int retval = -1;

	ra_dbg("cmd(%x): %x %x %x %x , buf:%x len:%x, flag:%s \n",
			n_cmd, cmd[0], cmd[1], cmd[2], cmd[3],
			(buf)? (*buf) : 0, n_buf,
			(flag == SPIC_READ_BYTES)? "read" : "write");
	
	// assert CS and we are already CLK normal high
	ra_and(RT2880_SPICTL_REG, ~(SPICTL_SPIENA_HIGH));

	// write command
	for (retval = 0; retval < n_cmd; retval++) {
		ra_outl(RT2880_SPIDATA_REG, cmd[retval]);
		ra_or(RT2880_SPICTL_REG, SPICTL_STARTWR);
		if (spic_busy_wait()) {
			retval = -1;
			goto end_trans;
		}
	}

	// read / write  data
	if (flag & SPIC_READ_BYTES) {
		for (retval = 0; retval < n_buf; retval++) {
			ra_or(RT2880_SPICTL_REG, SPICTL_STARTRD);
			if (spic_busy_wait())
				goto end_trans;
			buf[retval] = (u8) ra_inl(RT2880_SPIDATA_REG);
		}

	}
	else if (flag & SPIC_WRITE_BYTES) {
		for (retval = 0; retval < n_buf; retval++) {
			ra_outl(RT2880_SPIDATA_REG, buf[retval]);
			ra_or(RT2880_SPICTL_REG, SPICTL_STARTWR);
			if (spic_busy_wait())
				goto end_trans;
		}
	}

end_trans:
	// de-assert CS and
	ra_or (RT2880_SPICTL_REG, (SPICTL_SPIENA_HIGH));

	return retval;
}

static int spic_read(const u8 *cmd, size_t n_cmd, u8 *rxbuf, size_t n_rx)
{
	return spic_transfer(cmd, n_cmd, rxbuf, n_rx, SPIC_READ_BYTES);
}

static int spic_write(const u8 *cmd, size_t n_cmd, const u8 *txbuf, size_t n_tx)
{
	return spic_transfer(cmd, n_cmd, (u8 *)txbuf, n_tx, SPIC_WRITE_BYTES);
}

int spic_init(void)
{
	// GPIO-SPI mode
	ra_and(RALINK_REG_GPIOMODE, ~(1 << 1)); //use normal(SPI) mode instead of GPIO mode

	/* reset spi block */
	ra_or(RT2880_RSTCTRL_REG, RSTCTRL_SPI_RESET);
	udelay(1);
	ra_and(RT2880_RSTCTRL_REG, ~RSTCTRL_SPI_RESET);

	// FIXME, clk_div should depend on spi-flash. 
	// mode 0 (SPICLKPOL = 0) & (RXCLKEDGE_FALLING = 0)
	// mode 3 (SPICLKPOL = 1) & (RXCLKEDGE_FALLING = 0)
	ra_outl(RT2880_SPICFG_REG, SPICFG_MSBFIRST | SPICFG_TXCLKEDGE_FALLING | CFG_CLK_DIV | SPICFG_SPICLKPOL );

	// set idle state
	ra_outl(RT2880_SPICTL_REG, SPICTL_HIZSDO | SPICTL_SPIENA_HIGH);

	spi_wait_nsec = (8 * 1000 / ((get_surfboard_sysclk() / 1000 / 1000 / CFG_CLK_DIV) )) >> 1 ;
	//printk("spi_wait_nsec: %x \n", spi_wait_nsec);
	return 0;
}




/****************************************************************************/
struct chip_info {
	char		*name;
	u8		id;
	u32		jedec_id;
	unsigned long	sector_size;
	unsigned int	n_sectors;
	char		addr4b;
};

static struct chip_info chips_data [] = {
	/* REVISIT: fill in JEDEC ids, for parts that have them */
	{ "AT25DF321",		0x1f, 0x47000000, 64 * 1024, 64,  0 },
	{ "AT26DF161",		0x1f, 0x46000000, 64 * 1024, 32,  0 },
	{ "FL016AIF",		0x01, 0x02140000, 64 * 1024, 32,  0 },
	{ "FL064AIF",		0x01, 0x02160000, 64 * 1024, 128, 0 },
	{ "MX25L1605D",		0xc2, 0x2015c220, 64 * 1024, 32,  0 },
	{ "MX25L3205D",		0xc2, 0x2016c220, 64 * 1024, 64,  0 },
	{ "MX25L6405D",		0xc2, 0x2017c220, 64 * 1024, 128, 0 },
	{ "MX25L12805D",	0xc2, 0x2018c220, 64 * 1024, 256, 0 },
#ifdef MX_4B_MODE 
	{ "MX25L25635E",	0xc2, 0x2019c220, 64 * 1024, 512, 1 },
#endif
	{ "S25FL128P",		0x01, 0x20180301, 64 * 1024, 256, 0 },
	{ "S25FL129P",		0x01, 0x20184D01, 64 * 1024, 256, 0 },
	{ "S25FL032P",		0x01, 0x02154D00, 64 * 1024, 64,  0 },
	{ "S25FL064P",		0x01, 0x02164D00, 64 * 1024, 128, 0 },
	{ "EN25F16",		0x1c, 0x31151c31, 64 * 1024, 32,  0 },
	{ "EN25F32",            0x1c, 0x31161c31, 64 * 1024, 64,  0 },
	{ "W25Q32BV",		0xef, 0x40160000, 64 * 1024, 64,  0 },
};


struct flash_info {
	struct mutex		lock;
	struct mtd_info		mtd;
	struct chip_info	*chip;
	u8			command[5];
};

struct flash_info *flash;

/****************************************************************************/


static int raspi_read_devid(u8 *rxbuf, int n_rx)
{
	u8 code = OPCODE_RDID;
	int retval;

	retval = spic_read(&code, 1, rxbuf, n_rx);
	if (retval != n_rx) {
		printk("%s: ret: %x\n", __func__, retval);
		return retval;
	}
	return retval;
}

/*
 * Read the status register, returning its value in the location
 */
static int raspi_read_sr(u8 *val)
{
	ssize_t retval;
	u8 code = OPCODE_RDSR;

	retval = spic_read(&code, 1, val, 1);
	if (retval != 1) {
		printk("%s: ret: %x\n", __func__, retval);
		return -EIO;
	}
	return 0;
}

/*
 * write status register
 */
static int raspi_write_sr(u8 *val)
{
	ssize_t retval;
	u8 code = OPCODE_WRSR;

	retval = spic_write(&code, 1, val, 1);
	if (retval != 1) {
		printk("%s: ret: %x\n", __func__, retval);
		return -EIO;
	}
	return 0;
}

#ifdef MX_4B_MODE
static int raspi_4byte_mode(int enable)
{
	ssize_t retval;
	u8 code;

	if (enable)
		code = 0xB7; /* EN4B, enter 4-byte mode */
	else
		code = 0xE9; /* EX4B, exit 4-byte mode */
	retval = spic_read(&code, 1, 0, 0);
	if (retval != 0) {
		printk("%s: ret: %x\n", __func__, retval);
		return -1;
	}
	return 0;
}
#endif

/*
 * Set write enable latch with Write Enable command.
 * Returns negative if error occurred.
 */
static inline int raspi_write_enable(void)
{
	u8 code = OPCODE_WREN;

	return spic_write(&code, 1, NULL, 0);
}

/*
 * Set all sectors (global) unprotected if they are protected.
 * Returns negative if error occurred.
 */
static inline int raspi_unprotect(void)
{
	u8 sr = 0;

	if (raspi_read_sr(&sr) < 0) {
		printk("%s: read_sr fail: %x\n", __func__, sr);
		return -1;
	}

	if ((sr & (SR_BP0 | SR_BP1 | SR_BP2)) != 0) {
		sr = 0;
		raspi_write_sr(&sr);
	}
	return 0;
}

/*
 * Service routine to read status register until ready, or timeout occurs.
 * Returns non-zero if error.
 */
static int raspi_wait_ready(int sleep_ms)
{
	int count;
	int sr = 0;

	/*int timeout = sleep_ms * HZ / 1000;
	while (timeout) 
		timeout = schedule_timeout (timeout);*/

	/* one chip guarantees max 5 msec wait here after page writes,
	 * but potentially three seconds (!) after page erase.
	 */
	for (count = 0;  count < ((sleep_ms+1) *1000); count++) {
		if ((raspi_read_sr((u8 *)&sr)) < 0)
			break;
		else if (!(sr & (SR_WIP | SR_EPE))) {
			return 0;
		}

		udelay(500);
		/* REVISIT sometimes sleeping would be best */
	}

	printk("%s: read_sr fail: %x\n", __func__, sr);
	return -EIO;
}


/*
 * Erase one sector of flash memory at offset ``offset'' which is any
 * address within the sector which should be erased.
 *
 * Returns 0 if successful, non-zero otherwise.
 */
static int raspi_erase_sector(u32 offset)
{

	/* Wait until finished previous write command. */
	if (raspi_wait_ready(950))
		return -EIO;

	/* Send write enable, then erase commands. */
	raspi_write_enable();
	raspi_unprotect();

#ifdef MX_4B_MODE
	if (flash->chip->addr4b) {
		raspi_4byte_mode(1);
		flash->command[0] = OPCODE_SE;
		flash->command[1] = offset >> 24;
		flash->command[2] = offset >> 16;
		flash->command[3] = offset >> 8;
		flash->command[4] = offset;
		spic_write(flash->command, 5, 0 , 0);
		raspi_4byte_mode(0);
		return 0;
	}
#endif

	/* Set up command buffer. */
	flash->command[0] = OPCODE_SE;
	flash->command[1] = offset >> 16;
	flash->command[2] = offset >> 8;
	flash->command[3] = offset;

	spic_write(flash->command, 4, 0 , 0);
	return 0;
}

int raspi_set_lock (struct mtd_info *mtd, loff_t to, uint64_t len, int set)
{
	u32 page_offset, page_size;
	int retval;

	/*  */
	while (len > 0) {
		/* FIXME: 4b mode ? */
		/* write the next page to flash */
		flash->command[0] = (set == 0)? 0x39 : 0x36;
		flash->command[1] = to >> 16;
		flash->command[2] = to >> 8;
		flash->command[3] = to;

		raspi_wait_ready(1);
			
		raspi_write_enable();

		retval = spic_write(flash->command, 4, 0, 0);
			
		if (retval < 0) {
			return -EIO;
		}
			
		page_offset = (to & (mtd->erasesize-1));
		page_size = mtd->erasesize - page_offset;
				
		len -= mtd->erasesize;
		to += mtd->erasesize;
	}

	return 0;
	
}


/****************************************************************************/

/*
 * MTD implementation
 */

/*
 * Erase an address range on the flash chip.  The address range may extend
 * one or more erase sectors.  Return an error is there is a problem erasing.
 */
static int ramtd_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	u32 addr,len;

	//printk("%s: addr:%x len:%x\n", __func__, instr->addr, instr->len);
	/* sanity checks */
	if (instr->addr + instr->len > flash->mtd.size)
		return -EINVAL;
	if ((((size_t)instr->addr) % mtd->erasesize) != 0 || (((size_t)instr->len) % mtd->erasesize) != 0) {
		return -EINVAL;
	}

	addr = instr->addr;
	len = instr->len;

  	mutex_lock(&flash->lock);

	/* now erase those sectors */
	while (len > 0) {
		if (raspi_erase_sector(addr)) {
			instr->state = MTD_ERASE_FAILED;
			mutex_unlock(&flash->lock);
			return -EIO;
		}

		addr += mtd->erasesize;
		len -= mtd->erasesize;
	}

  	mutex_unlock(&flash->lock);

	instr->state = MTD_ERASE_DONE;
	mtd_erase_callback(instr);

	return 0;
}

/*
 * Read an address range from the flash chip.  The address range
 * may be any size provided it is within the physical boundaries.
 */
static int ramtd_read(struct mtd_info *mtd, loff_t from, size_t len,
	size_t *retlen, u_char *buf)
{
	size_t readlen;

	//printk("%s: from:%llx len:%x\n", __func__, from, len);
	/* sanity checks */
	if (len == 0)
		return 0;

	if (from + len > flash->mtd.size)
		return -EINVAL;

	/* Byte count starts at zero. */
	if (retlen)
		*retlen = 0;

	mutex_lock(&flash->lock);

	/* Wait till previous write/erase is done. */
	if (raspi_wait_ready(1)) {
		/* REVISIT status return?? */
		mutex_unlock(&flash->lock);
		return -EIO;
	}

	/* NOTE: OPCODE_FAST_READ (if available) is faster... */

	/* Set up the write data buffer. */
	flash->command[0] = OPCODE_READ;
#ifdef MX_4B_MODE
	if (flash->chip->addr4b) {
		raspi_4byte_mode(1);
		flash->command[1] = from >> 24;
		flash->command[2] = from >> 16;
		flash->command[3] = from >> 8;
		flash->command[4] = from;
		readlen = spic_read(flash->command, 5, buf, len);
		raspi_4byte_mode(0);
	}
	else
#endif
	{
		flash->command[1] = from >> 16;
		flash->command[2] = from >> 8;
		flash->command[3] = from;
		readlen = spic_read(flash->command, 4, buf, len);
	}
  	mutex_unlock(&flash->lock);

	if (retlen) 
		*retlen = readlen;

	if (readlen != len) 
		return -EIO;

	return 0;
}

inline int ramtd_lock (struct mtd_info *mtd, loff_t to, uint64_t len)
{
	return raspi_set_lock(mtd, to, len, 1);
}

inline int ramtd_unlock (struct mtd_info *mtd, loff_t to, uint64_t len)
{
	return raspi_set_lock(mtd, to, len, 0);
}


/*
 * Write an address range to the flash chip.  Data must be written in
 * FLASH_PAGESIZE chunks.  The address range may be any size provided
 * it is within the physical boundaries.
 */
static int ramtd_write(struct mtd_info *mtd, loff_t to, size_t len,
	size_t *retlen, const u_char *buf)
{
	u32 page_offset, page_size;
	int retval;

	//printk("%s: to:%llx len:%x\n", __func__, to, len);
	if (retlen)
		*retlen = 0;

	/* sanity checks */
	if (!len)
		return(0);

	if (to + len > flash->mtd.size)
		return -EINVAL;

  	mutex_lock(&flash->lock);

	/* Wait until finished previous write command. */
	if (raspi_wait_ready(2)) {
		mutex_unlock(&flash->lock);
		return -1;
	}

	raspi_write_enable();

	/* Set up the opcode in the write buffer. */
	flash->command[0] = OPCODE_PP;
#ifdef MX_4B_MODE
	if (flash->chip->addr4b) {
		flash->command[1] = to >> 24;
		flash->command[2] = to >> 16;
		flash->command[3] = to >> 8;
		flash->command[4] = to;
	}
	else
#endif
	{
		flash->command[1] = to >> 16;
		flash->command[2] = to >> 8;
		flash->command[3] = to;
	}

	/* what page do we start with? */
	page_offset = to % FLASH_PAGESIZE;

#ifdef MX_4B_MODE
	raspi_4byte_mode(1);
#endif
	/* write everything in PAGESIZE chunks */
	while (len > 0) {
		page_size = min_t(size_t, len, FLASH_PAGESIZE-page_offset);
		page_offset = 0;

		/* write the next page to flash */
#ifdef MX_4B_MODE
		if (flash->chip->addr4b) {
			flash->command[1] = to >> 24;
			flash->command[2] = to >> 16;
			flash->command[3] = to >> 8;
			flash->command[4] = to;
		}
		else
#endif
		{
			flash->command[1] = to >> 16;
			flash->command[2] = to >> 8;
			flash->command[3] = to;
		}

		raspi_wait_ready(3);
		raspi_write_enable();
		raspi_unprotect();

#ifdef MX_4B_MODE
		if (flash->chip->addr4b)
			retval = spic_write(flash->command, 5, buf, page_size);
		else
#endif
			retval = spic_write(flash->command, 4, buf, page_size);
		//printk("%s : to:%llx page_size:%x ret:%x\n", __func__, to, page_size, retval);

		if (retval > 0) {
			if (retlen)
				*retlen += retval;
				
			if (retval < page_size) {
				mutex_unlock(&flash->lock);
				printk("%s: retval:%x return:%x page_size:%x \n", 
				       __func__, retval, retval, page_size);
				return -EIO;
			}
		}

		len -= page_size;
		to += page_size;
		buf += page_size;
	}

#ifdef MX_4B_MODE
	raspi_4byte_mode(0);
#endif

	mutex_unlock(&flash->lock);

	return 0;
}


/****************************************************************************/

/*
 * SPI device driver setup and teardown
 */
struct chip_info *chip_prob(void)
{
	struct chip_info *info, *match;
	u8 buf[5];
	u32 jedec, weight;
	int i;

	raspi_read_devid(buf, 5);
	jedec = (u32)((u32)(buf[1] << 24) | ((u32)buf[2] << 16) | ((u32)buf[3] <<8) | (u32)buf[4]);

	printk("deice id : %x %x %x %x %x (%x)\n", buf[0], buf[1], buf[2], buf[3], buf[4], jedec);
	
	// FIXME, assign default as AT25D
	weight = 0xffffffff;
	match = &chips_data[0];
	for (i = 0; i < ARRAY_SIZE(chips_data); i++) {
		info = &chips_data[i];
		if (info->id == buf[0]) {
			if (info->jedec_id == jedec)
				return info;

			if (weight > (info->jedec_id ^ jedec)) {
				weight = info->jedec_id ^ jedec;
				match = info;
			}
		}
	}
	printk("Warning: un-recognized chip ID, please update SPI driver!\n");

	return match;
}

#define BOOT_FROM_NOR	0
#define BOOT_FROM_NAND	2
#define BOOT_FROM_SPI	3


extern int ra_check_flash_type(void);

/*
 * board specific setup should have ensured the SPI clock used here
 * matches what the READ command supports, at least until this driver
 * understands FAST_READ (for clocks over 25 MHz).
 */
static int __devinit raspi_prob(void)
{
	struct chip_info		*chip;
	unsigned			i;
	struct squashfs_super_block *sb;
#ifdef CONFIG_ROOTFS_IN_FLASH_NO_PADDING
	loff_t offs;
	struct __image_header {
		uint8_t unused[60];
		uint32_t ih_ksz;
	} hdr;
#endif
	printk(KERN_INFO "ralink spi driver\n");	
    	if(ra_check_flash_type()!=BOOT_FROM_SPI) { /* SPI */
	    return 0;
	}

	chip = chip_prob();
	
	flash = kzalloc(sizeof *flash, GFP_KERNEL);
	if (!flash)
		return -ENOMEM;

	mutex_init(&flash->lock);

	flash->chip = chip;
	memset(&flash->mtd, 0, sizeof(struct mtd_info));

	flash->mtd.name = "raspi";
	flash->mtd.type = MTD_NORFLASH;
	flash->mtd.writesize = 1;
	flash->mtd.flags = MTD_CAP_NORFLASH;
	flash->mtd.size = chip->sector_size * chip->n_sectors;
	flash->mtd.erasesize = chip->sector_size;
	flash->mtd.erase = ramtd_erase;
	flash->mtd.read = ramtd_read;
	flash->mtd.write = ramtd_write;
	flash->mtd.lock = ramtd_lock;
	flash->mtd.unlock = ramtd_unlock;
	flash->mtd.owner = THIS_MODULE;


	printk("%s(%02x %04x) (%d Kbytes)\n", 
	       chip->name, chip->id, chip->jedec_id, flash->mtd.size / 1024);

	printk("mtd .name = %s, .size = 0x%.8x (%uM) "
			".erasesize = 0x%.8x (%uK) .numeraseregions = %d\n",
		flash->mtd.name,
		flash->mtd.size, flash->mtd.size / (1024*1024),
		flash->mtd.erasesize, flash->mtd.erasesize / 1024,
		flash->mtd.numeraseregions);

	if (flash->mtd.numeraseregions)
		for (i = 0; i < flash->mtd.numeraseregions; i++)
			printk("mtd.eraseregions[%d] = { .offset = 0x%.8x, "
				".erasesize = 0x%.8x (%uK), "
				".numblocks = %d }\n",
				i, flash->mtd.eraseregions[i].offset,
				flash->mtd.eraseregions[i].erasesize,
				flash->mtd.eraseregions[i].erasesize / 1024,
				flash->mtd.eraseregions[i].numblocks);

		char *buf = vmalloc(4096);
		int offset = 0;
		int bootsize = 0x50000;
		int nvramsize = flash->mtd.erasesize;
			    while((offset+flash->mtd.erasesize)<flash->mtd.size)
			    {
			    int retlen;
			    flash->mtd.read(&flash->mtd,offset,sizeof(struct squashfs_super_block), &retlen, buf);
			    if (*((__u32 *) buf) == SQUASHFS_MAGIC)
				    {
					sb = (struct squashfs_super_block *)buf;
					int len;

				    	printk(KERN_EMERG "\nfound squashfs at %X with %d len\n",offset, le64_to_cpu(sb->bytes_used));
					rt2880_partitions[3].size=(((flash->mtd.size)-nvramsize)-bootsize);					
					rt2880_partitions[4].offset = offset;					
					rt2880_partitions[4].size = rt2880_partitions[3].size-(offset-bootsize);					
					len = offset + le64_to_cpu(sb->bytes_used);
					len += (flash->mtd.erasesize - 1);
					len &= ~(flash->mtd.erasesize - 1);
					rt2880_partitions[6].offset = flash->mtd.size - nvramsize;					
					rt2880_partitions[6].size = flash->mtd.erasesize;					
					rt2880_partitions[5].offset = len;					
					rt2880_partitions[5].size = rt2880_partitions[6].offset - len;					
					break;
				    } 
			    offset+=4096;
			    }
		vfree(buf);
	return add_mtd_partitions(&flash->mtd, rt2880_partitions, ARRAY_SIZE(rt2880_partitions));
}

static void __devexit raspi_remove(void)
{
	/* Clean up MTD stuff. */
	del_mtd_partitions(&flash->mtd);

	kfree(flash);
	
	flash = NULL;
}

int raspi_init(void)
{
	spic_init();
	return raspi_prob();
}


module_init(raspi_init);
module_exit(raspi_remove);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mike Lavender");
MODULE_DESCRIPTION("MTD SPI driver for ST M25Pxx flash chips");
