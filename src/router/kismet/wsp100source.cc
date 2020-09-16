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

/*
    WSP100 support added by Chris Waters.  chris.waters@networkchemistry.com

    This supports the WSP100 device under Cygwin and Linux

    2/6/2003 - gherlein@herlein.com added TZSP NULL packet generation
    to support late model firmware requirement for this
    heartbeat packet.  Must be sent within every 32 sec and
    must come from the listen port of kismet.

*/

#include "config.h"

#include "wsp100source.h"

#ifdef HAVE_WSP100

#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>

#include "util.h"

// Straight-C callback
int Wsp100PokeSensor(Timetracker::timer_event *evt, void *call_parm) {
    // Poke it
    ((Wsp100Source *) call_parm)->PokeSensor();

    // And we want to use the event record over again - that is, obey the
    // recurrance field
    return 1;
}

// Build UDP listener code

int Wsp100Source::OpenSource() {
    char listenhost[1024];
    struct hostent *filter_host;

    // Device is handled as a host:port pair - remote host we accept data
    // from, local port we open to listen for it.  yeah, it's a little weird.
    if (sscanf(interface.c_str(), "%1024[^:]:%hd", listenhost, &port) < 2) {
        snprintf(errstr, 1024, "Couldn't parse host:port: '%s'", 
                 interface.c_str());
        return -1;
    }

    if ((filter_host = gethostbyname(listenhost)) == NULL) {
        snprintf(errstr, 1024, "Couldn't resolve host: '%s'", listenhost);
        return -1;
    }

    memcpy((char *) &filter_addr.s_addr, filter_host->h_addr_list[0], 
           filter_host->h_length);

    if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        snprintf(errstr, 1024, "Couldn't create UDP socket: %s", strerror(errno));
        return -1;
    }

    memset(&serv_sockaddr, 0, sizeof(serv_sockaddr));
    serv_sockaddr.sin_family = AF_INET;
    serv_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_sockaddr.sin_port = htons(port);

    if (bind(udp_sock, (struct sockaddr *) &serv_sockaddr, sizeof(serv_sockaddr)) < 0) {
        snprintf(errstr, 1024, "Couldn't bind to UDP socket: %s", strerror(errno));
        return -1;
    }

    paused = 0;

    valid = 1;

    num_packets = 0;

    // Register 'poke' events
    poke_event_id = timetracker->RegisterTimer(TZSP_NULL_PACKET_SLICE, NULL, 1,
                                               &Wsp100PokeSensor, (void *) this);

    return 1;
}

int Wsp100Source::CloseSource() {
    if (valid)
        close(udp_sock);

    valid = 0;

    if (timetracker != NULL)
        timetracker->RemoveTimer(poke_event_id);

    return 1;
}

int Wsp100Source::FetchPacket(kis_packet *packet, uint8_t *data, uint8_t *moddata) {
    if (valid == 0)
        return 0;

    struct sockaddr_in cli_sockaddr;
#ifdef HAVE_SOCKLEN_T
    socklen_t cli_len;
#else
    int cli_len;
#endif
    cli_len = sizeof(cli_sockaddr);
    memset(&cli_sockaddr, 0, sizeof(cli_sockaddr));

    if ((read_len = recvfrom(udp_sock, (char *) data, MAX_PACKET_LEN, 0,
                             (struct sockaddr *) &cli_sockaddr, &cli_len)) < 0) {
        if (errno != EINTR) {
            snprintf(errstr, 1024, "recvfrom() error %d (%s)", errno, strerror(errno));
            return -1;
        }
    }

    // Find out if it came from an IP associated with our target sensor system
    if (cli_sockaddr.sin_addr.s_addr != filter_addr.s_addr)
        return 0;

    if (paused || Wsp2Common(packet, data, moddata) == 0) {
        return 0;
    }

    num_packets++;

    snprintf(packet->sourcename, 32, "%s", name.c_str());
    packet->parm = parameters;

    return(packet->caplen);
}

