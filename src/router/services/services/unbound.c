/*
 * unbound.c
 *
 * Copyright (C) 2015 Sebastian Gottschall <gottschall@dd-wrt.com>
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
#ifdef HAVE_UNBOUND
#include <stdlib.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <utils.h>
#include <syslog.h>
#include <signal.h>
#include <services.h>

static void unbound_config(void)
{
#ifdef _SC_NPROCESSORS_ONLN
	int cpucount = sysconf(_SC_NPROCESSORS_ONLN);
#else
	int cpucount = 1;
#endif
	int port = 7053;
	if (!nvram_matchi("dns_dnsmasq", 1) && !nvram_matchi("smartdns", 1))
		port = 53;
	FILE *fp = fopen("/tmp/unbound.conf", "wb");
	fprintf(fp, "server:\n"	//
		"verbosity: 1\n");

	fprintf(fp, "interface: 0.0.0.0@%d\n", port);
	if (nvram_match("ipv6_enable", "1"))
		fprintf(fp, "interface: ::0@%d\n", port);
	fprintf(fp, "outgoing-num-tcp: 10\n"	//
		"incoming-num-tcp: 10\n"	//
		"msg-buffer-size: 8192\n"	//
		"msg-cache-size: 1m\n"	//
		"num-queries-per-thread: 30\n"	//
		"rrset-cache-size: 2m\n"	//
		"infra-cache-numhosts: 200\n"	//
		"username: \"\"\n"	//
		"pidfile: \"/var/run/unbound.pid\"\n"	//
		"root-hints: \"/etc/unbound/named.cache\"\n"	//
		"target-fetch-policy: \"2 1 0 0 0 0\"\n"	//
		"harden-short-bufsize: yes\n"	//
		"harden-large-queries: yes\n"	//
		"auto-trust-anchor-file: \"/etc/unbound/root.key\"\n"	//
		"key-cache-size: 100k\n"	//
		"neg-cache-size: 10k\n");	//
	fprintf(fp, "num-threads: %d\n", cpucount);
	if (cpucount > 1)
		fprintf(fp, "so-reuseport: no\n");
	else
		fprintf(fp, "so-reuseport: yes\n");
	int slabs = 2;
	while (1) {
		if (slabs >= cpucount)
			break;
		slabs <<= 1;
	}
	fprintf(fp, "msg-cache-slabs: %d\n", slabs);
	fprintf(fp, "rrset-cache-slabs: %d\n", slabs);
	fprintf(fp, "infra-cache-slabs: %d\n", slabs);
	fprintf(fp, "key-cache-slabs: %d\n", slabs);
	int range = 1024 / cpucount;
	if (range > 50)
		range -= 50;
	fprintf(fp, "outgoing-range: %d\n", range);

	char *lan_ip = nvram_safe_get("lan_ipaddr");
	char *lan_mask = nvram_safe_get("lan_netmask");
	char *prefix;
	int do_6to4 = 0;
	if (nvram_match("ipv6_typ", "ipv6to4")) {
		do_6to4 = 1;
	}

	prefix = do_6to4 ? "0:0:0:1::" : nvram_safe_get("ipv6_prefix");

	if (nvram_matchi("ipv6_enable", 1) && nvram_matchi("radvd_enable", 1) && *prefix) {
		fprintf(fp, "access-control: %s/64 allow\n", prefix);
	}
	fprintf(fp, "access-control: 127.0.0.0/8 allow\n");
	fprintf(fp, "access-control: %s/%d allow\n", lan_ip, getmask(lan_mask));
	char vifs[256];
	getIfLists(vifs, 256);
	char var[256], *wordlist, *next;
	foreach(var, vifs, next) {
		if (strcmp(get_wan_face(), var)
		    && strcmp(nvram_safe_get("lan_ifname"), var)) {
			char *ipaddr = nvram_nget("%s_ipaddr", var);
			char *netmask = nvram_nget("%s_netmask", var);
			if (*ipaddr && strcmp(ipaddr, "0.0.0.0"))
				fprintf(fp, "access-control: %s/%d allow\n", ipaddr, getmask(netmask));

		}
	}

	FILE *in = fopen("/etc/hosts", "rb");
	char ip[32];
	char name[128];
	while (fscanf(in, "%s %s", &ip[0], &name[0]) == 2) {
		fprintf(fp, "local-data: \"%s A %s\"\n", name, ip);
	}
	fclose(in);
	fprintf(fp, "python:\n");
	fprintf(fp, "remote-control:\n");
	fclose(fp);

	int leasenum = nvram_geti("static_leasenum");

	if (leasenum > 0) {
		char *lease = nvram_safe_get("static_leases");
		char *leasebuf = (char *)malloc(strlen(lease) + 1);
		char *cp = leasebuf;

		strcpy(leasebuf, lease);
		int i;
		for (i = 0; i < leasenum; i++) {
			char *mac = strsep(&leasebuf, "=");
			char *host = strsep(&leasebuf, "=");
			char *ip = strsep(&leasebuf, "=");
			char *time = strsep(&leasebuf, " ");

			if (mac == NULL || host == NULL || ip == NULL)
				continue;

			addHost(host, ip, 1);
		}
		free(cp);
	}
	system("cp -R /etc/unbound /tmp/etc");

}

void start_unbound(void)
{
	FILE *fp = NULL;

	if (nvram_matchi("recursive_dns", 1)) {
		update_timezone();
		unbound_config();
		eval("unbound", "-c", getdefaultconfig("unbound.conf"));

		dd_loginfo("unbound", "recursive dns resolver daemon successfully started\n");
	}
	return;
}

void stop_unbound(void)
{
	stop_process("unbound", "daemon");
	return;
}
#endif
