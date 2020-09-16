
#ifndef _rc_h_
#define _rc_h_

// #include <code_pattern.h>
#include <bcmconfig.h>
//#include <netinet/in.h>

#include <fcntl.h>

#include <cy_conf.h>

/*
 * AhMan March 18 2005 
 */
#define PPPOE0          0

/*
 * udhcpc scripts 
 */
extern int udhcpc_main(int argc, char **argv);

//extern void runStartup(char *folder, char *extension);

extern void shutdown_system(void);

extern void start_dhcpc(char *wan_ifname,char *pidfile,char *script, int fork, int leasetime, int nodeconfig);

/*
 * ppp scripts 
 */
extern int ipup_main(int argc, char **argv);
extern int ipdown_main(int argc, char **argv);
extern int set_pppoepid_to_nv_main(int argc, char **argv);	// tallest
								// 1219
extern int disconnected_pppoe_main(int argc, char **argv);	// by tallest 
								// 0407

/*
 * http functions 
 */
extern int http_get(const char *server, char *buf, size_t count, off_t offset);
extern int http_post(const char *server, char *buf, size_t count);
extern int http_stats(const char *url);
extern void addHost(char *host, char *ip, int withdomain);

/*
 * init 
 */
extern int console_init(void);
extern pid_t ddrun_shell(int timeout, int nowait);
extern void signal_init(void);
extern void fatal_signal(int sig);

/*
 * interface 
 */
extern int ifconfig(char *ifname, int flags, char *addr, char *netmask);
extern int route_add(char *name, int metric, char *dst, char *gateway, char *genmask);
extern int route_del(char *name, int metric, char *dst, char *gateway, char *genmask);

/*
 * network 
 */
extern int isClient(void);
extern char *get_wdev(void);

/*
 * services 
 */

extern int check_process(char *name);
extern int config_vlan(void);
extern void config_loopback(void);

extern int flush_interfaces(void);
extern void start_nonstd_interfaces(void);
extern int setup_vlans(void);

/*
 * firewall 
 */

/*
 * routes 
 */
extern int set_routes(void);

#define GOT_IP			0x01
#define RELEASE_IP		0x02
#define	GET_IP_ERROR		0x03
#define RELEASE_WAN_CONTROL	0x04
#define SET_LED(val)
/*
 * 
 * #define SET_LED(val) \ { \ int filep; \ if(check_hw_type() ==
 * BCM4704_BCM5325F_CHIP) { \ if ((filep = open("/dev/ctmisc", O_RDWR,0))) \
 * { \ ioctl(filep, val, 0); \ close(filep); \ } \ } \ } 
 */
// //////////////////////////////////////////////////////////
#define BOOT 0
#define REDIAL 1
#define DELAY_PING

extern int httpd_main(int argc, char **argv);
extern int bird_main(int argc, char **argv);
extern int dnsmasq_main(int argc, char **argv);

#ifdef HAVE_IPROUTE2
extern int tc_main(int argc, char **argv);
#endif
#ifdef HAVE_DHCPFWD
extern int dhcpforward_main(int argc, char **argv);
#endif
#ifdef HAVE_PPPD
extern int pppd_main(int argc, char **argv);
#endif

#ifdef HAVE_SSHD
extern int dropbearconvert_main(int argc, char **argv);
extern int cli_main(int argc, char **argv);
extern int dropbearkey_main(int argc, char **argv);
extern int dropbear_main(int argc, char **argv);
extern int scp_main(int argc, char **argv);
#endif


extern int create_rc_file(char *name);
extern int wland_main(int argc, char **argv);

extern void start_vpn_modules(void);
extern void stop_vpn_modules(void);
extern void load_vpn_modules(void);
extern void unload_vpn_modules(void);
extern void depend_vpn_modules(void);

extern int zebra_ospf_init(void);
extern int zebra_ripd_init(void);

#define RC_FIREWALL "rc_firewall"
#define RC_STARTUP  "rc_startup"
#define RC_SHUTDOWN "rc_shutdown"
#define RC_USB  "rc_usb"

extern int ipupdate_main(int argc, char **argv);
extern int gpio_main(int argc, char **argv);

extern int write_boot(const char *path, const char *mtd);
extern void do_mssid(char *wlname);
extern int init_mtu(char *wan_proto);
extern int force_to_dial(void);
extern char *range(char *start, char *end, char *buf);

// static void start_heartbeat (int status);
extern void stop_heartbeat(void);
extern int hb_connect_main(int argc, char **argv);
extern int hb_disconnect_main(int argc, char **argv);
extern int ddns_success_main(int argc, char **argv);
extern int sendudp_main(int argc, char *argv[]);
void cfe_default(void);

// extern int nvram_restore(const char *path, char *mtd);
int reg_main(int argc, char **argv);

#endif				/* _rc_h_ */
