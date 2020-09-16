/*
 * ethernet.c
 *
 * Copyright (C) 2015 Sebastian Gottschall <gottschall@dd-wrt.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Id:
 * 
 * detects ethernet adapters and loads the drivers
 */
#include <malloc.h>
#include <shutils.h>
#include <utils.h>
#include <bcmnvram.h>

static char *pcidrivers[] = {
	"hv_netvsc",
	"vmxnet3",
	"virtio_net",
	"3c574_cs",
	"3c589_cs",
	"3c59x",
	"8139cp",
	"8139too",
	"8390",
	"acenic",
	"altera_tse",
	"alx",
	"amd8111e",
	"atl1",
	"atl1c",
	"atl1e",
	"atl2",
	"axnet_cs",
	"be2net",
	"bna",
	"bnx2x",
	"bnxt_en",
	"cassini",
	"cnic",
	"cpsw_ale",
	"de2104x",
	"de4x5",
	"dl2k",
	"dmfe",
	"dnet",
	"e100",
	"e1000",
	"e1000e",
	"e1000gcu",
	"e1000gbe",
	"ec_bhf",
	"enc28j60",
	"encx24j600-regmap",
	"encx24j600",
	"enic",
	"epic100",
	"et131x",
	"ethoc",
	"fealnx",
	"fm10k",
	"fmvj18x_cs",
	"forcedeth",
	"genet",
	"hamachi",
	"hp100",
	"i40e",
	"i40evf",
	"igb",
	"igbvf",
	"ipg",
	"ixgb",
	"ixgbe",
	"jme",
	"ks8842",
	"ks8851",
	"ks8851_mll",
	"ksz884x",
	"macb",
	"mlx4_core",
	"mlx4_en",
	"myri10ge",
	"natsemi",
	"nb8800",
	"ne2k-pci",
	"netxen_nic",
	"niu",
	"nmclan_cs",
	"ns83820",
	"pcnet32",
	"pcnet_cs",
	"qed",
	"qede",
	"qla3xxx",
	"qlcnic",
	"qlge",
	"r6040",
	"r8169",
	"s2io",
	"samsung-sxgbe",
	"sc92031",
	"sfc",
	"sis190",
	"sis900",
	"skge",
	"sk98lin",
//      "sky2",
	"smc91c92_cs",
	"smsc911x",
	"smsc9420",
	"starfire",
	"stmmac",
	"sundance",
	"sungem",
	"sunhme",
	"tehuti",
	"tg3",
	"tlan",
	"tulip",
	"typhoon",
	"uli526x",
	"via-rhine",
	"via-velocity",
	"vxge",
	"w5100",
	"w5300",
	"winbond-840",
	"xirc2ps_cs",
	"xircom_cb",
	"yellowfin",
	NULL
};

static char *usbdrivers[] = {
	"asix",
	"ax88179_178a",
	"catc",
	"ch9200",
	"cx82310_eth",
	"dm9601",
	"gl620a",
	"int51x1",
	"kalmia",
	"kaweth",
	"lan78xx",
	"lg-vl600",
	"mcs7830",
	"net1080",
	"pegasus",
	"plusb",
	"r8152",
	"rtl8150",
	"sierra_net",
	"smsc75xx",
	"smsc95xx",
	"sr9700",
	"sr9800",
	"usbnet",
	"zaurus",
	NULL
};

static int detect_driver(char **drivers, char *list, int delay, int insmod)
{
	int basecount = getifcount("eth");
	int ret;
	int newcount;
	int rcc = 0;
	int cnt = 0;
	char *driver;
	nvram_unset(list);
	while ((driver = drivers[cnt++]) != NULL) {
		if (insmod)
			ret = eval("insmod", driver);
		else
			ret = eval("modprobe", driver);
		if (delay)
			sleep(delay);
		if (!ret && (newcount = getifcount("eth")) > basecount) {
			basecount = newcount;
			char *pcid = nvram_safe_get(list);
			char *newdriver = malloc(strlen(pcid) + strlen(driver) + 2);
			if (*pcid)
				sprintf(newdriver, "%s %s", pcid, driver);
			else
				sprintf(newdriver, "%s", driver);
			nvram_set(list, newdriver);
			free(newdriver);
			rcc |= 1;
		} else {
			if (insmod)
				eval("rmmod", driver);
			else
				eval("modprobe", "-r", driver);
		}
	}
	return rcc;
}

static int detect_drivers(char *buspath, char *enabled, char *list, char **driverset, int delay, int insmod)
{
	char word[256];
	char *next, *wordlist;
	int rcc = 0;
	char s_hash[40] = { 0 };
	char *hash;
	/* some address properties change constantly, so we need a proper method just to detect the things we need */
	if (!strcmp(buspath, "/proc/bus/pci/devices")) {
		FILE *fp = fopen(buspath, "rb");
		char buf[512];
		char final[512] = { 0 };
		while (fgets(buf, sizeof(buf) - 1, fp)) {
			unsigned int defnr, vendor;
			sscanf(buf, "%x %x", &defnr, &vendor);
			snprintf(final, sizeof(final) - 1, "%s %x ", final, vendor);
		}
		fclose(fp);
		hash = hash_string(final, s_hash);
	} else {
		hash = hash_file_string(buspath, s_hash);
	}
	if (!hash)
		return 0;	// bus not present. ignore
	if (!nvram_match(enabled, hash)) {	// hash does not match, bus has been changed. so redetect drivers
		rcc = detect_driver(driverset, list, delay, insmod);
		nvram_set(enabled, hash);	// store new hash
		nvram_commit();
	} else {
		wordlist = nvram_safe_get(list);
		if (!*wordlist)
			return 0;
		foreach(word, wordlist, next) {
			if (insmod)
				eval("insmod", word);
			else
				eval("modprobe", word);
		}
		rcc = 1;
	}
	return rcc;
}

static int detect_pcidrivers(void)
{
	return detect_drivers("/proc/bus/pci/devices", "pci_detected", "pcidrivers", pcidrivers, 0, 0);
}

static int detect_usbdrivers(void)
{
	insmod("usb-common nls_base usbcore usbnet cdc_ether cdc_ncm dcd-wdm");
	return detect_drivers("/proc/bus/usb/devices", "usb_detected", "usbdrivers", usbdrivers, 0, 1);
}

static int detect_ethernet_devices(void)
{
	int rcc = 0;
	rcc |= detect_pcidrivers();
	rcc |= detect_usbdrivers();
	return rcc;
}

void start_detectdrivers(void)
{
	nvram_seti("usb_detected", 0);
	nvram_seti("pci_detected", 0);
	detect_ethernet_devices();
}

#ifdef TEST

void main(int argc, char *argv[])
{
	start_detectdrivers();
}

#endif
