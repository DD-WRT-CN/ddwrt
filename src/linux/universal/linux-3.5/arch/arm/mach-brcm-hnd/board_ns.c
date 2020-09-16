
#include <linux/types.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/platform_device.h>

#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/clkdev.h>
#include <asm/hardware/gic.h>

#include <mach/clkdev.h>
#include <mach/memory.h>
#include <mach/io_map.h>

#include <plat/bsp.h>
#include <plat/mpcore.h>
#include <plat/plat-bcm5301x.h>

#ifdef CONFIG_MTD
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/romfs_fs.h>
#include <linux/cramfs_fs.h>
#include <linux/squashfs_fs.h>
#endif

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmnvram.h>
#include <bcmendian.h>
#include <hndsoc.h>
#include <siutils.h>
#include <hndcpu.h>
#include <hndpci.h>
#include <pcicfg.h>
#include <bcmdevs.h>
#include <trxhdr.h>
#ifdef HNDCTF
#include <ctf/hndctf.h>
#endif /* HNDCTF */
#include <hndsflash.h>
#ifdef CONFIG_MTD_NFLASH
#include <hndnand.h>
#endif

extern char __initdata saved_root_name[];

/* Global SB handle */
si_t *bcm947xx_sih = NULL;
DEFINE_SPINLOCK(bcm947xx_sih_lock);
EXPORT_SYMBOL(bcm947xx_sih);
EXPORT_SYMBOL(bcm947xx_sih_lock);

/* Convenience */
#define sih bcm947xx_sih
#define sih_lock bcm947xx_sih_lock

#ifdef HNDCTF
ctf_t *kcih = NULL;
EXPORT_SYMBOL(kcih);
ctf_attach_t ctf_attach_fn = NULL;
EXPORT_SYMBOL(ctf_attach_fn);
#endif /* HNDCTF */



/* This is the main reference clock 25MHz from external crystal */
static struct clk clk_ref = {
	.name = "Refclk",
	.rate = 25 * 1000000,	/* run-time override */
	.fixed = 1,
	.type	= CLK_XTAL,
};


static struct clk_lookup board_clk_lookups[] = {
	{
	.con_id		= "refclk",
	.clk		= &clk_ref,
	}
};

extern int _memsize;

#if 0
#include <mach/uncompress.h>

void printch(int c)
{
putc(c);
flush();
}
#endif
void __init board_map_io(void)
{
early_printk("board_map_io\n");
	/* Install clock sources into the lookup table */
	clkdev_add_table(board_clk_lookups, 
			ARRAY_SIZE(board_clk_lookups));

	/* Map SoC specific I/O */
	soc_map_io( &clk_ref );
}


void __init board_init_irq(void)
{
early_printk("board_init_irq\n");
	soc_init_irq();
	
	/* serial_setup(sih); */
}

void board_init_timer(void)
{
early_printk("board_init_timer\n");
	soc_init_timer();
}

static int __init rootfs_mtdblock(void)
{
	int bootdev;
	int knldev;
	int block = 0;
#ifdef CONFIG_FAILSAFE_UPGRADE
	char *img_boot = nvram_get(BOOTPARTITION);
#endif

	bootdev = soc_boot_dev((void *)sih);
	knldev = soc_knl_dev((void *)sih);

	/* NANDBOOT */
	if (bootdev == SOC_BOOTDEV_NANDFLASH &&
	    knldev == SOC_KNLDEV_NANDFLASH) {
#ifdef CONFIG_FAILSAFE_UPGRADE
		if (img_boot && simple_strtol(img_boot, NULL, 10))
			return 5;
		else
			return 3;
#else
		return 3;
#endif
	}

	/* SFLASH/PFLASH only */
	if (bootdev != SOC_BOOTDEV_NANDFLASH &&
	    knldev != SOC_KNLDEV_NANDFLASH) {
#ifdef CONFIG_FAILSAFE_UPGRADE
		if (img_boot && simple_strtol(img_boot, NULL, 10))
			return 4;
		else
			return 2;
#else
		return 2;
#endif
	}

#ifdef BCMCONFMTD
	block++;
#endif
#ifdef CONFIG_FAILSAFE_UPGRADE
	if (img_boot && simple_strtol(img_boot, NULL, 10))
		block += 2;
#endif
	/* Boot from norflash and kernel in nandflash */
	return block+3;
}

