/*
 * dhcp.c
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <ctype.h>

#include <broadcom.h>

#define DHCP_MAX_COUNT 254
#define EXPIRES_NEVER 0xFFFFFFFF	/* static lease */

static int landhcp(void)
{
	if (!getWET())
		if (nvram_match("lan_proto", "dhcp")
		    && nvram_matchi("dhcpfwd_enable", 0))
			return 1;
	return 0;
}

static int hasmdhcp(void)
{
	if (nvram_exists("mdhcpd_count")) {
		int mdhcpcount = nvram_geti("mdhcpd_count");
		return mdhcpcount > 0 ? 1 : 0;
	}
	return 0;
}

void ej_dhcpenabled(webs_t wp, int argc, char_t ** argv)
{
	if (landhcp() || hasmdhcp())
		websWrite(wp, argv[0]);
	else
		websWrite(wp, argv[1]);
}

static char *dhcp_reltime(char *buf, time_t t, int sub)
{
	if (sub) {
		time_t now = time(NULL);
		t -= now;
	}
	if (t < 0)
		t = 0;
	int days = t / 86400;
	int min = t / 60;

	sprintf(buf, "%d day%s %02d:%02d:%02d", days, ((days == 1) ? "" : "s"), ((min / 60) % 24), (min % 60), (int)(t % 60));
	return buf;
}

/*
 * dump in array: hostname,mac,ip,expires read leases from leasefile as:
 * expires mac ip hostname 
 */
