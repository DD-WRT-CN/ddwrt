/*
 * The olsr.org Optimized Link-State Routing daemon (olsrd)
 *
 * (c) by the OLSR project
 *
 * See our Git repository to find out who worked on this file
 * and thus is a copyright holder on it.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */

#ifndef _OLSRD_CFGPARSER_H
#define _OLSRD_CFGPARSER_H

#include "compiler.h"
#include "defs.h"
#include "olsr_types.h"
#include "common/autobuf.h"
#include "pud/src/receiver.h"

/* set to 1 to collect all startup sleep into one sleep
 * (just as long as the longest sleep)
 * useful if many errors on many interfaces */
#define OLSR_COLLECT_STARTUP_SLEEP 1

#define TESTLIB_PATH 0
#define SYSLOG_NUMBERING 0

/* Default values not declared in olsr_protocol.h */
#define DEF_IP_VERSION       AF_INET
#define DEF_POLLRATE         0.05
#define DEF_NICCHGPOLLRT     2.5
#define DEF_WILL_AUTO        false
#define DEF_WILLINGNESS      3
#define DEF_ALLOW_NO_INTS    true
#define DEF_TOS              192
#define DEF_DEBUGLVL         1
#define DEF_IPC_CONNECTIONS  0
#define DEF_USE_HYST         false
#define DEF_FIB_METRIC       FIBM_FLAT
#define DEF_FIB_METRIC_DEFAULT            2
#define DEF_LQ_LEVEL         2
#define DEF_LQ_ALGORITHM     "etx_ff"
#define DEF_LQ_FISH          1
#define DEF_LQ_NAT_THRESH    1.0
#define DEF_LQ_AGING         0.05
#define DEF_CLEAR_SCREEN     true
#define DEF_OLSRPORT         698
#define DEF_RTPROTO          0 /* 0 means OS-specific default */
#define DEF_RT_NONE          -1
#define DEF_RT_AUTO          0

#define DEF_RT_TABLE_NR                   254
#define DEF_RT_TABLE_DEFAULT_NR           254
#define DEF_RT_TABLE_TUNNEL_NR            254

#define DEF_SGW_RT_TABLE_NR               254
#define DEF_SGW_RT_TABLE_DEFAULT_NR       223
#define DEF_SGW_RT_TABLE_TUNNEL_NR        224

#define DEF_RT_TABLE_PRI                  DEF_RT_NONE
#define DEF_RT_TABLE_DEFAULTOLSR_PRI      DEF_RT_NONE
#define DEF_RT_TABLE_TUNNEL_PRI           DEF_RT_NONE
#define DEF_RT_TABLE_DEFAULT_PRI          DEF_RT_NONE

#define DEF_SGW_RT_TABLE_PRI                    DEF_RT_NONE
#define DEF_SGW_RT_TABLE_PRI_BASE               32766
#define DEF_SGW_RT_TABLE_DEFAULTOLSR_PRI_ADDER  10
#define DEF_SGW_RT_TABLE_TUNNEL_PRI_ADDER       10
#define DEF_SGW_RT_TABLE_DEFAULT_PRI_ADDER      10

#define DEF_MIN_TC_VTIME     0.0
#define DEF_USE_NIIT         true
#define DEF_SMART_GW         false
#define DEF_SMART_GW_ALWAYS_REMOVE_SERVER_TUNNEL  false
#define DEF_GW_USE_COUNT     1
#define DEF_GW_TAKEDOWN_PERCENTAGE 25
#define DEF_GW_EGRESS_FILE    "/var/run/olsrd-sgw-egress.conf"
#define DEF_GW_EGRESS_FILE_PERIOD 5000
#define DEF_GW_OFFSET_TABLES 90
#define DEF_GW_OFFSET_RULES  0
#define DEF_GW_PERIOD        10*1000
#define DEF_GW_STABLE_COUNT  6
#define DEF_GW_ALLOW_NAT     true
#define DEF_GW_THRESH        0
#define DEF_GW_WEIGHT_EXITLINK_UP   1
#define DEF_GW_WEIGHT_EXITLINK_DOWN 1
#define DEF_GW_WEIGHT_ETX           1
#define DEF_GW_DIVIDER_ETX          0
#define DEF_GW_MAX_COST_MAX_ETX     2560
#define DEF_GW_TYPE          GW_UPLINK_IPV46
#define DEF_GW_UPLINK_NAT    true
#define DEF_UPLINK_SPEED     128
#define DEF_DOWNLINK_SPEED   1024
#define DEF_USE_SRCIP_ROUTES false