#define WATCHDOG_MIN    3000    /* milliseconds */
extern int panic_timeout;
extern int panic_on_oops;
static int watchdog = 0;

static void __init brcm_setup(void)
{
	/* Get global SB handle */
	sih = si_kattach(SI_OSH);

//	if (strncmp(boot_command_line, "root=/dev/mtdblock", strlen("root=/dev/mtdblock")) == 0)
//		sprintf(saved_root_name, "/dev/mtdblock%d", rootfs_mtdblock());
	/* Set watchdog interval in ms */
        watchdog = simple_strtoul(nvram_safe_get("watchdog"), NULL, 0);

	/* Ensure at least WATCHDOG_MIN */
	if ((watchdog > 0) && (watchdog < WATCHDOG_MIN))
		watchdog = WATCHDOG_MIN;

}

void soc_watchdog(void)
{
	if (watchdog > 0)
		si_watchdog_ms(sih, watchdog);
 }

void __init board_init(void)
{
early_printk("board_init\n");
	brcm_setup();
	/*
	 * Add common platform devices that do not have board dependent HW
	 * configurations
	 */
	soc_add_devices();

	return;
}

void __init board_fixup(struct tag *t, char **cmdline,struct meminfo *mi)
{
        early_printk("board_fixup\n" );
        
	u32 mem_size, lo_size ;
        early_printk("board_fixup\n" );

	/* Fuxup reference clock rate */
//	if (desc->nr == MACH_TYPE_BRCM_NS_QT )
//		clk_ref.rate = 17594;	/* Emulator ref clock rate */


	mem_size = _memsize;

	early_printk("board_fixup: mem=%uMiB\n", mem_size >> 20);

	lo_size = min(mem_size, DRAM_MEMORY_REGION_SIZE);

	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].size = lo_size;
	mi->nr_banks++;

	if (lo_size == mem_size)
		return;

	mi->bank[1].start = DRAM_LARGE_REGION_BASE + lo_size;
	mi->bank[1].size = mem_size - lo_size;
	mi->nr_banks++;
}

static struct sys_timer board_timer = {
   .init = board_init_timer,
};

//#if (( (IO_BASE_VA >>18) & 0xfffc) != 0x3c40)
//#error IO_BASE_VA 
//#endif

void brcm_reset(char mode, const char *cmd)
{
#ifdef CONFIG_OUTER_CACHE_SYNC
	outer_cache.sync = NULL;
#endif
	hnd_cpu_reset(sih);
}


MACHINE_START(BRCM_NS, "Northstar Prototype")
//   .phys_io = 					/* UART I/O mapping */
//	IO_BASE_PA,
//   .io_pg_offst = 				/* for early debug */
//	(IO_BASE_VA >>18) & 0xfffc,
   .fixup = board_fixup,			/* Opt. early setup_arch() */
   .map_io = board_map_io,			/* Opt. from setup_arch() */
   .init_irq = board_init_irq,			/* main.c after setup_arch() */
   .timer  = &board_timer,			/* main.c after IRQs */
   .init_machine = board_init,			/* Late archinitcall */
   .atag_offset = CONFIG_BOARD_PARAMS_PHYS,
    .handle_irq	= gic_handle_irq,
    .restart	= brcm_reset,

MACHINE_END

#ifdef	CONFIG_MACH_BRCM_NS_QT
MACHINE_START(BRCM_NS_QT, "Northstar Emulation Model")
//   .phys_io = 					/* UART I/O mapping */
//	IO_BASE_PA,
//   .io_pg_offst = 				/* for early debug */
//	(IO_BASE_VA >>18) & 0xfffc,
   .fixup = board_fixup,			/* Opt. early setup_arch() */
   .map_io = board_map_io,			/* Opt. from setup_arch() */
   .init_irq = board_init_irq,			/* main.c after setup_arch() */
   .timer  = &board_timer,			/* main.c after IRQs */
   .init_machine = board_init,			/* Late archinitcall */
   .atag_offset = CONFIG_BOARD_PARAMS_PHYS,
    .handle_irq	= gic_handle_irq,
    .restart	= brcm_reset,
