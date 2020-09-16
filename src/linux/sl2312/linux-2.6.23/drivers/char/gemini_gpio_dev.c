/*
 * 	GPIO driver for Gemini board
 * 	Provides /dev/gpio
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>	/* copy_to_user, copy_from_user */

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/arch/sl2312.h>
#include <asm/arch/irqs.h>
#include <asm/arch/gemini_gpio.h>

#define GEMINI_GPIO_BASE1		IO_ADDRESS(SL2312_GPIO_BASE)
#define GEMINI_GPIO_BASE2		IO_ADDRESS(SL2312_GPIO_BASE1)

#define GPIO_SET	2
#define MAX_GPIO_LINE	32*GPIO_SET

wait_queue_head_t gemini_gpio_wait[MAX_GPIO_LINE];

enum GPIO_REG
{
    GPIO_DATA_OUT   		= 0x00,
    GPIO_DATA_IN    		= 0x04,
    GPIO_PIN_DIR    		= 0x08,
    GPIO_BY_PASS    		= 0x0C,
    GPIO_DATA_SET   		= 0x10,
    GPIO_DATA_CLEAR 		= 0x14,
    GPIO_PULL_ENABLE 		= 0x18,
    GPIO_PULL_TYPE 			= 0x1C,
    GPIO_INT_ENABLE 		= 0x20,
    GPIO_INT_RAW_STATUS 	= 0x24,
    GPIO_INT_MASK_STATUS 	= 0x28,
    GPIO_INT_MASK 			= 0x2C,
    GPIO_INT_CLEAR 			= 0x30,
    GPIO_INT_TRIG 			= 0x34,
    GPIO_INT_BOTH 			= 0x38,
    GPIO_INT_POLAR 			= 0x3C
};

unsigned int regist_gpio_int0=0,regist_gpio_int1=0;

/* defines a specific GPIO bit number and state */
struct gpio_bit {
	unsigned char bit;
	unsigned char state;
};

#define GPIO_MAJOR    127

/*
 * ioctl calls that are permitted to the /dev/gpio interface
 */
#define GPIO_GET_BIT	0x0000001
#define GPIO_SET_BIT	0x0000002
#define GPIO_GET_CONFIG	0x0000003
#define GPIO_SET_CONFIG 0x0000004

//#define GPIO_CONFIG_OUT  1
//#define GPIO_CONFIG_IN   2



#define DEVICE_NAME "gpio"

//#define DEBUG

/*
 * GPIO interface
 */

/* /dev/gpio */
static int gpio_ioctl(struct inode *inode, struct file *file,
                     unsigned int cmd, unsigned long arg);

/* /proc/driver/gpio */
static int gpio_read_proc(char *page, char **start, off_t off,
                         int count, int *eof, void *data);

static unsigned char gpio_status;        /* bitmapped status byte.       */

/* functions for set/get gpio lines on storlink cpu */

void gpio_line_get(unsigned char pin, u32 * data)
{
	unsigned int set = pin >>5;		// each GPIO set has 32 pins
	unsigned int status,addr;

	addr = (set ? GEMINI_GPIO_BASE2:GEMINI_GPIO_BASE1) + GPIO_DATA_IN;
	status = __raw_readl(addr);
	if (set)
			*data = (status&(1<<(pin-32)))?1:0;
	else
			*data = (status&(1<<pin))?1:0;
#ifdef DEBUG
	printk(KERN_EMERG "status = %08X, pin = %d, get = %d\n", status, pin, *data);
#endif
}

void gpio_line_set(unsigned char pin, u32 high)
{
	unsigned char set = pin >>5;		// each GPIO set has 32 pins
	unsigned int status=0,addr;

	addr = (set ? GEMINI_GPIO_BASE2:GEMINI_GPIO_BASE1)+(high?GPIO_DATA_SET:GPIO_DATA_CLEAR);

	status &= ~(1 << (pin %32));
	status |= (1 << (pin % 32));
	__raw_writel(status,addr);
}

/*
 * pin = [0..63]
 * mode =
 * 			1 -- OUT
 * 			2 -- IN
 */
void gpio_line_config(unsigned char pin, unsigned char mode)
{
	unsigned char set = pin >>5;		// each GPIO set has 32 pins
	unsigned int status,addr;

	addr = (set ? GEMINI_GPIO_BASE2:GEMINI_GPIO_BASE1)+GPIO_PIN_DIR;
	status = __raw_readl(addr);

	status &= ~(1 << (pin %32));
	if (mode == 1)
			status |= (1 << (pin % 32)); /* PinDir: 0 - input, 1 - output */

	__raw_writel(status,addr);
#if 0
	/* enable pullup-high if mode is input */

	addr = (set ? GEMINI_GPIO_BASE2:GEMINI_GPIO_BASE1)+GPIO_PULL_ENABLE;
	status = __raw_readl(addr);

	status &= ~(1 << (pin %32));
	if (mode == 2) /* input */
			status |= (1 << (pin % 32)); /* PullEnable: 0 - disable, 1 - enable */

	__raw_writel(status,addr);

	addr = (set ? GEMINI_GPIO_BASE2:GEMINI_GPIO_BASE1)+GPIO_PULL_TYPE;
	status = __raw_readl(addr);

	status &= ~(1 << (pin %32));
	if (mode == 2) /* input */
			status |= (1 << (pin % 32)); /* PullType: 0 - low, 1 - high */

	__raw_writel(status,addr);
#endif
}

