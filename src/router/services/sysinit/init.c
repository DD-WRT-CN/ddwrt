/*
 * init.c
 *
 * Copyright (C) 2018 Sebastian Gottschall <gottschall@dd-wrt.com>
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

#include <bcmnvram.h>
#include <shutils.h>
#include <utils.h>
#include <wlutils.h>
#include <services.h>
#include <cyutils.h>
#include <revision.h>
#include <sys/stat.h>
#include <utils.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

extern int file2nvram(char *filename, char *varname);
extern int nvram2file(char *varname, char *filename);

#define start_single_service() eval("start_single_service");
#define stop_services() eval("stopservices");
#define start_services() eval("startservices");
static void set_systunes(void)
{
	writeprocsys("vm/dirty_writeback_centisecs", "1500");

}

static void set_tcp_params(void)
{
	eval("/etc/preinit");	// sets default values for ip_conntrack
	start_conntrack();

	FILE *fp = fopen("/proc/sys/net/ipv4/tcp_available_congestion_control", "rb");
	if (fp == NULL) {
		char *vegas = "1";
		char *westwood = "0";
		char *bic = "0";
		if (nvram_match("tcp_congestion_control", "westwood")) {
			westwood = "1";
			vegas = "0";
		}
		if (nvram_match("tcp_congestion_control", "bic")) {
			bic = "1";
			vegas = "0";
		}
		writeprocsysnet("ipv4/tcp_westwood", westwood);
		writeprocsysnet("ipv4/tcp_vegas_cong_avoid", vegas);
		writeprocsysnet("ipv4/tcp_bic", bic);
		writeprocsysnet("ipv4/tcp_vegas_alpha", "3");
		writeprocsysnet("ipv4/tcp_vegas_beta", "3");
	} else {
		fclose(fp);
		writeprocsysnet("ipv4/tcp_congestion_control", nvram_default_get("tcp_congestion_control", "westwood"));
	}
}

#define getRouterName() nvram_exists(NVROUTER_ALT)?nvram_safe_get(NVROUTER_ALT):nvram_safe_get(NVROUTER)

void start_post_sysinit(void)
{
	int brand = getRouterBrand();
	nvram_set("ntp_done", "0");
	led_control(LED_POWER, LED_ON);
	led_control(LED_SES, LED_OFF);
	led_control(LED_SES2, LED_OFF);
	led_control(LED_DIAG, LED_OFF);
	led_control(LED_BRIDGE, LED_OFF);
	led_control(LED_WLAN0, LED_OFF);
	led_control(LED_WLAN1, LED_OFF);
	led_control(LED_WLAN2, LED_OFF);
	led_control(LED_CONNECTED, LED_OFF);
	//led_control(LED_USB, LED_OFF); //these are switched in start_drivers
	//led_control(LED_USB1, LED_OFF);
	//led_control(USB_POWER, LED_OFF);
	//led_control(USB_POWER1, LED_OFF);
	led_control(LED_DMZ, LED_OFF);
	led_control(LED_VPN, LED_OFF);
	led_control(LED_SEC0, LED_OFF);
	led_control(LED_SEC1, LED_OFF);

	start_nvram();

	/* 
	 * Restore defaults if necessary 
	 */

#ifdef HAVE_SKYTEL
	nvram_set("vlan0ports", "0 1 2 3 4 5*");
	nvram_set("vlan1ports", "");
#else

	if (brand == ROUTER_WRT600N || brand == ROUTER_WRT610N) {
		nvram_set("vlan2hwname", "et0");
	}
#endif
	start_restore_defaults();

	set_ip_forward('1');
	set_tcp_params();
	set_systunes();
#ifdef HAVE_JFFS2
	start_jffs2();
#endif
#ifdef HAVE_MMC
	start_mmc();
#endif
#ifdef HAVE_SPEEDCHECKER
	start_speedchecker_init();
#endif

	start_mkfiles();
	char *hostname;

	/* 
	 * set hostname to wan_hostname or router_name 
	 */
	if (*(nvram_safe_get("wan_hostname")))
		hostname = nvram_safe_get("wan_hostname");
	else if (*(nvram_safe_get("router_name")))
		hostname = nvram_safe_get("router_name");
	else
		hostname = "dd-wrt";

	sethostname(hostname, strlen(hostname));
	stop_httpd();

	// create loginprompt
	FILE *fp = fopen("/tmp/loginprompt", "wb");

