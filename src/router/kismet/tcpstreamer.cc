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

#include "config.h"

#include <stdio.h>
#include "tcpstreamer.h"

// Near-useless constructor, allthe work is done in Setup(...)
TcpStreamer::TcpStreamer()
{
    sv_valid = 0;

    // Set the serv_fd to 0 as well so we don't close() an unopen sock
    serv_fd = 0;

    max_fd = 0;
}

TcpStreamer::~TcpStreamer()
{
    // Invalidate us immediately
    sv_valid = 0;
}

void TcpStreamer::Shutdown() {
    // Invalidate us immediately
    sv_valid = 0;

    if (serv_fd)
        close(serv_fd);
}

// Bind to a port and optional hostname/interface
int TcpStreamer::Setup(unsigned int in_max_clients, short int in_port,
                       vector<client_ipblock *> *in_ipb) {
    max_clients = in_max_clients;
    ipblock_vec = in_ipb;

    // If we don't have a host to bind to, try to find one
    // Die violently -- If we can't bind a socket, we're useless
    if (gethostname(hostname, MAXHOSTNAMELEN) < 0) {
        snprintf(errstr, 1024, "TcpStreamer gethostname() failed: %s", strerror(errno));
        return (-1);
    }
    // Copy the port to our local data
    port = in_port;

    // Set up our socket
    //bzero(&serv_sock, sizeof(serv_sock));
    memset(&serv_sock, 0, sizeof(serv_sock));
    serv_sock.sin_family = AF_INET;
    serv_sock.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_sock.sin_port = htons(port);

    // Debug("Server::Setup calling socket()");
    if ((serv_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        snprintf(errstr, 1024, "TcpStreamer socket() failed: %s", strerror(errno));
        return (-3);
    }

    // Debug("Server::Setup setting socket option SO_REUSEADDR");
    int i = 2;
    if (setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) == -1) {
        snprintf(errstr, 1024, "TcpStreamer setsockopt() failed: %s", strerror(errno));
        return (-4);
    }

    // Bind the named socket
    // Debug("Server::Setup calling bind()");
    if (bind(serv_fd, (struct sockaddr *) &serv_sock, sizeof(serv_sock)) < 0) {
        snprintf(errstr, 1024, "TcpStreamer bind() failed: %s", strerror(errno));
        return (-5);
    }

    // Start up listening to the socket
    if (listen(serv_fd, 5) < 0) {
        snprintf(errstr, 1024, "TcpStreamer listen() failed: %s", strerror(errno));
        return (-6);
    }

    // Zero the FD's
    FD_ZERO(&server_fds);
    FD_ZERO(&client_fds);

    // Set the server socket
    FD_SET(serv_fd, &server_fds);
    sv_valid = 1;

    if (serv_fd > max_fd)
        max_fd = serv_fd;

    return (1);
}

// Make one useable fd_set from the fd's flagged for system-wide monitoring
// and from the fd's flagged locally for clients connecting to us.  This lets
// us do 1 big unified select().
unsigned int TcpStreamer::MergeSet(fd_set in_set, unsigned int in_max,
                                   fd_set *out_set, fd_set *outw_set) {
    unsigned int max;

    FD_ZERO(out_set);
    FD_ZERO(outw_set);

    if (in_max < max_fd) {
        max = max_fd;
    } else {
        max = in_max;
        max_fd = max;
    }

    for (unsigned int x = 0; x <= max; x++) {
        if (FD_ISSET(x, &in_set) || FD_ISSET(x, &server_fds)) {
            FD_SET(x, out_set);
        }
        if (FD_ISSET(x, &client_fds) && droneclients[x]->FetchLen() != 0) {
            FD_SET(x, outw_set);
        }
    }

    return max;
}

int TcpStreamer::Poll(fd_set& in_rset, fd_set& in_wset)
{
    if (!sv_valid)
        return -1;

    // Look for new FD's
    int accept_fd = 0;
    if (FD_ISSET(serv_fd, &in_rset))
        if ((accept_fd = Accept()) < 0)
            return -1;

    // Write queued data from our ring buffers out and kill anything
    // that complains about it
    uint8_t *dptr = NULL;
    int dlen, ret;
    for (unsigned int x = 0; x <= max_fd; x++) {
        // Soak any data in the read buffer
        if (FD_ISSET(x, &in_rset) && FD_ISSET(x, &client_fds)) {
            int8_t buf;
            ret = read(x, &buf, 1);
            if (ret <= 0 && errno != EAGAIN) {
                if (!silent)
                    fprintf(stderr, "WARNING: Killing client fd %d read error %d: %s\n",
                            x, errno, strerror(errno));

                Kill(x);
                return 0;
            }

            if (ret == 1) {
                // printf("-- debug - got flush request for client stream.\n");
                if (buf == STREAM_COMMAND_FLUSH) {
                    // Flush this ring buffer, loose all packets queued
                    delete droneclients[x];
                    droneclients[x] = new KisRingBuffer(RING_LEN);
                }
            }
        }

        if (FD_ISSET(x, &in_wset) && FD_ISSET(x, &client_fds)) {
            droneclients[x]->FetchPtr(&dptr, &dlen);

            // debug
            // printf("got %d from ring: ", dlen);
            /*
            int pos;
            for (pos = 0; pos < dlen && pos < 10; pos++)
                printf("%02X ", dptr[pos]);
            printf(" ... ");

            for (pos = kismax(pos, dlen - 10); pos < dlen; pos++)
                printf("%02X ", dptr[pos]);
                */
            
            if ((ret = write(x, dptr, dlen)) <= 0) {
                if (!silent)
                    fprintf(stderr, "WARNING: Killing client fd %d write error %d: %s\n",
                            x, errno, strerror(errno));

                Kill(x);
                continue;
            } else {
                // printf(" wrote %d\n", ret);
                droneclients[x]->MarkRead(ret);
            }
        }
    }

    return accept_fd;
}

