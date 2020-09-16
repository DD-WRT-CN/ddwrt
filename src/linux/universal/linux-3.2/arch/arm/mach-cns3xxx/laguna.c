/*
 * Gateworks Corporation Laguna Platform
 *
 * Copyright 2000 Deep Blue Solutions Ltd
 * Copyright 2008 ARM Limited
 * Copyright 2008 Cavium Networks
 *		  Scott Shu
 * Copyright 2010 MontaVista Software, LLC.
 *		  Anton Vorontsov <avorontsov@mvista.com>
 * Copyright 2011 Gateworks Corporation
 *		  Chris Lang <clang@gateworks.com>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, Version 2, as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/serial_core.h>
#include <linux/serial_8250.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/leds.h>
#include <linux/i2c.h>
#include <linux/i2c/at24.h>
#include <linux/i2c/pca953x.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/if_ether.h>
#include <linux/export.h>
#include <linux/module.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <mach/cns3xxx.h>
#include <mach/platform.h>
#include <mach/irqs.h>
#include <linux/squashfs_fs.h>
#ifdef CONFIG_CPU_FREQ
#include <linux/cpufreq.h>
extern struct cpufreq_driver cns_cpu_freq_driver;
#endif
#include <mach/pm.h>
#include <mach/gpio.h>
#include "core.h"
#include "devices.h"

unsigned int numcpucores=1;
EXPORT_SYMBOL(numcpucores);

// Config 1 Bitmap
#define ETH0_LOAD           BIT(0)
#define ETH1_LOAD           BIT(1)
#define ETH2_LOAD           BIT(2)
#define SATA0_LOAD          BIT(3)
#define SATA1_LOAD          BIT(4)
#define PCM_LOAD            BIT(5)
#define I2S_LOAD            BIT(6)
#define SPI0_LOAD           BIT(7)
#define SPI1_LOAD           BIT(8)
#define PCIE0_LOAD          BIT(9)
#define PCIE1_LOAD          BIT(10)
#define USB0_LOAD           BIT(11)
#define USB1_LOAD           BIT(12)
#define USB1_ROUTE          BIT(13)
#define SD_LOAD             BIT(14)
#define UART0_LOAD          BIT(15)
#define UART1_LOAD          BIT(16)
#define UART2_LOAD          BIT(17)
#define MPCI0_LOAD          BIT(18)
#define MPCI1_LOAD          BIT(19)
#define MPCI2_LOAD          BIT(20)
#define MPCI3_LOAD          BIT(21)
#define FP_BUT_LOAD         BIT(22)
#define FP_BUT_HEADER_LOAD  BIT(23)
#define FP_LED_LOAD         BIT(24)
#define FP_LED_HEADER_LOAD  BIT(25)
#define FP_TAMPER_LOAD      BIT(26)
#define HEADER_33V_LOAD     BIT(27)
#define SATA_POWER_LOAD     BIT(28)
#define FP_POWER_LOAD       BIT(29)
#define GPIO_HEADER_LOAD    BIT(30)
#define GSP_BAT_LOAD        BIT(31)

// Config 2 Bitmap
#define FAN_LOAD            BIT(0)
#define SPI_FLASH_LOAD      BIT(1)
#define NOR_FLASH_LOAD      BIT(2)
#define GPS_LOAD            BIT(3)
#define SUPPLY_5V_LOAD      BIT(6)
#define SUPPLY_33V_LOAD     BIT(7)

struct laguna_board_info {
	char model[16];
	u32 config_bitmap;
	u32 config2_bitmap;
	u8 nor_flash_size;
	u8 spi_flash_size;
};

struct laguna_board_info laguna_info __initdata;

EXPORT_SYMBOL(laguna_info);
/*
 * NOR Flash
 */
