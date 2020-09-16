//proc_gpio: AR5315 GPIO pins in /proc/gpio/
// by olg 
// GPL'ed
// some code stolen from Yoshinori Sato <ysato@users.sourceforge.jp>

#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <asm/uaccess.h>	/* for copy_from_user */
#include <asm/gpio.h>
#include <adm5120_defs.h>
#include <adm5120_info.h>
#include <adm5120_switch.h>

#define PROCFS_MAX_SIZE 64
extern const char *get_arch_type( void );
struct proc_dir_entry *proc_gpio, *gpio_dir;

//Masks for data exchange through "void *data" pointer
#define GPIO_IN (1<<5)
#define GPIO_OUT (1<<6)
#define GPIO_DIR (1<<7)
#define PIN_MASK 0x1f
#define GPIO_CR_M(x)                (1 << (x))	/* mask for i/o */


#define GPIO_READ(r)		readl((r))
#define GPIO_WRITE(v, r)	writel((v), (r))
#define GPIO_REG(r)	(void __iomem *)(KSEG1ADDR(ADM5120_SWITCH_BASE)+r)

static void cleanup_proc( void );

//The buffer used to store the data returned by the proc file
static char procfs_buffer[PROCFS_MAX_SIZE];
static unsigned long procfs_buffer_size = 0;


#define PIN_IM(p)	((1 << GPIO_CONF0_IM_SHIFT) << p)
#define PIN_IV(p)	((1 << GPIO_CONF0_IV_SHIFT) << p)
#define PIN_OE(p)	((1 << GPIO_CONF0_OE_SHIFT) << p)
#define PIN_OV(p)	((1 << GPIO_CONF0_OV_SHIFT) << p)


static int
gpio_proc_read( char *buf, char **start, off_t offset,
		int len, int *eof, void *data )
{
	void __iomem **reg;
	u32 t;
	int ret;

	reg = GPIO_REG(SWITCH_REG_GPIO_CONF0);
        
	if( ( unsigned int )data & GPIO_IN )
	    {
	    t = GPIO_READ(reg);
	    t &= PIN_IV((( unsigned int )data & PIN_MASK));
	    if (t)
		ret=1;
	    else 
		ret=0;	    
	    }
	if( ( unsigned int )data & GPIO_OUT )
	    {
	    t = GPIO_READ(reg);
	    t &= PIN_OV((( unsigned int )data & PIN_MASK));
	    if (t)
		ret=1;
	    else 
		ret=0;	    
	    }
	if( ( unsigned int )data & GPIO_DIR )
	    {
	    t = GPIO_READ(reg);
	    t &= PIN_OE((( unsigned int )data & PIN_MASK));
	    if (t)
		ret=1;
	    else 
		ret=0;	    
	    }

    if( ret )
	buf[0] = '1';
    else
	buf[0] = '0';
    buf[1] = 0;

    *eof = 1;

    return ( 2 );

}

static int
gpio_proc_info_read( char *buf, char **start, off_t offset,
		     int len, int *eof, void *data )
{
    *eof = 1;
    return 0;
}

static int
gpio_proc_write( struct file *file, const char *buffer, unsigned long count,
		 void *data )
{
    u32 reg = 0;

    /* get buffer size */
    procfs_buffer_size = count;
    if( procfs_buffer_size > PROCFS_MAX_SIZE )
    {
	procfs_buffer_size = PROCFS_MAX_SIZE;
    }
    /* write data to the buffer */
    if( copy_from_user( procfs_buffer, buffer, procfs_buffer_size ) )
    {
	return -EFAULT;
    }

    procfs_buffer[procfs_buffer_size] = 0;

	if( procfs_buffer[0] == '0' || procfs_buffer[0] == 'i' )
	    reg = 0;
	if( procfs_buffer[0] == '1' || procfs_buffer[0] == 'o' )
	    reg = 1;
	if( ( unsigned int )data & GPIO_IN )
	{
	    adm5120_gpio_set_value(( unsigned int )data & PIN_MASK,reg);
	}
	if( ( unsigned int )data & GPIO_OUT )
	{
	    adm5120_gpio_direction_output(( unsigned int )data & PIN_MASK,reg);
	}
	if( ( unsigned int )data & GPIO_DIR )
	{
	    if (!reg)
	    adm5120_gpio_direction_input(( unsigned int )data & PIN_MASK);
		else
	    adm5120_gpio_direction_output(( unsigned int )data & PIN_MASK,0);
	}
    return procfs_buffer_size;
}

static __init int register_proc( void )
{
    unsigned char i, flag = 0;
    char proc_name[16];
    int gpiocount;

    gpiocount = 22;

    /* create directory gpio */
    gpio_dir = proc_mkdir( "gpio", NULL );
    if( gpio_dir == NULL )
	goto fault;
    gpio_dir->owner = THIS_MODULE;

    for( i = 0; i < gpiocount * 3; i++ )	//create for every GPIO "x_in"," x_out" and "x_dir"
    {
	if( i / gpiocount == 0 )
	{
	    flag = GPIO_IN;
	    sprintf( proc_name, "%i_in", i );
	}
	if( i / gpiocount == 1 )
	{
	    flag = GPIO_OUT;
	    sprintf( proc_name, "%i_out", i % gpiocount );
	}
	if( i / gpiocount == 2 )
	{
	    flag = GPIO_DIR;
	    sprintf( proc_name, "%i_dir", i % gpiocount );
	}

	proc_gpio = create_proc_entry( proc_name, S_IRUGO, gpio_dir );
	if( proc_gpio )
	{
	    proc_gpio->read_proc = gpio_proc_read;
	    proc_gpio->write_proc = gpio_proc_write;
	    proc_gpio->owner = THIS_MODULE;
	    proc_gpio->data = ( ( i % gpiocount ) | flag );
	}
	else
	    goto fault;

    }

    proc_gpio = create_proc_entry( "info", S_IRUGO, gpio_dir );
    if( proc_gpio )
    {
	proc_gpio->read_proc = gpio_proc_info_read;
	proc_gpio->owner = THIS_MODULE;
    }
    else
	goto fault;

    printk( KERN_NOTICE
	    "gpio_proc: module loaded and /proc/gpio/ created\n" );
    return 0;

  fault:
    cleanup_proc(  );
    return -EFAULT;
}

static void cleanup_proc( void )
{
    unsigned char i;
    char proc_name[16];
    int gpiocount;

    gpiocount = 8;

    for( i = 0; i < gpiocount; i++ )
    {
	sprintf( proc_name, "%i_in", i );
	remove_proc_entry( proc_name, gpio_dir );
	sprintf( proc_name, "%i_out", i );
	remove_proc_entry( proc_name, gpio_dir );
	sprintf( proc_name, "%i_dir", i );
	remove_proc_entry( proc_name, gpio_dir );
    }
    remove_proc_entry( "info", gpio_dir );
    remove_proc_entry( "gpio", NULL );
    printk( KERN_INFO "gpio_proc: unloaded and /proc/gpio/ removed\n" );

}

module_init( register_proc );
module_exit( cleanup_proc );

MODULE_AUTHOR( "olg" );
MODULE_DESCRIPTION( "ADM5120 GPIO pins in /proc/gpio/" );
MODULE_LICENSE( "GPL" );
