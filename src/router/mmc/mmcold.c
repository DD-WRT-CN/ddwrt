#include <linux/delay.h>
#include <linux/timer.h>
#include "module.h"
#include <linux/version.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/blkpg.h>
#include <linux/hdreg.h>
#include <linux/major.h>
#include <asm/uaccess.h>
#include <asm/io.h>


#define DEVICE_NAME "mmc"
#define DEVICE_NR(device) (MINOR(device))
#define DEVICE_ON(device)
#define DEVICE_OFF(device)
#define MAJOR_NR 121

#include <linux/blk.h>

static int DDDI = -1;
static int DDDO = -1;
static int DDCLK = -1;
static int DDCS = -1;


MODULE_AUTHOR ("Madsuk/Rohde & Sebastian Gottschall (autodetection for newer models)");
MODULE_DESCRIPTION ("Driver MMC/SD-Cards");
MODULE_SUPPORTED_DEVICE ("WRT54G, WHR-HP, WHR-G54S, WL-500GD");
MODULE_LICENSE ("GPL");

/* Tornado & Eko - MODULE_PARM added for user defined GPIO's
Usage - insmod mmc DDDI=0x04 DDDO=0x10 DDCLK=0x08 DDCS=0x80 */

//16.May.07 - Eko - more cleanup
//17.May.07 - Eko - loops inversion: gain 0.5% speed :(

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,52))
MODULE_PARM(DDDI,"i");
MODULE_PARM(DDDO,"i");
MODULE_PARM(DDCLK,"i");
MODULE_PARM(DDCS,"i");
#else
#include <linux/moduleparam.h>
module_param(DDDI, int, 0600);
module_param(DDDO, int, 0600);
module_param(DDCLK, int, 0600);
module_param(DDCS, int, 0600);
#endif

MODULE_PARM_DESC(DDDI, "GPIO DI Value.");
MODULE_PARM_DESC(DDDO, "GPIO DO Value.");
MODULE_PARM_DESC(DDCLK, "GPIO CLK Value");
MODULE_PARM_DESC(DDCS, "GPIO CS Value.");


#define SD_DI_TMP 0x04
#define SD_DO_TMP 0x10
#define SD_CLK_TMP 0x08
#define SD_CS_TMP 0x80


static int SD_DI = SD_DI_TMP;
static int SD_DO = SD_DO_TMP;
static int SD_CLK = SD_CLK_TMP;
static int SD_CS = SD_CS_TMP;

/* we have only one device */
static int hd_sizes[1 << 6];
static int hd_blocksizes[1 << 6];
static int hd_hardsectsizes[1 << 6];
static int hd_maxsect[1 << 6];
static struct hd_struct hd[1 << 6];

static struct timer_list mmc_timer;
static int mmc_media_detect = 0;
static int mmc_media_changed = 1;

typedef unsigned int uint32;

static unsigned char port_state = 0x00;
static volatile uint32 *gpioaddr_input = (uint32 *) 0xb8000060;
static volatile uint32 *gpioaddr_output = (uint32 *) 0xb8000064;
static volatile uint32 *gpioaddr_enable = (uint32 *) 0xb8000068;
static volatile uint32 *gpioaddr_control = (uint32 *) 0xb800006c;

static void
mmc_spi_cs_low (void)
{
    port_state &= ~(SD_CS);
    *gpioaddr_output = port_state;
}

static void
mmc_spi_cs_high (void)
{
    port_state |= SD_CS;
    *gpioaddr_output = port_state;
}

static unsigned char
mmc_spi_io (unsigned char data_out)
{
    int i;
    unsigned char result, tmp_data;

    for (i = 0x80; i; i>>=1)
    {
        if (data_out & i)
            port_state |= SD_DI;
        else
            port_state &= ~SD_DI;

        *gpioaddr_output = port_state;
        port_state |= SD_CLK;
        *gpioaddr_output = port_state;

        tmp_data = *gpioaddr_input;

        port_state &= ~SD_CLK;
        *gpioaddr_output = port_state;

        result <<= 1;
        if (tmp_data & SD_DO)
            result++;
    }

    return (result);
}

/* eventually add some __asm__ __volatile__("nop;");
   to meet timing requirements?
 */
static inline void
mmc_spi_o (unsigned char data_out)
{
    int i;

    for (i = 0x80; i; i>>=1)
    {
        if (data_out & i)
            port_state |= SD_DI;
        else
            port_state &= ~SD_DI;
        *gpioaddr_output = port_state;

        port_state |= SD_CLK;
        *gpioaddr_output = port_state;

        port_state &= ~SD_CLK;
        *gpioaddr_output = port_state;
    }
}