static struct mtd_partition laguna_norflash_partitions[] = {
	/* Bootloader */
	{
		.name = "bootloader",
		.offset = 0,
		.size = SZ_256K,
		.mask_flags = 0, /* force read-only */
	},
	/* Bootloader params */
	{
		.name = "params",
		.offset = SZ_256K,
		.size = SZ_128K,
		.mask_flags = 0,
	},
	/* linux */
	{
		.name = "linux",
		.offset = SZ_256K + SZ_128K,
		.size = SZ_2M,
		.mask_flags = 0,
	},
	/* Root FS */
	{
		.name = "rootfs",
		.offset = SZ_256K + SZ_128K + SZ_2M,
		.size = SZ_16M - SZ_256K - SZ_128K - SZ_2M,
		.mask_flags = 0,
	},
	/* ddwrt */
	{
		.name = "ddwrt",
		.offset = SZ_256K + SZ_128K + SZ_2M,
		.size = SZ_128K,
		.mask_flags = 0,
	},
	/* NVRAM */
	{
		.name = "nvram",
		.offset = SZ_256K + SZ_128K + SZ_2M,
		.size = SZ_128K,
		.mask_flags = 0,
	}
};

static struct physmap_flash_data laguna_nor_pdata = {
	.width = 2,
	.parts = laguna_norflash_partitions,
	.nr_parts = ARRAY_SIZE(laguna_norflash_partitions),
};

static struct resource laguna_norflash_resource = {
	.start = CNS3XXX_FLASH_BASE,
	.end = CNS3XXX_FLASH_BASE + SZ_128M - 1,
	.flags = IORESOURCE_MEM | IORESOURCE_MEM_32BIT,
};

static struct platform_device laguna_norflash_device = {
	.name = "physmap-flash",
	.id = 0,
	.resource = &laguna_norflash_resource,
	.num_resources = 1,
	.dev = {
		.platform_data = &laguna_nor_pdata,
	},
};

/*
 * SPI
 */
/* SPI Flash */
static struct mtd_partition laguna_spiflash_partitions[] = {
	/* Bootloader */
	{
		.name		= "bootloader",
		.offset		= 0,
		.size		= SZ_256K,
	},
	/* Bootloader params */
	{
		.name		= "params",
		.offset		= SZ_256K,
		.size		= SZ_256K,
	},
	/* linux */
	{
		.name = "linux",
		.offset = SZ_512K,
		.size = SZ_2M,
		.mask_flags = 0,
	},
	/* FileSystem */
	{
		.name		= "rootfs",
		.offset		= SZ_512K + SZ_2M ,
		.size		= SZ_16M - SZ_512K - SZ_2M,
	},
	/* ddwrt */
	{
		.name = "ddwrt",
		.offset = SZ_512K + SZ_2M,
		.size = SZ_128K,
		.mask_flags = 0,
	},
	/* NVRAM */
	{
		.name = "nvram",
		.offset = SZ_512K + SZ_2M,
		.size = SZ_128K,
		.mask_flags = 0,
	}
};

static struct flash_platform_data laguna_spi_pdata = {
	.parts = laguna_spiflash_partitions,
	.nr_parts = ARRAY_SIZE(laguna_spiflash_partitions),
};

static struct spi_board_info __initdata laguna_spi_devices[] = {
	{
		.modalias = "m25p80",
		.platform_data = &laguna_spi_pdata,
		.max_speed_hz = 50000000,
		.bus_num = 1,
		.chip_select = 0,
	},
};

static struct platform_device laguna_spi_controller_device = {
	.name = "cns3xxx_spi",
};

/*
 * LED's
 */
static struct gpio_led laguna_gpio_leds[] = {
	{
		.name = "user1", /* Green Led */
		.gpio = 115,
		.active_low = 1,
	},{
		.name = "user2", /* Red Led */
		.gpio = 114,
		.active_low = 1,
	},{
		.name = "pwr1", /* Green Led */
		.gpio = 116,
		.active_low = 1,
	},{
		.name = "pwr2", /* Yellow Led */
		.gpio = 117,
		.active_low = 1,
	},{
		.name = "txd1", /* Green Led */
		.gpio = 118,
		.active_low = 1,
	},{
		.name = "txd2", /* Yellow Led */
		.gpio = 119,
		.active_low = 1,
	},{
		.name = "rxd1", /* Green Led */
		.gpio = 120,
		.active_low = 1,
	},{
		.name = "rxd2", /* Yellow Led */
		.gpio = 121,
		.active_low = 1,
	},{
		.name = "ser1", /* Green Led */
		.gpio = 122,
		.active_low = 1,
	},{
		.name = "ser2", /* Yellow Led */
		.gpio = 123,
		.active_low = 1,
	},{
		.name = "enet1", /* Green Led */
		.gpio = 124,
		.active_low = 1,
	},{
		.name = "enet2", /* Yellow Led */
		.gpio = 125,
		.active_low = 1,
	},{
		.name = "sig1_1", /* Green Led */
		.gpio = 126,
		.active_low = 1,
	},{
		.name = "sig1_2", /* Yellow Led */
		.gpio = 127,
		.active_low = 1,
	},{
		.name = "sig2_1", /* Green Led */
		.gpio = 128,
		.active_low = 1,
	},{
		.name = "sig2_2", /* Yellow Led */
		.gpio = 129,
		.active_low = 1,
	},{
		.name = "sig3_1", /* Green Led */
		.gpio = 130,
		.active_low = 1,
	},{
		.name = "sig3_2", /* Yellow Led */
		.gpio = 131,
		.active_low = 1,
	},{
		.name = "net1", /*Green Led */
		.gpio = 109,
		.active_low = 1,
	},{
		.name = "net2", /* Red Led */
		.gpio = 110,
		.active_low = 1,
	},{
		.name = "mod1", /* Green Led */
		.gpio = 111,
		.active_low = 1,
	},{
		.name = "mod2", /* Red Led */
		.gpio = 112,
		.active_low = 1,
	},
};

