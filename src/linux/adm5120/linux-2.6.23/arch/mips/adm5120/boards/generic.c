/*
 *  $Id: generic.c 9423 2007-10-24 08:19:16Z juhosg $
 *
 *  Generic ADM5120 based board
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

#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/bootinfo.h>
#include <asm/gpio.h>

#include <adm5120_board.h>
#include <adm5120_platform.h>
#include <adm5120_irq.h>

static struct platform_device *generic_devices[] __initdata = {
	&adm5120_flash0_device,
	&adm5120_hcd_device,
};

static struct platform_device *wp54_devices[] __initdata = {
	&adm5120_flash0_device,
};


static struct adm5120_pci_irq generic_pci_irqs[] __initdata = {
	PCIIRQ(2, 0, 1, ADM5120_IRQ_PCI0)
};


static struct adm5120_pci_irq np28g_pci_irqs[] __initdata = {
	PCIIRQ(2, 0, 1, ADM5120_IRQ_PCI0),
	PCIIRQ(3, 0, 1, ADM5120_IRQ_PCI0),
	PCIIRQ(3, 1, 2, ADM5120_IRQ_PCI1),
	PCIIRQ(3, 2, 3, ADM5120_IRQ_PCI2)
};

static u8 wp54_vlans[6] __initdata = {
	0x41, 0x42, 0x00, 0x00, 0x00, 0x00
};
unsigned char np28g_vlans[6] __initdata = {
	0x50, 0x42, 0x44, 0x48, 0x00, 0x00
};


static void switch_bank_gpio5(unsigned bank)
{
	switch (bank) {
	case 0:
		gpio_set_value(ADM5120_GPIO_PIN5, 0);
		break;
	case 1:
		gpio_set_value(ADM5120_GPIO_PIN5, 1);
		break;
	}
}

static void wp54_reset(void)
{
	gpio_set_value(ADM5120_GPIO_PIN3, 0);
}

static void np28g_reset(void)
{
	gpio_set_value(ADM5120_GPIO_PIN4, 0);
}

static void __init wp54_setup(void)
{

	gpio_request(ADM5120_GPIO_PIN5, NULL); /* for flash A20 line */
	gpio_direction_output(ADM5120_GPIO_PIN5, 0);

	gpio_request(ADM5120_GPIO_PIN3, NULL); /* for system reset */
	gpio_direction_output(ADM5120_GPIO_PIN3, 1);

	adm5120_flash0_data.switch_bank = switch_bank_gpio5;
}

static void __init np2xg_setup(void)
{
	gpio_request(ADM5120_GPIO_PIN5, NULL); /* for flash A20 line */
	gpio_direction_output(ADM5120_GPIO_PIN5, 0);

	gpio_request(ADM5120_GPIO_PIN4, NULL);
	gpio_direction_output(ADM5120_GPIO_PIN4, 1);

	/* setup data for flash0 device */
	adm5120_flash0_data.switch_bank = switch_bank_gpio5;

	/* TODO: setup mac address */
}


/*--------------------------------------------------------------------------*/
#ifdef CONFIG_ADM5120_COMPEX_WP54G

ADM5120_BOARD_START(GENERIC, "ADM5120")
	.board_setup	= wp54_setup,
	.eth_num_ports	= 2,
	.eth_vlans	= wp54_vlans,
	.num_devices	= ARRAY_SIZE(wp54_devices),
	.devices	= wp54_devices,
	.pci_nr_irqs	= ARRAY_SIZE(generic_pci_irqs),
	.pci_irq_map	= generic_pci_irqs,
	.board_reset 	= wp54_reset,
	
ADM5120_BOARD_END

#elif CONFIG_ADM5120_COMPEX_NP28G

ADM5120_BOARD_START(GENERIC, "ADM5120")
	.board_setup	= np2xg_setup,
	.eth_num_ports	= 4,
	.eth_vlans	= np28g_vlans,
	.num_devices	= ARRAY_SIZE(wp54_devices),
	.devices	= wp54_devices,
	.pci_nr_irqs	= ARRAY_SIZE(np28g_pci_irqs),
	.pci_irq_map	= np28g_pci_irqs,
	.board_reset 	= np28g_reset,
	
ADM5120_BOARD_END


#else
ADM5120_BOARD_START(GENERIC, "ADM5120")
	.eth_num_ports	= 1,
	.num_devices	= ARRAY_SIZE(generic_devices),
	.devices	= generic_devices,
	.pci_nr_irqs	= ARRAY_SIZE(generic_pci_irqs),
	.pci_irq_map	= generic_pci_irqs,
	
ADM5120_BOARD_END
#endif