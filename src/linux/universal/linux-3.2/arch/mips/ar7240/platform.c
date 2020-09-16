#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/resource.h>

#include <linux/console.h>
#include <asm/serial.h>

#include <linux/tty.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/serial_8250.h>

#include <asm/mach-ar7240/ar7240.h>
#include <asm/mach-ar71xx/ar71xx.h>
#include "nvram.h"
#include "devices.h"
#include <asm/mach-ar71xx/ar933x_uart_platform.h>
#include <linux/ar8216_platform.h>

void serial_print(char *fmt, ...);

#ifdef CONFIG_WASP_SUPPORT
extern uint32_t ath_ref_clk_freq;
#else
extern uint32_t ar7240_ahb_freq;
#endif

/* 
 * OHCI (USB full speed host controller) 
 */
static struct resource ar7240_usb_ohci_resources[] = {
	[0] = {
		.start		= AR7240_USB_OHCI_BASE,
		.end		= AR7240_USB_OHCI_BASE + AR7240_USB_WINDOW - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= AR7240_CPU_IRQ_USB,
        .end	    = AR7240_CPU_IRQ_USB,
		.flags		= IORESOURCE_IRQ,
	},
};

/* 
 * The dmamask must be set for OHCI to work 
 */
static u64 ohci_dmamask = ~(u32)0;
static struct platform_device ar7240_usb_ohci_device = {
	.name		= "ar7240-ohci",
	.id		    = 0,
	.dev = {
		.dma_mask		= &ohci_dmamask,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(ar7240_usb_ohci_resources),
	.resource	= ar7240_usb_ohci_resources,
};

/* 
 * EHCI (USB full speed host controller) 
 */
static struct resource ar7240_usb_ehci_resources[] = {
	[0] = {
		.start		= AR7240_USB_EHCI_BASE,
		.end		= AR7240_USB_EHCI_BASE + AR7240_USB_WINDOW - 1,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= AR7240_CPU_IRQ_USB,
		.end		= AR7240_CPU_IRQ_USB,
		.flags		= IORESOURCE_IRQ,
	},
};

/* 
 * The dmamask must be set for EHCI to work 
 */
static u64 ehci_dmamask = ~(u32)0;

static struct platform_device ar7240_usb_ehci_device = {
	.name		= "ar71xx-ehci",
	.id		    = 0,
	.dev = {
		.dma_mask		= &ehci_dmamask,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(ar7240_usb_ehci_resources),
	.resource	= ar7240_usb_ehci_resources,
};



static struct resource ar7240_uart_resources[] = {
	{
		.start = AR7240_UART_BASE,
		.end = AR7240_UART_BASE+0x0fff,
		.flags = IORESOURCE_MEM,
	},
};

#define AR71XX_UART_FLAGS (UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_IOREMAP)

static struct plat_serial8250_port ar7240_uart_data[] = {
	{
                .mapbase        = AR7240_UART_BASE,
                .irq            = AR7240_MISC_IRQ_UART,
                .flags          = AR71XX_UART_FLAGS,
                .iotype         = UPIO_MEM32,
                .regshift       = 2,
                .uartclk        = 0, /* ar7240_ahb_freq, */
	},
        { },
};

static struct platform_device ar7240_uart = {
	 .name               = "serial8250",
        .id                 = 0,
        .dev.platform_data  = ar7240_uart_data,
        .num_resources      = 1, 
        .resource           = ar7240_uart_resources

};


static struct resource ar933x_uart_resources[] = {
	{
		.start  = AR933X_UART_BASE,
		.end    = AR933X_UART_BASE + AR71XX_UART_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = AR7240_MISC_IRQ_UART,
		.end    = AR7240_MISC_IRQ_UART,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct ar933x_uart_platform_data ar933x_uart_data;
static struct platform_device ar933x_uart_device = {
	.name           = "ar933x-uart",
	.id             = -1,
	.resource       = ar933x_uart_resources,
	.num_resources  = ARRAY_SIZE(ar933x_uart_resources),
	.dev = {
		.platform_data  = &ar933x_uart_data,
	},
};

static struct resource ath_uart_resources[] = {
	{
	 .start = AR933X_UART_BASE,
	 .end = AR933X_UART_BASE + 0x0fff,
	 .flags = IORESOURCE_MEM,
	 },
};

static struct plat_serial8250_port ath_uart_data[] = {
	{
	 .mapbase = (u32) KSEG1ADDR(AR933X_UART_BASE),
	 .membase = (void __iomem *)((u32) (KSEG1ADDR(AR933X_UART_BASE))),
	 .irq = AR7240_MISC_IRQ_UART,
	 .flags = (UPF_BOOT_AUTOCONF | UPF_SKIP_TEST),
	 .iotype = UPIO_MEM32,
	 .regshift = 2,
	 .uartclk = 0,		/* ath_ahb_freq, */
	 },
	{},
};

static struct platform_device ath_uart = {
	.name = "serial8250",
	.id = 0,
	.dev.platform_data = ath_uart_data,
	.num_resources = 1,
	.resource = ath_uart_resources
};



static struct platform_device *ar7241_platform_devices[] __initdata = {
	&ar7240_usb_ehci_device
};

static struct platform_device *ar7240_platform_devices[] __initdata = {
	&ar7240_usb_ohci_device
};

static struct platform_device *ar724x_platform_devices[] __initdata = {
#ifdef CONFIG_MACH_HORNET
	&ar933x_uart_device,
	&ath_uart
#else
	&ar7240_uart
#endif
};

static struct ar8327_pad_cfg db120_ar8327_pad0_cfg = {
	.mode = AR8327_PAD_MAC_RGMII,
	.txclk_delay_en = true,
	.rxclk_delay_en = true,
	.txclk_delay_sel = AR8327_CLK_DELAY_SEL1,
	.rxclk_delay_sel = AR8327_CLK_DELAY_SEL2,
 };


static struct ar8327_platform_data db120_ar8327_data = {
	.pad0_cfg = &db120_ar8327_pad0_cfg,
	.cpuport_cfg = {
		.force_link = 1,

#ifdef CONFIG_DIR615I
		.speed = AR8327_PORT_SPEED_100,
#else
		.speed = AR8327_PORT_SPEED_1000,
#endif		
		.duplex = 1,
		.txpause = 1,
		.rxpause = 1,
 	}
 };


static struct mdio_board_info db120_mdio0_info[] = {
	{
		.bus_id = "ag71xx-mdio.0",
		.phy_addr = 0,
		.platform_data = &db120_ar8327_data,
	},
 };


extern __init ap91_pci_init(u8 *cal_data, u8 *mac_addr);
void ar9xxx_add_device_wmac(u8 *cal_data, u8 *mac_addr) __init;

static void *getCalData(int slot)
{
u8 *base;
for (base=(u8 *) KSEG1ADDR(0x1f000000);base<KSEG1ADDR (0x1ffff000);base+=0x1000) {
	u32 *cal = (u32 *)base;
	if (*cal==0xa55a0000 || *cal==0x5aa50000) { //protection bit is always zero on inflash devices, so we can use for match it
		if (slot) {
			base+=0x4000;
		}
		printk(KERN_INFO "found calibration data for slot %d on 0x%08X\n",slot,base);
		return base;
	}
    }
return NULL;
}

enum ar71xx_soc_type ar71xx_soc;
EXPORT_SYMBOL_GPL(ar71xx_soc);

#if defined(CONFIG_DIR825C1) || defined(CONFIG_DIR615I)
#define DIR825C1_MAC_LOCATION_0			0x1ffe0004
#define DIR825C1_MAC_LOCATION_1			0x1ffe0018
#define DIR615I_MAC_LOCATION_0			0x1fffffb4
static u8 mac0[6];
static u8 mac1[6];

static void dir825b1_read_ascii_mac(u8 *dest, unsigned int src_addr)
{
	int ret;
	u8 *src = (u8 *)KSEG1ADDR(src_addr);

	ret = sscanf(src, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
		     &dest[0], &dest[1], &dest[2],
		     &dest[3], &dest[4], &dest[5]);

	if (ret != 6)
		memset(dest, 0, 6);
}



#endif


int __init ar7240_platform_init(void)
{
	int ret;
	void *ee;
#ifdef CONFIG_WR741
	u8 *mac = (u8 *) KSEG1ADDR(0x1f01fc00);
#else
	u8 *mac = NULL;//(u8 *) KSEG1ADDR(0x1fff0000);
#endif

#if defined(CONFIG_AR7242_RTL8309G_PHY) || defined(CONFIG_DIR615E)
#ifdef CONFIG_DIR615E
	const char *config = (char *) KSEG1ADDR(0x1f030000);
#else
	const char *config = (char *) KSEG1ADDR(0x1f040000);
#endif
	u8 wlan_mac[6];
	if (nvram_parse_mac_addr(config, 0x10000,"lan_mac=", wlan_mac) == 0) {
		mac = wlan_mac;
	}
#endif



        /* need to set clock appropriately */
#ifdef CONFIG_MACH_HORNET

	ath_uart_data[0].uartclk = ar71xx_ref_freq;
	ar933x_uart_data.uartclk = ar71xx_ref_freq;
#endif

#ifdef CONFIG_WASP_SUPPORT
	ar7240_uart_data[0].uartclk = ath_ref_clk_freq;
#else
	ar7240_uart_data[0].uartclk = ar7240_ahb_freq;
#endif
#ifdef CONFIG_WASP_SUPPORT
#define DB120_MAC0_OFFSET	0
#define DB120_MAC1_OFFSET	6
#ifdef CONFIG_DIR825C1
	u8 *art = (u8 *) KSEG1ADDR(0x1fff1000);
#else
	u8 *art = (u8 *) KSEG1ADDR(0x1fff0000);
#endif
	void __iomem *base;
	u32 t;

	dir825b1_read_ascii_mac(mac0, DIR825C1_MAC_LOCATION_0);
	dir825b1_read_ascii_mac(mac1, DIR825C1_MAC_LOCATION_1);


#ifndef CONFIG_DIR615I
	base = ioremap(AR934X_GMAC_BASE, AR934X_GMAC_SIZE);


	t = __raw_readl(base + AR934X_GMAC_REG_ETH_CFG);
	t &= ~(AR934X_ETH_CFG_RGMII_GMAC0 | AR934X_ETH_CFG_MII_GMAC0 |
	       AR934X_ETH_CFG_MII_GMAC0 | AR934X_ETH_CFG_SW_ONLY_MODE);
	t |= AR934X_ETH_CFG_RGMII_GMAC0 | AR934X_ETH_CFG_SW_ONLY_MODE;

	__raw_writel(t, base + AR934X_GMAC_REG_ETH_CFG);


	iounmap(base);
#else
	dir825b1_read_ascii_mac(mac0, DIR615I_MAC_LOCATION_0);
	base = ioremap(AR934X_GMAC_BASE, AR934X_GMAC_SIZE);
	
	t = __raw_readl(base + AR934X_GMAC_REG_ETH_CFG);
	t &= ~(AR934X_ETH_CFG_RGMII_GMAC0 | AR934X_ETH_CFG_MII_GMAC0 |
	       AR934X_ETH_CFG_MII_GMAC0 | AR934X_ETH_CFG_SW_ONLY_MODE);
//	t |= AR934X_ETH_CFG_MII_GMAC0 | AR934X_ETH_CFG_SW_ONLY_MODE;

	__raw_writel(t, base + AR934X_GMAC_REG_ETH_CFG);


	iounmap(base);

#endif


#ifdef CONFIG_DIR615I
	ar71xx_add_device_mdio(1, 0x0);
	ar71xx_add_device_mdio(0, 0x0);
	ar71xx_init_mac(ar71xx_eth0_data.mac_addr, art + DB120_MAC0_OFFSET, 0);
	ar71xx_eth0_data.phy_if_mode = PHY_INTERFACE_MODE_MII;
	ar71xx_eth0_data.mii_bus_dev = &ar71xx_mdio1_device.dev;
	ar71xx_eth0_data.phy_mask = BIT(4);
	ar71xx_switch_data.phy4_mii_en = 1;

	ar71xx_eth1_data.speed = SPEED_1000;
	ar71xx_eth1_data.duplex = DUPLEX_FULL;
	ar71xx_eth1_data.phy_if_mode = PHY_INTERFACE_MODE_GMII;

	ar71xx_add_device_eth(0);
	ar71xx_add_device_eth(1);
#else
	ar71xx_add_device_mdio(1, 0x0);
	ar71xx_add_device_mdio(0, 0x0);

#ifdef CONFIG_DIR825C1
	ar71xx_init_mac(ar71xx_eth0_data.mac_addr, mac0, 0);
#else
	ar71xx_init_mac(ar71xx_eth0_data.mac_addr, art + DB120_MAC0_OFFSET, 0);
#endif

	mdiobus_register_board_info(db120_mdio0_info,
				    ARRAY_SIZE(db120_mdio0_info));


	ar71xx_eth0_data.phy_if_mode = PHY_INTERFACE_MODE_RGMII;
	ar71xx_eth0_data.phy_mask = BIT(0);
	ar71xx_eth0_data.mii_bus_dev = &ar71xx_mdio0_device.dev;
	ar71xx_eth0_pll_data.pll_1000 = 0x06000000;

	ar71xx_add_device_eth(0);

	/* GMAC1 is connected to the internal switch */
	ar71xx_init_mac(ar71xx_eth1_data.mac_addr, art + DB120_MAC1_OFFSET, 0);
	ar71xx_eth1_data.phy_if_mode = PHY_INTERFACE_MODE_GMII;
	ar71xx_eth1_data.speed = SPEED_1000;
	ar71xx_eth1_data.duplex = DUPLEX_FULL;

	ar71xx_add_device_eth(1);
#endif

#endif


	ret = platform_add_devices(ar724x_platform_devices, 
                                ARRAY_SIZE(ar724x_platform_devices));

        if (ret < 0)
		return ret; 

	if (is_ar7241() || is_ar7242()  || is_ar933x() || is_ar934x()) {
	    ret = platform_add_devices(ar7241_platform_devices, 
                                ARRAY_SIZE(ar7241_platform_devices));
        }
        if (is_ar7240()) {
	    ret = platform_add_devices(ar7240_platform_devices, 
                                ARRAY_SIZE(ar7240_platform_devices));
        }
        
#ifdef CONFIG_MACH_HORNET
	ee = (u8 *) KSEG1ADDR(0x1fff1000);
	ar9xxx_add_device_wmac(ee, mac);
#elif CONFIG_WASP_SUPPORT
	ee = (u8 *) KSEG1ADDR(0x1fff1000);
	ar9xxx_add_device_wmac(ee, mac0);

#ifdef CONFIG_DIR825C1
	ap91_pci_init(ee+0x4000, mac1);
#else
#ifndef CONFIG_DIR615I
	ap91_pci_init(NULL, NULL);
#endif
#endif
#else
	ee = getCalData(0);
	if (ee && !mac) {
	    if (!memcmp(((u8 *)ee)+0x20c,"\xff\xff\xff\xff\xff\xff",6) || !memcmp(((u8 *)ee)+0x20c,"\x00\x00\x00\x00\x00\x00",6)) {
		printk("Found empty mac address in calibration dataset, leave the responsibility to the driver to use the correct one\n");
		mac = ((u8 *)ee)-0x1000;
	    }
	    if (mac && (!memcmp(mac,"\xff\xff\xff\xff\xff\xff",6) || !memcmp(mac,"\x00\x00\x00\x00\x00\x00",6))) {
		printk("Found empty mac address in dataset, leave the responsibility to the driver to use the correct one\n");
		mac = NULL;
	    }
	    
	}
	ap91_pci_init(ee, mac);
#endif
return ret;
}

arch_initcall(ar7240_platform_init);
    
