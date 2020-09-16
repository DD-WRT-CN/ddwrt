/*
 * wireless.c
 *
 * Copyright (C) 2009 Sebastian Gottschall <gottschall@dd-wrt.com>
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
 * detects atheros wireless adapters and loads the drivers
 */

// extern int getath9kdevicecount(void);
#include <wlutils.h>

extern void delete_ath9k_devices(char *physical_iface);

static void setWirelessLed(int phynum, int ledpin)
{
#ifdef HAVE_ATH9K
	char trigger[32];
	char sysname[32];
	sprintf(trigger, "phy%dtpt", phynum);
	if (ledpin < 32) {
		sprintf(sysname, "generic_%d", ledpin);
	} else {
		sprintf(sysname, "wireless_generic_%d", ledpin - 32);
	}
	sysprintf("echo %s > /sys/devices/platform/leds-gpio/leds/%s/trigger", trigger, sysname);
#endif
}

#define setWirelessLedGeneric(a,b) setWirelessLed(a,b)
#define setWirelessLedPhy0(b) setWirelessLed(0,b+32)
#define setWirelessLedPhy1(b) setWirelessLed(1,b+48)

static char *has_device(char *dev)
{
	char path[64];
	static char devstr[32];
	sprintf(path, "/sys/bus/pci/devices/0000:00:%s/device", dev);
	FILE *fp = fopen(path, "rb");
	if (fp) {
		fscanf(fp, "%s", devstr);
		fclose(fp);
		return devstr;
	}

	return "";
}

static int phy_lookup_by_number(int idx)
{
	int err;
	int phy = getValueFromPath("/sys/class/ieee80211/phy%d/index", idx, "%d", &err);
	if (err)
		return -1;
	return phy;
}

static int totalwifi = 0;
int detectchange(char *mod)
{
	static int lastidx = -1;
	int cnt = 0;
	if (mod)
		insmod(mod);
	while (phy_lookup_by_number(cnt) >= 0) {
		cnt++;
	}
	if (!cnt) {
		if (mod)
			rmmod(mod);
		return 0;
	}
	if (cnt > lastidx) {
		lastidx = cnt;
		totalwifi++;
		return 1;
	}
	if (mod)
		rmmod(mod);
	return 0;
}

#define AR5416_DEVID_PCI	0x0023
#define AR5416_DEVID_PCIE	0x0024
#define AR9160_DEVID_PCI	0x0027
#define AR9280_DEVID_PCI	0x0029
#define AR9280_DEVID_PCIE	0x002a
#define AR9285_DEVID_PCIE	0x002b
#define AR2427_DEVID_PCIE	0x002c
#define AR9287_DEVID_PCI	0x002d
#define AR9287_DEVID_PCIE	0x002e
#define AR9300_DEVID_PCIE	0x0030
#define AR9300_DEVID_AR9340	0x0031
#define AR9300_DEVID_AR9485_PCIE 0x0032
#define AR9300_DEVID_AR9580	0x0033
#define AR9300_DEVID_AR9462	0x0034
#define AR9300_DEVID_AR9330	0x0035

#define AR5416_AR9100_DEVID	0x000b

static void load_mac80211(void)
{
	insmod("compat");
	insmod("compat_firmware_class");
	insmod("cfg80211");
	insmod("mac80211");

}

#define RADIO_ALL 0xff
#define RADIO_ATH9K 0x1
#define RADIO_ATH10K 0x2
#define RADIO_LEGACY 0x4
#define RADIO_BRCMFMAC 0x8
#define RADIO_WIL6210 0x10
#define RADIO_RTLWIFI 0x20
#define RADIO_MT76 0x40
#define RADIO_IWLWIFI 0x80