#ifndef HAVE_MAKSAT
#ifndef HAVE_TRIMAX
#ifndef HAVE_WIKINGS
#ifndef HAVE_ESPOD
#ifndef HAVE_IPR
#ifndef HAVE_NEXTMEDIA
#ifndef HAVE_ERC
#ifndef HAVE_CORENET
#ifdef HAVE_TMK
	fprintf(fp, "KMT-WAS %s (c) 2020 KMT GmbH\nRelease: " BUILD_DATE " (SVN revision: %s)\nBoard: %s\n", DIST, SVN_REVISION, getRouterName());
#elif HAVE_SANSFIL
	fprintf(fp, "SANSFIL %s (c) 2020 NewMedia-NET GmbH\nRelease: " BUILD_DATE " (SVN revision: %s)\nBoard: %s\n", DIST, SVN_REVISION, getRouterName());
#elif HAVE_KORENRON
	fprintf(fp, "KORENRON %s (c) 2020 NewMedia-NET GmbH\nRelease: " BUILD_DATE " (SVN revision: %s)\nBoard: %s\n", DIST, SVN_REVISION, getRouterName());
#elif HAVE_TESTEM
	fprintf(fp, "TESTEM %s (c) 2020 NewMedia-NET GmbH\nRelease: " BUILD_DATE " (SVN revision: %s)\nBoard: %s\n", DIST, SVN_REVISION, getRouterName());
#elif HAVE_HOBBIT
	fprintf(fp, "HQ-NDS %s (c) 2020 NewMedia-NET GmbH\nRelease: " BUILD_DATE " (SVN revision: %s)\nBoard: %s\n", DIST, SVN_REVISION, getRouterName());
#elif HAVE_ONNET
#ifdef HAVE_ONNET_BLANK
	fprintf(fp, "Enterprise AP %s (c) 2020 NewMedia-NET GmbH\nRelease: " BUILD_DATE " (SVN revision: %s)\n", DIST, SVN_REVISION, getRouterName());
#elif HAVE_UNFY
	//fprintf(fp, "UNIFY %s (c) 2013 \nRelease: " BUILD_DATE " (SVN revision: %s)\n", DIST, SVN_REVISION);
	fprintf(fp, "Firmware %s (c) 2020 \nRelease: " BUILD_DATE " (SVN revision: %s)\nBoard: %s\n", DIST, SVN_REVISION, getRouterName());
#else
	fprintf(fp, "OTAi %s (c) 2020 NewMedia-NET GmbH\nRelease: " BUILD_DATE " (SVN revision: %s)\nBoard: %s\n", DIST, SVN_REVISION, getRouterName());
#endif
#elif HAVE_RAYTRONIK
	fprintf(fp, "RAYTRONIK %s (c) 2020 NewMedia-NET GmbH\nRelease: " BUILD_DATE " (SVN revision: %s)\nBoard: %s\n", DIST, SVN_REVISION, getRouterName());
#elif HAVE_ANTAIRA
	fprintf(fp, "Antaira %s\nRelease: " BUILD_DATE " (SVN revision: %s)\nBoard: %s\n", DIST, SVN_REVISION, getRouterName());
#elif HAVE_HDWIFI
	fprintf(fp, "HDWIFI %s (c) 2020 NewMedia-NET GmbH\nRelease: " BUILD_DATE " (SVN revision: %s)\nBoard: %s\n", DIST, SVN_REVISION, getRouterName());
#else
#ifdef DIST
	if (*(DIST))
		fprintf(fp, "DD-WRT v3.0-r%s %s (c) 2020 NewMedia-NET GmbH\nRelease: " BUILD_DATE "\nBoard: %s\n", SVN_REVISION, DIST, getRouterName());
	else
		fprintf(fp, "DD-WRT v3.0-r%s custom (c) 2020 NewMedia-NET GmbH\nRelease: " BUILD_DATE "\nBoard: %s\n", SVN_REVISION, getRouterName());
#else
	fprintf(fp, "DD-WRT v3.0-r%s custom (c) 2020 NewMedia-NET GmbH\nRelease: " BUILD_DATE "\nBoard: %s\n", SVN_REVISION, getRouterName());
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif

	fclose(fp);

#ifndef HAVE_MADWIFI
#ifdef HAVE_QTN
	nvram_seti("qtn_ready", 0);
#endif
#endif

}

void start_init_user(void)
{
	start_single_service();
#ifdef HAVE_CHILLI
	start_chilli();
#endif
#ifdef HAVE_WIFIDOG
	start_wifidog();
#endif
}