MACHINE_END
#endif


#ifdef CONFIG_MTD

static spinlock_t *bcm_mtd_lock = NULL;

spinlock_t *partitions_lock_init(void)
{
	if (!bcm_mtd_lock) {
		bcm_mtd_lock = (spinlock_t *)kzalloc(sizeof(spinlock_t), GFP_KERNEL);
		if (!bcm_mtd_lock)
			return NULL;

		spin_lock_init( bcm_mtd_lock );
	}
	return bcm_mtd_lock;
}
EXPORT_SYMBOL(partitions_lock_init);

static struct nand_hw_control *nand_hwcontrol = NULL;
struct nand_hw_control *nand_hwcontrol_lock_init(void)
{
	if (!nand_hwcontrol) {
		nand_hwcontrol = (struct nand_hw_control *)kzalloc(sizeof(struct nand_hw_control), GFP_KERNEL);
		if (!nand_hwcontrol)
			return NULL;

		spin_lock_init(&nand_hwcontrol->lock);
		init_waitqueue_head(&nand_hwcontrol->wq);
	}
	return nand_hwcontrol;
}
EXPORT_SYMBOL(nand_hwcontrol_lock_init);


/* Find out prom size */
static uint32 boot_partition_size(uint32 flash_phys) {
	uint32 bootsz, *bisz;

	/* Default is 256K boot partition */
	bootsz = 256 * 1024;

	/* Do we have a self-describing binary image? */
	bisz = (uint32 *)(flash_phys + BISZ_OFFSET);
	if (bisz[BISZ_MAGIC_IDX] == BISZ_MAGIC) {
		int isz = bisz[BISZ_DATAEND_IDX] - bisz[BISZ_TXTST_IDX];

		if (isz > (1024 * 1024))
			bootsz = 2048 * 1024;
		else if (isz > (512 * 1024))
			bootsz = 1024 * 1024;
		else if (isz > (256 * 1024))
			bootsz = 512 * 1024;
		else if (isz <= (128 * 1024))
			bootsz = 128 * 1024;
	}
	return bootsz;
}

size_t rootfssize=0;

#if defined(BCMCONFMTD)
#define MTD_PARTS 1
#else
#define MTD_PARTS 0
#endif
#if defined(PLC)
#define PLC_PARTS 1
#else
#define PLC_PARTS 0
#endif
#if defined(CONFIG_FAILSAFE_UPGRADE)
#define FAILSAFE_PARTS 2
#else
#define FAILSAFE_PARTS 0
#endif
/* boot;nvram;kernel;rootfs;empty */
#define FLASH_PARTS_NUM	(6+MTD_PARTS+PLC_PARTS+FAILSAFE_PARTS)

static struct mtd_partition bcm947xx_flash_parts[FLASH_PARTS_NUM+1] = {{0}};
static uint lookup_flash_rootfs_offset(struct mtd_info *mtd, int *trx_off, size_t size)
{
	struct romfs_super_block *romfsb;
	struct cramfs_super *cramfsb;
	struct squashfs_super_block *squashfsb;
	struct trx_header *trx;
	unsigned char buf[512];
	int off;
	size_t len;

	romfsb = (struct romfs_super_block *) buf;
	cramfsb = (struct cramfs_super *) buf;
	squashfsb = (void *) buf;
	trx = (struct trx_header *) buf;

	/* Look at every 64 KB boundary */
	for (off = 0; off < size; off += (64 * 1024)) {
		memset(buf, 0xe5, sizeof(buf));

		/*
		 * Read block 0 to test for romfs and cramfs superblock
		 */
		if (mtd_read(mtd, off, sizeof(buf), &len, buf) ||
		    len != sizeof(buf))
			continue;

		/* Try looking at TRX header for rootfs offset */
		if (le32_to_cpu(trx->magic) == TRX_MAGIC) {
			*trx_off = off;
			if (trx->offsets[1] == 0)
				continue;
			/*
			 * Read to test for romfs and cramfs superblock
			 */
			off += le32_to_cpu(trx->offsets[1]);
			memset(buf, 0xe5, sizeof(buf));
			if (mtd_read(mtd, off, sizeof(buf), &len, buf) || len != sizeof(buf))
				continue;
		}

		/* romfs is at block zero too */
		if (romfsb->word0 == ROMSB_WORD0 &&
		    romfsb->word1 == ROMSB_WORD1) {
			printk(KERN_NOTICE
			       "%s: romfs filesystem found at block %d\n",
			       mtd->name, off / mtd->erasesize);
			break;
		}

		/* so is cramfs */
		if (cramfsb->magic == CRAMFS_MAGIC) {
			printk(KERN_NOTICE
			       "%s: cramfs filesystem found at block %d\n",
			       mtd->name, off / mtd->erasesize);
			break;
		}

		if (squashfsb->s_magic == SQUASHFS_MAGIC) {
			rootfssize = le64_to_cpu(squashfsb->bytes_used);;
			printk(KERN_NOTICE
			       "%s: squash filesystem found at block %d\n",
			       mtd->name, off / mtd->erasesize);
			break;
		}
	}

	return off;
}

