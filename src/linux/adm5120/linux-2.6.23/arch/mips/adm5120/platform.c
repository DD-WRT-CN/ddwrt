/*
 *  $Id: platform.c 9340 2007-10-17 08:10:47Z juhosg $
 *
 *  Generic ADM5120 platform devices
 *
 *  Copyright (C) 2007 OpenWrt.org
 *  Copyright (C) 2007 Gabor Juhos <juhosg at openwrt.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <asm/bootinfo.h>
#include <asm/gpio.h>

#include <adm5120_defs.h>
#include <adm5120_info.h>
#include <adm5120_irq.h>
#include <adm5120_switch.h>
#include <adm5120_nand.h>
#include <adm5120_platform.h>

#if 1
/*
 * TODO:remove global adm5120_eth* variables when the switch driver will be
 *	converted into a real platform driver
 */
unsigned int adm5120_eth_num_ports = 6;
EXPORT_SYMBOL_GPL(adm5120_eth_num_ports);

unsigned char adm5120_eth_macs[6][6] = {
	{'\00', 'A', 'D', 'M', '\x51', '\x20' },
	{'\00', 'A', 'D', 'M', '\x51', '\x21' },
	{'\00', 'A', 'D', 'M', '\x51', '\x22' },
	{'\00', 'A', 'D', 'M', '\x51', '\x23' },
	{'\00', 'A', 'D', 'M', '\x51', '\x24' },
	{'\00', 'A', 'D', 'M', '\x51', '\x25' }
};
EXPORT_SYMBOL_GPL(adm5120_eth_macs);

unsigned char adm5120_eth_vlans[6] = {
	0x41, 0x42, 0x44, 0x48, 0x50, 0x60
};
EXPORT_SYMBOL_GPL(adm5120_eth_vlans);
#endif

/* Built-in ethernet switch */
struct resource adm5120_switch_resources[] = {
	[0] = {
		.start	= ADM5120_SWITCH_BASE,
		.end	= ADM5120_SWITCH_BASE+ADM5120_SWITCH_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= ADM5120_IRQ_SWITCH,
		.end	= ADM5120_IRQ_SWITCH,
		.flags	= IORESOURCE_IRQ,
	},
};

struct adm5120_switch_platform_data adm5120_switch_data;
struct platform_device adm5120_switch_device = {
	.name	= "adm5120-switch",
	.id	= -1,
	.num_resources	= ARRAY_SIZE(adm5120_switch_resources),
	.resource	= adm5120_switch_resources,
	.dev.platform_data = &adm5120_switch_data,
};

/* USB Host Controller */
struct resource adm5120_hcd_resources[] = {
	[0] = {
		.start	= ADM5120_USBC_BASE,
		.end	= ADM5120_USBC_BASE+ADM5120_USBC_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= ADM5120_IRQ_USBC,
		.end	= ADM5120_IRQ_USBC,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 adm5120_hcd_dma_mask = ~(u32)0;

struct platform_device adm5120_hcd_device = {
	.name		= "adm5120-hcd",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(adm5120_hcd_resources),
	.resource	= adm5120_hcd_resources,
	.dev = {
		.dma_mask	= &adm5120_hcd_dma_mask,
		.coherent_dma_mask = 0xFFFFFFFF,
	}
};

/* NOR flash 0 */
struct adm5120_flash_platform_data adm5120_flash0_data;
struct platform_device adm5120_flash0_device =	{
	.name	= "adm5120-flash",
	.id	= 0,
	.dev.platform_data = &adm5120_flash0_data,
};

/* NOR flash 1 */
struct adm5120_flash_platform_data adm5120_flash1_data;
struct platform_device adm5120_flash1_device =	{
	.name	= "adm5120-flash",
	.id	= 1,
	.dev.platform_data = &adm5120_flash1_data,
};

/* NAND flash */
struct resource adm5120_nand_resource[] = {
	[0] = {
		.start	= ADM5120_NAND_BASE,
		.end	= ADM5120_NAND_BASE + ADM5120_NAND_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_nand_data adm5120_nand_data = {
	.ctrl.dev_ready	= adm5120_nand_ready,
	.ctrl.cmd_ctrl	= adm5120_nand_cmd_ctrl,
};

struct platform_device adm5120_nand_device = {
	.name 		= "gen_nand",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(adm5120_nand_resource),
	.resource	= adm5120_nand_resource,
	.dev.platform_data = &adm5120_nand_data,
};

/* built-in UARTs */
struct amba_pl010_data adm5120_uart0_data = {
	.set_mctrl = adm5120_uart_set_mctrl
};

struct amba_device adm5120_uart0_device = {
	.dev		= {
		.bus_id	= "APB:UART0",
		.platform_data = &adm5120_uart0_data,
	},
	.res		= {
		.start	= ADM5120_UART0_BASE,
		.end	= ADM5120_UART0_BASE + ADM5120_UART_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	.irq		= { ADM5120_IRQ_UART0, -1 },
	.periphid	= 0x0041010,
};

struct amba_pl010_data adm5120_uart1_data = {
	.set_mctrl = adm5120_uart_set_mctrl
};

struct amba_device adm5120_uart1_device = {
	.dev		= {
		.bus_id	= "APB:UART1",
		.platform_data = &adm5120_uart1_data,
	},
	.res		= {
		.start	= ADM5120_UART1_BASE,
		.end	= ADM5120_UART1_BASE + ADM5120_UART_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	.irq		= { ADM5120_IRQ_UART1, -1 },
	.periphid	= 0x0041010,
};

void adm5120_uart_set_mctrl(struct amba_device *dev, void __iomem *base,
		unsigned int mctrl)
{
}

int adm5120_nand_ready(struct mtd_info *mtd)
{
	return ((adm5120_nand_get_status() & ADM5120_NAND_STATUS_READY) != 0);
}

void adm5120_nand_cmd_ctrl(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	if (ctrl & NAND_CTRL_CHANGE) {
		adm5120_nand_set_cle(ctrl & NAND_CLE);
		adm5120_nand_set_ale(ctrl & NAND_ALE);
		adm5120_nand_set_cen(ctrl & NAND_NCE);
	}

	if (cmd != NAND_CMD_NONE)
		NAND_WRITE_REG(NAND_REG_DATA, cmd);
}