static inline unsigned char
mmc_spi_i (void)
{
    int i;
    unsigned char result, tmp_data;

    port_state |= SD_DI;

    for (i = 8; i; i--)
    {
        *gpioaddr_output = port_state;

        port_state |= SD_CLK;
        *gpioaddr_output = port_state;
        tmp_data = *gpioaddr_input;

        port_state &= ~SD_CLK;
        *gpioaddr_output = port_state;

        result <<= 1;

        if (tmp_data & SD_DO)
            result++;
    }

    return result;
}

static int
mmc_write_block (unsigned int dest_addr, unsigned char *data)
{
    unsigned int address;
    unsigned char r = 0;
    unsigned char ab0, ab1, ab2, ab3;
    int i;

    address = dest_addr;

    ab3 = 0xff & (address >> 24);
    ab2 = 0xff & (address >> 16);
    ab1 = 0xff & (address >> 8);
    ab0 = 0xff & address;
    mmc_spi_cs_low ();
    for (i = 4; i; i--)
        mmc_spi_io (0xff);

    mmc_spi_io (0x58);
    mmc_spi_io (ab3);		/* msb */
    mmc_spi_io (ab2);
    mmc_spi_io (ab1);
    mmc_spi_io (ab0);		/* lsb */
    mmc_spi_io (0xff);
    for (i = 8; i; i--)
    {
        r = mmc_spi_io (0xff);
        if (r == 0x00)
            break;
    }
    if (r != 0x00)
    {
        mmc_spi_cs_high ();
        mmc_spi_io (0xff);
        return (1);
    }

    mmc_spi_io (0xfe);
    for (i = 512; i; i--)
        mmc_spi_o (*data++);
    for (i = 2; i; i--)
        mmc_spi_io (0xff);

    for (i = 1000000; i; i--)
    {
        r = mmc_spi_io (0xff);
        if (r == 0xff)
            break;
    }
    if (r != 0xff)
    {
        mmc_spi_cs_high ();
        mmc_spi_io (0xff);
        return (3);
    }
    mmc_spi_cs_high ();
    mmc_spi_io (0xff);
    return (0);
}

static int
mmc_read_block (unsigned char *data, unsigned int src_addr)
{
    unsigned int address;
    unsigned char r = 0;
    unsigned char ab0, ab1, ab2, ab3;
    int i;

    address = src_addr;

    ab3 = 0xff & (address >> 24);
    ab2 = 0xff & (address >> 16);
    ab1 = 0xff & (address >> 8);
    ab0 = 0xff & address;

    mmc_spi_cs_low ();
    for (i = 4; i; i--)
        mmc_spi_io (0xff);
    mmc_spi_io (0x51);
    mmc_spi_io (ab3);		/* msb */
    mmc_spi_io (ab2);
    mmc_spi_io (ab1);
    mmc_spi_io (ab0);		/* lsb */

    mmc_spi_io (0xff);
    for (i = 8; i; i--)
    {
        r = mmc_spi_io (0xff);
        if (r == 0x00)
            break;
    }
    if (r != 0x00)
    {
        mmc_spi_cs_high ();
        mmc_spi_io (0xff);
        return (1);
    }
    for (i = 100000; i; i--)
    {
        r = mmc_spi_io (0xff);
        if (r == 0xfe)
            break;
    }
    if (r != 0xfe)
    {
        mmc_spi_cs_high ();
        mmc_spi_io (0xff);
        return (2);
    }
    for (i = 512; i; i--)
    {
        *data++ = mmc_spi_i ();
    }
    for (i = 2; i; i--)
    {
        r = mmc_spi_io (0xff);
    }
    mmc_spi_cs_high ();
    mmc_spi_io (0xff);

    return (0);
}

