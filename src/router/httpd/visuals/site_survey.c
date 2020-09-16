/*
 * site_survey.c
 *
 * Copyright (C) 2005 - 2018 Sebastian Gottschall <gottschall@dd-wrt.com>
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
#define VISUALSOURCE 1

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include <dd_defs.h>
#include <code_pattern.h>
#include <broadcom.h>
#include <proto/802.11.h>

#include <wlutils.h>

int getrate(int rate, int bw)
{
	int result = rate * 10;
	switch (rate) {
	case 150:
		if (bw == 20)
			result = 722;
		if (bw == 80)
			result = 4333;
		if (bw == 160)
			result = 8667;
		break;
	case 300:
		if (bw == 20)
			result = 1444;
		if (bw == 80)
			result = 8667;
		if (bw == 160)
			result = 17333;
		break;
	case 450:
		if (bw == 20)
			result = 2167;
		if (bw == 80)
			result = 13000;
		if (bw == 160)
			result = 23400;	// this rate is not specified
		break;
	case 600:
		if (bw == 20)
			result = 2889;
		if (bw == 80)
			result = 17333;
		if (bw == 160)
			result = 34667;
		break;
	}
	return result;
}

static char *speedstr(int speed, char *buf)
{
	if (speed % 10)
		sprintf(buf, "%d.%d", speed / 10, speed % 10);
	else
		sprintf(buf, "%d", speed / 10);
	return buf;
}

static struct site_survey_list *site_survey_lists;

static int open_site_survey(void)
{
	FILE *fp;
	site_survey_lists = malloc(sizeof(struct site_survey_list) * SITE_SURVEY_NUM);
	bzero(site_survey_lists, sizeof(struct site_survey_list) * SITE_SURVEY_NUM);

	if ((fp = fopen(SITE_SURVEY_DB, "r"))) {
		fread(&site_survey_lists[0], sizeof(struct site_survey_list) * SITE_SURVEY_NUM, 1, fp);
		fclose(fp);
		return TRUE;
	}
	return FALSE;
}

static char *dtim_period(int dtim, char *mem)
{
	if (dtim)
		snprintf(mem, 32, "%d", dtim);
	else
		snprintf(mem, 32, "%s", "None");
	return mem;
}

#ifdef FBNFW

void ej_list_fbn(webs_t wp, int argc, char_t ** argv)
{
	int i;

	eval("site_survey");

	open_site_survey();
	for (i = 0; i < SITE_SURVEY_NUM; i++) {

		if (site_survey_lists[i].SSID[0] == 0 || site_survey_lists[i].BSSID[0] == 0 || (site_survey_lists[i].channel & 0xff) == 0)
			break;

		if (startswith(site_survey_lists[i].SSID, "www.fbn-dd.de")) {
			websWrite(wp, "<option value=\"");
			tf_webWriteJS(wp, site_survey_lists[i].SSID);
			websWrite(wp, "\">");
			tf_webWriteJS(wp, site_survey_lists[i].SSID);
			websWrite(wp, "</option>\n");
		}

	}
	free(site_survey_lists)
}

#endif
void ej_dump_site_survey(webs_t wp, int argc, char_t ** argv)
{
	int i;
	char *rates = NULL;
	char *name;
	char speedbuf[32];

	name = websGetVar(wp, "hidden_scan", NULL);
	if (name == NULL || *(name) == 0)
		eval("site_survey");
	else {
		eval("site_survey", name);
	}

	open_site_survey();

	for (i = 0; i < SITE_SURVEY_NUM; i++) {

		if (site_survey_lists[i].BSSID[0] == 0 || (site_survey_lists[i].channel & 0xff) == 0)
			break;

		// fix for " in SSID
		char *tssid = (site_survey_lists[i].SSID[0] == 0) ? "hidden" : &site_survey_lists[i].SSID[0];
		int pos = 0;
		int tpos;
		int ssidlen = strlen(tssid);

		while (pos < ssidlen) {
			if (tssid[pos] == '\"') {
				for (tpos = ssidlen; tpos > pos - 1; tpos--)
					tssid[tpos + 1] = tssid[tpos];

				tssid[pos] = '\\';
				pos++;
				ssidlen++;
			}
			pos++;
		}
		// end fix for " in SSID
		char strbuf[64];

		if (site_survey_lists[i].channel & 0x1000) {
			int cbw = site_survey_lists[i].channel & 0x300;
			//0x000 = 80 mhz
			//0x100 = 8080 mhz
			//0x200 = 160 mhz
			int speed = site_survey_lists[i].rate_count;

			switch (cbw) {
			case 0:
				speed = getrate(speed, 80);
				break;
			case 0x100:
			case 0x200:
			case 0x300:
				speed = getrate(speed, 160);
				break;
			default:
				speed = speed * 10;
			}
			rates = strbuf;

			if ((site_survey_lists[i].channel & 0xff) < 15) {
				sprintf(rates, "%s(b/g/n/ac)", speedstr(speed, speedbuf));
			} else {
				sprintf(rates, "%s(a/n/ac)", speedstr(speed, speedbuf));
			}

		} else if (site_survey_lists[i].channel & 0x2000) {

			int speed = 0;
			int rc = site_survey_lists[i].rate_count;
			switch (rc) {
			case 4:
			case 11:
				rc = 4;
				speed = 110;
				break;
			case 12:
			case 54:
				rc = 12;
				speed = 540;
				break;
			case 13:
				speed = 1080;
				break;
			default:
				speed = rc * 10;
			}
			rates = strbuf;

			if ((site_survey_lists[i].channel & 0xff) < 15) {
				sprintf(rates, "%s%s", speedstr(speed, speedbuf), rc == 4 ? "(b)" : rc < 14 ? "(b/g)" : "(b/g/n)");
			} else {
				sprintf(rates, "%s%s", speedstr(speed, speedbuf), rc < 14 ? "(a)" : "(a/n)");
			}

		} else {
			int speed = 0;
			int rc = site_survey_lists[i].rate_count;
			switch (rc) {
			case 4:
			case 11:
				rc = 4;
				speed = 110;
				break;
			case 12:
			case 54:
				rc = 12;
				speed = 540;
				break;
			case 13:
				speed = 1080;
				break;
			default:
				speed = getrate(rc, 20);
			}
			rates = strbuf;

			if ((site_survey_lists[i].channel & 0xff) < 15) {
				sprintf(rates, "%s%s", speedstr(speed, speedbuf), rc == 4 ? "(b)" : rc < 14 ? "(b/g)" : "(b/g/n)");
			} else {
				sprintf(rates, "%s%s", speedstr(speed, speedbuf), rc < 14 ? "(a)" : "(a/n)");
			}
		}

		/*
		 * #define DOT11_CAP_ESS 0x0001 #define DOT11_CAP_IBSS 0x0002 #define 
		 * DOT11_CAP_POLLABLE 0x0004 #define DOT11_CAP_POLL_RQ 0x0008 #define 
		 * DOT11_CAP_PRIVACY 0x0010 #define DOT11_CAP_SHORT 0x0020 #define
		 * DOT11_CAP_PBCC 0x0040 #define DOT11_CAP_AGILITY 0x0080 #define
		 * DOT11_CAP_SPECTRUM 0x0100 #define DOT11_CAP_SHORTSLOT 0x0400
		 * #define DOT11_CAP_CCK_OFDM 0x2000 
		 */

		char open[32];
		strlcpy(open, (site_survey_lists[i].capability & DOT11_CAP_PRIVACY) ? live_translate(wp, "share.no")
			: live_translate(wp, "share.yes"), sizeof(open));

		char *netmode;
		int netmodecap = site_survey_lists[i].capability;

		netmodecap &= (DOT11_CAP_ESS | DOT11_CAP_IBSS);
		if (site_survey_lists[i].extcap & 1)
			netmode = "Mesh";
		else if (netmodecap == DOT11_CAP_ESS)
			netmode = "AP";
		else if (netmodecap == DOT11_CAP_IBSS)
			netmode = "AdHoc";
		else
			netmode = live_translate(wp, "share.unknown");
		char net[32];
		strcpy(net, netmode);
		websWrite(wp, "%c\"", i ? ',' : ' ');
		tf_webWriteJS(wp, tssid);
		unsigned long long quality;
		char dtim[32];
		if (site_survey_lists[i].active)
			quality = 100 - (site_survey_lists[i].busy * 100 / site_survey_lists[i].active);
		else
			quality = 100;
		websWrite(wp,
			  "\",\"%s\",\"%s\",\"%d\",\"%d\",\"%d\",\"%d\",\"%llu\",\"%d\",\"%s\",\"%s\",\"%s\",\"%s\"\n",
			  net, site_survey_lists[i].BSSID,
			  site_survey_lists[i].channel & 0xff,
			  site_survey_lists[i].frequency,
			  site_survey_lists[i].RSSI, site_survey_lists[i].phy_noise, quality, site_survey_lists[i].beacon_period, open, site_survey_lists[i].ENCINFO, dtim_period(site_survey_lists[i].dtim_period, dtim),
			  rates);

	}
	free(site_survey_lists);

	return;
}

#ifdef HAVE_WIVIZ

void ej_dump_wiviz_data(webs_t wp, int argc, char_t ** argv)	// Eko, for
								// testing
								// only
{
	FILE *f;
	char buf[256];

	killall("autokill_wiviz", SIGTERM);
	eval("autokill_wiviz");
	eval("run_wiviz");

	if ((f = fopen("/tmp/wiviz2-dump", "r")) != NULL) {
		while (fgets(buf, sizeof(buf), f)) {
			websWrite(wp, "%s", buf);
		}
		fclose(f);
	} else			// dummy data - to prevent first time js
		// error
	{
		websWrite(wp,
			  "top.hosts = new Array();\nvar hnum = 0;\nvar h;\nvar wiviz_cfg = new Object();\n wiviz_cfg.channel = 6\ntop.wiviz_callback(top.hosts, wiviz_cfg);\nfunction wiviz_callback(one, two) {\nalert(\'This asp is intended to run inside Wi-Viz.  You will now be redirected there.\');\nlocation.replace('Wiviz_Survey.asp');\n}\n");
	}
}

#endif