#define DEF_IF_MODE          IF_MODE_MESH

/* Bounds */

#define MIN_INTERVAL         0.01

#define MAX_POLLRATE         1.0
#define MIN_POLLRATE         0.01
#define MAX_NICCHGPOLLRT     100.0
#define MIN_NICCHGPOLLRT     1.0
#define MAX_DEBUGLVL         9
#define MIN_DEBUGLVL         0
#define MAX_TOS              252
#define MIN_TOS              0
#define MAX_WILLINGNESS      7
#define MIN_WILLINGNESS      0
#define MAX_MPR_COVERAGE     20
#define MIN_MPR_COVERAGE     1
#define MAX_TC_REDUNDANCY    2
#define MIN_TC_REDUNDANCY    0
#define MAX_HYST_PARAM       1.0
#define MIN_HYST_PARAM       0.0
#define MAX_LQ_LEVEL         2
#define MIN_LQ_LEVEL         0
#define MAX_LQ_AGING         1.0
#define MIN_LQ_AGING         0.01

#define MIN_SMARTGW_USE_COUNT_MIN  1
#define MAX_SMARTGW_USE_COUNT_MAX  64

#define MIN_SMARTGW_EGRESS_FILE_PERIOD 1000

#define MAX_SMARTGW_EGRESS_INTERFACE_COUNT_MAX 32

#define MIN_SMARTGW_PERIOD   1*1000
#define MAX_SMARTGW_PERIOD   320000*1000

#define MIN_SMARTGW_STABLE   1
#define MAX_SMARTGW_STABLE   254

#define MIN_SMARTGW_THRES    10
#define MAX_SMARTGW_THRES    100

#define MIN_SMARTGW_SPEED    1
#define MAX_SMARTGW_SPEED    320000000

#ifndef IPV6_ADDR_SITELOCAL
#define IPV6_ADDR_SITELOCAL    0x0040U
#endif /* IPV6_ADDR_SITELOCAL */

#include "interfaces.h"

enum smart_gw_uplinktype {
  GW_UPLINK_NONE,
  GW_UPLINK_IPV4,
  GW_UPLINK_IPV6,
  GW_UPLINK_IPV46,
  GW_UPLINK_CNT,
};


typedef enum {
  FIBM_FLAT,
  FIBM_CORRECT,
  FIBM_APPROX,
  FIBM_CNT
} olsr_fib_metric_options;

enum olsr_if_mode {
  IF_MODE_MESH,
  IF_MODE_ETHER,
  IF_MODE_SILENT,
  IF_MODE_CNT
};


struct olsr_msg_params {
  float emission_interval;
  float validity_time;
};

struct olsr_lq_mult {
  union olsr_ip_addr addr;
  uint32_t value;
  struct olsr_lq_mult *next;
};

struct olsr_if_weight {
  int value;
  bool fixed;
};

struct if_config_options {
  union olsr_ip_addr ipv4_multicast;
  union olsr_ip_addr ipv6_multicast;

  union olsr_ip_addr ipv4_src;
  struct olsr_ip_prefix ipv6_src;

  int mode;

  struct olsr_if_weight weight;
  struct olsr_msg_params hello_params;
  struct olsr_msg_params tc_params;
  struct olsr_msg_params mid_params;
  struct olsr_msg_params hna_params;
  struct olsr_lq_mult *lq_mult;
  int orig_lq_mult_cnt;
  bool autodetect_chg;
};