static void
mmc_request (request_queue_t * q)
{
    unsigned int mmc_address;
    unsigned char *buffer_address;
    int nr_sectors;
    int i;
    int cmd;
    int rc, code;

    (void) q;
    while (1)
    {
        code = 1;			// Default is success
        INIT_REQUEST;
        mmc_address =
            (CURRENT->sector +
             hd[MINOR (CURRENT->rq_dev)].start_sect) * hd_hardsectsizes[0];
        buffer_address = CURRENT->buffer;
        nr_sectors = CURRENT->current_nr_sectors;
        cmd = CURRENT->cmd;
        if (((CURRENT->sector + CURRENT->current_nr_sectors +
                hd[MINOR (CURRENT->rq_dev)].start_sect) > hd[0].nr_sects)
                || (mmc_media_detect == 0))
        {
            code = 0;
        }
        else if (cmd == READ)
        {
            spin_unlock_irq (&io_request_lock);
            for (i = 0; i < nr_sectors; i++)
            {
                rc = mmc_read_block (buffer_address, mmc_address);
                if (rc != 0)
                {
                    printk ("mmc: error in mmc_read_block (%d)\n", rc);
                    code = 0;
                    break;
                }
                else
                {
                    mmc_address += hd_hardsectsizes[0];
                    buffer_address += hd_hardsectsizes[0];
                }
            }
            spin_lock_irq (&io_request_lock);
        }
        else if (cmd == WRITE)
        {
            spin_unlock_irq (&io_request_lock);
            for (i = 0; i < nr_sectors; i++)
            {
                rc = mmc_write_block (mmc_address, buffer_address);
                if (rc != 0)
                {
                    printk ("mmc: error in mmc_write_block (%d)\n", rc);
                    code = 0;
                    break;
                }
                else
                {
                    mmc_address += hd_hardsectsizes[0];
                    buffer_address += hd_hardsectsizes[0];
                }
            }
            spin_lock_irq (&io_request_lock);
        }
        else
        {
            code = 0;
        }
        end_request (code);
    }
}


static int
mmc_open (struct inode *inode, struct file *filp)
{
    int device;
    (void) filp;

    if (mmc_media_detect == 0)
        return -ENODEV;

#if defined(MODULE)
    MOD_INC_USE_COUNT;
#endif
    return 0;
}

static int
mmc_release (struct inode *inode, struct file *filp)
{
    (void) filp;
    fsync_dev (inode->i_rdev);
    invalidate_buffers (inode->i_rdev);

#if defined(MODULE)
    MOD_DEC_USE_COUNT;
#endif
    return 0;
}

static struct gendisk hd_gendisk;
static int
mmc_revalidate (kdev_t dev)
{
    int target, max_p, start, i;
    if (mmc_media_detect == 0)
        return -ENODEV;

    target = DEVICE_NR (dev);

    max_p = hd_gendisk.max_p;
    start = target << 6;
    for (i = max_p - 1; i >= 0; i--)
    {
        int minor = start + i;
        invalidate_device (MKDEV (MAJOR_NR, minor), 1);
        hd_gendisk.part[minor].start_sect = 0;
        hd_gendisk.part[minor].nr_sects = 0;
    }

    grok_partitions (&hd_gendisk, target, 1 << 6, hd_sizes[0] * 2);

    return 0;
}

static int
mmc_ioctl (struct inode *inode, struct file *filp, unsigned int cmd,
           unsigned long arg)
{
    if (!inode || !inode->i_rdev)
        return -EINVAL;

    switch (cmd)
    {
    case BLKGETSIZE:
        return put_user (hd[MINOR (inode->i_rdev)].nr_sects,
                         (unsigned long *) arg);
    case BLKGETSIZE64:
        return put_user ((u64) hd[MINOR (inode->i_rdev)].nr_sects, (u64 *) arg);
    case BLKRRPART:
        if (!capable (CAP_SYS_ADMIN))
            return -EACCES;

        return mmc_revalidate (inode->i_rdev);
    case HDIO_GETGEO:
    {
        struct hd_geometry *loc, g;
        loc = (struct hd_geometry *) arg;
        if (!loc)
            return -EINVAL;
        g.heads = 4;
        g.sectors = 16;
        g.cylinders = hd[0].nr_sects / (4 * 16);
        g.start = hd[MINOR (inode->i_rdev)].start_sect;
        return copy_to_user (loc, &g, sizeof (g)) ? -EFAULT : 0;
    }
    default:
        return blk_ioctl (inode->i_rdev, cmd, arg);
    }
}

static int
mmc_card_init (void)
{
    unsigned char r = 0;
    short i, j;
    unsigned long flags;

    save_flags (flags);
    cli ();

    mmc_spi_cs_high ();
    for (i = 20; i; i--)
        mmc_spi_io (0xff);

    mmc_spi_cs_low ();

    mmc_spi_io (0x40);
    for (i = 4; i; i--)
        mmc_spi_io (0x00);
    mmc_spi_io (0x95);
    for (i = 8; i; i--)
    {
        r = mmc_spi_io (0xff);
        if (r == 0x01)
            break;
    }
    mmc_spi_cs_high ();
    mmc_spi_io (0xff);
    if (r != 0x01)
    {
        restore_flags (flags);
        return (1);
    }

    for (j = 10000; j; j--)
    {
        mmc_spi_cs_low ();

        mmc_spi_io (0x41);
        for (i = 4; i; i--)
            mmc_spi_io (0x00);
        mmc_spi_io (0xff);
        for (i = 8; i; i--)
        {
            r = mmc_spi_io (0xff);
            if (r == 0x00)
                break;
        }
        mmc_spi_cs_high ();
        mmc_spi_io (0xff);
        if (r == 0x00)
        {
            restore_flags (flags);
            return (0);
        }
    }
    restore_flags (flags);

    return (2);
}