void start_init_restart(void)
{

#ifdef HAVE_OVERCLOCKING
	start_overclocking();
#endif
	cprintf("RESET NVRAM VARS\n");
	nvram_set("wl0_lazy_wds", nvram_safe_get("wl_lazy_wds"));

	cprintf("RESTART\n");
#ifndef HAVE_MADWIFI
	int cnt = get_wl_instances();
#ifdef HAVE_QTN
	cnt = 1;
	nvram_seti("qtn_ready", 0);
#endif
#endif

#if !defined(HAVE_MADWIFI) && !defined(HAVE_RT2880) && !defined(HAVE_RT61)
	int c;
	for (c = 0; c < cnt; c++) {
		wlconf_down(get_wl_instance_name(c));
		char *next;
		char var[80];
		char *vifs = nvram_nget("wl%d_vifs", c);

		foreach(var, vifs, next) {
			eval("ifconfig", var, "down");
		}

	}
#endif

}

void start_init_stop(void)
{

	lcdmessage("STOPPING SERVICES");
	cprintf("STOP\n");
	killall("udhcpc", SIGKILL);

	cprintf("STOP SERVICES\n");

	stop_services();
	stop_radio_timer();
#if !defined(HAVE_MADWIFI) && !defined(HAVE_RT2880) && !defined(HAVE_RT61)
	stop_nas();
#endif
	cprintf("STOP WAN\n");
	stop_ttraff();
	stop_wan();
#if !defined(HAVE_MADWIFI) && !defined(HAVE_RT2880) && !defined(HAVE_RT61)
	stop_wlconf();
#endif
	cprintf("STOP LAN\n");
#ifdef HAVE_MADWIFI
	stop_stabridge();
#endif
#ifdef HAVE_EMF
	stop_emf();
#endif
#ifdef HAVE_IPVS
	stop_ipvs();
#endif
#ifdef HAVE_VLANTAGGING
	stop_bridging();
#endif
#ifdef HAVE_BONDING
	stop_bonding();
#endif

#ifdef HAVE_VLANTAGGING
	stop_bridgesif();
	stop_vlantagging();
#endif
	stop_lan();
#ifndef HAVE_RB500
	stop_resetbutton();
#endif
#ifdef HAVE_IPV6
	stop_ipv6();
#endif
#ifdef HAVE_REGISTER
	if (isregistered_real())
#endif
	{
		start_run_rc_shutdown();
	}

}

void start_init_start(void)
{
	set_tcp_params();
	lcdmessage("START SERVICES");
	nvram_set("wl0_lazy_wds", nvram_safe_get("wl_lazy_wds"));

#ifdef HAVE_SYSLOG
	start_syslog();
#endif

#ifdef HAVE_SMARTD
	start_smartd();
#endif
#ifdef HAVE_IPV6
	start_ipv6();
#endif
#ifndef HAVE_RB500
	start_resetbutton();
#endif
	start_setup_vlans();
#if !defined(HAVE_MADWIFI) && !defined(HAVE_RT2880) && !defined(HAVE_RT61)
//                      start_wlconf(); // doesnt make any sense. its already triggered by start lan
#endif
#ifdef HAVE_VLANTAGGING
	start_bridging();
#endif
	start_lan();
#ifdef HAVE_IPVS
	start_ipvs();
#endif
#ifdef HAVE_BONDING
	start_bonding();
#endif
#ifdef HAVE_REGISTER
	start_mkfiles();
#endif
#ifdef HAVE_MADWIFI
	start_stabridge();
#endif

	cprintf("start services\n");
	start_services();

	cprintf("start wan boot\n");
#ifdef HAVE_VLANTAGGING
	start_vlantagging();
	start_bridgesif();
#endif
	start_wan_boot();
	start_ttraff();

	cprintf("diag STOP LED\n");
#if !defined(HAVE_MADWIFI) && !defined(HAVE_RT2880) && !defined(HAVE_RT61)
	diag_led(DIAG, STOP_LED);
#endif
	cprintf("set led release wan control\n");

#ifdef HAVE_RADIOOFF
	if (nvram_matchi("radiooff_button", 1)
	    && nvram_matchi("radiooff_boot_off", 1)) {
		start_radio_off();
		led_control(LED_SEC0, LED_OFF);
		led_control(LED_SEC1, LED_OFF);
	} else
#endif
	{
		start_radio_on();
	}
#ifdef HAVE_EMF
	start_emf();
#endif
#if !defined(HAVE_MADWIFI) && !defined(HAVE_RT2880) && !defined(HAVE_RT2860) && !defined(HAVE_RT61)
	start_nas();
#endif
	start_radio_timer();
#ifdef HAVE_CHILLI
	start_chilli();
#endif
#ifdef HAVE_WIFIDOG
	start_wifidog();
#endif
	cprintf("start syslog\n");
	system("/etc/postinit&");
	start_httpd();
	led_control(LED_DIAG, LED_OFF);
	lcdmessage("System Ready");
#ifndef HAVE_RB500
	stop_resetbutton();
	start_resetbutton();
#endif
	load_drivers(1);
	eval("startservice_f", "modules_wait");
#ifdef HAVE_X86
	eval("restart_f", "bootconfig");
#endif

}