struct olsr_if {
  char *name;
  bool configured;
  bool host_emul;
  union olsr_ip_addr hemu_ip;
  struct interface_olsr *interf;
  struct if_config_options *cnf, *cnfi;
  struct olsr_if *next;
};

struct ip_prefix_list {
  struct olsr_ip_prefix net;
  struct ip_prefix_list *next;
};

struct hyst_param {
  float scaling;
  float thr_high;
  float thr_low;
};

struct plugin_param {
  char *key;
  char *value;
  struct plugin_param *next;
};

struct plugin_entry {
  char *name;
  struct plugin_param *params;
  struct plugin_entry *next;
};

/*
 * The config struct
 */

struct olsrd_config {
  char * configuration_file;
  uint16_t olsrport;
  int debug_level;
  bool no_fork;
  char * pidfile;
  bool host_emul;
  int ip_version;
  bool allow_no_interfaces;
  uint8_t tos;
  uint8_t rt_proto;
  uint8_t rt_table;
  uint8_t rt_table_default;
  uint8_t rt_table_tunnel;
  int32_t rt_table_pri;
  int32_t rt_table_tunnel_pri;
  int32_t rt_table_defaultolsr_pri;
  int32_t rt_table_default_pri;
  uint8_t willingness;
  bool willingness_auto;
  int ipc_connections;
  bool use_hysteresis;
  olsr_fib_metric_options fib_metric;
  int fib_metric_default;
  struct hyst_param hysteresis_param;
  struct plugin_entry *plugins;
  struct ip_prefix_list *hna_entries;
  struct ip_prefix_list *ipc_nets;
  struct if_config_options *interface_defaults;
  struct olsr_if *interfaces;
  float pollrate;
  float nic_chgs_pollrate;
  bool clear_screen;
  uint8_t tc_redundancy;
  uint8_t mpr_coverage;
  uint8_t lq_level;
  uint8_t lq_fish;
  float lq_aging;
  char *lq_algorithm;

  float min_tc_vtime;

  bool set_ip_forward;

  char *lock_file;
  bool use_niit;

  bool smart_gw_active;
  bool smart_gw_always_remove_server_tunnel;
  bool smart_gw_allow_nat;
  bool smart_gw_uplink_nat;
  uint8_t smart_gw_use_count;
  uint8_t smart_gw_takedown_percentage;
  char *smart_gw_instance_id;
  char *smart_gw_policyrouting_script;
  struct sgw_egress_if * smart_gw_egress_interfaces;
  uint8_t smart_gw_egress_interfaces_count;
  char *smart_gw_egress_file;
  uint32_t smart_gw_egress_file_period;
  char *smart_gw_status_file;
  uint32_t smart_gw_offset_tables;
  uint32_t smart_gw_offset_rules;
  uint32_t smart_gw_period;
  uint8_t smart_gw_stablecount;
  uint8_t smart_gw_thresh;
  uint8_t smart_gw_weight_exitlink_up;
  uint8_t smart_gw_weight_exitlink_down;
  uint8_t smart_gw_weight_etx;
  uint32_t smart_gw_divider_etx;
  uint32_t smart_gw_path_max_cost_etx_max;
  enum smart_gw_uplinktype smart_gw_type;
  uint32_t smart_gw_uplink;
  uint32_t smart_gw_downlink;
  bool smart_gateway_bandwidth_zero;
  struct olsr_ip_prefix smart_gw_prefix;

  /* Main address of this node */
  union olsr_ip_addr main_addr;
  union olsr_ip_addr unicast_src_ip;
  bool use_src_ip_routes;

  /* Stuff set by olsrd */
  uint8_t maxplen;                     /* maximum prefix len */
  size_t ipsize;                       /* Size of address */
  bool del_gws;                        /* Delete InternetGWs at startup */
  float will_int;
  float max_jitter;
  int exit_value;                      /* Global return value for process termination */
  float max_tc_vtime;

