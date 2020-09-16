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

#ifndef __TCPSTREAMER_H__
#define __TCPSTREAMER_H__

#include "config.h"

#include <stdio.h>
#include <string>
#include <time.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <map>
#include <vector>

#include "util.h"
#include "packet.h"
#include "packetstream.h"

// Global in kismet_drone.cc
extern int silent;

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define RING_LEN (MAX_PACKET_LEN * 8)
// #define RING_LEN (MAX_PACKET_LEN)

// Allowed IP information
struct client_ipblock {
    // Allowed network
    in_addr network;
    // Allowed mask
    in_addr mask;
};

class TcpStreamer {
public:
    TcpStreamer();
    ~TcpStreamer();

    int Valid() { return sv_valid; };

    int Setup(unsigned int in_max_clients, short int in_port, vector<client_ipblock *> *in_ipb);

    unsigned int MergeSet(fd_set in_set, unsigned int in_max, fd_set *out_set,
	    fd_set *outw_set);

    int FetchDescriptor() { return serv_fd; }

    void Kill(int in_fd);

    int Poll(fd_set& in_rset, fd_set& in_wset);

    void Shutdown();

    char *FetchError() { return errstr; }

    inline int isClient(int fd) { return FD_ISSET(fd, &client_fds); }

    int WritePacket(const kis_packet *in_packet);
    int WriteVersion(int in_fd);

    // How many clients are connected?
    int FetchNumClients();

protected:
    int Accept();

    char errstr[1024];

    // Active server
    int sv_valid;

    unsigned int max_clients;

    // Clients to be written out to
    map<int, KisRingBuffer *> droneclients;

    // Server info
    short int port;
    char hostname[MAXHOSTNAMELEN];

    vector<client_ipblock *> *ipblock_vec;

    // Socket items
    unsigned int serv_fd;
    struct sockaddr_in serv_sock;

    // Master list of Fd's
    fd_set server_fds;

    fd_set client_fds;

    unsigned int max_fd;
};

#endif