static int
mmc_card_config (void)
{
    unsigned char r = 0;
    unsigned char div;
    short i;
    unsigned char csd[32];
    unsigned int c_size;
    unsigned int c_size_mult;
    unsigned int mult;
    unsigned int read_bl_len;
    unsigned int blocknr = 0;
    unsigned int block_len = 0;
    unsigned int size = 0;

    mmc_spi_cs_low ();
    for (i = 4; i; i--)
        mmc_spi_io (0xff);
    mmc_spi_io (0x49);
    for (i = 4; i; i--)
        mmc_spi_io (0x00);
    mmc_spi_io (0xff);
    for (i = 8; i; i--)
    {
        r = mmc_spi_io (0xff);
        if (r == 0x00)
            break;
    }
    if (r != 0x00)
    {
        mmc_spi_cs_high ();
        mmc_spi_io (0xff);
        return (1);
    }
    for (i = 8; i; i--)
    {
        r = mmc_spi_io (0xff);
        if (r == 0xfe)
            break;
    }
    if (r != 0xfe)
    {
        mmc_spi_cs_high ();
        mmc_spi_io (0xff);
        return (2);
    }
    for (i = 0; i < 16; i++)
    {
        r = mmc_spi_io (0xff);
        csd[i] = r;
    }
    for (i = 2; i; i--)
    {
        r = mmc_spi_io (0xff);
    }
    mmc_spi_cs_high ();
    mmc_spi_io (0xff);
    if (r == 0x00)
        return (3);

    c_size = csd[8] + csd[7] * 256 + (csd[6] & 0x03) * 256 * 256;
    c_size >>= 6;
    c_size_mult = csd[10] + (csd[9] & 0x03) * 256;
    c_size_mult >>= 7;
    read_bl_len = csd[5] & 0x0f;
    mult = 1;
    mult <<= c_size_mult + 2;
    blocknr = (c_size + 1) * mult;
    block_len = 1;
    block_len <<= read_bl_len;
    if (block_len > 512)
    {
        div = block_len / 512;
        printk ("Work around for large cards >= 1gb !\n");
        printk ("Attention : Card reports to have a block_len of %i bytes, cutting down to %i x 512 bytes !\n", block_len, div);
        block_len = 512;
        blocknr = blocknr * div;
    }
    size = block_len * blocknr;
    size >>= 10;

    for (i = 0; i < (1 << 6); i++)
    {
        hd_blocksizes[i] = 1024;
        hd_hardsectsizes[i] = block_len;
        hd_maxsect[i] = 256;
    }
    hd_sizes[0] = size;
    hd[0].nr_sects = blocknr;


    printk ("Size = %d, hardsectsize = %d, sectors = %d\n",
            size, block_len, blocknr);

    return 0;
}

static int
mmc_hardware_init (void)
{
    unsigned char gpio_outen;
// OLD HACK ADDED
    unsigned char gpio_control;

    // Set inputs/outputs here

    gpio_control = *gpioaddr_control;

    gpio_control = (gpio_control & ~(SD_DI | SD_CLK | SD_CS | SD_DO));
    *gpioaddr_control = gpio_control;

    gpio_outen = *gpioaddr_enable;

    gpio_outen = (gpio_outen | SD_DI | SD_CLK | SD_CS) & ~SD_DO;
    *gpioaddr_enable = gpio_outen;

    port_state = *gpioaddr_input;

    // Clock low
    port_state &= ~(SD_CLK | SD_DI | SD_CS);
    *gpioaddr_output = port_state;

    return 0;
}

static int
mmc_check_media_change (kdev_t dev)
{
    (void) dev;
    if (mmc_media_changed == 1)
    {
        mmc_media_changed = 0;
        return 1;
    }
    else
        return 0;
}

static struct block_device_operations mmc_bdops =
    {
    open:
        mmc_open,
    release:
        mmc_release,
    ioctl:
        mmc_ioctl,
#if 0
    check_media_change:
        mmc_check_media_change,
    revalidate:
        mmc_revalidate,
#endif
    };