void ej_dumpleases(webs_t wp, int argc, char_t ** argv)
{
	FILE *fp;
	unsigned long expires;
	int i;
	int count = 0;
	int macmask;
	char ip[32];
	char hostname[256];
	char buf[512];
	char *p;
	char *buff;
	struct lease_t lease;
	struct in_addr addr;
	char *ipaddr, mac[32] = "", expires_time[256] = "";

	cprintf("entry dumpleases\n");
	if (argc < 1) {
		return;
	}
	cprintf("entry dumpleases:%d\n", __LINE__);
	macmask = atoi(argv[0]);
	cprintf("entry dumpleases:%d\n", __LINE__);
	if (landhcp() || hasmdhcp()) {
		cprintf("entry dumpleases:%d\n", __LINE__);

		if (nvram_invmatchi("dhcpd_usenvram", 1)) {
			/*
			 * Parse leases file 
			 */
			cprintf("open default leases\n");
			if (nvram_matchi("dhcpd_usejffs", 1)) {
				fp = fopen("/jffs/dnsmasq.leases", "r");
				if (!fp) {
					fp = fopen("/tmp/dnsmasq.leases", "r");
				}
				if (!fp) {
					return;
				}
			} else {
				fp = fopen("/tmp/dnsmasq.leases", "r");
				if (!fp) {
					fp = fopen("/jffs/dnsmasq.leases", "r");
				}
			}
			if (!fp) {
				return;
			}
			cprintf("entry dumpleases:%d\n", __LINE__);

			if (fp) {
				while (fgets(buf, sizeof(buf), fp)) {
					cprintf("entry dumpleases:%d\n", __LINE__);
					if (sscanf(buf, "%lu %17s %15s %255s", &expires, mac, ip, hostname) != 4)
						continue;
					p = mac;
					cprintf("entry dumpleases:%d\n", __LINE__);
					while ((*p = toupper(*p)) != 0)
						++p;
					cprintf("entry dumpleases:%d\n", __LINE__);
					if ((p = strrchr(ip, '.')) == NULL)
						continue;
					cprintf("entry dumpleases:%d\n", __LINE__);
					if (nvram_matchi("maskmac", 1)
					    && macmask) {
						mac[0] = 'x';
						mac[1] = 'x';
						mac[3] = 'x';
						mac[4] = 'x';
						mac[6] = 'x';
						mac[7] = 'x';
						mac[9] = 'x';
						mac[10] = 'x';
					}
					cprintf("entry dumpleases:%d\n", __LINE__);
					websWrite(wp,
						  "%c'%s','%s','%s','%s','%s'", (count ? ',' : ' '), (hostname[0] ? hostname : live_translate(wp, "share.unknown")), ip, mac,
						  ((expires == 0) ? live_translate(wp, "share.sttic")
						   : dhcp_reltime(buf, expires, 1)), p + 1, 1);
					cprintf("entry dumpleases:%d\n", __LINE__);
					++count;
				}
				fclose(fp);
				cprintf("entry dumpleases:%d\n", __LINE__);
			}
		} else {
			for (i = 0; i < DHCP_MAX_COUNT; ++i) {
				sprintf(buf, "dnsmasq_lease_%d.%d.%d.%d",
					get_single_ip(nvram_safe_get("lan_ipaddr"), 0), get_single_ip(nvram_safe_get("lan_ipaddr"), 1), get_single_ip(nvram_safe_get("lan_ipaddr"), 2), i);

				cprintf("entry dumpleases:%d\n", __LINE__);
				buff = nvram_safe_get(buf);
				cprintf("entry dumpleases:%d\n", __LINE__);
				if (sscanf(buff, "%lu %17s %15s %255s", &expires, mac, ip, hostname) != 4)
					continue;
				cprintf("entry dumpleases:%d\n", __LINE__);
				p = mac;
				while ((*p = toupper(*p)) != 0)
					++p;
				cprintf("entry dumpleases:%d\n", __LINE__);
				if ((p = strrchr(ip, '.')) == NULL)
					continue;
				cprintf("entry dumpleases:%d\n", __LINE__);
				if (nvram_matchi("maskmac", 1) && macmask) {
					mac[0] = 'x';
					mac[1] = 'x';
					mac[3] = 'x';
					mac[4] = 'x';
					mac[6] = 'x';
					mac[7] = 'x';
					mac[9] = 'x';
					mac[10] = 'x';
				}
				cprintf("entry dumpleases:%d\n", __LINE__);
				websWrite(wp, "%c'%s','%s','%s','%s','%s'",
					  (count ? ',' : ' '), (hostname[0] ? hostname : live_translate(wp, "share.unknown")), ip, mac,
					  ((expires == 0) ? live_translate(wp, "share.sttic") : dhcp_reltime(buf, expires, 1)), p + 1);
				cprintf("entry dumpleases:%d\n", __LINE__);
				++count;
			}
		}
	} else {
		cprintf("entry dumpleases:%d\n", __LINE__);

		cprintf("entry dumpleases:%d\n", __LINE__);
		/*
		 * Write out leases file from udhcpd 
		 */
		killall("udhcpd", SIGUSR1);
		cprintf("entry dumpleases:%d\n", __LINE__);

		/*
		 * Parse leases file 
		 */
		if (!(fp = fopen("/tmp/udhcpd.leases", "r")))
			fp = fopen("/jffs/udhcpd.leases", "r");
		cprintf("entry dumpleases:%d\n", __LINE__);

		if (fp) {
			cprintf("entry dumpleases:%d\n", __LINE__);
			while (fread(&lease, sizeof(lease), 1, fp)) {
				cprintf("entry dumpleases:%d\n", __LINE__);
				strcpy(mac, "");
				cprintf("entry dumpleases:%d\n", __LINE__);

				for (i = 0; i < 6; i++) {
					sprintf(mac + strlen(mac), "%02X", lease.chaddr[i]);
					if (i != 5)
						sprintf(mac + strlen(mac), ":");
				}
				mac[17] = '\0';
				if (!strcmp(mac, "00:00:00:00:00:00"))
					continue;
				cprintf("entry dumpleases:%d\n", __LINE__);
				if (nvram_matchi("maskmac", 1) && macmask) {

					mac[0] = 'x';
					mac[1] = 'x';
					mac[3] = 'x';
					mac[4] = 'x';
					mac[6] = 'x';
					mac[7] = 'x';
					mac[9] = 'x';
					mac[10] = 'x';
				}
				cprintf("entry dumpleases:%d\n", __LINE__);
				addr.s_addr = lease.yiaddr;

				char client[32];
				ipaddr = (char *)inet_ntop(AF_INET, &addr, client, 16);

				expires = ntohl(lease.expires);

				cprintf("entry dumpleases:%d\n", __LINE__);
				strcpy(expires_time, "");
				cprintf("entry dumpleases:%d\n", __LINE__);
				if (!expires) {
					continue;
					strcpy(expires_time, live_translate(wp, "share.expired"));
				} else if (expires == (long)EXPIRES_NEVER) {
					strcpy(expires_time, live_translate(wp, "share.never"));
				} else {
					if (expires > 86400)	// 60 * 60 * 24
					{
						sprintf(expires_time + strlen(expires_time), "%ld days ", expires / 86400);	// 60 
						// * 
						// 60 
						// * 
						// 24
						expires %= 86400;	// 60 * 60 * 24
					}
					if (expires > 3600)	// 60 * 60 
					{
						sprintf(expires_time + strlen(expires_time), "%02ld:", expires / (60 * 60));	// hours
						expires %= 3600;	// 60 * 60
					} else {
						sprintf(expires_time + strlen(expires_time), "00:");	// no 
						// hours
					}
					if (expires > 60) {
						sprintf(expires_time + strlen(expires_time), "%02ld:", expires / 60);	// minutes
						expires %= 60;
					} else {
						sprintf(expires_time + strlen(expires_time), "00:");	// no 
						// minutes
					}

					sprintf(expires_time + strlen(expires_time), "%02ld:", expires);	// seconds

					expires_time[strlen(expires_time) - 1] = '\0';
				}
				cprintf("entry dumpleases:%d\n", __LINE__);
				websWrite(wp, "%c\"%s\",\"%s\",\"%s\",\"%s\",\"%d\"", count ? ',' : ' ', !*lease.hostname ? "&nbsp;" : lease.hostname, ipaddr, mac, expires_time, get_single_ip(ipaddr, 3));
				cprintf("entry dumpleases:%d\n", __LINE__);
				count++;
			}
			cprintf("entry dumpleases:%d\n", __LINE__);
			fclose(fp);
		}
	}
	return;
}

void ej_dhcp_remaining_time(webs_t wp, int argc, char_t ** argv)
{
	// tofu12

	if (nvram_invmatch("wan_proto", "dhcp") && nvram_invmatch("wan_proto", "dhcp_auth"))
		return;

	long exp;
	char buf[128];
	struct sysinfo si;
	long n;

	exp = 0;
	if (file_to_buf("/tmp/udhcpc.expires", buf, sizeof(buf))) {
		n = atol(buf);
		if (n > 0) {
			sysinfo(&si);
			exp = n - si.uptime;
		}
	}
	websWrite(wp, dhcp_reltime(buf, exp, 0));

	return;
}
