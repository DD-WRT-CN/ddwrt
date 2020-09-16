/*
 * Broadcom BCM5325E/536x switch configuration module
 *
 * Copyright (C) 2005 Felix Fietkau <nbd@nbd.name>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
 * 02110-1301, USA.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/sockios.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

#include "switch-core.h"

#define DRIVER_NAME "adm6996"
#define sbh bcm947xx_sbh

static int eecs = 2;
static int eesk = 3;
static int eedi = 5;
static int eerc = 6;

MODULE_AUTHOR("Felix Fietkau <openwrt@nbd.name>");
MODULE_LICENSE("GPL");
MODULE_PARM(eecs, "i");
MODULE_PARM(eesk, "i");
MODULE_PARM(eedi, "i");
MODULE_PARM(eerc, "i");
EXPORT_NO_SYMBOLS;

extern void *bcm947xx_sbh;
extern __u32 sb_gpioin(void *sbh);
extern __u32 sb_gpiointpolarity(void *sbh, __u32 mask, __u32 val);
extern __u32 sb_gpiointmask(void *sbh, __u32 mask, __u32 val);
extern __u32 sb_gpioouten(void *sbh, __u32 mask, __u32 val);
extern __u32 sb_gpioout(void *sbh, __u32 mask, __u32 val);

/* Minimum timing constants */
#define EECK_EDGE_TIME  3   /* 3us - max(adm 2.5us, 93c 1us) */
#define EEDI_SETUP_TIME 1   /* 1us - max(adm 10ns, 93c 400ns) */
#define EECS_SETUP_TIME 1   /* 1us - max(adm no, 93c 200ns) */

/* Handy macros for writing fixed length values */
#define adm_write8(cs, b) { __u8 val = (__u8) (b); adm_write(cs, &val, sizeof(val)*8); }
#define adm_write16(cs, w) { __u16 val = hton16(w); adm_write(cs, (__u8 *)&val, sizeof(val)*8); }
#define adm_write32(cs, i) { uint32 val = hton32(i); adm_write(cs, (__u8 *)&val, sizeof(val)*8); }


static void adm_write(int cs, char *buf, unsigned int bits)
{
	int i, len = (bits + 7) / 8;
	__u8 mask;

	sb_gpioout(sbh, eecs, (cs ? eecs : 0));
	udelay(EECK_EDGE_TIME);

	/* Byte assemble from MSB to LSB */
	for (i = 0; i < len; i++) {
		/* Bit bang from MSB to LSB */
		for (mask = 0x80; mask && bits > 0; mask >>= 1, bits --) {
			/* Clock low */
			sb_gpioout(sbh, eesk, 0);
			udelay(EECK_EDGE_TIME);

			/* Output on rising edge */
			sb_gpioout(sbh, eedi, ((mask & buf[i]) ? eedi : 0));
			udelay(EEDI_SETUP_TIME);

			/* Clock high */
			sb_gpioout(sbh, eesk, eesk);
			udelay(EECK_EDGE_TIME);
		}
	}

	/* Clock low */
	sb_gpioout(sbh, eesk, 0);
	udelay(EECK_EDGE_TIME);

	if (cs)
		sb_gpioout(sbh, eecs, 0);
}


static void adm_read(int cs, char *buf, unsigned int bits)
{
	int i, len = (bits + 7) / 8;
	__u8 mask;

	sb_gpioout(sbh, eecs, (cs ? eecs : 0));
	udelay(EECK_EDGE_TIME);

	/* Byte assemble from MSB to LSB */
	for (i = 0; i < len; i++) {
		__u8 byte;

		/* Bit bang from MSB to LSB */
		for (mask = 0x80, byte = 0; mask && bits > 0; mask >>= 1, bits --) {
			__u8 gp;

			/* Clock low */
			sb_gpioout(sbh, eesk, 0);
			udelay(EECK_EDGE_TIME);

			/* Input on rising edge */
			gp = sb_gpioin(sbh);
			if (gp & eedi)
				byte |= mask;

			/* Clock high */
			sb_gpioout(sbh, eesk, eesk);
			udelay(EECK_EDGE_TIME);
		}

		*buf++ = byte;
	}

	/* Clock low */
	sb_gpioout(sbh, eesk, 0);
	udelay(EECK_EDGE_TIME);

	if (cs)
		sb_gpioout(sbh, eecs, 0);
}


/* Enable outputs with specified value to the chip */
static void adm_enout(__u8 pins, __u8 val)
{   
	/* Prepare GPIO output value */
	sb_gpioout(sbh, pins, val);
	
	/* Enable GPIO outputs */
	sb_gpioouten(sbh, pins, pins);
	udelay(EECK_EDGE_TIME);
}


/* Disable outputs to the chip */
static void adm_disout(__u8 pins)
{   
	/* Disable GPIO outputs */
	sb_gpioouten(sbh, pins, 0);
	udelay(EECK_EDGE_TIME);
}


