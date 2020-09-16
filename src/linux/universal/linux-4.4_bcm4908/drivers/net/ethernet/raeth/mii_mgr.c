#include <linux/module.h>
#include <linux/version.h>
#include <linux/netdevice.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
#include <asm/rt2880/rt_mmap.h>
#endif

#include "ra2882ethreg.h"
#include "raether.h"


#if defined (CONFIG_RALINK_RT3052) || defined (CONFIG_RALINK_RT3352) || defined (CONFIG_RALINK_RT5350) 
#define PHY_CONTROL_0 		0xC0   
#define PHY_CONTROL_1 		0xC4   
#define MDIO_PHY_CONTROL_0	(RALINK_ETH_SW_BASE + PHY_CONTROL_0)
#define MDIO_PHY_CONTROL_1 	(RALINK_ETH_SW_BASE + PHY_CONTROL_1)

#define GPIO_MDIO_BIT		(1<<7)
#define GPIO_PURPOSE_SELECT	0x60
#define GPIO_PRUPOSE		(RALINK_SYSCTL_BASE + GPIO_PURPOSE_SELECT)

#elif defined (CONFIG_RALINK_RT6855)  || defined (CONFIG_RALINK_RT6855A)

#define PHY_CONTROL_0 		0x7004   
#define MDIO_PHY_CONTROL_0	(RALINK_ETH_SW_BASE + PHY_CONTROL_0)

#define GPIO_MDIO_BIT		(1<<7)
#define GPIO_PURPOSE_SELECT	0x60
#define GPIO_PRUPOSE		(RALINK_SYSCTL_BASE + GPIO_PURPOSE_SELECT)

#elif defined (CONFIG_RALINK_MT7620)

#define PHY_CONTROL_0 		0x7004   
#define MDIO_PHY_CONTROL_0	(RALINK_ETH_SW_BASE + PHY_CONTROL_0)

#define GPIO_MDIO_BIT		(2<<7)
#define GPIO_PURPOSE_SELECT	0x60
#define GPIO_PRUPOSE		(RALINK_SYSCTL_BASE + GPIO_PURPOSE_SELECT)

#elif defined (CONFIG_RALINK_MT7621)

#define PHY_CONTROL_0 		0x0004   
#define MDIO_PHY_CONTROL_0	(RALINK_ETH_SW_BASE + PHY_CONTROL_0)

#define GPIO_MDIO_BIT		(2<<7)
#define GPIO_PURPOSE_SELECT	0x60
#define GPIO_PRUPOSE		(RALINK_SYSCTL_BASE + GPIO_PURPOSE_SELECT)

#else 
#define PHY_CONTROL_0       	0x00
#define PHY_CONTROL_1       	0x04
#define MDIO_PHY_CONTROL_0	(RALINK_FRAME_ENGINE_BASE + PHY_CONTROL_0)
#define MDIO_PHY_CONTROL_1	(RALINK_FRAME_ENGINE_BASE + PHY_CONTROL_1)
#define enable_mdio(x)
#endif

#if defined (CONFIG_RALINK_RT3052) || defined (CONFIG_RALINK_RT3352) || defined (CONFIG_RALINK_RT5350) || \
    defined (CONFIG_RALINK_RT6855) || defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_MT7621)
void enable_mdio(int enable)
{
#if !defined (CONFIG_P5_MAC_TO_PHY_MODE) && !defined(CONFIG_GE1_RGMII_AN) && !defined(CONFIG_GE2_RGMII_AN) && \
    !defined (CONFIG_GE1_MII_AN) && !defined (CONFIG_GE2_MII_AN)
	u32 data = sysRegRead(GPIO_PRUPOSE);
	if (enable)
		data &= ~GPIO_MDIO_BIT;
	else
		data |= GPIO_MDIO_BIT;
	sysRegWrite(GPIO_PRUPOSE, data);
#endif
}
#elif defined (CONFIG_RALINK_RT6855A)
void enable_mdio(int enable)
{
	/*need to check RT6855A MII/GPIO pin share scheme*/
}
#endif

#if defined (CONFIG_RALINK_RT6855) || defined (CONFIG_RALINK_RT6855A) || \
    defined (CONFIG_RALINK_MT7620) || defined (CONFIG_RALINK_MT7621)