int Wsp100Source::Wsp2Common(kis_packet *packet, uint8_t *data, uint8_t *moddata) {
    memset(packet, 0, sizeof(kis_packet));

    uint16_t datalink_type = 0;
    datalink_type = kptoh16(&data[2]);

    if (datalink_type != KWTAP_ENCAP_IEEE_802_11) {
        return 0;
    }

    // Iterate through the dynamic list of tags
    uint8_t pos = 4;
    while (pos < read_len) {
        uint8_t tag, len = 0, bail = 0;
        tag = data[pos++];

        switch (tag) {
        case WSP100_TAG_PAD:
            len = 0;
            break;
        case WSP100_TAG_END:
            len = 0;
            bail = 1;
            break;
        case WSP100_TAG_RADIO_SIGNAL:
            len = data[pos++];
            packet->signal = data[pos];
            break;
        case WSP100_TAG_RADIO_NOISE:
            len = data[pos++];
            packet->noise = data[pos];
            break;
        case WSP100_TAG_RADIO_RATE:
            // We don't handle the rate yet
            len = data[pos++];
            break;
        case WSP100_TAG_RADIO_TIME:
            len = data[pos++];

            /*
             Either the packet timestamp or my decoding of it is broken, so put our
             own timestamp in

             time_sec = kptoh32(&data[pos]);
             in_header->ts.tv_sec = time_sec;
             */

            gettimeofday(&packet->ts, NULL);

            break;
        case WSP100_TAG_RADIO_MSG:
            // We don't really care about this since we get the packet type from
            // the packet contents later
            len = data[pos++];
            break;
        case WSP100_TAG_RADIO_CF:
            // What does this mean?  Ignore it for now.
            len = data[pos++];
            break;
        case WSP100_TAG_RADIO_UNDECR:
            // We don't really care about this, either
            len = data[pos++];
            break;
        case WSP100_TAG_RADIO_FCSERR:
            len = data[pos++];
            packet->error = data[pos];
            break;
        case WSP100_TAG_RADIO_CHANNEL:
            // We get this off the other data inside the packets so we ignore this...
            len = data[pos++];
            break;
        default:
            // Unknown tag, try to keep going by getting the length and skipping
            len = data[pos++];
            break;
        }

        pos += len;

        if (bail)
            break;
    }

    packet->caplen = read_len - pos;
    packet->len = packet->caplen;

    packet->data = data;
    packet->moddata = moddata;
    packet->modified = 0;

    // We use the GPS coordinates of the capturing server
    if (gpsd != NULL) {
        gpsd->FetchLoc(&packet->gps_lat, &packet->gps_lon, &packet->gps_alt,
                       &packet->gps_spd, &packet->gps_heading, &packet->gps_fix);
    }

    memcpy(packet->data, data + pos, packet->caplen);

    packet->carrier = carrier_80211b;

    return 1;

}

void Wsp100Source::PokeSensor() {
    struct sockaddr_in poke_sockaddr;
    uint8_t null_frame[4] = TZSP_NULL_PACKET;

    memset(&poke_sockaddr, 0, sizeof(poke_sockaddr));
    poke_sockaddr.sin_family = AF_INET;
    poke_sockaddr.sin_addr.s_addr = filter_addr.s_addr;
    poke_sockaddr.sin_port = htons(TZSP_PORT);
    sendto(udp_sock, &null_frame, sizeof(null_frame), 0, (struct sockaddr *) &poke_sockaddr,
           sizeof(poke_sockaddr));
}

KisPacketSource *wsp100source_registrant(string in_name, string in_device,
                                         char *in_err) {
    return new Wsp100Source(in_name, in_device);
}

int monitor_wsp100(const char *in_dev, int initch, char *in_err, void **in_if) {
    // Split the device
    vector<string> wsp100_bits;
    char cmdline[2048];

    wsp100_bits = StrTokenize(in_dev, ":");

    if (wsp100_bits.size() < 3) {
        snprintf(in_err, STATUS_MAX, "Malformed wsp100 device string '%s', should be "
                 "localip:localport:remoteip", in_dev);
        return -1;
    }

    // sanitize it
    for (unsigned int x = 0; x < wsp100_bits[0].size(); x++) {
        if (!isdigit(wsp100_bits[0][x]) && wsp100_bits[0][x] != '.') {
            snprintf(in_err, STATUS_MAX, "Malformed wsp100 localip '%s', should be "
                     "x.x.x.x", wsp100_bits[0].c_str());
            return -1;
        }
    }
    for (unsigned int x = 0; x < wsp100_bits[1].size(); x++) {
        if (!isdigit(wsp100_bits[0][x])) {
            snprintf(in_err, STATUS_MAX, "Malformed wsp100 localport '%s'",
                     wsp100_bits[0].c_str());
            return -1;
        }
    }
    for (unsigned int x = 0; x < wsp100_bits[2].size(); x++) {
        char bit = wsp100_bits[0][x];
        if (!isalnum(bit) && bit != '.' && bit != '-' && bit != '_') {
            snprintf(in_err, STATUS_MAX, "Malformed wsp100 remoteip '%s', should be "
                     "x.x.x.x or domain name", wsp100_bits[2].c_str());
            return -1;
        }
    }

    // Do system calls to talk snmp.
    // sendor.longhostaddress
    snprintf(cmdline, 2048, "snmpset -v1 -c public %s .1.3.6.1.4.1.14422.1.1.5 a %s",
             wsp100_bits[2].c_str(), wsp100_bits[0].c_str());
    if (ExecSysCmd(cmdline, in_err) < 0)
        return -1;
    // sensor.channel
    snprintf(cmdline, 2048, "snmpset -v 1 -c public %s .1.3.6.1.4.1.14422.1.3.1 i %d",
             wsp100_bits[2].c_str(), initch);
    if (ExecSysCmd(cmdline, in_err) < 0)
        return -1;
    // sensor.serverport
    snprintf(cmdline, 2048, "snmpset -v 1 -c public %s .1.3.6.1.4.1.14422.1.4.1 i %s",
             wsp100_bits[2].c_str(), wsp100_bits[1].c_str());
    if (ExecSysCmd(cmdline, in_err) < 0)
        return -1;
    // sensor.enable
    snprintf(cmdline, 2038, "snmpset -v 1 -c public %s .1.3.6.1.4.1.14422.1.1.4 i 1",
             wsp100_bits[2].c_str());
    if (ExecSysCmd(cmdline, in_err) < 0)
        return -1;

    return 0;
}

int chancontrol_wsp100(const char *in_dev, int in_ch, char *in_err, void *in_ext) {
    fprintf(stderr, "Need to implement wsp100 channel change...\n");
    return 0;
}

// wsp100
#endif