/* Advance clock(s) */
static void adm_adclk(int clocks)
{
	int i;
	for (i = 0; i < clocks; i++) {
		/* Clock high */
		sb_gpioout(sbh, eesk, eesk);
		udelay(EECK_EDGE_TIME);

		/* Clock low */
		sb_gpioout(sbh, eesk, 0);
		udelay(EECK_EDGE_TIME);
	}
}

static __u32 adm_rreg(__u8 table, __u8 addr)
{
	/* cmd: 01 10 T DD R RRRRRR */
	__u8 bits[6] = {
		0xFF, 0xFF, 0xFF, 0xFF,
		(0x06 << 4) | ((table & 0x01) << 3 | (addr&64)>>6),
		((addr&62)<<2)
	};

	__u8 rbits[4];

	/* Enable GPIO outputs with all pins to 0 */
	adm_enout((__u8)(eecs | eesk | eedi), 0);

	adm_write(0, bits, 46);
	adm_disout((__u8)(eedi));
	adm_adclk(2);
	adm_read (0, rbits, 32);

	/* Extra clock(s) required per datasheet */
	adm_adclk(2);

	/* Disable GPIO outputs */
	adm_disout((__u8)(eecs | eesk));

	if (!table) /* EEPROM has 16-bit registers, but pumps out two registers in one request */
		return (addr & 0x01 ?  (rbits[0]<<8) | rbits[1] : (rbits[2]<<8) | (rbits[3]));
	else
		return (rbits[0]<<24) | (rbits[1]<<16) | (rbits[2]<<8) | rbits[3];
}



/* Write chip configuration register */
/* Follow 93c66 timing and chip's min EEPROM timing requirement */
void
adm_wreg(__u8 addr, __u16 val)
{
	/* cmd(27bits): sb(1) + opc(01) + addr(bbbbbbbb) + data(bbbbbbbbbbbbbbbb) */
	__u8 bits[4] = {
		(0x05 << 5) | (addr >> 3),
		(addr << 5) | (__u8)(val >> 11),
		(__u8)(val >> 3),
		(__u8)(val << 5)
	};

	/* Enable GPIO outputs with all pins to 0 */
	adm_enout((__u8)(eecs | eesk | eedi), 0);

	/* Write cmd. Total 27 bits */
	adm_write(1, bits, 27);

	/* Extra clock(s) required per datasheet */
	adm_adclk(2);

	/* Disable GPIO outputs */
	adm_disout((__u8)(eecs | eesk | eedi));
}


/* Port configuration registers */
static int port_conf[] = { 0x01, 0x03, 0x05, 0x07, 0x08, 0x09 };

/* Bits in VLAN port mapping */
static int vlan_ports[] = { 1 << 0, 1 << 2, 1 << 4, 1 << 6, 1 << 7, 1 << 8 };

static int handle_vlan_port_read(char *buf, int nr)
{
	int ports, i, c, len = 0;
			
	if ((nr < 0) || (nr > 15))
		return 0;

	/* Get VLAN port map */
	ports = adm_rreg(0, 0x13 + nr);
	
	for (i = 0; i <= 5; i++) {
		if (ports & vlan_ports[i]) {
			c = adm_rreg(0, port_conf[i]);
			len += sprintf(buf + len, (c & (1 << 4) ? "%dt\t" : (i == 5 ? "%du\t" : "%d\t")), i);
		}
	}
	len += sprintf(buf + len, "\n");

	return len;
}

static int handle_vlan_port_write(char *buf, int nr)
{
	int i, c, ports;
	int map = parse_vlan_ports(buf);

	if (map == -1)
		return -1;

	ports = adm_rreg(0, 0x13 + nr);
	for (i = 0; i <= 5; i++) {
		if (map & (1 << i)) {
			ports |= vlan_ports[i];

			c = adm_rreg(0, port_conf[i]);
			
			/* Tagging */
			if (map & (1 << (8 + i)))
				c |= (1 << 4);
			else
				c &= ~(1 << 4);

			c = (c & ~(0xf << 10)) | (nr << 10);
			
			adm_wreg(port_conf[i], (__u16) c);
		} else {
			ports &= ~(vlan_ports[i]);
		}
	}
	adm_wreg(0x13 + nr, (__u16) ports);

	return 0;
}

static int handle_port_enable_read(char *buf, int nr)
{
	return sprintf(buf, "%d\n", ((adm_rreg(0, port_conf[nr]) & (1 << 5)) ? 0 : 1));
}

static int handle_port_enable_write(char *buf, int nr)
{
	int reg = adm_rreg(0, port_conf[nr]);
	
	if (buf[0] == '0')
		reg |= (1 << 5);
	else if (buf[0] == '1')
		reg &= ~(1 << 5);
	else return -1;

	adm_wreg(port_conf[nr], (__u16) reg);
	return 0;
}

