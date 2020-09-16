/*
 * udhcpd.c
 *
 * Copyright (C) 2007 Sebastian Gottschall <gottschall@dd-wrt.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License.
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
#ifdef HAVE_UDHCPD
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <utils.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <services.h>

extern int usejffs;

void stop_udhcpd(void);
extern void addHost(char *host, char *ip, int withdomain);

static int adjust_dhcp_range(void)
{
	struct in_addr ipaddr, netaddr, netmask;

	char *lan_ipaddr = nvram_safe_get("lan_ipaddr");
	char *lan_netmask = nvram_safe_get("lan_netmask");
	char *dhcp_num = nvram_safe_get("dhcp_num");
	char *dhcp_start = nvram_safe_get("dhcp_start");

	int legal_start_ip, legal_end_ip, legal_total_ip, dhcp_start_ip;
	int set_dhcp_start_ip = 0, set_dhcp_num = 0;
	int adjust_ip = 0, adjust_num = 0;

	inet_aton(lan_ipaddr, &netaddr);
	inet_aton(lan_netmask, &netmask);
	inet_aton(dhcp_start, &ipaddr);

	legal_total_ip = 254 - get_single_ip(lan_netmask, 3);
	legal_start_ip = (get_single_ip(lan_ipaddr, 3) & get_single_ip(lan_netmask, 3)) + 1;
	legal_end_ip = legal_start_ip + legal_total_ip - 1;
	dhcp_start_ip = atoi(dhcp_start);

	cprintf("legal_total_ip=[%d] legal_start_ip=[%d] legal_end_ip=[%d] dhcp_start_ip=[%d]\n", legal_total_ip, legal_start_ip, legal_end_ip, dhcp_start_ip);

	if ((dhcp_start_ip > legal_end_ip)
	    || (dhcp_start_ip < legal_start_ip)) {
		cprintf("Illegal DHCP Start IP: We need to adjust DHCP Start ip.\n");
		set_dhcp_start_ip = legal_start_ip;
		adjust_ip = 1;
		if (atoi(dhcp_num) > legal_total_ip) {
			cprintf("DHCP num is exceed, we need to adjust.");
			set_dhcp_num = legal_total_ip;
			adjust_num = 1;
		}
	} else {
		cprintf("Legal DHCP Start IP: We need to check DHCP num.\n");
		if ((atoi(dhcp_num) + dhcp_start_ip) > legal_end_ip) {
			cprintf("DHCP num is exceed, we need to adjust.\n");
			set_dhcp_num = legal_end_ip - dhcp_start_ip + 1;
			adjust_num = 1;
		}
	}

	if (adjust_ip) {
		char ip[20];

		cprintf("set_dhcp_start_ip=[%d]\n", set_dhcp_start_ip);
		snprintf(ip, sizeof(ip), "%d", set_dhcp_start_ip);
		nvram_set("dhcp_start", ip);
	}
	if (adjust_num) {
		char num[5];

		cprintf("set_dhcp_num=[%d]\n", set_dhcp_num);
		snprintf(num, sizeof(num), "%d", set_dhcp_num);
		nvram_set("dhcp_num", num);
	}

	return 1;
}

void start_udhcpd(void)
{
	FILE *fp = NULL;
	struct dns_lists *dns_list = NULL;
	int i = 0;

	if (nvram_matchi("dhcpfwd_enable", 1)) {
		return;
	}
#ifndef HAVE_RB500
#ifndef HAVE_XSCALE
	if (getWET())		// dont 
		// start 
		// any 
		// dhcp 
		// services 
		// in 
		// bridge 
		// mode
	{
		nvram_set("lan_proto", "static");
		return;
	}
#endif
#endif

	if (nvram_invmatch("lan_proto", "dhcp")
	    || nvram_matchi("dhcp_dnsmasq", 1)) {
		stop_udhcpd();
		return;
	}

	/*
	 * Automatically adjust DHCP Start IP and num when LAN IP or netmask is
	 * changed 
	 */
	adjust_dhcp_range();

	cprintf("%s %d.%d.%d.%s %s %s\n",
		nvram_safe_get("lan_ifname"),
		get_single_ip(nvram_safe_get("lan_ipaddr"), 0),
		get_single_ip(nvram_safe_get("lan_ipaddr"), 1), get_single_ip(nvram_safe_get("lan_ipaddr"), 2), nvram_safe_get("dhcp_start"), nvram_safe_get("dhcp_end"), nvram_safe_get("lan_lease"));

	/*
	 * Touch leases file 
	 */

	usejffs = 0;

	if (nvram_matchi("dhcpd_usejffs", 1)) {
		if (!(fp = fopen("/jffs/udhcpd.leases", "a"))) {
			usejffs = 0;
		} else {
			usejffs = 1;
		}
	}
	if (!usejffs) {
		if (!(fp = fopen("/tmp/udhcpd.leases", "a"))) {
			perror("/tmp/udhcpd.leases");
			return;
		}
	}
	fclose(fp);

	/*
	 * Write configuration file based on current information 
	 */
	if (!(fp = fopen("/tmp/udhcpd.conf", "w"))) {
		perror("/tmp/udhcpd.conf");
		return;
	}
	fprintf(fp, "pidfile /var/run/udhcpd.pid\n");
	fprintf(fp, "start %d.%d.%d.%s\n", get_single_ip(nvram_safe_get("lan_ipaddr"), 0), get_single_ip(nvram_safe_get("lan_ipaddr"), 1), get_single_ip(nvram_safe_get("lan_ipaddr"), 2), nvram_safe_get("dhcp_start"));
	fprintf(fp, "end %d.%d.%d.%d\n",
		get_single_ip(nvram_safe_get("lan_ipaddr"), 0), get_single_ip(nvram_safe_get("lan_ipaddr"), 1), get_single_ip(nvram_safe_get("lan_ipaddr"), 2), nvram_geti("dhcp_start") + nvram_geti("dhcp_num") - 1);
	int dhcp_max = nvram_geti("dhcp_num") + nvram_geti("static_leasenum");
	fprintf(fp, "max_leases %d\n", dhcp_max);
	fprintf(fp, "interface %s\n", nvram_safe_get("lan_ifname"));
	fprintf(fp, "remaining yes\n");
	fprintf(fp, "auto_time 30\n");	// N seconds to update lease table
	if (usejffs)
		fprintf(fp, "lease_file /jffs/udhcpd.leases\n");
	else
		fprintf(fp, "lease_file /tmp/udhcpd.leases\n");
	fprintf(fp, "statics_file /tmp/udhcpd.statics\n");

	if (nvram_invmatch("lan_netmask", "")
	    && nvram_invmatch("lan_netmask", "0.0.0.0"))
		fprintf(fp, "option subnet %s\n", nvram_safe_get("lan_netmask"));

	if (nvram_invmatch("dhcp_gateway", "")
	    && nvram_invmatch("dhcp_gateway", "0.0.0.0"))
		fprintf(fp, "option router %s\n", nvram_safe_get("dhcp_gateway"));
	else if (nvram_invmatch("lan_ipaddr", "")
		 && nvram_invmatch("lan_ipaddr", "0.0.0.0"))
		fprintf(fp, "option router %s\n", nvram_safe_get("lan_ipaddr"));

	if (nvram_invmatch("wan_wins", "")
	    && nvram_invmatch("wan_wins", "0.0.0.0"))
		fprintf(fp, "option wins %s\n", nvram_safe_get("wan_wins"));

	// Wolf add - keep lease within reasonable timeframe
	if (nvram_geti("dhcp_lease") < 10) {
		nvram_seti("dhcp_lease", 10);
		nvram_commit();
	}
	if (nvram_geti("dhcp_lease") > 5760) {
		nvram_seti("dhcp_lease", 5760);
		nvram_commit();
	}

	fprintf(fp, "option lease %d\n", nvram_geti("dhcp_lease") ? nvram_geti("dhcp_lease") * 60 : 86400);

	dns_list = get_dns_list(0);

	if (!dns_list || dns_list->num_servers == 0) {
		if (nvram_invmatch("lan_ipaddr", ""))
			fprintf(fp, "option dns %s\n", nvram_safe_get("lan_ipaddr"));
	} else if (nvram_matchi("local_dns", 1)) {
		if (nvram_invmatch("lan_ipaddr", "") || dns_list) {
			fprintf(fp, "option dns");

			if (nvram_invmatch("lan_ipaddr", ""))
				fprintf(fp, " %s", nvram_safe_get("lan_ipaddr"));
			if (dns_list) {
				for (i = 0; i < dns_list->num_servers; i++)
					fprintf(fp, " %s", dns_list->dns_server[i].ip);
			}
			fprintf(fp, "\n");
		}
	} else {
		if (dns_list && dns_list->num_servers > 0) {
			fprintf(fp, "option dns");
			for (i = 0; i < dns_list->num_servers; i++)
				fprintf(fp, " %s", dns_list->dns_server[i].ip);
			fprintf(fp, "\n");
		}
	}

	free_dns_list(dns_list);

	/*
	 * DHCP Domain 
	 */
	if (nvram_match("dhcp_domain", "wan")) {
		if (nvram_invmatch("wan_domain", ""))
			fprintf(fp, "option domain %s\n", nvram_safe_get("wan_domain"));
		else if (nvram_invmatch("wan_get_domain", ""))
			fprintf(fp, "option domain %s\n", nvram_safe_get("wan_get_domain"));
	} else {
		if (nvram_invmatch("lan_domain", ""))
			fprintf(fp, "option domain %s\n", nvram_safe_get("lan_domain"));
	}

	fwritenvram("dhcpd_options", fp);
	fclose(fp);

	/*
	 * Sveasoft addition - create static leases file 
	 */
	// Static for router
	if (!(fp = fopen("/tmp/udhcpd.statics", "w"))) {
		perror("/tmp/udhcpd.statics");
		return;
	}
	char mac[18];
	getLANMac(mac);
	if (!*mac)
		strcpy(mac, nvram_safe_get("et0macaddr_safe"));

	if (nvram_matchi("local_dns", 1)) {
		if (nvram_matchi("port_swap", 1))
			fprintf(fp, "%s %s %s\n", nvram_safe_get("lan_ipaddr"), nvram_safe_get("et1macaddr"), nvram_safe_get("router_name"));
		else
			fprintf(fp, "%s %s %s\n", nvram_safe_get("lan_ipaddr"), mac, nvram_safe_get("router_name"));
	}
	int leasenum = nvram_geti("static_leasenum");

	if (leasenum > 0) {
		char *lease = nvram_safe_get("static_leases");
		char *leasebuf = (char *)malloc(strlen(lease) + 1);
		char *cp = leasebuf;

		strcpy(leasebuf, lease);
		for (i = 0; i < leasenum; i++) {
			char *mac = strsep(&leasebuf, "=");
			char *host = strsep(&leasebuf, "=");
			char *ip = strsep(&leasebuf, "=");
			char *time = strsep(&leasebuf, " ");

			if (mac == NULL || host == NULL || ip == NULL)
				continue;
			fprintf(fp, "%s %s %s\n", ip, mac, host);
			addHost(host, ip, 1);
		}
		free(cp);
	}

	fclose(fp);
	/*
	 * end Sveasoft addition 
	 */

	dns_to_resolv();

	eval("udhcpd", "/tmp/udhcpd.conf");
	dd_loginfo("udhcpd", "udhcp daemon successfully started\n");

	cprintf("done\n");
	return;
}

void stop_udhcpd(void)
{
	stop_process("udhcpd", "daemon");
	return;
}

#endif
