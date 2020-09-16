/*
 *  Atheros AP91 reference board PCI initialization
 *
 *  Copyright (C) 2009 Gabor Juhos <juhosg@openwrt.org>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#ifndef _AR71XX_DEV_AP91_PCI_H
#define _AR71XX_DEV_AP91_PCI_H

void ap91_pci_init(u8 *cal_data, u8 *mac_addr);
void ap91_pci_setup_wmac_led_pin(int pin);
void ap91_pci_setup_wmac_gpio(u32 mask, u32 val);
void ap91_set_tx_gain_buffalo(void);
void ap91_set_eeprom(void);
void __init ap91_wmac_disable_2ghz(void);
void __init ap91_wmac_disable_5ghz(void);

#endif /* _AR71XX_DEV_AP91_PCI_H */

