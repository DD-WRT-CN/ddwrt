/*
 * interface.c
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#ifdef __UCLIBC__
#include <error.h>
#endif
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <proto/ethernet.h>
#include <shutils.h>
#include <bcmnvram.h>
#include <rc.h>
#include <cy_conf.h>
#include <utils.h>
#include <services.h>

static char *getPhyDev()
{
	if (f_exists("/proc/switch/eth0/enable"))
		return "eth0";

	if (f_exists("/proc/switch/eth1/enable"))
		return "eth1";

	if (f_exists("/proc/switch/eth2/enable"))
		return "eth2";

	return "eth0";
}

#define MAX_VLAN_GROUPS	16
#define MAX_DEV_IFINDEX	16

/*
 * configure vlan interface(s) based on nvram settings 
 */
void start_config_vlan(void)
{
	int s;
	struct ifreq ifr;
	int i, j;
	char ea[ETHER_ADDR_LEN];
	char *phy = getPhyDev();

	// configure ports
	writevaproc("1", "/proc/switch/%s/reset", phy);
	writevaproc("1", "/proc/switch/%s/enable_vlan", phy);
	for (i = 0; i < 16; i++) {
		char vlanb[16];

		sprintf(vlanb, "vlan%dports", i);
		if (!nvram_exists(vlanb) || nvram_match(vlanb, ""))
			continue;
		writevaproc(nvram_safe_get(vlanb), "/proc/switch/%s/vlan/%d/ports", phy, i);
	}
	/*
	 * set vlan i/f name to style "vlan<ID>" 
	 */

	eval("vconfig", "set_name_type", "VLAN_PLUS_VID_NO_PAD");

	/*
	 * create vlan interfaces 
	 */
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		return;

	for (i = 0; i < MAX_VLAN_GROUPS; i++) {
		char vlan_id[16];
		char *hwname, *hwaddr;

		if (!(hwname = nvram_nget("vlan%dhwname", i))) {
			continue;
		}
		if (!(hwaddr = nvram_nget("%smacaddr", hwname))) {
			continue;
		}
		if (!*hwname || !*hwaddr) {
			continue;
		}
		ether_atoe(hwaddr, ea);
		for (j = 1; j <= MAX_DEV_IFINDEX; j++) {
			ifr.ifr_ifindex = j;
			if (ioctl(s, SIOCGIFNAME, &ifr)) {
				continue;
			}
			if (ioctl(s, SIOCGIFHWADDR, &ifr)) {
				continue;
			}
			if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
				continue;
			}
			if (!bcmp(ifr.ifr_hwaddr.sa_data, ea, ETHER_ADDR_LEN)) {
				break;
			}
		}
		if (j > MAX_DEV_IFINDEX) {
			continue;
		}
		if (ioctl(s, SIOCGIFFLAGS, &ifr))
			continue;
		if (!(ifr.ifr_flags & IFF_UP))
			eval("ifconfig", ifr.ifr_name, "0.0.0.0", "up");
		snprintf(vlan_id, sizeof(vlan_id), "%d", i);
		eval("vconfig", "add", ifr.ifr_name, vlan_id);
	}

	close(s);

	return;
}

/*
 * begin lonewolf mods 
 */