  int niit4to6_if_index;
  int niit6to4_if_index;

  /*many potential parameters or helper variables for smartgateway*/
  bool has_ipv4_gateway;
  bool has_ipv6_gateway;

  int ioctl_s;                         /* Socket used for ioctl calls */
#ifdef __linux__
  int rtnl_s;                          /* Socket used for rtnetlink messages */
  int rt_monitor_socket;
#endif /* __linux__ */

#if defined __FreeBSD__ || defined __FreeBSD_kernel__ || defined __APPLE__ || defined __NetBSD__ || defined __OpenBSD__
  int rts;                             /* Socket used for route changes on BSDs */
#endif /* defined __FreeBSD__ || defined __FreeBSD_kernel__ || defined __APPLE__ || defined __NetBSD__ || defined __OpenBSD__ */
  float lq_nat_thresh;

  TransmitGpsInformation * pud_position;
};

#if defined __cplusplus
extern "C" {
#endif /* defined __cplusplus */

  extern const char *GW_UPLINK_TXT[];
  extern const char *FIB_METRIC_TXT[];
  extern const char *OLSR_IF_MODE[];

/*
 * List functions
 */

  /**
   * Count the number of olsr interfaces
   *
   * @return the number of olsr interfaces
   */
  static INLINE unsigned int getNrOfOlsrInterfaces(struct olsrd_config * cfg) {
    struct olsr_if * ifn;
    unsigned int i = 0;

      for (ifn = cfg->interfaces; ifn; ifn = ifn->next, i++) {}
      return i;
  }


  void ip_prefix_list_add(struct ip_prefix_list **, const union olsr_ip_addr *, uint8_t);

  int ip_prefix_list_remove(struct ip_prefix_list **, const union olsr_ip_addr *, uint8_t);

  struct ip_prefix_list *ip_prefix_list_find(struct ip_prefix_list *, const union olsr_ip_addr *net, uint8_t prefix_len);

/*
 * Interface to parser
 */

  int olsrd_parse_cnf(const char *);

  int olsrd_sanity_check_cnf(struct olsrd_config *);

  void olsrd_free_cnf(struct olsrd_config **);

  void olsrd_print_cnf(struct olsrd_config *);

  void olsrd_cfgfile_init(void);

  void olsrd_cfgfile_cleanup(void);

  void olsrd_write_cnf_autobuf(struct autobuf *out, struct olsrd_config *cnf);

  void olsrd_write_cnf_autobuf_uncached(struct autobuf *out, struct olsrd_config *cnf);

  int olsrd_write_cnf(struct olsrd_config *, const char *);

  struct if_config_options *get_default_if_config(void);

  struct olsrd_config *olsrd_get_default_cnf(char * configuration_file);

#if defined _WIN32
  void win32_stdio_hack(unsigned int);

  void *win32_olsrd_malloc(size_t size);

  void win32_olsrd_free(void *ptr);
#endif /* defined _WIN32 */

  /*
   * Smart-Gateway uplink/downlink accessors
   */

  static INLINE void set_smart_gateway_bandwidth_zero(struct olsrd_config *cnf) {
    cnf->smart_gateway_bandwidth_zero = !cnf->smart_gw_uplink || !cnf->smart_gw_downlink;
  }

  static INLINE void smartgw_set_uplink(struct olsrd_config *cnf, uint32_t uplink) {
    cnf->smart_gw_uplink = uplink;
    set_smart_gateway_bandwidth_zero(cnf);
  }

  static INLINE void smartgw_set_downlink(struct olsrd_config *cnf, uint32_t downlink) {
    cnf->smart_gw_downlink = downlink;
    set_smart_gateway_bandwidth_zero(cnf);
  }

  static INLINE bool smartgw_is_zero_bandwidth(struct olsrd_config *cnf) {
    return cnf->smart_gateway_bandwidth_zero;
  }

#if defined __cplusplus
}
#endif /* defined __cplusplus */
#endif /* _OLSRD_CFGPARSER_H */

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