#define GPIO_IS_OPEN             0x01    /* means /dev/gpio is in use     */

/*
 *      Now all the various file operations that we export.
 */
static int gpio_ioctl(struct inode *inode, struct file *file,
                         unsigned int cmd, unsigned long arg)
{
		struct gpio_bit bit;
		u32 val;
//		printk(KERN_EMERG "enter ioctl %X\n",cmd);

		if (copy_from_user(&bit, (struct gpio_bit *)arg,sizeof(bit)))
				return -EFAULT;
//		printk(KERN_EMERG "cmd : %X\n",cmd);
		switch (cmd) {
				
				case GPIO_GET_BIT:
						gpio_line_get(bit.bit, &val);
						//printk(KERN_EMERG "get bit %d\n",val);
						bit.state = val;
						return copy_to_user((void *)arg, &bit, sizeof(bit)) ? -EFAULT : 0;
				case GPIO_SET_BIT:
						val = bit.state;
						//printk(KERN_EMERG "set bit %d = %d\n",bit.bit, val);
						gpio_line_set(bit.bit, val);
						return 0;
				case GPIO_GET_CONFIG:
						// gpio_line_config(bit.bit, bit.state);
						return copy_to_user((void *)arg, &bit, sizeof(bit)) ? -EFAULT : 0;
				case GPIO_SET_CONFIG:
						val = bit.state;
						//printk(KERN_EMERG "set config %d = %d\n",bit.bit, bit.state);
						gpio_line_config(bit.bit, bit.state);
						return 0;
		}
		return -EINVAL;
}


static int gpio_open(struct inode *inode, struct file *file)
{
        if (gpio_status & GPIO_IS_OPEN)
                return -EBUSY;

        gpio_status |= GPIO_IS_OPEN;
        return 0;
}


static int gpio_release(struct inode *inode, struct file *file)
{
        /*
         * Turn off all interrupts once the device is no longer
         * in use and clear the data.
         */

        gpio_status &= ~GPIO_IS_OPEN;
        return 0;
}


/*
 *      The various file operations we support.
 */

static struct file_operations gpio_fops = {
        .owner          = THIS_MODULE,
        .ioctl          = gpio_ioctl,
        .open           = gpio_open,
        .release        = gpio_release,
};



#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *dir;

/*
 *      Info exported via "/proc/driver/gpio".
 */
static int gpio_get_status(char *buf)
{
    char *p = buf;
	u32 val = 0;
	int i;
	int bit;
#ifdef DEBUG
	u32 addr;

	for (i = 0; i < 0x20; i+=4 ) {
			addr = IO_ADDRESS(SL2312_GPIO_BASE) + i;
			val = __raw_readl(addr);
			p+=sprintf(p, "GPIO0: 0x%02X: %08X\n", i, val );
	}
	for (i = 0; i < 0x20; i+=4 ) {
			addr = IO_ADDRESS(SL2312_GPIO_BASE1) + i;
			val = __raw_readl(addr);
			p+=sprintf(p, "GPIO1: 0x%02X: %08X\n", i, val );
	}
#endif

	for (i = 0; i < 32; i++) {
			gpio_line_get(i, &bit);
			if (bit)
					val |= (1 << i);
	}
	p += sprintf(p, "gpio0\t: 0x%08x\n", val);

	val = 0;
	for (i = 32; i < 64; i++) {
			gpio_line_get(i, &bit);
			if (bit)
					val |= (1 << (i-32));
	}
	p += sprintf(p, "gpio1\t: 0x%08x\n", val);

	return p - buf;
}


/* /proc/driver/gpio read op
 */
static int gpio_read_proc(char *page, char **start, off_t off,
                             int count, int *eof, void *data)
{
        int len = gpio_get_status (page);

        if (len <= off+count)
			*eof = 1;
        *start = page + off;
        len -= off;
        if ( len > count )
			len = count;
        if ( len < 0 )
			len = 0;
        return len;
}
#endif /* CONFIG_PROC_FS */


static int __init gpio_init_module(void)
{
        int retval;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *res;
#endif

        /* register /dev/gpio file ops */
	retval = register_chrdev(GPIO_MAJOR, DEVICE_NAME, &gpio_fops);
	//retval = misc_register(&gpio_dev);
        if(retval < 0)
                return retval;

#ifdef CONFIG_PROC_FS
	dir = proc_mkdir("driver/gpio", NULL);
	if (!dir) {
//		misc_deregister(&gpio_dev);
		return -ENOMEM;
	}
        /* register /proc/driver/gpio */
	res = create_proc_entry("info", 0644, dir);
	if (res) {
		res->read_proc= gpio_read_proc;
	} else {
//		misc_deregister(&gpio_dev);
		return -ENOMEM;
	}
#endif

	printk("%s: GPIO driver loaded\n", __FILE__);

	return 0;
}

static void __exit gpio_cleanup_module(void)
{
	remove_proc_entry ("info", dir);
//        misc_deregister(&gpio_dev);

	printk("%s: GPIO driver unloaded\n", __FILE__);
}

module_init(gpio_init_module);
module_exit(gpio_cleanup_module);

MODULE_AUTHOR("Jonas Majauskas");
MODULE_LICENSE("GPL");