static struct gpio_led_platform_data laguna_gpio_leds_data = {
	.num_leds = 22,
	.leds = laguna_gpio_leds,
};

static struct platform_device laguna_gpio_leds_device = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &laguna_gpio_leds_data,
};

/*
 * Ethernet
 */
static struct cns3xxx_plat_info laguna_net_data = {
	.ports = 0,
	.phy = {
		0,
		1,
		2,
	},
};

static struct platform_device laguna_net_device = {
	.name = "cns3xxx_eth",
	.id = 0,
	.dev.platform_data = &laguna_net_data,
};

/*
 * UART
 */
static void __init laguna_early_serial_setup(void)
{
#ifdef CONFIG_SERIAL_8250_CONSOLE
	static struct uart_port laguna_serial_port = {
		.membase        = (void __iomem *)CNS3XXX_UART0_BASE_VIRT,
		.mapbase        = CNS3XXX_UART0_BASE,
		.irq            = IRQ_CNS3XXX_UART0,
		.iotype         = UPIO_MEM,
		.flags          = UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE,
		.regshift       = 2,
		.uartclk        = 24000000,
		.line           = 0,
		.type           = PORT_16550A,
		.fifosize       = 16,
	};

	early_serial_setup(&laguna_serial_port);
#endif
}

static struct resource laguna_uart_resources[] = {
	{
		.start = CNS3XXX_UART0_BASE,
		.end   = CNS3XXX_UART0_BASE + SZ_4K - 1,
		.flags    = IORESOURCE_MEM
	},{
		.start = CNS3XXX_UART2_BASE,
		.end   = CNS3XXX_UART2_BASE + SZ_4K - 1,
		.flags    = IORESOURCE_MEM
	},{
		.start = CNS3XXX_UART2_BASE,
		.end   = CNS3XXX_UART2_BASE + SZ_4K - 1,
		.flags    = IORESOURCE_MEM
	},
};

static struct plat_serial8250_port laguna_uart_data[] = {
	{
		.membase        = (char*) (CNS3XXX_UART0_BASE_VIRT),
		.mapbase        = (CNS3XXX_UART0_BASE),
		.irq            = IRQ_CNS3XXX_UART0,
		.iotype         = UPIO_MEM,
		.flags          = UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE | UPF_NO_TXEN_TEST,
		.regshift       = 2,
		.uartclk        = 24000000,
		.type           = PORT_16550A,
	},{
		.membase        = (char*) (CNS3XXX_UART1_BASE_VIRT),
		.mapbase        = (CNS3XXX_UART1_BASE),
		.irq            = IRQ_CNS3XXX_UART1,
		.iotype         = UPIO_MEM,
		.flags          = UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE | UPF_NO_TXEN_TEST,
		.regshift       = 2,
		.uartclk        = 24000000,
		.type           = PORT_16550A,
	},{
		.membase        = (char*) (CNS3XXX_UART2_BASE_VIRT),
		.mapbase        = (CNS3XXX_UART2_BASE),
		.irq            = IRQ_CNS3XXX_UART2,
		.iotype         = UPIO_MEM,
		.flags          = UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE | UPF_NO_TXEN_TEST,
		.regshift       = 2,
		.uartclk        = 24000000,
		.type           = PORT_16550A,
	},
};