static struct gendisk hd_gendisk =
    {
    major:
        MAJOR_NR,
    major_name:
        DEVICE_NAME,
    minor_shift:
        6,
    max_p:
        1 << 6,
    part:
        hd,
    sizes:
        hd_sizes,
    fops:
        &mmc_bdops,
    };

static int
mmc_init (void)
{
    int rc;

    rc = mmc_hardware_init ();
	if (rc != 0)
		rc = mmc_hardware_init (); //another shot
	
	if (rc != 0)
		{
		printk ("mmc: error in mmc_hardware_init (%d)\n", rc); 
 		return -1; 
		}
			
	rc = mmc_card_init ();
    if (rc != 0)
    	rc = mmc_card_init ();  //another shot

	if (rc != 0)
		{
		printk ("mmc: this board has no MMC mod installed!\n", rc); 
 		return -1; 
		}    	

    memset (hd_sizes, 0, sizeof (hd_sizes));
    rc = mmc_card_config ();

    if (rc != 0)
    	{
        printk ("mmc: error in mmc_card_config (%d)\n", rc);
        return -1;
    	}


    blk_size[MAJOR_NR] = hd_sizes;

    memset (hd, 0, sizeof (hd));
    hd[0].nr_sects = hd_sizes[0] * 2;

    blksize_size[MAJOR_NR] = hd_blocksizes;
    hardsect_size[MAJOR_NR] = hd_hardsectsizes;
    max_sectors[MAJOR_NR] = hd_maxsect;

    hd_gendisk.nr_real = 1;

    register_disk (&hd_gendisk, MKDEV (MAJOR_NR, 0), 1 << 6,
                   &mmc_bdops, hd_sizes[0] * 2);

    return 0;
}

static void
mmc_exit (void)
{
    blk_size[MAJOR_NR] = NULL;
    blksize_size[MAJOR_NR] = NULL;
    hardsect_size[MAJOR_NR] = NULL;
    max_sectors[MAJOR_NR] = NULL;
    hd[0].nr_sects = 0;
}

static int
mmc_check_media (void)
{
    int old_state;
    int rc = 0;

    old_state = mmc_media_detect;

    // TODO: Add card detection here
    mmc_media_detect = 1;
    if (old_state != mmc_media_detect)
    {
        mmc_media_changed = 1;
        if (mmc_media_detect == 1)
        {
            rc = mmc_init ();
            if (rc != 0)
                printk ("mmc: error in mmc_init (%d)\n", rc);
        }
        else
        {
            mmc_exit ();
        }
    }
    return rc;
    /* del_timer(&mmc_timer);
       mmc_timer.expires = jiffies + 10*HZ;
       add_timer(&mmc_timer); */
}

static int
mmc_driver_init (void)
{


    int rc;

    if (DDDI != -1)
        SD_DI = DDDI;

    if (DDDO != -1)
        SD_DO = DDDO;

    if (DDCLK != -1)
        SD_CLK = DDCLK;

    if (DDCS != -1)
        SD_CS = DDCS;

    printk("mmc: starting module with: SD_DI=0x%x, SD_DO=0x%x, SD_CLK=0x%x, SD_CS=0x%x\n", SD_DI, SD_DO, SD_CLK, SD_CS);

    rc = devfs_register_blkdev (MAJOR_NR, DEVICE_NAME, &mmc_bdops);
    if (rc < 0)
    {
        printk (KERN_WARNING "mmc: can't get major %d\n", MAJOR_NR);
        return rc;
    }

    blk_init_queue (BLK_DEFAULT_QUEUE (MAJOR_NR), mmc_request);

    read_ahead[MAJOR_NR] = 8;
    add_gendisk (&hd_gendisk);

    rc = mmc_check_media ();

    /*init_timer(&mmc_timer);
       mmc_timer.expires = jiffies + HZ;
       mmc_timer.function = (void *)mmc_check_media;
       add_timer(&mmc_timer); */

    return rc;
}
module_init (mmc_driver_init);

static void
mmc_driver_exit (void)
{
    int i;
    del_timer (&mmc_timer);

    for (i = 0; i < (1 << 6); i++)
        fsync_dev (MKDEV (MAJOR_NR, i));

    blk_cleanup_queue (BLK_DEFAULT_QUEUE (MAJOR_NR));
    del_gendisk (&hd_gendisk);
    devfs_unregister_blkdev (MAJOR_NR, DEVICE_NAME);
    mmc_exit ();
}
module_exit (mmc_driver_exit);
