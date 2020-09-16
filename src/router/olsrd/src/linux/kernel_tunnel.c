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

#ifdef __linux__

#include "kernel_tunnel.h"
#include "kernel_routes.h"
#include "log.h"
#include "olsr_types.h"
#include "net_os.h"
#include "olsr_cookie.h"
#include "ipcalc.h"

#include <assert.h>

//ipip includes
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/ip.h>
#include <linux/if_tunnel.h>
#include <linux/version.h>

#if !defined LINUX_VERSION_CODE || !defined KERNEL_VERSION
  #error "Both LINUX_VERSION_CODE and KERNEL_VERSION need to be defined"
#endif /* !defined LINUX_VERSION_CODE || !defined KERNEL_VERSION */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
  #define LINUX_IPV6_TUNNEL
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24) */

#ifdef LINUX_IPV6_TUNNEL
#include <linux/ip6_tunnel.h>
#endif /* LINUX_IPV6_TUNNEL */

//ifup includes
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <net/if.h>

static bool store_iptunnel_state;
static struct olsr_cookie_info *tunnel_cookie;
static struct avl_tree tunnel_tree;

int olsr_os_init_iptunnel(const char * dev) {
  tunnel_cookie = olsr_alloc_cookie("iptunnel", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(tunnel_cookie, sizeof(struct olsr_iptunnel_entry));
  avl_init(&tunnel_tree, avl_comp_default);

  store_iptunnel_state = olsr_if_isup(dev);
  if (store_iptunnel_state) {
    return 0;
  }
  if (olsr_if_set_state(dev, true)) {
    return -1;
  }

  return olsr_os_ifip(if_nametoindex(dev), &olsr_cnf->main_addr, true);
}

void olsr_os_cleanup_iptunnel(const char * dev) {
  while (tunnel_tree.count > 0) {
    struct olsr_iptunnel_entry *t;

    /* kill tunnel */
    t = (struct olsr_iptunnel_entry *)avl_walk_first(&tunnel_tree);
    t->usage = 1;

    olsr_os_del_ipip_tunnel(t);
  }
  if (olsr_cnf->smart_gw_always_remove_server_tunnel || !store_iptunnel_state) {
    olsr_if_set_state(dev, false);
  }

  olsr_free_cookie(tunnel_cookie);
}

/**
 * creates an ipip tunnel (for ipv4 or ipv6)
 *
 * @param name interface name
 * @param target pointer to tunnel target IP, NULL if tunnel should be removed.
 * Must be of type 'in_addr_t *' for ipv4 and of type 'struct in6_addr *' for
 * ipv6
 * @return 0 if an error happened,
 *   if_index for successful created tunnel, 1 for successful deleted tunnel
 */
int os_ip_tunnel(const char *name, void *target) {
	struct ifreq ifr;
	int err;
	void * p;
	char buffer[INET6_ADDRSTRLEN];
	const char * tunName;
	struct ip_tunnel_parm p4;
#ifdef LINUX_IPV6_TUNNEL
	struct ip6_tnl_parm p6;
#endif /* LINUX_IPV6_TUNNEL */

	assert (name != NULL);

	memset(&ifr, 0, sizeof(ifr));

	if (olsr_cnf->ip_version == AF_INET) {
		p = &p4;
		tunName = TUNNEL_ENDPOINT_IF;

		memset(&p4, 0, sizeof(p4));
		p4.iph.version = 4;
		p4.iph.ihl = 5;
		p4.iph.ttl = 64;
		p4.iph.protocol = IPPROTO_IPIP;
		if (target) {
			p4.iph.daddr = *((in_addr_t *) target);
		}
		strscpy(p4.name, name, sizeof(p4.name));
	} else {
#ifdef LINUX_IPV6_TUNNEL
		p = (void *) &p6;
		tunName = TUNNEL_ENDPOINT_IF6;

		memset(&p6, 0, sizeof(p6));
		p6.proto = 0; /* any protocol */
		if (target) {
			p6.raddr = *((struct in6_addr *) target);
		}
		strscpy(p6.name, name, sizeof(p6.name));
#else /* LINUX_IPV6_TUNNEL */
		return 0;
#endif /* LINUX_IPV6_TUNNEL */
	}

	strscpy(ifr.ifr_name, target != NULL ? tunName : name, sizeof(ifr.ifr_name));
	ifr.ifr_ifru.ifru_data = p;

	if ((err = ioctl(olsr_cnf->ioctl_s, target != NULL ? SIOCADDTUNNEL : SIOCDELTUNNEL, &ifr))) {
		olsr_syslog(OLSR_LOG_ERR, "Cannot %s tunnel %s to %s: %s (%d)\n", target != NULL ? "add" : "remove", name,
				target != NULL ? inet_ntop(olsr_cnf->ip_version, target, buffer, sizeof(buffer)) : "-", strerror(errno),
				errno);
		return 0;
	}

	olsr_syslog(OLSR_LOG_INFO, "Tunnel %s %s, to %s", name, target != NULL ? "added" : "removed",
			target != NULL ? inet_ntop(olsr_cnf->ip_version, target, buffer, sizeof(buffer)) : "-");

	return target != NULL ? if_nametoindex(name) : 1;
}

/**
 * demands an ipip tunnel to a certain target. If no tunnel exists it will be created
 * @param target ip address of the target
 * @param transportV4 true if IPv4 traffic is used, false for IPv6 traffic
 * @param name pointer to name string buffer (length IFNAMSIZ)
 * @return NULL if an error happened, pointer to olsr_iptunnel_entry otherwise
 */
struct olsr_iptunnel_entry *olsr_os_add_ipip_tunnel(union olsr_ip_addr *target, bool transportV4 __attribute__ ((unused)), char *name) {
  struct olsr_iptunnel_entry *t;

  assert(olsr_cnf->ip_version == AF_INET6 || transportV4);

  t = (struct olsr_iptunnel_entry *)avl_find(&tunnel_tree, target);
  if (t == NULL) {
    int if_idx;
    struct ipaddr_str buf;

    if_idx = os_ip_tunnel(name, (olsr_cnf->ip_version == AF_INET) ? (void *) &target->v4.s_addr : (void *) &target->v6);
    if (if_idx == 0) {
      // cannot create tunnel
      olsr_syslog(OLSR_LOG_ERR, "Cannot create tunnel %s\n", name);
      return NULL;
    }

    if (olsr_if_set_state(name, true)) {
      os_ip_tunnel(name, NULL);
      return NULL;
    }

    /* set originator IP for tunnel */
    olsr_os_ifip(if_idx, &olsr_cnf->main_addr, true);

    t = olsr_cookie_malloc(tunnel_cookie);
    memcpy(&t->target, target, sizeof(*target));
    t->node.key = &t->target;

    strscpy(t->if_name, name, sizeof(t->if_name));
    t->if_index = if_idx;

    avl_insert(&tunnel_tree, &t->node, AVL_DUP_NO);
  }

  t->usage++;
  return t;
}

/**
 * Release an olsr ipip tunnel. Tunnel will be deleted
 * if this was the last user
 * @param t pointer to olsr_iptunnel_entry
 */
void olsr_os_del_ipip_tunnel(struct olsr_iptunnel_entry *t) {
  if (t->usage == 0) {
    return;
  }
  t->usage--;

  if (t->usage > 0) {
    return;
  }

  olsr_if_set_state(t->if_name, false);
  os_ip_tunnel(t->if_name, NULL);

  avl_delete(&tunnel_tree, &t->node);
  olsr_cookie_free(tunnel_cookie, t);
}
#endif /* __linux__ */