struct mtd_partition *
init_mtd_partitions(hndsflash_t *sfl_info, struct mtd_info *mtd, size_t size)
{
	int bootdev;
	int knldev;
	int nparts = 0;
	uint32 offset = 0;
	uint rfs_off = 0;
	uint vmlz_off, knl_size;
	uint32 top = 0;
	uint32 bootsz;
	uint32 maxsize=0;
#ifdef CONFIG_FAILSAFE_UPGRADE
	char *img_boot = nvram_get(BOOTPARTITION);
	char *imag_1st_offset = nvram_get(IMAGE_FIRST_OFFSET);
	char *imag_2nd_offset = nvram_get(IMAGE_SECOND_OFFSET);
	unsigned int image_first_offset = 0;
	unsigned int image_second_offset = 0;
	char dual_image_on = 0;

	/* The image_1st_size and image_2nd_size are necessary if the Flash does not have any
	 * image
	 */
	dual_image_on = (img_boot != NULL && imag_1st_offset != NULL && imag_2nd_offset != NULL);

	if (dual_image_on) {
		image_first_offset = simple_strtol(imag_1st_offset, NULL, 10);
		image_second_offset = simple_strtol(imag_2nd_offset, NULL, 10);
		printk("The first offset=%x, 2nd offset=%x\n", image_first_offset,
			image_second_offset);

	}
#endif	/* CONFIG_FAILSAFE_UPGRADE */
	
	if (nvram_match("boardnum","24") && nvram_match("boardtype", "0x0646")
	    && nvram_match("boardrev", "0x1110")
	    && nvram_match("gpio7", "wps_button")) {
	    maxsize = 0x200000;
	    size = maxsize;
	}


	bootdev = soc_boot_dev((void *)sih);
	knldev = soc_knl_dev((void *)sih);

	if (bootdev == SOC_BOOTDEV_NANDFLASH) {
		/* Do not init MTD partitions on NOR flash when NAND boot */
		return NULL;	
	}

	if (knldev != SOC_KNLDEV_NANDFLASH) {
		rfs_off = lookup_flash_rootfs_offset(mtd, &vmlz_off, size);

		/* Size pmon */
		bcm947xx_flash_parts[nparts].name = "boot";
		bcm947xx_flash_parts[nparts].size = vmlz_off;
		bcm947xx_flash_parts[nparts].offset = top;
		bcm947xx_flash_parts[nparts].mask_flags = MTD_WRITEABLE; /* forces on read only */
		nparts++;

		/* Setup kernel MTD partition */
		bcm947xx_flash_parts[nparts].name = "linux";
#ifdef CONFIG_FAILSAFE_UPGRADE
		if (dual_image_on) {
			bcm947xx_flash_parts[nparts].size = image_second_offset-image_first_offset;
		} else {
			bcm947xx_flash_parts[nparts].size = mtd->size - vmlz_off;

			/* Reserve for NVRAM */
			bcm947xx_flash_parts[nparts].size -= ROUNDUP(NVRAM_SPACE, mtd->erasesize);
#ifdef PLC
			/* Reserve for PLC */
			bcm947xx_flash_parts[nparts].size -= ROUNDUP(0x1000, mtd->erasesize);
#endif
#ifdef BCMCONFMTD
			bcm947xx_flash_parts[nparts].size -= (mtd->erasesize *4);
#endif
		}
#else

		bcm947xx_flash_parts[nparts].size = mtd->size - vmlz_off;
		
#ifdef PLC
		/* Reserve for PLC */
		bcm947xx_flash_parts[nparts].size -= ROUNDUP(0x1000, mtd->erasesize);
#endif
		/* Reserve for NVRAM */
		bcm947xx_flash_parts[nparts].size -= ROUNDUP(NVRAM_SPACE, mtd->erasesize);

#ifdef BCMCONFMTD
		bcm947xx_flash_parts[nparts].size -= (mtd->erasesize *4);
#endif
#endif	/* CONFIG_FAILSAFE_UPGRADE */
		bcm947xx_flash_parts[nparts].offset = vmlz_off;
		knl_size = bcm947xx_flash_parts[nparts].size;
		offset = bcm947xx_flash_parts[nparts].offset + knl_size;
		nparts++; 
		
		/* Setup rootfs MTD partition */
		bcm947xx_flash_parts[nparts].name = "rootfs";
		bcm947xx_flash_parts[nparts].offset = rfs_off;
		bcm947xx_flash_parts[nparts].size = rootfssize;
		size_t offs = bcm947xx_flash_parts[nparts].offset + bcm947xx_flash_parts[nparts].size;
		offs += (mtd->erasesize - 1);
		offs &= ~(mtd->erasesize - 1);
		offs -= bcm947xx_flash_parts[nparts].offset;
		bcm947xx_flash_parts[nparts].size = offs;

		bcm947xx_flash_parts[nparts].mask_flags = MTD_WRITEABLE; /* forces on read only */
		nparts++;
#ifdef CONFIG_FAILSAFE_UPGRADE
		if (dual_image_on) {
			offset = image_second_offset;
			rfs_off = lookup_flash_rootfs_offset(mtd, &offset, size);
			vmlz_off = offset;
			/* Setup kernel2 MTD partition */
			bcm947xx_flash_parts[nparts].name = "linux2";
			bcm947xx_flash_parts[nparts].size = mtd->size - image_second_offset;
			/* Reserve for NVRAM */
			bcm947xx_flash_parts[nparts].size -= ROUNDUP(NVRAM_SPACE, mtd->erasesize);

#ifdef BCMCONFMTD
			bcm947xx_flash_parts[nparts].size -= (mtd->erasesize *4);
#endif
#ifdef PLC
			/* Reserve for PLC */
			bcm947xx_flash_parts[nparts].size -= ROUNDUP(0x1000, mtd->erasesize);
#endif
			bcm947xx_flash_parts[nparts].offset = image_second_offset;
			knl_size = bcm947xx_flash_parts[nparts].size;
			offset = bcm947xx_flash_parts[nparts].offset + knl_size;
			nparts++;

			/* Setup rootfs MTD partition */
			bcm947xx_flash_parts[nparts].name = "rootfs2";
			bcm947xx_flash_parts[nparts].size =
				knl_size - (rfs_off - image_second_offset);
			bcm947xx_flash_parts[nparts].offset = rfs_off;
			/* forces on read only */
			bcm947xx_flash_parts[nparts].mask_flags = MTD_WRITEABLE;
			nparts++;
		}
#endif	/* CONFIG_FAILSAFE_UPGRADE */

	} else {
		bootsz = boot_partition_size(sfl_info->base);
		printk("Boot partition size = %d(0x%x)\n", bootsz, bootsz);
		/* Size pmon */
		if (maxsize)
		    bootsz = maxsize;
		bcm947xx_flash_parts[nparts].name = "boot";
		bcm947xx_flash_parts[nparts].size = bootsz;
		bcm947xx_flash_parts[nparts].offset = top;
		bcm947xx_flash_parts[nparts].mask_flags = MTD_WRITEABLE; /* forces on read only */
		offset = bcm947xx_flash_parts[nparts].size;
		nparts++;
	}

#ifdef BCMCONFMTD
	/* Setup CONF MTD partition */
	bcm947xx_flash_parts[nparts].name = "confmtd";
	bcm947xx_flash_parts[nparts].size = mtd->erasesize * 4;
	bcm947xx_flash_parts[nparts].offset = offset;
	offset = bcm947xx_flash_parts[nparts].offset + bcm947xx_flash_parts[nparts].size;
	nparts++;
#endif	/* BCMCONFMTD */

#ifdef PLC
	/* Setup plc MTD partition */
	bcm947xx_flash_parts[nparts].name = "plc";
	bcm947xx_flash_parts[nparts].size = ROUNDUP(0x1000, mtd->erasesize);
	bcm947xx_flash_parts[nparts].offset =
		size - (ROUNDUP(NVRAM_SPACE, mtd->erasesize) + ROUNDUP(0x1000, mtd->erasesize));
	nparts++;
#endif
	if (rootfssize)
	{
	bcm947xx_flash_parts[nparts].name = "ddwrt";
	bcm947xx_flash_parts[nparts].offset = bcm947xx_flash_parts[2].offset + bcm947xx_flash_parts[2].size;
	bcm947xx_flash_parts[nparts].offset += (mtd->erasesize - 1);
	bcm947xx_flash_parts[nparts].offset &= ~(mtd->erasesize - 1);
	bcm947xx_flash_parts[nparts].size = (size - bcm947xx_flash_parts[nparts].offset) - ROUNDUP(NVRAM_SPACE, mtd->erasesize);
	nparts++;	
	}
	/* Setup nvram MTD partition */
	bcm947xx_flash_parts[nparts].name = "nvram";
	bcm947xx_flash_parts[nparts].size = ROUNDUP(NVRAM_SPACE, mtd->erasesize);
	if (maxsize)
	    bcm947xx_flash_parts[nparts].offset = (size - 0x10000) - bcm947xx_flash_parts[nparts].size;
	    else
	    bcm947xx_flash_parts[nparts].offset = size - bcm947xx_flash_parts[nparts].size;
	nparts++;

	return bcm947xx_flash_parts;
}