// Accept an incoming connection
int TcpStreamer::Accept() {
    unsigned int new_fd;
    struct sockaddr_in client_addr;
#ifdef HAVE_SOCKLEN_T
    socklen_t client_len;
#else
    int client_len;
#endif

    //bzero(&client_addr, sizeof(struct sockaddr_in));
    memset(&client_addr, 0, sizeof(struct sockaddr_in));

    client_len = sizeof(struct sockaddr_in);

    // Accept should never block, thanks to select
    if ((new_fd = accept(serv_fd, (struct sockaddr *) &client_addr,
                         &client_len)) < 0) {
        snprintf(errstr, 1024, "TcpStreamer accept() failed: %s\n", strerror(errno));
        return -1;
    }

    char inhost[16];

    snprintf(inhost, 16, "%s", inet_ntoa(client_addr.sin_addr));

    int legal_ip = 0;
    for (unsigned int ibvi = 0; ibvi < ipblock_vec->size(); ibvi++) {
        if ((client_addr.sin_addr.s_addr & (*ipblock_vec)[ibvi]->mask.s_addr) == (*ipblock_vec)[ibvi]->network.s_addr) {
            legal_ip = 1;
            break;
        }
    }

    if (legal_ip == 0) {
        snprintf(errstr, 1024, "TcpStreamer accept() connect from untrusted host %s", inhost);
        close(new_fd);
        return -1;
    } else {
        snprintf(errstr, 1024, "%s", inhost);
    }

    if (FetchNumClients() >= (int) max_clients) {
        snprintf(errstr, 1024, "TcpStreamer accept() max clients already connected, rejecting %s",
                 inhost);
        close(new_fd);
        return -1;
    }

    if (new_fd > max_fd)
        max_fd = new_fd;

    // Set the file descriptor
    FD_SET(new_fd, &server_fds);
    FD_SET(new_fd, &client_fds);

    // Allocate a ring buffer
    droneclients[new_fd] = new KisRingBuffer(RING_LEN);

    WriteVersion(new_fd);

    return new_fd;
}

// Kill a connection
void TcpStreamer::Kill(int in_fd) {
    // remove us from the fd lists
    FD_CLR(in_fd, &server_fds);
    FD_CLR(in_fd, &client_fds);

    // Close the fd
    if (in_fd)
        close(in_fd);

    // Kill the ring buffer
    delete droneclients[in_fd];
}

int TcpStreamer::WriteVersion(int in_fd) {
    stream_frame_header hdr;
    stream_version_packet vpkt;

    hdr.frame_sentinel = (uint32_t) htonl(STREAM_SENTINEL);
    hdr.frame_type = STREAM_FTYPE_VERSION;
    hdr.frame_len = (uint32_t) htonl(sizeof(struct stream_version_packet));

    vpkt.drone_version = (uint16_t) htons(STREAM_DRONE_VERSION);

    if (!FD_ISSET(in_fd, &client_fds))
        return -1;

    // See if we have room in the ring and drop out if we don't
    if (droneclients[in_fd]->InsertDummy(sizeof(struct stream_frame_header) +
                                         sizeof(struct stream_version_packet)) == 0) {
        if (!silent)
            fprintf(stderr, "WARNING: Client fd %d ring buffer full, version packet dropped.\n",
                    in_fd);

        return 0;
    }

    // Add the data to the ring
    if (droneclients[in_fd]->InsertData((uint8_t *) &hdr,
                                        sizeof(struct stream_frame_header)) == 0) {
        if (!silent)
            fprintf(stderr, "WARNING: Client fd %d failed to insert data into ring buffer, killing.\n",
                    in_fd);

        Kill(in_fd);
        return -1;
    }

    if (droneclients[in_fd]->InsertData((uint8_t *) &vpkt,
                                        sizeof(struct stream_version_packet)) == 0) {
        if (!silent)
            fprintf(stderr, "WARNING: Client fd %d failed to insert data into ring buffer, killing.\n",
                    in_fd);

        Kill(in_fd);
        return -1;
    }

    return 1;
}


