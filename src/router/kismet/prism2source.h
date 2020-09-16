/*
    This file is part of Kismet

    Kismet is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kismet is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Kismet; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __PRISM2SOURCE_H__
#define __PRISM2SOURCE_H__

#include "config.h"
#include "util.h"
#include "ifcontrol.h"

#ifdef HAVE_LINUX_NETLINK

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <unistd.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#include "packet.h"
#include "packetsource.h"

#define PRISM2_READ_TIMEOUT 100
#define MCAST_GRP_SNIFF 2
#define PRISM_ERR_TIMEOUT -2

class Prism2Source : public KisPacketSource {
public:
    Prism2Source(string in_name, string in_dev) : KisPacketSource(in_name, in_dev) { }

    int OpenSource();
    int CloseSource();

    int FetchDescriptor() { return fd; }

    int FetchPacket(kis_packet *packet, uint8_t *data, uint8_t *moddata);

    int SetChannel(unsigned int chan);

    int FetchChannel();

protected:
    typedef struct {
        uint32_t did __attribute__ ((packed));
        uint16_t status __attribute__ ((packed));
        uint16_t len __attribute__ ((packed));
        uint32_t data __attribute__ ((packed));
    } p80211item_t;

    typedef struct {
        uint32_t msgcode __attribute__ ((packed));
        uint32_t msglen __attribute__ ((packed));
        uint8_t devname[DEVNAME_LEN] __attribute__ ((packed));
        p80211item_t hosttime __attribute__ ((packed));
        p80211item_t mactime __attribute__ ((packed));
        p80211item_t channel __attribute__ ((packed));
        p80211item_t rssi __attribute__ ((packed));
        p80211item_t sq __attribute__ ((packed));
        p80211item_t signal __attribute__ ((packed));
        p80211item_t noise __attribute__ ((packed));
        p80211item_t rate __attribute__ ((packed));
        p80211item_t istx __attribute__ ((packed));
        p80211item_t frmlen __attribute__ ((packed));
    } sniff_packet_t;

    int Prism2Common(kis_packet *packet, uint8_t *data, uint8_t *moddata);

    int read_sock;
    int write_sock;
    int fd;

    uint8_t buffer[MAX_PACKET_LEN];
};

KisPacketSource *prism2source_registrant(string in_name, string in_device, char *in_err);
int monitor_wlanng_legacy(const char *in_dev, int initch, char *in_err, void **in_if);
int chancontrol_wlanng_legacy(const char *in_dev, int initch, char *in_err, 
                              void *in_ext);

#endif

#endif