EXPORT_SYMBOL(init_mtd_partitions);

#endif /* CONFIG_MTD_PARTITIONS */

#ifdef CONFIG_MTD_NFLASH
#define NFLASH_PARTS_NUM 7
static struct mtd_partition bcm947xx_nflash_parts[NFLASH_PARTS_NUM] = {{0}};

static uint
lookup_nflash_rootfs_offset(hndnand_t *nfl, struct mtd_info *mtd, int offset, size_t size)
{
	struct romfs_super_block *romfsb;
	struct cramfs_super *cramfsb;
	struct squashfs_super_block *squashfsb;
	struct squashfs_super_block *squashfsb2;
	struct trx_header *trx;
	unsigned char *buf;
	uint blocksize, pagesize, mask, blk_offset, off, shift = 0;
	int ret;

	pagesize = nfl->pagesize;
	buf = (unsigned char *)kmalloc(pagesize, GFP_KERNEL);
	if (!buf) {
		printk("lookup_nflash_rootfs_offset: kmalloc fail\n");
		return 0;
	}

	romfsb = (struct romfs_super_block *) buf;
	cramfsb = (struct cramfs_super *) buf;
	squashfsb = (void *) buf;
	squashfsb2 = (void *) &buf[0x60];
	trx = (struct trx_header *) buf;

	/* Look at every block boundary till 16MB; higher space is reserved for application data. */
	
	blocksize = mtd->erasesize;//65536;

	if (nvram_match("boardnum","24") && nvram_match("boardtype", "0x0646")
	    && nvram_match("boardrev", "0x1110")
	    && nvram_match("gpio7", "wps_button")) {
		printk(KERN_INFO "DIR-686L Hack for detecting filesystems\n");
		blocksize = 65536;
	}


	printk("lookup_nflash_rootfs_offset: offset = 0x%x\n", offset);
	for (off = offset; off < offset + size; off += blocksize) {
		mask = blocksize - 1;
		blk_offset = off & ~mask;
		if (hndnand_checkbadb(nfl, blk_offset) != 0)
			continue;
		memset(buf, 0xe5, pagesize);
		if ((ret = hndnand_read(nfl, off, pagesize, buf)) != pagesize) {
			printk(KERN_NOTICE
			       "%s: nflash_read return %d\n", mtd->name, ret);
			continue;
		}

		/* Try looking at TRX header for rootfs offset */
		if (le32_to_cpu(trx->magic) == TRX_MAGIC) {
			mask = pagesize - 1;
			off = offset + (le32_to_cpu(trx->offsets[1]) & ~mask) - blocksize;
			shift = (le32_to_cpu(trx->offsets[1]) & mask);
			romfsb = (struct romfs_super_block *)((unsigned char *)romfsb + shift);
			cramfsb = (struct cramfs_super *)((unsigned char *)cramfsb + shift);
			squashfsb = (struct squashfs_super_block *)
				((unsigned char *)squashfsb + shift);
			squashfsb2 = NULL;
			printk(KERN_INFO "found TRX Header on nflash!\n");
			continue;
		}

		/* romfs is at block zero too */
		if (romfsb->word0 == ROMSB_WORD0 &&
		    romfsb->word1 == ROMSB_WORD1) {
			printk(KERN_NOTICE
			       "%s: romfs filesystem found at block %d\n",
			       mtd->name, off / blocksize);
			break;
		}

		/* so is cramfs */
		if (cramfsb->magic == CRAMFS_MAGIC) {
			printk(KERN_NOTICE
			       "%s: cramfs filesystem found at block %d\n",
			       mtd->name, off / blocksize);
			break;
		}
		
		if (squashfsb->s_magic == SQUASHFS_MAGIC) {
			rootfssize = le64_to_cpu(squashfsb->bytes_used);;
			printk(KERN_NOTICE
			       "%s: squash filesystem with lzma found at block %d\n",
			       mtd->name, off / blocksize);
			break;
		}
		if (squashfsb2 && squashfsb2->s_magic == SQUASHFS_MAGIC) {
			rootfssize = le64_to_cpu(squashfsb2->bytes_used);;
			off+=0x60;
			printk(KERN_NOTICE
			       "%s: squash filesystem with lzma found at block %d\n",
			       mtd->name, off / blocksize);
			break;
		}

	}

	if (buf)
		kfree(buf);

	return shift + off;
}