static struct platform_device laguna_uart = {
	.name     = "serial8250",
	.id     = PLAT8250_DEV_PLATFORM,
	.dev.platform_data  = laguna_uart_data,
	.num_resources    = 3,
	.resource   = laguna_uart_resources
};

/*
 * USB
 */
static struct resource cns3xxx_usb_ehci_resources[] = {
	[0] = {
		.start = CNS3XXX_USB_BASE,
		.end   = CNS3XXX_USB_BASE + SZ_16M - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_CNS3XXX_USB_EHCI,
		.flags = IORESOURCE_IRQ,
	},
};

static u64 cns3xxx_usb_ehci_dma_mask = DMA_BIT_MASK(32);

static struct platform_device cns3xxx_usb_ehci_device = {
	.name          = "cns3xxx-ehci",
	.num_resources = ARRAY_SIZE(cns3xxx_usb_ehci_resources),
	.resource      = cns3xxx_usb_ehci_resources,
	.dev           = {
		.dma_mask          = &cns3xxx_usb_ehci_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static struct resource cns3xxx_usb_ohci_resources[] = {
	[0] = {
		.start = CNS3XXX_USB_OHCI_BASE,
		.end   = CNS3XXX_USB_OHCI_BASE + SZ_16M - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_CNS3XXX_USB_OHCI,
		.flags = IORESOURCE_IRQ,
	},
};

static u64 cns3xxx_usb_ohci_dma_mask = DMA_BIT_MASK(32);

static struct platform_device cns3xxx_usb_ohci_device = {
	.name          = "cns3xxx-ohci",
	.num_resources = ARRAY_SIZE(cns3xxx_usb_ohci_resources),
	.resource      = cns3xxx_usb_ohci_resources,
	.dev           = {
		.dma_mask          = &cns3xxx_usb_ohci_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static struct resource cns3xxx_usb_otg_resources[] = {
	[0] = {
		.start = CNS3XXX_USBOTG_BASE,
		.end   = CNS3XXX_USBOTG_BASE + SZ_16M - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_CNS3XXX_USB_OTG,
		.flags = IORESOURCE_IRQ,
	},
};

static u64 cns3xxx_usb_otg_dma_mask = DMA_BIT_MASK(32);

static struct platform_device cns3xxx_usb_otg_device = {
	.name          = "dwc_otg",
	.num_resources = ARRAY_SIZE(cns3xxx_usb_otg_resources),
	.resource      = cns3xxx_usb_otg_resources,
	.dev           = {
		.dma_mask          = &cns3xxx_usb_otg_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

/*
 * I2C
 */
static struct resource laguna_i2c_resource[] = {
	{
		.start    = CNS3XXX_SSP_BASE + 0x20,
		.end      = 0x7100003f,
		.flags    = IORESOURCE_MEM,
	},{
		.start    = IRQ_CNS3XXX_I2C,
		.flags    = IORESOURCE_IRQ,
	},
};

static struct platform_device laguna_i2c_controller = {
	.name   = "cns3xxx-i2c",
	.num_resources  = 2,
	.resource = laguna_i2c_resource,
};

static struct memory_accessor *at24_mem_acc;

static void at24_setup(struct memory_accessor *mem_acc, void *context)
{
	char buf[16];

	at24_mem_acc = mem_acc;

	/* Read MAC addresses */
	if (at24_mem_acc->read(at24_mem_acc, buf, 0x100, 6) == 6)
		memcpy(&laguna_net_data.hwaddr[0], buf, ETH_ALEN);
	if (at24_mem_acc->read(at24_mem_acc, buf, 0x106, 6) == 6)
		memcpy(&laguna_net_data.hwaddr[1], buf, ETH_ALEN);
	if (at24_mem_acc->read(at24_mem_acc, buf, 0x10C, 6) == 6)
		memcpy(&laguna_net_data.hwaddr[2], buf, ETH_ALEN);
	if (at24_mem_acc->read(at24_mem_acc, buf, 0x112, 6) == 6)
		memcpy(&laguna_net_data.hwaddr[3], buf, ETH_ALEN);

	/* Read out Model Information */
	if (at24_mem_acc->read(at24_mem_acc, buf, 0x130, 16) == 16)
		memcpy(&laguna_info.model, buf, 16);
	if (at24_mem_acc->read(at24_mem_acc, buf, 0x140, 1) == 1)
		memcpy(&laguna_info.nor_flash_size, buf, 1);
	if (at24_mem_acc->read(at24_mem_acc, buf, 0x141, 1) == 1)
		memcpy(&laguna_info.spi_flash_size, buf, 1);
	if (at24_mem_acc->read(at24_mem_acc, buf, 0x142, 4) == 4)
		memcpy(&laguna_info.config_bitmap, buf, 4);
	if (at24_mem_acc->read(at24_mem_acc, buf, 0x146, 4) == 4)
		memcpy(&laguna_info.config2_bitmap, buf, 4);
};

static struct at24_platform_data laguna_eeprom_info = {
	.byte_len = 1024,
	.page_size = 16,
	.flags = AT24_FLAG_READONLY,
	.setup = at24_setup,
};

static struct pca953x_platform_data laguna_pca_data = {
 	.gpio_base = 100,
};

static struct pca953x_platform_data laguna_pca2_data = {
 	.gpio_base = 116,
	.irq_base = -1,
};

static struct i2c_board_info __initdata laguna_i2c_devices[] = {
	{
		I2C_BOARD_INFO("pca9555", 0x23),
		.platform_data = &laguna_pca_data,
	},{
		I2C_BOARD_INFO("pca9555", 0x27),
		.platform_data = &laguna_pca2_data,
	},{
		I2C_BOARD_INFO("gsp", 0x29),
	},{
		I2C_BOARD_INFO ("24c08",0x50),
		.platform_data = &laguna_eeprom_info,
	},{
		I2C_BOARD_INFO("ds1672", 0x68),
	},
};

/*
 * Watchdog
 */

static struct resource laguna_watchdog_resource[] = {
	{
		.start = CNS3XXX_TC11MP_TWD_BASE,
		.end = CNS3XXX_TC11MP_TWD_BASE + 0x100 - 1,
		.flags = IORESOURCE_MEM,
	},{
		.start = IRQ_LOCALWDOG,
		.end = IRQ_LOCALWDOG,
		.flags = IORESOURCE_IRQ,
	}
};

static struct platform_device laguna_watchdog = {
	.name = "mpcore_wdt",
	.id = -1,
	.num_resources = ARRAY_SIZE(laguna_watchdog_resource),
	.resource = laguna_watchdog_resource,
};

static struct platform_device laguna_gpio_dev = {
	.name = "GPIODEV",
	.id = -1,
};

/*
 * Initialization
 */

static void __init laguna_init(void)
{
	platform_device_register(&laguna_watchdog);

	platform_device_register(&laguna_i2c_controller);

	i2c_register_board_info(0, laguna_i2c_devices,
			ARRAY_SIZE(laguna_i2c_devices));


	pm_power_off = cns3xxx_power_off;
}

static struct map_desc laguna_io_desc[] __initdata = {
	{
		.virtual	= CNS3XXX_UART0_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_UART0_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},{
		.virtual	= CNS3XXX_UART1_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_UART1_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},{
		.virtual	= CNS3XXX_UART2_BASE_VIRT,
		.pfn		= __phys_to_pfn(CNS3XXX_UART2_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},
};

static void __init laguna_map_io(void)
{
	cns3xxx_common_init();
	iotable_init(laguna_io_desc, ARRAY_SIZE(laguna_io_desc));

	laguna_early_serial_setup();
}

 
#define SPI_MEM_MAP_VALUE(reg_offset)		(*((u32 volatile *)(CNS3XXX_SSP_BASE_VIRT + reg_offset)))

#define SPI_CONFIGURATION_REG			SPI_MEM_MAP_VALUE(0x40)
#define SPI_SERVICE_STATUS_REG			SPI_MEM_MAP_VALUE(0x44)
#define SPI_BIT_RATE_CONTROL_REG		SPI_MEM_MAP_VALUE(0x48)
#define SPI_TRANSMIT_CONTROL_REG		SPI_MEM_MAP_VALUE(0x4C)
#define SPI_TRANSMIT_BUFFER_REG			SPI_MEM_MAP_VALUE(0x50)
#define SPI_RECEIVE_CONTROL_REG			SPI_MEM_MAP_VALUE(0x54)
#define SPI_RECEIVE_BUFFER_REG			SPI_MEM_MAP_VALUE(0x58)
#define SPI_FIFO_TRANSMIT_CONFIG_REG		SPI_MEM_MAP_VALUE(0x5C)
#define SPI_FIFO_TRANSMIT_CONTROL_REG		SPI_MEM_MAP_VALUE(0x60)
#define SPI_FIFO_RECEIVE_CONFIG_REG		SPI_MEM_MAP_VALUE(0x64)
#define SPI_INTERRUPT_STATUS_REG		SPI_MEM_MAP_VALUE(0x68)
#define SPI_INTERRUPT_ENABLE_REG		SPI_MEM_MAP_VALUE(0x6C)

#define SPI_TRANSMIT_BUFFER_REG_ADDR		(CNS3XXX_SSP_BASE +0x50)
#define SPI_RECEIVE_BUFFER_REG_ADDR		(CNS3XXX_SSP_BASE +0x58)

static int __init laguna_model_setup(void)
{
	u32 __iomem *mem;
	u32 reg;

	printk("Running on Gateworks Laguna %s\n", laguna_info.model);

	if (strncmp(laguna_info.model, "GW", 2) == 0) {
		printk("CONFIG BITMAP = 0x%08X\n",laguna_info.config_bitmap);
		if (laguna_info.config_bitmap & ETH0_LOAD)
			laguna_net_data.ports |= BIT(0);
		if (strncmp(laguna_info.model, "GW2388", 6) == 0)
		{
		if (laguna_info.config_bitmap & ETH1_LOAD)
			laguna_net_data.ports |= BIT(1);
		if (laguna_info.config_bitmap & ETH2_LOAD)
			laguna_net_data.ports |= BIT(2);
		}
		if (laguna_net_data.ports)
			platform_device_register(&laguna_net_device);

		if (laguna_info.config_bitmap & (SATA0_LOAD | SATA1_LOAD))
			cns3xxx_ahci_init();


		gpio_line_set(3, 1);
		gpio_line_config(3, CNS3XXX_GPIO_OUT);
		gpio_set_value(3, 1);

		if (strncmp(laguna_info.model, "GW2380", 6) == 0)
		{
		if (laguna_info.config_bitmap & (USB0_LOAD)) {
			cns3xxx_pwr_power_up(1 << PM_PLL_HM_PD_CTRL_REG_OFFSET_PLL_USB);

			/* DRVVBUS pins share with GPIOA */
			mem = (void __iomem *)(CNS3XXX_MISC_BASE_VIRT + 0x0010);
			reg = __raw_readl(mem);
			reg |= 0x8;
			__raw_writel(reg, mem);

			/* Enable OTG */
			mem = (void __iomem *)(CNS3XXX_MISC_BASE_VIRT + 0x0808);
			reg = __raw_readl(mem);
			reg &= ~(1 << 10);
			__raw_writel(reg, mem);

			platform_device_register(&cns3xxx_usb_otg_device);
		}

		if (laguna_info.config_bitmap & (USB1_LOAD)) {
			platform_device_register(&cns3xxx_usb_ehci_device);
			platform_device_register(&cns3xxx_usb_ohci_device);
		}
		}else{
		if (laguna_info.config_bitmap & (USB0_LOAD)) {

			printk(KERN_EMERG "power up USB0\n");
			cns3xxx_pwr_power_up(1 << PM_PLL_HM_PD_CTRL_REG_OFFSET_PLL_USB);

			/* DRVVBUS pins share with GPIOA */
			mem = (void __iomem *)(CNS3XXX_MISC_BASE_VIRT + 0x0010);
			reg = __raw_readl(mem);
			reg |= 0x8;
			__raw_writel(reg, mem);

			/* Enable OTG */
			mem = (void __iomem *)(CNS3XXX_MISC_BASE_VIRT + 0x0808);
			reg = __raw_readl(mem);
			reg &= ~(1 << 10);
			__raw_writel(reg, mem);

			platform_device_register(&cns3xxx_usb_otg_device);
		}

		if (laguna_info.config_bitmap & (USB1_LOAD)) {
			printk(KERN_EMERG "power up ehci/ohci\n");
			platform_device_register(&cns3xxx_usb_ehci_device);
			platform_device_register(&cns3xxx_usb_ohci_device);
		}
		}


		if (laguna_info.config_bitmap & (SD_LOAD))
		{
			printk(KERN_EMERG "init sdhci\n");
			cns3xxx_sdhci_init();
		}
		if (laguna_info.config_bitmap & (UART0_LOAD))
			laguna_uart.num_resources = 1;
		if (laguna_info.config_bitmap & (UART1_LOAD))
			laguna_uart.num_resources = 2;
		if (laguna_info.config_bitmap & (UART2_LOAD))
			laguna_uart.num_resources = 3;
		platform_device_register(&laguna_uart);

		printk(KERN_EMERG "notflash size %d\n",laguna_info.nor_flash_size);
		if ((laguna_info.config2_bitmap & (NOR_FLASH_LOAD)) && (strncmp(laguna_info.model, "GW2388", 6) == 0 || strncmp(laguna_info.model, "GW2387", 6) == 0)) {
			printk(KERN_EMERG "detecting NOR FLASH\n");
//			if (laguna_info.nor_flash_size < 1 || laguna_info.nor_flash_size > 5)
//			    laguna_info.nor_flash_size = 2; //guess default for wrong config 
			laguna_norflash_partitions[2].size = SZ_16M - SZ_256K - SZ_128K  - SZ_128K;
			laguna_norflash_resource.end = CNS3XXX_FLASH_BASE + SZ_16M - 1;
			switch (laguna_info.nor_flash_size) {
				case 1:
					laguna_norflash_partitions[2].size = SZ_8M - SZ_256K - SZ_128K  - SZ_128K;
					laguna_norflash_resource.end = CNS3XXX_FLASH_BASE + SZ_8M - 1;
				break;
				case 2:
					laguna_norflash_partitions[2].size = SZ_16M - SZ_256K - SZ_128K  - SZ_128K;
					laguna_norflash_resource.end = CNS3XXX_FLASH_BASE + SZ_16M - 1;
				break;
				case 3:
					laguna_norflash_partitions[2].size = SZ_32M - SZ_256K - SZ_128K  - SZ_128K;
					laguna_norflash_resource.end = CNS3XXX_FLASH_BASE + SZ_32M - 1;
				break;
				case 4:
					laguna_norflash_partitions[2].size = SZ_64M - SZ_256K - SZ_128K  - SZ_128K;
					laguna_norflash_resource.end = CNS3XXX_FLASH_BASE + SZ_64M - 1;
				break;
				case 5:
					laguna_norflash_partitions[2].size = SZ_128M - SZ_256K - SZ_128K  - SZ_128K;
					laguna_norflash_resource.end = CNS3XXX_FLASH_BASE + SZ_128M - 1;
				break;
			}
			unsigned int flashsize = laguna_norflash_partitions[2].size + SZ_256K + SZ_128K + SZ_128K;
			unsigned char *buf = (unsigned char *)ioremap(laguna_norflash_resource.start,laguna_norflash_resource.end - laguna_norflash_resource.start + 1);
			unsigned int offset=0;
			int tmplen;
			int filesyssize=0;
			int erasesize=0x20000;
			while(offset<laguna_norflash_resource.end)
			{
			if (*((__u32 *) buf) == SQUASHFS_MAGIC) 
			{
		//	printk(KERN_EMERG "found squashfs\n");
	    		struct squashfs_super_block *sb = (struct squashfs_super_block *) buf;
			filesyssize = le64_to_cpu(sb->bytes_used);
			tmplen = offset + filesyssize;
			tmplen +=  (erasesize - 1);
			tmplen &= ~(erasesize - 1);
			filesyssize = tmplen - offset;
			laguna_norflash_partitions[3].offset = offset;
			laguna_norflash_partitions[3].size = filesyssize;
			laguna_norflash_partitions[4].offset = offset + filesyssize;
			laguna_norflash_partitions[4].size = (flashsize- SZ_128K) - laguna_norflash_partitions[4].offset;
			laguna_norflash_partitions[5].offset = (flashsize - SZ_128K);
			laguna_norflash_partitions[5].size = SZ_128K;
			break;
			}
			buf+=0x1000;
			offset+=0x1000;
			}

			platform_device_register(&laguna_norflash_device);
		}

		if ((laguna_info.config2_bitmap & (SPI_FLASH_LOAD)) && strncmp(laguna_info.model, "GW2380", 6) == 0) {
		//	printk(KERN_EMERG "detecting SPI FLASH\n");
			SPI_CONFIGURATION_REG = 0x40000000;
			HAL_MISC_ENABLE_SPI_SERIAL_FLASH_BANK_ACCESS();
			laguna_spiflash_partitions[2].size		= SZ_16M - SZ_512K;
#if 0
			switch (laguna_info.spi_flash_size) {
				case 1:
					laguna_spiflash_partitions[2].size		= SZ_4M - SZ_512K;
				break;
				case 2:
					laguna_spiflash_partitions[2].size		= SZ_8M - SZ_512K;
				break;
				case 3:
					laguna_spiflash_partitions[2].size		= SZ_16M - SZ_512K;
				break;
				case 4:
					laguna_spiflash_partitions[2].size		= SZ_32M - SZ_512K;
				break;
				case 5:
					laguna_spiflash_partitions[2].size		= SZ_64M - SZ_512K;
				break;
			}
#endif
			unsigned int flashsize = laguna_spiflash_partitions[2].size + SZ_512K;
			unsigned char *buf = (unsigned char *)ioremap(CNS3XXX_SPI_FLASH_BASE,SZ_16M-1);
		//	unsigned char *buf = (unsigned char *)CNS3XXX_SPI_FLASH_BASE;
			unsigned int offset=0;
			int tmplen;
			int filesyssize=0;
			int erasesize=0x40000;
			while(offset<SZ_8M)
			{
			if (*((__u32 *) buf) == SQUASHFS_MAGIC) 
			{
			struct squashfs_super_block *sb;
			char *block = vmalloc(0x40000);
			memcpy(block,buf,0x40000);
			sb = (struct squashfs_super_block*)block;
			printk(KERN_EMERG "found squashfs @0x%08X magic=0x%08X\n",offset,*((__u32 *) buf));
			filesyssize = le64_to_cpu(sb->bytes_used);
			vfree(block);
			tmplen = offset + filesyssize;
			tmplen +=  (erasesize - 1);
			tmplen &= ~(erasesize - 1);
			filesyssize = tmplen - offset;
			laguna_spiflash_partitions[3].offset = offset;
			laguna_spiflash_partitions[3].size = filesyssize;
			laguna_spiflash_partitions[4].offset = offset + filesyssize;
			laguna_spiflash_partitions[4].size = (flashsize- SZ_256K) - laguna_spiflash_partitions[4].offset;
			laguna_spiflash_partitions[5].offset = (flashsize - SZ_256K);
			laguna_spiflash_partitions[5].size = SZ_256K;
			break;
			}
			buf+=0x1000;
			offset+=0x1000;
			}
			HAL_MISC_DISABLE_SPI_SERIAL_FLASH_BANK_ACCESS();

		if (strncmp(laguna_info.model, "GW2380", 6) == 0)
			spi_register_board_info(laguna_spi_devices, ARRAY_SIZE(laguna_spi_devices));

		}

		if (laguna_info.config_bitmap & (SPI0_LOAD | SPI1_LOAD))
		{
			platform_device_register(&laguna_spi_controller_device);
		}

		/*
		 *	Do any model specific setup not known by the bitmap by matching
		 *  the first 6 characters of the model name
		 */

		if (strncmp(laguna_info.model, "GW2388", 6) == 0)
		{
			laguna_gpio_leds_data.num_leds = 2;
		}
		else if (strncmp(laguna_info.model, "GW2380", 6) == 0) {
			laguna_gpio_leds[0].gpio = 107;
			laguna_gpio_leds[1].gpio = 106;
			laguna_gpio_leds_data.num_leds = 2;
		}
			platform_device_register(&laguna_gpio_leds_device);
			platform_device_register(&laguna_gpio_dev);
	} else {
		// Do some defaults here, not sure what yet
	}
	return 0;
}

late_initcall(laguna_model_setup);

MACHINE_START(GW2388, "Gateworks Corporation Laguna Platform")
	.atag_offset	= 0x100,
	.map_io		= laguna_map_io,
	.init_irq	= cns3xxx_init_irq,
	.timer		= &cns3xxx_timer,
	.init_machine	= laguna_init,
MACHINE_END