void start_modules_wait(void)
{

	cprintf("run rc file\n");
#ifndef HAVE_MICRO
#ifdef HAVE_REGISTER
#ifndef HAVE_ERC
	if (isregistered_real())
#endif
#endif
	{
// start init scripts                           
		eval("/etc/init.d/rcS");
		eval("/opt/etc/init.d/rcS");
		eval("/jffs/etc/init.d/rcS");
		eval("/mmc/etc/init.d/rcS");
		// startup script
		// (siPath impl)
		cprintf("start modules\n");
		start_modules();
#ifdef HAVE_MILKFISH
		start_milkfish_boot();
#endif
		if (nvram_invmatch("rc_custom", ""))	// create
			// custom
			// script
		{
			nvram2file("rc_custom", "/tmp/.rc_custom");
			chmod("/tmp/.rc_custom", 0700);
		}
		stop_run_rc_startup();
		start_run_rc_startup();
	}
#endif

}

int restart_dns_main(int argc, char *argv[])
{
#ifdef HAVE_DNSMASQ
	stop_dnsmasq();
	start_dnsmasq();
#endif
	stop_smartdns();
	start_smartdns();
	return 0;
}

int ipfmt_main(int argc, char *argv[])
{
	char cidr[24];
	char fmt;
	struct in_addr addr, msk, outfmt;
	int valid;
	const char usage[] = ""	//
	    "ipfmt <print option> <addr> <netmask>\n"	//
	    "ipfmt <print option> <addr/cidr>\n"	//
	    "\n"		//
	    "print options:\n"	//
	    "        b  : broadcast\n"	//
	    "        n  : network\n"	//
	    "        c  : cidr\n"	//
	    "        N  : netmask\n"	//
	    "\n";		//

	if (argc < 3) {
		puts(usage);
		return 0;
	}

	fmt = argv[1][0];
	if (argc == 3) {
		valid = inet_cidr_to_addr(argv[2], &addr, &msk);
		if (valid == -EINVAL) {
			fprintf(stderr, "invalid cidr string\n");
			return 1;
		}
	} else {
		valid = inet_aton(argv[2], &addr);
		if (valid == -EINVAL) {
			fprintf(stderr, "invalid address\n");
			return 1;
		}
		valid = inet_aton(argv[3], &msk);
		if (valid == -EINVAL) {
			fprintf(stderr, "invalid netmask\n");
			return 1;
		}
	}

	switch (fmt) {
	case 'b':
		outfmt = inet_bcastaddr_of(inet_netaddr_of(addr, msk), msk);
		break;
	case 'n':
		outfmt = inet_netaddr_of(addr, msk);
		break;
	case 'N':
		outfmt = msk;
		break;
	case 'c':
		inet_addr_to_cidr(addr, msk, cidr);
		puts(cidr);
		return 0;
	default:
		fprintf(stderr, "invalid option\n%s", usage);
		break;
	}
	puts(inet_ntoa(outfmt));
	return 0;

}

int get_wanface_main(int argc, char **argv)
{
	fprintf(stdout, "%s", get_wan_face());
	return 0;
}

int get_wanip_main(int argc, char **argv)
{
	fprintf(stdout, "%s", get_wan_ipaddr());
	return 0;
}

int get_nfmark_main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: get_nfmark <service> <mark>\n\n" "	services: FORWARD\n" "		  HOTSPOT\n" "		  QOS\n\n" "	eg: get_nfmark QOS 10\n");
		return 1;
	}

	fprintf(stdout, "%s\n", get_NFServiceMark(argv[1], atol(argv[2])));
	return 0;
}

#ifdef HAVE_PPTPD
int pptpd_main(int argc, char **argv)
{

	if (!argv[1]) {
		fprintf(stderr, "usage: poptop [start|stop|restart]\n");
		return EINVAL;
	} else if (strstr(argv[1], "start"))
		start_pptpd();
	else if (strstr(argv[1], "stop"))
		stop_pptpd();
	else if (strstr(argv[1], "restart")) {
		stop_pptpd();
		start_pptpd();
		return 0;
	} else {
		fprintf(stderr, "usage: poptop [start|stop|restart]\n");
		return EINVAL;
	}
	return 0;
}
#endif