struct mtd_partition *
init_nflash_mtd_partitions(hndnand_t *nfl, struct mtd_info *mtd, size_t size)
{
	int bootdev;
	int knldev;
	int nparts = 0;
	uint32 offset = 0;
	uint shift = 0;
	uint32 top = 0;
	uint32 bootsz;
#ifdef CONFIG_FAILSAFE_UPGRADE
	char *img_boot = nvram_get(BOOTPARTITION);
	char *imag_1st_offset = nvram_get(IMAGE_FIRST_OFFSET);
	char *imag_2nd_offset = nvram_get(IMAGE_SECOND_OFFSET);
	unsigned int image_first_offset = 0;
	unsigned int image_second_offset = 0;
	char dual_image_on = 0;

	/* The image_1st_size and image_2nd_size are necessary if the Flash does not have any
	 * image
	 */
	dual_image_on = (img_boot != NULL && imag_1st_offset != NULL && imag_2nd_offset != NULL);

	if (dual_image_on) {
		image_first_offset = simple_strtol(imag_1st_offset, NULL, 10);
		image_second_offset = simple_strtol(imag_2nd_offset, NULL, 10);
		printk("The first offset=%x, 2nd offset=%x\n", image_first_offset,
			image_second_offset);

	}
#endif	/* CONFIG_FAILSAFE_UPGRADE */

	bootdev = soc_boot_dev((void *)sih);
	knldev = soc_knl_dev((void *)sih);

	if (bootdev == SOC_BOOTDEV_NANDFLASH) {
		bootsz = boot_partition_size(nfl->base);
		if (bootsz > mtd->erasesize) {
			/* Prepare double space in case of bad blocks */
			bootsz = (bootsz << 1);
		} else {
			/* CFE occupies at least one block */
			bootsz = mtd->erasesize;
		}
		printk("Boot partition size = %d(0x%x)\n", bootsz, bootsz);

		/* Size pmon */
		bcm947xx_nflash_parts[nparts].name = "boot";
		bcm947xx_nflash_parts[nparts].size = bootsz;
		bcm947xx_nflash_parts[nparts].offset = top;
		bcm947xx_nflash_parts[nparts].mask_flags = MTD_WRITEABLE; /* forces on read only */
		offset = bcm947xx_nflash_parts[nparts].size;
		nparts++;

		/* Setup NVRAM MTD partition */
		bcm947xx_nflash_parts[nparts].name = "nvram";
		bcm947xx_nflash_parts[nparts].size = NFL_BOOT_SIZE - offset;
		bcm947xx_nflash_parts[nparts].offset = offset;

		offset = NFL_BOOT_SIZE;
		nparts++;
	}

	if (knldev == SOC_KNLDEV_NANDFLASH) {
		/* Setup kernel MTD partition */
		bcm947xx_nflash_parts[nparts].name = "linux";
#ifdef CONFIG_FAILSAFE_UPGRADE
		if (dual_image_on) {
			bcm947xx_nflash_parts[nparts].size =
				image_second_offset - image_first_offset;
		} else
#endif
		{
			bcm947xx_nflash_parts[nparts].size =
				nparts ? (NFL_BOOT_OS_SIZE - NFL_BOOT_SIZE) : NFL_BOOT_OS_SIZE;
		}
		bcm947xx_nflash_parts[nparts].offset = offset;

		shift = lookup_nflash_rootfs_offset(nfl, mtd, offset,
			bcm947xx_nflash_parts[nparts].size);

#ifdef CONFIG_FAILSAFE_UPGRADE
		if (dual_image_on)
			offset = image_second_offset;
		else
#endif
		offset = NFL_BOOT_OS_SIZE;
		nparts++;

		/* Setup rootfs MTD partition */
		bcm947xx_nflash_parts[nparts].name = "rootfs";
#ifdef CONFIG_FAILSAFE_UPGRADE
		if (dual_image_on)
			bcm947xx_nflash_parts[nparts].size = image_second_offset - shift;
		else
#endif
		bcm947xx_nflash_parts[nparts].size = NFL_BOOT_OS_SIZE - shift;
		bcm947xx_nflash_parts[nparts].offset = shift;
		bcm947xx_nflash_parts[nparts].mask_flags = MTD_WRITEABLE;

		nparts++;

#ifdef CONFIG_FAILSAFE_UPGRADE
		/* Setup 2nd kernel MTD partition */
		if (dual_image_on) {
			bcm947xx_nflash_parts[nparts].name = "linux2";
			bcm947xx_nflash_parts[nparts].size = NFL_BOOT_OS_SIZE - image_second_offset;
			bcm947xx_nflash_parts[nparts].offset = image_second_offset;
			shift = lookup_nflash_rootfs_offset(nfl, mtd, image_second_offset,
			                                    bcm947xx_nflash_parts[nparts].size);
			nparts++;
			/* Setup rootfs MTD partition */
			bcm947xx_nflash_parts[nparts].name = "rootfs2";
			bcm947xx_nflash_parts[nparts].size = NFL_BOOT_OS_SIZE - shift;
			bcm947xx_nflash_parts[nparts].offset = shift;
			bcm947xx_nflash_parts[nparts].mask_flags = MTD_WRITEABLE;
			nparts++;
		}
#endif	/* CONFIG_FAILSAFE_UPGRADE */

	}
#if 0
	if (rootfssize)
	{
	bcm947xx_nflash_parts[nparts].name = "ddwrt";
	bcm947xx_nflash_parts[nparts].offset = bcm947xx_nflash_parts[3].offset + bcm947xx_nflash_parts[3].size;
	bcm947xx_nflash_parts[nparts].offset += (mtd->erasesize - 1);
	bcm947xx_nflash_parts[nparts].offset &= ~(mtd->erasesize - 1);
	bcm947xx_nflash_parts[nparts].size = (size - bcm947xx_nflash_parts[nparts].offset) - ROUNDUP(NVRAM_SPACE, mtd->erasesize);
	nparts++;	
	}
#endif
	return bcm947xx_nflash_parts;
}

EXPORT_SYMBOL(init_nflash_mtd_partitions);
#endif /* CONFIG_MTD_NFLASH */