u32 mii_mgr_read(u32 phy_addr, u32 phy_register, u32 *read_data)
{
	u32 volatile status = 0;
	u32 rc = 0;
	unsigned long volatile t_start = jiffies;
	u32 volatile data = 0;

	/* We enable mdio gpio purpose register, and disable it when exit. */
	enable_mdio(1);

	// make sure previous read operation is complete
	while (1) {
			// 0 : Read/write operation complete
		if(!( sysRegRead(MDIO_PHY_CONTROL_0) & (0x1 << 31))) 
		{
			break;
		}
		else if (time_after(jiffies, t_start + 5*HZ)) {
			enable_mdio(0);
			printk("\n MDIO Read operation is ongoing !!\n");
			return rc;
		}
	}

	data  = (0x01 << 16) | (0x02 << 18) | (phy_addr << 20) | (phy_register << 25);
	sysRegWrite(MDIO_PHY_CONTROL_0, data);
	data |= (1<<31);
	sysRegWrite(MDIO_PHY_CONTROL_0, data);
	//printk("\n Set Command [0x%08X] to PHY !!\n",MDIO_PHY_CONTROL_0);


	// make sure read operation is complete
	t_start = jiffies;
	while (1) {
		if (!(sysRegRead(MDIO_PHY_CONTROL_0) & (0x1 << 31))) {
			status = sysRegRead(MDIO_PHY_CONTROL_0);
			*read_data = (u32)(status & 0x0000FFFF);

			enable_mdio(0);
			return 1;
		}
		else if (time_after(jiffies, t_start+5*HZ)) {
			enable_mdio(0);
			printk("\n MDIO Read operation is ongoing and Time Out!!\n");
			return 0;
		}
	}
}

u32 mii_mgr_write(u32 phy_addr, u32 phy_register, u32 write_data)
{
	unsigned long volatile t_start=jiffies;
	u32 volatile data;

	enable_mdio(1);

	// make sure previous write operation is complete
	while(1) {
		if (!(sysRegRead(MDIO_PHY_CONTROL_0) & (0x1 << 31))) 
		{
			break;
		}
		else if (time_after(jiffies, t_start + 5 * HZ)) {
			enable_mdio(0);
			printk("\n MDIO Write operation ongoing\n");
			return 0;
		}
	}

	data = (0x01 << 16)| (1<<18) | (phy_addr << 20) | (phy_register << 25) | write_data;
	sysRegWrite(MDIO_PHY_CONTROL_0, data);
	data |= (1<<31);
	sysRegWrite(MDIO_PHY_CONTROL_0, data); //start operation
	//printk("\n Set Command [0x%08X] to PHY !!\n",MDIO_PHY_CONTROL_0);

	t_start = jiffies;

	// make sure write operation is complete
	while (1) {
		if (!(sysRegRead(MDIO_PHY_CONTROL_0) & (0x1 << 31))) //0 : Read/write operation complete
		{
			enable_mdio(0);
			return 1;
		}
		else if (time_after(jiffies, t_start + 5 * HZ)) {
			enable_mdio(0);
			printk("\n MDIO Write operation Time Out\n");
			return 0;
		}
	}
}

#else // not rt6855