void start_setup_vlans(void)
{
#ifdef HAVE_SWCONFIG

	if (!nvram_exists("sw_cpuport") && !nvram_exists("sw_wancpuport"))
		return;
#ifdef HAVE_R9000
	if (!nvram_exists("port7vlans") || nvram_matchi("vlans", 0))
		return;		// for some reason VLANs are not set up, and
#else
	if (!nvram_exists("port5vlans") || nvram_matchi("vlans", 0))
		return;		// for some reason VLANs are not set up, and
#endif
#ifdef HAVE_R9000
	sysprintf(". /usr/sbin/resetswitch.sh");
#else
	eval("swconfig", "dev", "switch0", "set", "reset", "1");
	eval("swconfig", "dev", "switch0", "set", "enable_vlan", "1");
	eval("swconfig", "dev", "switch0", "set", "igmp_v3", "1");
#endif
	char buildports[18][32];
	char tagged[18];
	char snoop[5];
	memset(&tagged[0], 0, sizeof(tagged));
	memset(&snoop[0], 0, sizeof(snoop));
	memset(&buildports[0][0], 0, 16 * 32);
	int vlan_number;
	int i;
#ifdef HAVE_R9000
	for (i = 0; i < 7; i++) {
#else
	for (i = 0; i < 5; i++) {
#endif
		char *vlans = nvram_nget("port%dvlans", i);
		char *next;
		char vlan[4];

		foreach(vlan, vlans, next) {
			int tmp = atoi(vlan);
			if (tmp >= 16) {
				if (vlan_number == 16)
					tagged[vlan_number] = 1;

				if (i == 0 && nvram_exists("sw_wancpuport")) {
					if (!nvram_match("sw_wan", "-1")) {
						switch (vlan_number) {
						case 17:
							eval("swconfig", "dev", "switch0", "port", nvram_safe_get("sw_wan"), "set", "igmp_snooping", "1");
							break;
						case 16:
							sysprintf("swconfig dev switch0 vlan %d set ports \"%st %st\"", vlan_number, nvram_safe_get("sw_wancpuport"), nvram_safe_get("sw_wan"));
							break;
						}
					}
				} else {
					if (vlan_number == 17) {
						eval("swconfig", "dev", "switch0", "port", nvram_nget("sw_lan%d", i), "set", "igmp_snooping", "1");
					}

				}
			} else {
				vlan_number = tmp;
				char *ports = &buildports[vlan_number][0];
				if (i == 0 && nvram_exists("sw_wancpuport")) {	// wan port
					if (!nvram_match("sw_wan", "-1")) {
						sysprintf("swconfig dev switch0 vlan %d set ports \"%st %s\"", vlan_number, nvram_safe_get("sw_wancpuport"), nvram_safe_get("sw_wan"));
					}
				} else {
					if (i == 0) {
						if (strlen(ports))
							snprintf(ports, 31, "%s %s", ports, nvram_nget("sw_wan", i));
						else
							snprintf(ports, 31, "%s", nvram_nget("sw_wan", i));
					} else {

						if (strlen(ports))
							snprintf(ports, 31, "%s %s", ports, nvram_nget("sw_lan%d", i));
						else
							snprintf(ports, 31, "%s", nvram_nget("sw_lan%d", i));
					}

				}
				char buff[32];
				snprintf(buff, 9, "%d", tmp);
				eval("vconfig", "set_name_type", "VLAN_PLUS_VID_NO_PAD");
				char *lanphy = "eth0";
				char *wanphy = "eth0";
				if (nvram_exists("sw_wancpuport") && nvram_match("wan_default", "eth0")) {
					lanphy = "eth1";
					wanphy = "eth0";
				}
				if (nvram_exists("sw_wancpuport") && nvram_match("wan_default", "eth1")) {
					lanphy = "eth0";
					wanphy = "eth1";
				}
				if (i == 0 && nvram_exists("sw_wancpuport"))
					eval("vconfig", "add", wanphy, buff);
				else
					eval("vconfig", "add", lanphy, buff);
				snprintf(buff, 9, "vlan%d", tmp);
				if (strcmp(nvram_safe_get("wan_ifname"), buff)) {
					if (*(nvram_nget("vlan%d_ipaddr", vlan_number)))
						eval("ifconfig", buff, nvram_nget("%s_ipaddr", buff), "netmask", nvram_nget("%s_netmask", buff), "up");
					else
						eval("ifconfig", buff, "0.0.0.0", "up");
					char hwaddr[32];
					sprintf(hwaddr, "%s_hwaddr", buff);
					if (!nvram_match(hwaddr, ""))
						set_hwaddr(buff, nvram_safe_get(hwaddr));
				}

			}
		}
	}
#ifdef HAVE_R9000
	for (vlan_number = 0; vlan_number < 16; vlan_number++) {
		char *ports = &buildports[vlan_number][0];
		if (strlen(ports)) {
//                          sw_user_lan_ports_vlan_config "7" "6" "0" "0" "0" "normal_lan"
//                          sw_user_lan_ports_vlan_config "1" "" "0" "1" "0" "wan"
			sysprintf(". /usr/sbin/config_vlan.sh \"%d\" \"%s\" \"0\" \"0\" \"0\" \"normal_lan\"");
		}
	}
	sysprintf(". /usr/sbin/config_vlan.sh \"2\" \"\" \"0\" \"1\" \"0\"  \"wan\"");
	sysprintf(". /tmp/ssdk_new.sh");
#else
	for (vlan_number = 0; vlan_number < 16; vlan_number++) {
		char *ports = &buildports[vlan_number][0];
		if (strlen(ports)) {
			if (nvram_exists("sw_wancpuport"))
				sysprintf("swconfig dev switch0 vlan %d set ports \"%st %s%s\"", vlan_number, nvram_safe_get("sw_lancpuport"), ports, tagged[vlan_number] ? "t" : "");
			else
				sysprintf("swconfig dev switch0 vlan %d set ports \"%st %s%s\"", vlan_number, nvram_safe_get("sw_cpuport"), ports, tagged[vlan_number] ? "t" : "");
		} else {
			sysprintf("swconfig dev switch0 vlan %d set ports \"\"", vlan_number);
		}
	}
	eval("swconfig", "dev", "switch0", "set", "apply");
#endif
#else
	/*
	 * VLAN #16 is just a convieniant way of storing tagging info.  There is
	 * no VLAN #16 
	 */

	if (!nvram_exists("port5vlans") || nvram_matchi("vlans", 0))
		return;		// for some reason VLANs are not set up, and
	// we don't want to disable everything!

	if (nvram_matchi("wan_vdsl", 1) && !nvram_matchi("fromvdsl", 1)) {
		nvram_seti("vdsl_state", 0);
		enable_dtag_vlan(1);
		return;
	}

	int i, j, ret = 0, tmp, workaround = 0, found;
	char *vlans, *next, vlan[4], buff[70], buff2[16];
	FILE *fp;
	char portsettings[18][64];
	char tagged[18];
	unsigned char mac[20];;
	struct ifreq ifr;
	char *phy = getPhyDev();

	strcpy(mac, nvram_safe_get("et0macaddr"));

	int vlanmap[6] = { 0, 1, 2, 3, 4, 5 };	// 0=wan; 1,2,3,4=lan; 5=internal 

	getPortMapping(vlanmap);

	int ast = 0;
	char *asttemp;
	char *lanifnames = nvram_safe_get("lan_ifnames");

	if (strstr(lanifnames, "vlan1") && !strstr(lanifnames, "vlan0"))
		asttemp = nvram_safe_get("vlan1ports");
	else if (strstr(lanifnames, "vlan2") && !strstr(lanifnames, "vlan0")
		 && !strstr(lanifnames, "vlan1"))
		asttemp = nvram_safe_get("vlan2ports");
	else
		asttemp = nvram_safe_get("vlan0ports");

	if (strstr(asttemp, "5*"))
		ast = 5;
	if (strstr(asttemp, "8*"))
		ast = 8;
	if (strstr(asttemp, "7u"))
		ast = 7;

	bzero(&portsettings[0][0], 18 * 64);
	bzero(&tagged[0], sizeof(tagged));
	for (i = 0; i < 6; i++) {
		vlans = nvram_nget("port%dvlans", i);
		int use = vlanmap[i];

		if (vlans) {
			int lastvlan = 0;
			int portmask = 3;
			int mask = 0;

			foreach(vlan, vlans, next) {
				tmp = atoi(vlan);
				if (tmp < 16) {
					lastvlan = tmp;
					if (i == 5) {
						snprintf(buff, 9, "%d", tmp);
						eval("vconfig", "set_name_type", "VLAN_PLUS_VID_NO_PAD");
						eval("vconfig", "add", phy, buff);
						snprintf(buff, 9, "vlan%d", tmp);
						if (strcmp(nvram_safe_get("wan_ifname"), buff)) {
							if (*(nvram_nget("%s_ipaddr", buff)))
								eval("ifconfig", buff, nvram_nget("%s_ipaddr", buff), "netmask", nvram_nget("%s_netmask", buff), "up");
							else
								eval("ifconfig", buff, "0.0.0.0", "up");
						}
					}

					sprintf((char *)
						&portsettings[tmp][0], "%s %d", (char *)
						&portsettings[tmp][0], use);
				} else {
					if (tmp == 16)	// vlan tagged
						tagged[use] = 1;
					if (tmp == 17)	// no auto negotiate
						mask |= 4;
					if (tmp == 18)	// no full speed
						mask |= 1;
					if (tmp == 19)	// no full duplex
						mask |= 2;
					if (tmp == 20)	// disabled
						mask |= 8;
					if (tmp == 21)	// no gigabit
						mask |= 16;

				}
			}
			if (mask & 8 && use < 5) {
				writevaproc("0", "/proc/switch/%s/port/%d/enable", phy, use);
			} else {
				writevaproc("1", "/proc/switch/%s/port/%d/enable", phy, use);
			}
			if (use < 5) {
				snprintf(buff, 69, "/proc/switch/%s/port/%d/media", phy, use);
				if ((fp = fopen(buff, "r+"))) {
					if ((mask & 4) == 4) {
						if (!(mask & 16)) {
							if (mask & 2)
								fputs("1000HD", fp);
							else
								fputs("1000FD", fp);

						} else {
							switch (mask & 3) {
							case 0:
								fputs("100FD", fp);
								break;
							case 1:
								fputs("10FD", fp);
								break;
							case 2:
								fputs("100HD", fp);
								break;
							case 3:
								fputs("10HD", fp);
								break;
							}
						}
					} else {
						fprintf(stderr, "set port %d to AUTO\n", use);
						fputs("AUTO", fp);
					}
					fclose(fp);
				}
			}

		}
	}

	for (i = 0; i < 16; i++) {
		char port[64];

		strcpy(port, &portsettings[i][0]);
		bzero(&portsettings[i][0], 64);
		foreach(vlan, port, next) {
			int vlan_number = atoi(vlan);
			if (vlan_number < 5 && vlan_number >= 0 && tagged[vlan_number]) {
				sprintf(&portsettings[i][0], "%s %st", &portsettings[i][0], vlan);
			} else if ((vlan_number == 5 || vlan_number == 8 || vlan_number == 7)
				   && tagged[vlan_number] && !ast) {
				sprintf(&portsettings[i][0], "%s %st", &portsettings[i][0], vlan);
			} else if ((vlan_number == 5 || vlan_number == 8 || vlan_number == 7)
				   && tagged[vlan_number] && ast) {
				sprintf(&portsettings[i][0], "%s %s*", &portsettings[i][0], vlan);
			} else {
				sprintf(&portsettings[i][0], "%s %s", &portsettings[i][0], vlan);
			}
		}
	}
	for (i = 0; i < 16; i++) {
		writevaproc(" ", "/proc/switch/%s/vlan/%d/ports", phy, i);
	}
	for (i = 0; i < 16; i++) {
		fprintf(stderr, "configure vlan ports to %s\n", portsettings[i]);
		writevaproc(portsettings[i], "/proc/switch/%s/vlan/%d/ports", phy, i);
	}
#endif
}

int flush_interfaces(void)
{
	char all_ifnames[256] = { 0 }, *c, *next, buff[32], buff2[32];

#ifdef HAVE_MADWIFI
#ifdef HAVE_GATEWORX
	snprintf(all_ifnames, 255, "%s %s %s", "ixp0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_NORTHSTAR
	snprintf(all_ifnames, 255, "%s %s %s", "vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_EROUTER
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_LAGUNA
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_VENTANA
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_NEWPORT
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_X86
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WR810N
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_EAP9550
	snprintf(all_ifnames, 255, "%s %s %s", "eth2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_RT2880
	snprintf(all_ifnames, 255, "%s %s %s", "vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_MAGICBOX
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_UNIWIP
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_MVEBU
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_R9000
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_IPQ806X
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WDR4900
	snprintf(all_ifnames, 255, "%s %s %s", "vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_RB600
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1 eth2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WRT54G2
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_RTG32
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_DIR300
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_MR3202A
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_LS2
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 vlan0 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_SOLO51
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 vlan0 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_LS5
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_FONERA
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 vlan0 vlan1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WHRAG108
	snprintf(all_ifnames, 255, "%s %s %s", "eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WNR2000
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WR650AC
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_DIR862
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WILLY
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_MMS344
	snprintf(all_ifnames, 255, "%s %s %s", "vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_CPE880
	snprintf(all_ifnames, 255, "%s %s %s", "vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_ARCHERC7V4
	snprintf(all_ifnames, 255, "%s %s %s", "vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WZR450HP2
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WDR3500
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_JWAP606
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_LIMA
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_RAMBUTAN
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WASP
	snprintf(all_ifnames, 255, "%s %s %s", "vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WDR2543
	snprintf(all_ifnames, 255, "%s %s %s", "vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WHRHPGN
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_DIR615E
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WA901v1
	snprintf(all_ifnames, 255, "%s %s %s", "eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_CARAMBOLA
	snprintf(all_ifnames, 255, "%s %s %s", "vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WR710
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WR703
	snprintf(all_ifnames, 255, "%s %s %s", "eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WA7510
	snprintf(all_ifnames, 255, "%s %s %s", "eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WR741
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_DAP3310
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_DAP3410
	snprintf(all_ifnames, 255, "%s %s %s", "vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_UBNTM
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_PB42
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_JJAP93
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_JJAP005
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_JJAP501
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_AC722
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_HORNET
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_AC622
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_RS
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_JA76PF
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_ALFAAP94
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_JWAP003
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WA901
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WR941
	snprintf(all_ifnames, 255, "%s %s %s", "vlan0 vlan1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WR1043
	snprintf(all_ifnames, 255, "%s %s %s", "vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WZRG450
	snprintf(all_ifnames, 255, "%s %s %s", "vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_DIR632
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_AP83
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_AP94
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WP546
	snprintf(all_ifnames, 255, "%s %s %s", "eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_LSX
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_DANUBE
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_WBD222
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1 eth2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_STORM
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_OPENRISC
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1 eth2 eth3", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_ADM5120
	snprintf(all_ifnames, 255, "%s %s %s", "eth0 eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_TW6600
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_RDAT81
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_RCAA01
	snprintf(all_ifnames, 255, "%s %s %s", "vlan0 vlan1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_CA8PRO
	snprintf(all_ifnames, 255, "%s %s %s", "vlan0 vlan1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#elif HAVE_CA8
	snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#else
	snprintf(all_ifnames, 255, "%s %s %s", "ixp0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#endif
#elif HAVE_RT2880
	snprintf(all_ifnames, 255, "%s %s %s", "vlan1 vlan2", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#else
	if (wl_probe("eth2"))
		snprintf(all_ifnames, 255, "%s %s %s", "eth0", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
	else
		snprintf(all_ifnames, 255, "%s %s %s %s", "eth0", "eth1", nvram_safe_get("lan_ifnames"), nvram_safe_get("wan_ifnames"));
#endif
	// strcpy(all_ifnames, "eth0 ");
	// strcpy(all_ifnames, "eth0 eth1 "); //James, note: eth1 is the wireless 
	// interface on V2/GS's. I think we need a check here.
	// strcat(all_ifnames, nvram_safe_get("lan_ifnames"));
	// strcat(all_ifnames, " ");
	// strcat(all_ifnames, nvram_safe_get("wan_ifnames"));

	c = nvram_safe_get("port5vlans");
	if (c) {
		foreach(buff, c, next) {
			if (atoi(buff) > 15)
				continue;
			snprintf(buff2, sizeof(buff2), " vlan%s", buff);
			strcat(all_ifnames, buff2);
		}
	}

	foreach(buff, all_ifnames, next) {
		if (strcmp(buff, "br0") == 0)
			continue;
		eval("ifconfig", buff, "0.0.0.0", "down");

		// eval ("ifconfig", buff, "down");
		eval("ip", "addr", "flush", "dev", buff);
		eval("ifconfig", buff, "0.0.0.0", "up");

		// eval ("ifconfig", buff, "up");
	}

	return 0;
}