static void detect_wireless_devices(int mask)
{
	int loadath9k = 1;
	int loadlegacy = 1;
	int loadath5k = 0;
#ifdef HAVE_RT61
	if (!strcmp(has_device("0e.0"), "0x3592"))
		nvram_set("rtchip", "3062");
	else
		nvram_set("rtchip", "2860");
#endif
#ifdef HAVE_XSCALE
	loadath9k = 0;
	loadlegacy = 0;
	char path[32];
	int i = 0;
	char *bus[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c", "d", "e", "f" };
	for (i = 0; i < 16; i++) {
		sprintf(path, "0%s.0", bus[i]);
		if (!strcmp(has_device(path), "0x0023"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x0024"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x0027"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x0029"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x002a"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x002b"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x002c"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x002d"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x002e"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x0030"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x0031"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x0032"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x0033"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x0034"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x0035"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x0036"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x0037"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x0038"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x000b"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0xabcd"))
			loadath9k = 1;
		if (!strcmp(has_device(path), "0x001b"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x0013"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x0207"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x0007"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0xff16"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x0012"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x1014"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x101a"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x0015"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x0016"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x0017"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x0018"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x0019"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x001a"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x001b"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x018a"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x001c"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x001d"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0x9013"))
			loadlegacy = 1;
		if (!strcmp(has_device(path), "0xff1a"))
			loadlegacy = 1;
	}
#endif
#ifdef HAVE_ATH5K
#ifndef HAVE_ATH5KONLY
	if (nvram_match("use_ath5k", "1"))
#endif
	{
		loadath5k = loadlegacy;
		loadlegacy = 0;
	}
#endif
#ifndef HAVE_NOWIFI
	nvram_default_get("rate_control", "minstrel");
#ifdef HAVE_MADWIFI
	if (loadlegacy && (mask & RADIO_LEGACY)) {
		fprintf(stderr, "load ATH 802.11 a/b/g Driver\n");
		insmod("ath_hal");
		if (nvram_exists("rate_control")) {
			char rate[64];

			sprintf(rate, "ratectl=%s", nvram_safe_get("rate_control"));
			eval("insmod", "ath_pci", rate);
			eval("insmod", "ath_ahb", rate);
		} else {
			insmod("ath_pci");
			insmod("ath_ahb");
		}
	}
#ifdef HAVE_ATH9K
	{
		if (loadath9k || loadath5k) {
			fprintf(stderr, "load ATH9K 802.11n Driver\n");
			load_mac80211();
			// some are just for future use and not (yet) there
			insmod("ath");
#ifdef HAVE_ATH5K
			if (loadath5k && (mask & RADIO_LEGACY)) {
				fprintf(stderr, "load ATH5K 802.11 Driver\n");
				insmod("ath5k");
				if (!detectchange(NULL))
					rmmod("ath5k");
			}
#endif
			if (!nvram_match("no_ath9k", "1") && (mask & RADIO_ATH9K)) {
				int od = nvram_default_geti("power_overdrive", 0);
				char overdrive[32];
				sprintf(overdrive, "overdrive=%d", od);

#ifdef HAVE_WZRG450
				eval("insmod", "ath9k", overdrive, "blink=1");
#elif HAVE_PERU
				char *dis = getUEnv("rndis");
				if (dis && !strcmp(dis, "1"))
					eval("insmod", "ath9k", overdrive, "no_ahb=1");
				else
					eval("insmod", "ath9k", overdrive);

#else
				eval("insmod", "ath9k", overdrive);
#endif
				if (!detectchange(NULL))
					rmmod("ath9k");
			}
			if (!totalwifi)
				rmmod("ath");
			delete_ath9k_devices(NULL);
		}
	}
#endif

#ifdef HAVE_ATH10K
	fprintf(stderr, "load ATH/QCA 802.11ac Driver\n");
	if ((mask & RADIO_ATH10K)) {
		insmod("hwmon");
		insmod("thermal_sys");
		nvram_default_get("ath10k_encap", "0");
		if (!nvram_match("noath10k", "1")) {
			insmod("ath");
			if (nvram_match("ath10k_encap", "1"))
				eval("insmod", "ath10k", "ethernetmode=1");
			else
				insmod("ath10k");
			if (!detectchange(NULL)) {
				rmmod("ath10k");
				rmmod("ath");
			}
		}
	}
#endif
#ifdef HAVE_WIL6210
	if ((mask & RADIO_WIL6210)) {
		eval("insmod", "wil6210", "led_id=2");
		if (!detectchange(NULL))
			rmmod("wil6210");
	}
#endif
#ifdef HAVE_BRCMFMAC
	if ((mask & RADIO_BRCMFMAC)) {
		fprintf(stderr, "load Broadcom FMAC Driver\n");
		insmod("brcmutil");
		insmod("brcmfmac");
		if (!detectchange("brcmfmac")) {
			rmmod("brcmutil");
		}
	}
#endif
#ifdef HAVE_RTLWIFI
	if ((mask & RADIO_RTLWIFI)) {
		fprintf(stderr, "load Realtek RTLWIFI Driver\n");
		int total = 0;
		insmod("rtlwifi");
		insmod("rtl_pci");
		insmod("rtl_usb");
		insmod("btcoexist");
		int wificnt = 0;
		wificnt += detectchange("rtl8188ee");
		insmod("rtl8192c-common");
		wificnt += detectchange("rtl8192ce");
		wificnt += detectchange("rtl8192de");
		wificnt += detectchange("rtl8192se");
		wificnt += detectchange("rtl8821ae");
		wificnt += detectchange("rtl8192cu");
		wificnt += detectchange("rtl8192ee");
		if (!wificnt)
			rmmod("rtl8192c-common");
		total += wificnt;
		wificnt = 0;

		insmod("rtl8723-common");
		wificnt += detectchange("rtl8723be");
		wificnt += detectchange("rtl8723ae");
		if (!wificnt)
			rmmod("rtl8723-common");
		total += wificnt;
		wificnt = 0;
		if (!total) {
			rmmod("btcoexist");
			rmmod("rtl_usb");
			rmmod("rtl_pci");
			rmmod("rtlwifi");
		}

		insmod("rtw88_core");
		insmod("rtw88_pci");
		insmod("rtw88_8822b");
		insmod("rtw88_8822c");
		insmod("rtw88_8723d");
		wificnt += detectchange("rtw88_8822be");
		wificnt += detectchange("rtw88_8822ce");
		wificnt += detectchange("rtw88_8723de");
		if (!wificnt) {
			rmmod("rtw88_8723d");
			rmmod("rtw88_8822c");
			rmmod("rtw88_8822b");
			rmmod("rtw88_pci");
			rmmod("rtw88_core");
		}
	}
#endif
#ifdef HAVE_X86
	if ((mask & RADIO_MT76)) {
		fprintf(stderr, "load Medatek MT76 Driver\n");
		int wificnt = 0;
		int total = 0;
		insmod("mt76");
		insmod("mt76-usb");
		insmod("mt76-sdio");
		insmod("mt7615-common");
		insmod("mt7663-usb-sdio-common");
		wificnt += detectchange("mt7615e");
		wificnt += detectchange("mt7663u");
		wificnt += detectchange("mt7663s");
		if (!wificnt) {
			rmmod("mt7615_common");
			rmmod("mt7615-common");
			rmmod("mt7663-usb-sdio-common");
			rmmod("mt7663_usb_sdio_common");
			rmmod("mt76-sdio");
		}
		total += wificnt;
		wificnt = 0;
		insmod("mt76x02-lib");
		insmod("mt76x02-usb");
		insmod("mt76x2-common");
		wificnt += detectchange("mt76x2e");
		wificnt += detectchange("mt76x2u");
		total += wificnt;
		wificnt = 0;
		insmod("mt76x0-common");
		wificnt += detectchange("mt76x0e");
		wificnt += detectchange("mt76x0u");
		if (!wificnt) {
			rmmod("mt76x0-common");
			rmmod("mt76x2-common");
			rmmod("mt76x02-usb");
			rmmod("mt76x02-lib");
		}
		total += wificnt;
		wificnt = 0;
		wificnt += detectchange("mt7603e");
		wificnt += detectchange("mt7915e");
		total += wificnt;
		if (!total) {
			rmmod("mt76-usb");
			rmmod("mt76_usb");
			rmmod("mt76");
		}
		wificnt = 0;
		insmod("rt2x00lib");
		insmod("rt2x00mmio");
		insmod("rt2x00pci");
		insmod("rt2800lib");
		insmod("rt2800mmio");
		if (!detectchange("rt2800pci")) {
			rmmod("rt2800mmio");
			rmmod("rt2800lib");
			rmmod("rt2x00pci");
			rmmod("rt2x00mmio");
			rmmod("rt2x00lib");
		}

	}
	if ((mask & RADIO_IWLWIFI)) {
		insmod("libipw");
		if (!detectchange("ipw2100") && !detectchange("ipw2200"))
			rmmod("libipw");
		insmod("iwlegacy");
		if (!detectchange("iwl3945") && !detectchange("iwl4965")) {
			rmmod("iwlegacy");
		}
		insmod("iwlwifi");
		if (!detectchange("iwldwm") && !detectchange("iwlmvm")) {
			rmmod("iwlwifi");
		}

	}
#endif
	if (!totalwifi) {
		rmmod("mac80211");
		rmmod("cfg80211");
		rmmod("compat_firmware_class");
		rmmod("compat");

	}
#endif
#endif
}
