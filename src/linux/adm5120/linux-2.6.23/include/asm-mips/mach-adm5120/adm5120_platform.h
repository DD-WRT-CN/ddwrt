/*
 *  $Id: adm5120_platform.h 9418 2007-10-23 12:31:13Z juhosg $
 *
 *  ADM5120 specific platform definitions
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
 */

#ifndef _ADM5120_PLATFORM_H_
#define _ADM5120_PLATFORM_H_

#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>

#include <linux/amba/bus.h>
#include <linux/amba/serial.h>

struct adm5120_flash_platform_data {
	void			(*set_vpp)(struct map_info *, int);
	void			(*switch_bank)(unsigned);
	u32			window_size;
#ifdef CONFIG_MTD_PARTITIONS
	unsigned int		nr_parts;
	struct mtd_partition	*parts;
#endif
};

struct adm5120_switch_platform_data {
	/* TODO: not yet implemented */
};

struct adm5120_pci_irq {
	u8	slot;
	u8	func;
	u8	pin;
	unsigned irq;
};

#define PCIIRQ(s,f,p,i) {.slot = (s), .func = (f), .pin  = (p), .irq  = (i)}

#ifdef CONFIG_PCI
extern void adm5120_pci_set_irq_map(unsigned int nr_irqs,
		struct adm5120_pci_irq *map) __init;
#else
static inline void adm5120_pci_set_irq_map(unsigned int nr_irqs,
		struct adm5120_pci_irq *map)
{
}
#endif

extern struct adm5120_flash_platform_data adm5120_flash0_data;
extern struct adm5120_flash_platform_data adm5120_flash1_data;
extern struct platform_nand_data adm5120_nand_data;
extern struct adm5120_switch_platform_data adm5120_switch_data;
extern struct amba_pl010_data adm5120_uart0_data;
extern struct amba_pl010_data adm5120_uart1_data;

extern struct platform_device adm5120_flash0_device;
extern struct platform_device adm5120_flash1_device;
extern struct platform_device adm5120_nand_device;
extern struct platform_device adm5120_hcd_device;
extern struct platform_device adm5120_switch_device;
extern struct amba_device adm5120_uart0_device;
extern struct amba_device adm5120_uart1_device;

extern void adm5120_uart_set_mctrl(struct amba_device *dev, void __iomem *base,
		unsigned int mctrl);

extern void adm5120_nand_cmd_ctrl(struct mtd_info *mtd, int cmd,
		unsigned int ctrl);
extern int adm5120_nand_ready(struct mtd_info *mtd);

#endif /* _ADM5120_PLATFORM_H_ */
