/*
 *  $Id: board.c 9423 2007-10-24 08:19:16Z juhosg $
 *
 *  ADM5120 generic board code
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

#include <adm5120_info.h>
#include <adm5120_defs.h>
#include <adm5120_irq.h>
#include <adm5120_board.h>
#include <adm5120_platform.h>

#define PFX	"adm5120: "

static LIST_HEAD(adm5120_boards);
static char adm5120_board_name[ADM5120_BOARD_NAMELEN];

const char *get_system_type(void)
{
	return adm5120_board_name;
}

static struct adm5120_board * __init adm5120_board_find(unsigned long machtype)
{
	struct list_head *this;
	struct adm5120_board *board;
	void *ret;

	ret = NULL;
	list_for_each(this, &adm5120_boards) {
		board = list_entry(this, struct adm5120_board, list);
		if (board->mach_type == machtype) {
			ret = board;
			break;
		}
	}

	return ret;
}

static int __init adm5120_board_setup(void)
{
	struct adm5120_board *board;
	int err;

	board = adm5120_board_find(mips_machtype);
	if (board == NULL) {
		printk(KERN_ALERT PFX"no board registered for "
			"machtype %lu, trying generic\n", mips_machtype);
		board = adm5120_board_find(MACH_ADM5120_GENERIC);
		if (board == NULL)
			panic(PFX "unsupported board\n");
	}

	printk(KERN_INFO PFX "setting up board '%s'\n", board->name);

	memcpy(&adm5120_board_name, board->name, ADM5120_BOARD_NAMELEN);

	adm5120_board_reset = board->board_reset;
	if (board->eth_num_ports > 0)
		adm5120_eth_num_ports = board->eth_num_ports;

	if (board->eth_vlans)
		memcpy(adm5120_eth_vlans, board->eth_vlans,
			sizeof(adm5120_eth_vlans));

	if (board->board_setup)
		board->board_setup();

	/* register UARTs */
	amba_device_register(&adm5120_uart0_device, &iomem_resource);
	amba_device_register(&adm5120_uart1_device, &iomem_resource);

	/* register built-in ethernet switch */
	platform_device_register(&adm5120_switch_device);

	/* setup PCI irq map */
	adm5120_pci_set_irq_map(board->pci_nr_irqs, board->pci_irq_map);

	/* register board devices */
	if (board->num_devices > 0 && board->devices != NULL) {
		err = platform_add_devices(board->devices, board->num_devices);
		if (err)
			printk(KERN_ALERT PFX "adding board devices failed\n");
	}

	return 0;
}
arch_initcall(adm5120_board_setup);

void __init adm5120_board_register(struct adm5120_board *board)
{
	list_add_tail(&board->list, &adm5120_boards);
	printk(KERN_INFO PFX "registered board '%s'\n", board->name);
}