u32 mii_mgr_read(u32 phy_addr, u32 phy_register, u32 *read_data)
{
	u32 volatile status = 0;
	u32 rc = 0;
	unsigned long volatile t_start = jiffies;
#if !defined (CONFIG_RALINK_RT3052) && !defined (CONFIG_RALINK_RT3352) && !defined (CONFIG_RALINK_RT5350)
	u32 volatile data = 0;
#endif

	/* We enable mdio gpio purpose register, and disable it when exit. */
	enable_mdio(1);

	// make sure previous read operation is complete
	while (1) {
#if defined (CONFIG_RALINK_RT3052) || defined (CONFIG_RALINK_RT3352) || defined (CONFIG_RALINK_RT5350)
		// rd_rdy: read operation is complete
		if(!( sysRegRead(MDIO_PHY_CONTROL_1) & (0x1 << 1))) 
#else
			// 0 : Read/write operation complet
		if(!( sysRegRead(MDIO_PHY_CONTROL_0) & (0x1 << 31))) 
#endif
		{
			break;
		}
		else if (time_after(jiffies, t_start + 5*HZ)) {
			enable_mdio(0);
			printk("\n MDIO Read operation is ongoing !!\n");
			return rc;
		}
	}

#if defined (CONFIG_RALINK_RT3052) || defined (CONFIG_RALINK_RT3352) || defined (CONFIG_RALINK_RT5350)
	sysRegWrite(MDIO_PHY_CONTROL_0 , (1<<14) | (phy_register << 8) | (phy_addr));
#else
	data  = (phy_addr << 24) | (phy_register << 16);
	sysRegWrite(MDIO_PHY_CONTROL_0, data);
	data |= (1<<31);
	sysRegWrite(MDIO_PHY_CONTROL_0, data);
#endif
	//printk("\n Set Command [0x%08X] to PHY !!\n",MDIO_PHY_CONTROL_0);


	// make sure read operation is complete
	t_start = jiffies;
	while (1) {
#if defined (CONFIG_RALINK_RT3052) || defined (CONFIG_RALINK_RT3352) || defined (CONFIG_RALINK_RT5350)
		if (sysRegRead(MDIO_PHY_CONTROL_1) & (0x1 << 1)) {
			status = sysRegRead(MDIO_PHY_CONTROL_1);
			*read_data = (u32)(status >>16);

			enable_mdio(0);
			return 1;
		}
#else
		if (!(sysRegRead(MDIO_PHY_CONTROL_0) & (0x1 << 31))) {
			status = sysRegRead(MDIO_PHY_CONTROL_0);
			*read_data = (u32)(status & 0x0000FFFF);

			enable_mdio(0);
			return 1;
		}
#endif
		else if (time_after(jiffies, t_start+5*HZ)) {
			enable_mdio(0);
			printk("\n MDIO Read operation is ongoing and Time Out!!\n");
			return 0;
		}
	}
}


u32 mii_mgr_write(u32 phy_addr, u32 phy_register, u32 write_data)
{
	unsigned long volatile t_start=jiffies;
	u32 volatile data;

	enable_mdio(1);

	// make sure previous write operation is complete
	while(1) {
#if defined (CONFIG_RALINK_RT3052) || defined (CONFIG_RALINK_RT3352) || defined (CONFIG_RALINK_RT5350)
		if (!(sysRegRead(MDIO_PHY_CONTROL_1) & (0x1 << 0)))
#else
		if (!(sysRegRead(MDIO_PHY_CONTROL_0) & (0x1 << 31))) 
#endif
		{
			break;
		}
		else if (time_after(jiffies, t_start + 5 * HZ)) {
			enable_mdio(0);
			printk("\n MDIO Write operation ongoing\n");
			return 0;
		}
	}

#if defined (CONFIG_RALINK_RT3052) || defined (CONFIG_RALINK_RT3352) || defined (CONFIG_RALINK_RT5350)
	data = ((write_data & 0xFFFF) << 16);
	data |= (phy_register << 8) | (phy_addr);
	data |= (1<<13);
	sysRegWrite(MDIO_PHY_CONTROL_0, data);
#else
	data = (1<<30) | (phy_addr << 24) | (phy_register << 16) | write_data;
	sysRegWrite(MDIO_PHY_CONTROL_0, data);
	data |= (1<<31);
	sysRegWrite(MDIO_PHY_CONTROL_0, data); //start operation
#endif
	//printk("\n Set Command [0x%08X] to PHY !!\n",MDIO_PHY_CONTROL_0);

	t_start = jiffies;

	// make sure write operation is complete
	while (1) {
#if defined (CONFIG_RALINK_RT3052) || defined (CONFIG_RALINK_RT3352) || defined (CONFIG_RALINK_RT5350)
		if (sysRegRead(MDIO_PHY_CONTROL_1) & (0x1 << 0)) //wt_done ?= 1
#else
		if (!(sysRegRead(MDIO_PHY_CONTROL_0) & (0x1 << 31))) //0 : Read/write operation complete
#endif
		{
			enable_mdio(0);
			return 1;
		}
		else if (time_after(jiffies, t_start + 5 * HZ)) {
			enable_mdio(0);
			printk("\n MDIO Write operation Time Out\n");
			return 0;
		}
	}
}




#endif




EXPORT_SYMBOL(mii_mgr_write);
EXPORT_SYMBOL(mii_mgr_read);
