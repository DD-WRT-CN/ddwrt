/*
 * site_survey_madwifi.c
 *
 * Copyright (C) 2006 Sebastian Gottschall <gottschall@dd-wrt.com>
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
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <getopt.h>
#include <err.h>
#include <wlutils.h>
#ifndef __UCLIBC__
/* Convenience types.  */
typedef unsigned char __u_char;
typedef unsigned short int __u_short;
typedef unsigned int __u_int;
typedef unsigned long int __u_long;

/* Fixed-size types, underlying types depend on word size and compiler.  */
typedef signed char __int8_t;
typedef unsigned char __uint8_t;
typedef signed short int __int16_t;
typedef unsigned short int __uint16_t;
typedef signed int __int32_t;
typedef unsigned int __uint32_t;
#endif

#include "wireless_copy.h"
#include "net80211/ieee80211.h"
#include "net80211/ieee80211_crypto.h"
#include "net80211/ieee80211_ioctl.h"

static int copy_essid(char buf[], size_t bufsize, const u_int8_t *essid, size_t essid_len)
{
	const u_int8_t *p;
	int maxlen;
	int i;

	if (essid_len > bufsize)
		maxlen = bufsize;
	else
		maxlen = essid_len;
	/*
	 * determine printable or not 
	 */
	for (i = 0, p = essid; i < maxlen; i++, p++) {
		if (*p < ' ' || *p > 0x7e)
			break;
	}
	if (i != maxlen) {	/* not printable, print as hex */
		if (bufsize < 3)
			return 0;
#if 0
		strlcpy(buf, "0x", bufsize);
#else
		strncpy(buf, "0x", bufsize);
#endif
		bufsize -= 2;
		p = essid;
		for (i = 0; i < maxlen && bufsize >= 2; i++) {
			sprintf(&buf[2 + 2 * i], "%02x", *p++);
			bufsize -= 2;
		}
		maxlen = 2 + 2 * i;
	} else {		/* printable, truncate as needed */
		memcpy(buf, essid, maxlen);
	}
	if (maxlen != essid_len)
		memcpy(buf + maxlen - 3, "...", 3);
	return maxlen;
}

#define sys_restart() kill(1, SIGHUP)

int write_site_survey(void);
static int open_site_survey(void);
int write_site_survey(void);

struct site_survey_list *site_survey_lists;

static const char *ieee80211_ntoa(const uint8_t mac[IEEE80211_ADDR_LEN])
{
	static char a[18];
	int i;

	i = snprintf(a, sizeof(a), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return (i < 17 ? NULL : a);
}

int site_survey_main(int argc, char *argv[])
{
	char *name = nvram_safe_get("wl0_ifname");
	unsigned char mac[20];
	int i = 0;
	char *dev = name;

	unlink(SITE_SURVEY_DB);
	int ap = 0, oldap = 0;

	unsigned char buf[24 * 1024];
	char ssid[31];
	unsigned char *cp;
	int len;

	system2("airoscan-ng wifi0");
	len = do80211priv("ath0", IEEE80211_IOCTL_SCAN_RESULTS, buf, sizeof(buf));

	if (len == -1)
		fprintf(stderr, "unable to get scan results");
	if (len < sizeof(struct ieee80211req_scan_result))
		return;
	cp = buf;
	do {
		struct ieee80211req_scan_result *sr;
		unsigned char *vp;
		char ssid[14];

		sr = (struct ieee80211req_scan_result *)cp;
		vp = (u_int8_t *)(sr + 1);
		memset(ssid, 0, sizeof(ssid));
		strncpy(site_survey_lists[i].SSID, vp, sr->isr_ssid_len);
		strcpy(site_survey_lists[i].BSSID, ieee80211_ntoa(sr->isr_bssid));
		site_survey_lists[i].channel = ieee80211_mhz2ieee(sr->isr_freq);
		site_survey_lists[i].frequency = sr->isr_freq;
		int noise = 256;

		noise -= (int)sr->isr_noise;
		site_survey_lists[i].phy_noise = -noise;
		site_survey_lists[i].RSSI = (int)site_survey_lists[i].phy_noise + (int)sr->isr_rssi;
		site_survey_lists[i].capability = sr->isr_capinfo;
		site_survey_lists[i].rate_count = sr->isr_nrates;
		cp += sr->isr_len, len -= sr->isr_len;
		i++;
	}
	while (len >= sizeof(struct ieee80211req_scan_result));

	write_site_survey();
	open_site_survey();
	for (i = 0; i < SITE_SURVEY_NUM && site_survey_lists[i].BSSID[0]
	     && site_survey_lists[i].channel != 0; i++) {
		fprintf(stderr,
			"[%2d] SSID[%20s] BSSID[%s] channel[%2d] frequency[%4d] rssi[%d] noise[%d] beacon[%d] cap[%x] dtim[%d] rate[%d]\n",
			i, site_survey_lists[i].SSID,
			site_survey_lists[i].BSSID,
			site_survey_lists[i].channel,
			site_survey_lists[i].frequency,
			site_survey_lists[i].RSSI, site_survey_lists[i].phy_noise, site_survey_lists[i].beacon_period, site_survey_lists[i].capability, site_survey_lists[i].dtim_period, site_survey_lists[i].rate_count);
	}

	return 0;
}

int write_site_survey(void)
{
	FILE *fp;

	if ((fp = fopen(SITE_SURVEY_DB, "w"))) {
		fwrite(&site_survey_lists[0], sizeof(site_survey_lists), 1, fp);
		fclose(fp);
		return 0;
	}
	return 1;
}

static int open_site_survey(void)
{
	FILE *fp;

	bzero(site_survey_lists, sizeof(site_survey_lists));

	if ((fp = fopen(SITE_SURVEY_DB, "r"))) {
		fread(&site_survey_lists[0], sizeof(site_survey_lists), 1, fp);
		fclose(fp);
		return 1;
	}
	return 0;
}