static int handle_port_media_read(char *buf, int nr)
{
	int len;
	int media = 0;
	int reg = adm_rreg(0, port_conf[nr]);

	if (reg & (1 << 1))
		media |= SWITCH_MEDIA_AUTO;
	if (reg & (1 << 2))
		media |= SWITCH_MEDIA_100;
	if (reg & (1 << 3))
		media |= SWITCH_MEDIA_FD;

	len = print_media(buf, media);
	return len + sprintf(buf + len, "\n");
}

static int handle_port_media_write(char *buf, int nr)
{
	int media = parse_media(buf);
	int reg = adm_rreg(0, port_conf[nr]);

	if (media < 0)
		return -1;
	
	reg &= ~((1 << 1) | (1 << 2) | (1 << 3));
	if (media & SWITCH_MEDIA_AUTO)
		reg |= 1 << 1;
	if (media & SWITCH_MEDIA_100)
		reg |= 1 << 2;
	if (media & SWITCH_MEDIA_FD)
		reg |= 1 << 3;

	adm_wreg(port_conf[nr], reg);
	
	return 0;
}

static int handle_vlan_enable_read(char *buf, int nr)
{
	return sprintf(buf, "%d\n", ((adm_rreg(0, 0x11) & (1 << 5)) ? 1 : 0));
}

static int handle_vlan_enable_write(char *buf, int nr)
{
	int reg = adm_rreg(0, 0x11);
	
	if (buf[0] == '1')
		reg |= (1 << 5);
	else if (buf[0] == '0')
		reg &= ~(1 << 5);
	else return -1;

	adm_wreg(0x11, (__u16) reg);
	return 0;
}

static int handle_reset(char *buf, int nr)
{
	int i;

	/*
	 * Reset sequence: RC high->low(100ms)->high(30ms)
	 *
	 * WAR: Certain boards don't have the correct power on 
	 * reset logic therefore we must explicitly perform the
	 * sequence in software.
	 */
	/* Keep RC high for at least 20ms */
	adm_enout(eerc, eerc);
	for (i = 0; i < 20; i ++)
		udelay(1000);
	/* Keep RC low for at least 100ms */
	adm_enout(eerc, 0);
	for (i = 0; i < 100; i++)
		udelay(1000);
	/* Set default configuration */
	adm_enout((__u8)(eesk | eedi), eesk);
	/* Keep RC high for at least 30ms */
	adm_enout(eerc, eerc);
	for (i = 0; i < 30; i++)
		udelay(1000);
	/* Leave RC high and disable GPIO outputs */
	adm_disout((__u8)(eecs | eesk | eedi));

	/* set up initial configuration for ports */
	for (i = 0; i <= 5; i++) {
		int cfg = 0x8000 | /* Auto MDIX */
			(((i == 5) ? 1 : 0) << 4) | /* Tagging */
			0xf; /* full duplex, 100Mbps, auto neg, flow ctrl */
		adm_wreg(port_conf[i], cfg);
	}
	
	/* vlan mode select register (0x11): vlan on, mac clone */
	adm_wreg(0x11, 0xff30);

	return 0;
}

static int handle_registers(char *buf, int nr)
{
	int i, len = 0;
	
	for (i = 0; i <= 0x33; i++) {
		len += sprintf(buf + len, "0x%02x: 0x%04x\n", i, adm_rreg(0, i));
	}

	return len;
}

static int handle_counters(char *buf, int nr)
{
	int i, len = 0;

	for (i = 0; i <= 0x3c; i++) {
		len += sprintf(buf + len, "0x%02x: 0x%08x\n", i, adm_rreg(1, i));
	}

	return len;
}

static int __init adm_init()
{
	eecs = (1 << eecs);
	eesk = (1 << eesk);
	eedi = (1 << eedi);

	switch_config cfg[] = {
		{"registers", handle_registers, NULL},
		{"counters", handle_counters, NULL},
		{"reset", NULL, handle_reset},
		{"enable_vlan", handle_vlan_enable_read, handle_vlan_enable_write},
		{NULL, NULL, NULL}
	};
	switch_config port[] = {
		{"enabled", handle_port_enable_read, handle_port_enable_write},
		{"media", handle_port_media_read, handle_port_media_write},
		{NULL, NULL, NULL}
	};
	switch_config vlan[] = {
		{"ports", handle_vlan_port_read, handle_vlan_port_write},
		{NULL, NULL, NULL}
	};
	switch_driver driver = {
		name: DRIVER_NAME,
		ports: 6,
		vlans: 16,
		driver_handlers: cfg,
		port_handlers: port,
		vlan_handlers: vlan,
	};

	return switch_register_driver(&driver);
}

static void __exit adm_exit()
{
	switch_unregister_driver(DRIVER_NAME);
}


module_init(adm_init);
module_exit(adm_exit);
