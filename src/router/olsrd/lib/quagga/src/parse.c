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

/* -------------------------------------------------------------------------
 * File               : parse.c
 * Description        : functions to parse received zebra packets
 * ------------------------------------------------------------------------- */

#include "defs.h"
#include "olsr.h"

#include "common.h"
#include "packet.h"
#include "client.h"
#include "parse.h"

static void free_zroute(struct zroute *);
static struct zroute *zparse_route(unsigned char *);

static void
free_zroute(struct zroute *r)
{

  if(r->ifindex_num)
    free(r->ifindex);
  if(r->nexthop_num)
    free(r->nexthop);

}

static struct zroute
*zparse_route(unsigned char *opt)
{
  struct zroute *r;
  int c;
  size_t size;
  uint16_t length;
  unsigned char *pnt;

  memcpy(&length, opt, sizeof length);
  length = ntohs (length);

  r = olsr_malloc(sizeof *r, "QUAGGA: New zebra route");
  pnt = &opt[3];
  switch (zebra.version) {
  case 0:
    break;
  case 1:
  case 2:
    pnt = &opt[6];
    break;
  default:
    olsr_exit("QUAGGA: Unsupported zebra packet version", EXIT_FAILURE);
    break;
  }
  r->type = *pnt++;
  r->flags = *pnt++;
  r->message = *pnt++;
  r->prefixlen = *pnt++;
  size = (r->prefixlen + 7) / 8;
  memset(&r->prefix, 0, sizeof r->prefix);
  if (olsr_cnf->ip_version == AF_INET)
    memcpy(&r->prefix.v4.s_addr, pnt, size);
  else
    memcpy(r->prefix.v6.s6_addr, pnt, size);
  pnt += size;

  if (r->message & ZAPI_MESSAGE_NEXTHOP) {
    r->nexthop_num = *pnt++;
    r->nexthop = olsr_malloc((sizeof *r->nexthop) * r->nexthop_num, "QUAGGA: New zebra route nexthop");
    for (c = 0; c < r->nexthop_num; c++) {
      if (olsr_cnf->ip_version == AF_INET) {
        memcpy(&r->nexthop[c].v4.s_addr, pnt, sizeof r->nexthop[c].v4.s_addr);
        pnt += sizeof r->nexthop[c].v4.s_addr;
      } else {
        memcpy(r->nexthop[c].v6.s6_addr, pnt, sizeof r->nexthop[c].v6.s6_addr);
        pnt += sizeof r->nexthop[c].v6.s6_addr;
      }
    }
  }

  if (r->message & ZAPI_MESSAGE_IFINDEX) {
    r->ifindex_num = *pnt++;
    r->ifindex = olsr_malloc(sizeof(uint32_t) * r->ifindex_num, "QUAGGA: New zebra route ifindex");
    for (c = 0; c < r->ifindex_num; c++) {
      memcpy(&r->ifindex[c], pnt, sizeof r->ifindex[c]);
      r->ifindex[c] = ntohl (r->ifindex[c]);
      pnt += sizeof r->ifindex[c];
    }
  }
  switch (zebra.version) {
  case 0:
  case 1:
  case 2:
    break;
  default:
    olsr_exit("QUAGGA: Unsupported zebra packet version", EXIT_FAILURE);
    break;
  }

  if (r->message & ZAPI_MESSAGE_DISTANCE) {
    r->distance = *pnt++;
  }

// Quagga v0.98.6 BUG workaround: metric is always sent by zebra
// even without ZAPI_MESSAGE_METRIC message.
  if ((r->message & ZAPI_MESSAGE_METRIC) || !zebra.version) {
    memcpy(&r->metric, pnt, sizeof r->metric);
    r->metric = ntohl(r->metric);
    pnt += sizeof r->metric;
  }

  if (pnt - opt != length) {
    olsr_exit("QUAGGA: Length does not match", EXIT_FAILURE);
  }

  return r;
}

void
zparse(void *foo __attribute__ ((unused)))
{
  unsigned char *data, *f;
  uint16_t command;
  uint16_t length;
  ssize_t len;
  struct zroute *route;

  if (!(zebra.status & STATUS_CONNECTED)) {
    zclient_reconnect();
    return;
  }
  data = zclient_read(&len);
  if (data) {
    f = data;
    do {
      length = ntohs(*((uint16_t *)(void *) f));
      if (!length) { // something weird happened
        olsr_exit("QUAGGA: Zero message length", EXIT_FAILURE);
      }

      /* Zebra Protocol Header
       *
       * Version 0: 2 bytes length, 1 byte command
       *
       * Version 1: 2 bytes length, 1 byte marker, 1 byte version, 2 bytes command
       */
      switch (zebra.version) {
        case 0:
          command = f[2];
          break;

        case 1:
        case 2:
          if ((f[2] != ZEBRA_HEADER_MARKER) || (f[3] != zebra.version)) {
            olsr_exit("QUAGGA: Invalid zebra header received", EXIT_FAILURE);
          }

          command = ntohs(*((uint16_t *)(void *) &f[4]));
          break;

        default:
          olsr_exit("QUAGGA: Unsupported zebra packet version", EXIT_FAILURE);
          break;
      }

      if (olsr_cnf->ip_version == AF_INET) {
        switch (command) {
          case ZEBRA_IPV4_ROUTE_ADD:
            route = zparse_route(f);
            ip_prefix_list_add(&olsr_cnf->hna_entries, &route->prefix, route->prefixlen);
            free_zroute(route);
            free(route);
            break;

          case ZEBRA_IPV4_ROUTE_DELETE:
            route = zparse_route(f);
            ip_prefix_list_remove(&olsr_cnf->hna_entries, &route->prefix, route->prefixlen);
            free_zroute(route);
            free(route);
            break;

          default:
            break;
        }
      } else {
        switch (command) {
          case ZEBRA_IPV6_ROUTE_ADD:
            route = zparse_route(f);
            ip_prefix_list_add(&olsr_cnf->hna_entries, &route->prefix, route->prefixlen);
            free_zroute(route);
            free(route);
            break;

          case ZEBRA_IPV6_ROUTE_DELETE:
            route = zparse_route(f);
            ip_prefix_list_remove(&olsr_cnf->hna_entries, &route->prefix, route->prefixlen);
            free_zroute(route);
            free(route);
            break;

          default:
            break;
        }
      }

      f += length;
    }
    while ((f - data) < len);

    free(data);
  }
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