int TcpStreamer::WritePacket(const kis_packet *in_packet) {

    if (in_packet->data == NULL)
        return 0;

    if (in_packet->caplen <= 0 || in_packet->len <= 0)
        return 0;

    stream_frame_header hdr;
    stream_packet_header packhdr;

    // Our one and only current frame type
    hdr.frame_sentinel = (uint32_t) htonl(STREAM_SENTINEL);
    hdr.frame_type = STREAM_FTYPE_PACKET;

    // Size of the header so we can skip the headers of unknown versions
    packhdr.header_len = (uint32_t) htonl(sizeof(struct stream_packet_header));
    packhdr.drone_version = (uint16_t) htons(STREAM_DRONE_VERSION);
    // Payload
    packhdr.len = (uint32_t) htonl(in_packet->len);
    packhdr.caplen = (uint32_t) htonl(in_packet->caplen);
    packhdr.tv_sec = (uint64_t) kis_hton64(in_packet->ts.tv_sec);
    packhdr.tv_usec = (uint64_t) kis_hton64(in_packet->ts.tv_usec);
    packhdr.quality = (uint16_t) htons(in_packet->quality);
    packhdr.signal = (uint16_t) htons(in_packet->signal);
    packhdr.noise = (uint16_t) htons(in_packet->noise);
    packhdr.error = in_packet->error;
    packhdr.channel = in_packet->channel;
    packhdr.carrier = in_packet->carrier;
    packhdr.encoding = in_packet->encoding;
    packhdr.datarate = (uint32_t) htonl(in_packet->datarate);
    // GPS first-iteration
    Float2Pair(in_packet->gps_lat, &packhdr.gps_lat, &packhdr.gps_lat_mant);
    Float2Pair(in_packet->gps_lon, &packhdr.gps_lon, &packhdr.gps_lon_mant);
    Float2Pair(in_packet->gps_alt, &packhdr.gps_alt, &packhdr.gps_alt_mant);
    Float2Pair(in_packet->gps_spd, &packhdr.gps_spd, &packhdr.gps_spd_mant);
    packhdr.gps_fix = in_packet->gps_fix;
    // endianswap
    packhdr.gps_lat = (int16_t) htons(packhdr.gps_lat);
    packhdr.gps_lat_mant = (int64_t) kis_hton64(packhdr.gps_lat_mant);
    packhdr.gps_lon = (int16_t) htons(packhdr.gps_lon);
    packhdr.gps_lon_mant = (int64_t) kis_hton64(packhdr.gps_lon_mant);
    packhdr.gps_alt = (int16_t) htons(packhdr.gps_alt);
    packhdr.gps_alt_mant = (int64_t) kis_hton64(packhdr.gps_alt_mant);
    packhdr.gps_spd = (int16_t) htons(packhdr.gps_spd);
    packhdr.gps_spd_mant = (int64_t) kis_hton64(packhdr.gps_spd_mant);

    // Sourcename
    memcpy(packhdr.sourcename, in_packet->sourcename, 32);

    hdr.frame_len = (uint32_t) htonl(sizeof(struct stream_packet_header) + in_packet->caplen);

    int nsent = 0;
    for (unsigned int x = serv_fd; x <= max_fd; x++) {
        if (!FD_ISSET(x, &client_fds))
            continue;

        // See if we have room in the ring and drop out if we don't
        if (droneclients[x]->InsertDummy(sizeof(struct stream_frame_header) +
                                         sizeof(stream_packet_header) +
                                         in_packet->caplen) == 0) {
            if (!silent)
                fprintf(stderr, "WARNING: Client fd %d ring buffer full, packet dropped.\n",
                        x);
            continue;
        }

        if (droneclients[x]->InsertData((uint8_t *) &hdr,
                                        sizeof(struct stream_frame_header)) == 0) {
            if (!silent)
                fprintf(stderr, "WARNING: Client fd %d failed to insert data into ring buffer, killing.\n",
                        x);
            Kill(x);
            continue;
        }

        if (droneclients[x]->InsertData((uint8_t *) &packhdr,
                                        sizeof(stream_packet_header)) == 0) {
            if (!silent)
                fprintf(stderr, "WARNING: Client fd %d failed to insert data into ring buffer, killing.\n",
                        x);
            Kill(x);
            continue;
        }

        if (droneclients[x]->InsertData((uint8_t *) in_packet->data,
                                        in_packet->caplen) == 0) {
            if (!silent)
                fprintf(stderr, "WARNING: Client fd %d failed to insert data into ring buffer, killing.\n",
                        x);
            Kill(x);
            continue;
        }

        nsent++;
    }

    return nsent;
}

int TcpStreamer::FetchNumClients() {
    int num = 0;

    for (unsigned int x = serv_fd + 1; x <= max_fd; x++) {
        if (FD_ISSET(x, &client_fds))
            num++;
    }

    return num;
}


