/*
 *  TCP networking functions
 *
 *  Copyright (C) 2006-2007  Christophe Devine
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License, version 2.1 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA
 */

#include "xyssl/config.h"

#if defined(XYSSL_NET_C)

#include "xyssl/net.h"

#if defined(WIN32) || defined(_WIN32_WCE)

#include <winsock2.h>
#include <windows.h>

#if defined(_WIN32_WCE)
#pragma comment( lib, "ws2.lib" )
#else
#pragma comment( lib, "ws2_32.lib" )
#endif

#define read(fd,buf,len)        recv(fd,buf,len,0)
#define write(fd,buf,len)       send(fd,buf,len,0)
#define close(fd)               closesocket(fd)

static int wsa_init_done = 0;

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>

#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/*
 * htons() is not always available
 */
static unsigned short net_htons( int port )
{
    unsigned char buf[4];

    buf[0] = (unsigned char)( port >> 8 );
    buf[1] = (unsigned char)( port      );
    buf[2] = buf[3] = 0;

    return( *(unsigned short *) buf );
}

/*
 * Initiate a TCP connection with host:port
 */
int net_connect( int *fd, char *host, int port )
{
    struct sockaddr_in server_addr;
    struct hostent *server_host;

    struct addrinfo *res, *r;
    int s, sock;

#if defined(WIN32) || defined(_WIN32_WCE)
    WSADATA wsaData;

    if( wsa_init_done == 0 )
    {
        if( WSAStartup( MAKEWORD(2,0), &wsaData ) == SOCKET_ERROR )
            return( XYSSL_ERR_NET_SOCKET_FAILED );

        wsa_init_done = 1;
    }
#else
    signal( SIGPIPE, SIG_IGN );
#endif

    s = getaddrinfo(host, NULL, NULL, &res);
    if (s != 0)
    	return (XYSSL_ERR_NET_SOCKET_FAILED);

//    if( ( server_host = gethostbyname( host ) ) == NULL )
//        return( XYSSL_ERR_NET_UNKNOWN_HOST );

    for (r = res; r != NULL; r = r->ai_next) {
	  *fd = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
	  if (*fd == -1)
		 continue;

	  if ( connect(*fd, r->ai_addr, r->ai_addrlen) != -1 )
		 break;		// success

	 close(*fd);
    }

    if (r == NULL)
	 return (XYSSL_ERR_NET_SOCKET_FAILED);

//    if( ( *fd = socket( AF_INET, SOCK_STREAM, IPPROTO_IP ) ) < 0 )
//        return( XYSSL_ERR_NET_SOCKET_FAILED );

/*    
    memcpy( (void *) &server_addr.sin_addr,
            (void *) server_host->h_addr,
                     server_host->h_length );

    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = net_htons( port );

    if( connect( *fd, (struct sockaddr *) &server_addr,
                 sizeof( server_addr ) ) < 0 )
    {
        close( *fd );
        return( XYSSL_ERR_NET_CONNECT_FAILED );
    }
*/

    return( 0 );
}

/*
 * Create a listening socket on bind_ip:port
 */
int net_bind( int *fd, char *bind_ip, int port )
{
    int n, c[4];
    struct sockaddr_in server_addr;

#if defined(WIN32) || defined(_WIN32_WCE)
    WSADATA wsaData;

    if( wsa_init_done == 0 )
    {
        if( WSAStartup( MAKEWORD(2,0), &wsaData ) == SOCKET_ERROR )
            return( XYSSL_ERR_NET_SOCKET_FAILED );

        wsa_init_done = 1;
    }
#else
    signal( SIGPIPE, SIG_IGN );
#endif

    if( ( *fd = socket( AF_INET, SOCK_STREAM, IPPROTO_IP ) ) < 0 )
        return( XYSSL_ERR_NET_SOCKET_FAILED );

    n = 1;
    setsockopt( *fd, SOL_SOCKET, SO_REUSEADDR,
                (const char *) &n, sizeof( n ) );

    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = net_htons( port );

    if( bind_ip != NULL )
    {
        memset( c, 0, sizeof( c ) );
        sscanf( bind_ip, "%d.%d.%d.%d", &c[0], &c[1], &c[2], &c[3] );

        for( n = 0; n < 4; n++ )
            if( c[n] < 0 || c[n] > 255 )
                break;

        if( n == 4 )
            server_addr.sin_addr.s_addr =
                ( c[0] << 24 ) | ( c[1] << 16 ) |
                ( c[2] <<  8 ) | ( c[3]       );
    }

    if( bind( *fd, (struct sockaddr *) &server_addr,
              sizeof( server_addr ) ) < 0 )
    {
        close( *fd );
        return( XYSSL_ERR_NET_BIND_FAILED );
    }

    if( listen( *fd, 10 ) != 0 )
    {
        close( *fd );
        return( XYSSL_ERR_NET_LISTEN_FAILED );
    }

    return( 0 );
}

/*
 * Check if the current operation is blocking
 */
static int net_is_blocking( void )
{
#if defined(WIN32) || defined(_WIN32_WCE)
    return( WSAGetLastError() == WSAEWOULDBLOCK );
#else
    switch( errno )
    {
#if defined EAGAIN
        case EAGAIN:
#endif
#if defined EWOULDBLOCK && EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
            return( 1 );
    }
    return( 0 );
#endif
}

/*
 * Accept a connection from a remote client
 */
int net_accept( int bind_fd, int *client_fd, void *client_ip )
{
    struct sockaddr_in client_addr;

#if defined(__socklen_t_defined)
    socklen_t n = (socklen_t) sizeof( client_addr );
#else
    int n = (int) sizeof( client_addr );
#endif

    *client_fd = accept( bind_fd, (struct sockaddr *)
                         &client_addr, &n );

    if( *client_fd < 0 )
    {
        if( net_is_blocking() != 0 )
            return( XYSSL_ERR_NET_TRY_AGAIN );

        return( XYSSL_ERR_NET_ACCEPT_FAILED );
    }

    if( client_ip != NULL )
        memcpy( client_ip, &client_addr.sin_addr.s_addr,
                    sizeof( client_addr.sin_addr.s_addr ) );

    return( 0 );
}

/*
 * Set the socket blocking or non-blocking
 */
int net_set_block( int fd )
{
#if defined(WIN32) || defined(_WIN32_WCE)
    long n = 0;
    return( ioctlsocket( fd, FIONBIO, &n ) );
#else
    return( fcntl( fd, F_SETFL, fcntl( fd, F_GETFL ) & ~O_NONBLOCK ) );
#endif
}

int net_set_nonblock( int fd )
{
#if defined(WIN32) || defined(_WIN32_WCE)
    long n = 1;
    return( ioctlsocket( fd, FIONBIO, &n ) );
#else
    return( fcntl( fd, F_SETFL, fcntl( fd, F_GETFL ) | O_NONBLOCK ) );
#endif
}

/*
 * Portable usleep helper
 */
void net_usleep( unsigned long usec )
{
    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = usec;
    select( 0, NULL, NULL, NULL, &tv );
}

/*
 * Read at most 'len' characters
 */
int net_recv( void *ctx, unsigned char *buf, int len )
{ 
    int ret = read( *((int *) ctx), buf, len );

    if( len > 0 && ret == 0 )
        return( XYSSL_ERR_NET_CONN_RESET );

    if( ret < 0 )
    {
        if( net_is_blocking() != 0 )
            return( XYSSL_ERR_NET_TRY_AGAIN );

#if defined(WIN32) || defined(_WIN32_WCE)
        if( WSAGetLastError() == WSAECONNRESET )
            return( XYSSL_ERR_NET_CONN_RESET );
#else
        if( errno == EPIPE || errno == ECONNRESET )
            return( XYSSL_ERR_NET_CONN_RESET );

        if( errno == EINTR )
            return( XYSSL_ERR_NET_TRY_AGAIN );
#endif

        return( XYSSL_ERR_NET_RECV_FAILED );
    }

    return( ret );
}

/*
 * Write at most 'len' characters
 */
int net_send( void *ctx, unsigned char *buf, int len )
{
    int ret = write( *((int *) ctx), buf, len );

    if( ret < 0 )
    {
        if( net_is_blocking() != 0 )
            return( XYSSL_ERR_NET_TRY_AGAIN );

#if defined(WIN32) || defined(_WIN32_WCE)
        if( WSAGetLastError() == WSAECONNRESET )
            return( XYSSL_ERR_NET_CONN_RESET );
#else
        if( errno == EPIPE || errno == ECONNRESET )
            return( XYSSL_ERR_NET_CONN_RESET );

        if( errno == EINTR )
            return( XYSSL_ERR_NET_TRY_AGAIN );
#endif

        return( XYSSL_ERR_NET_SEND_FAILED );
    }

    return( ret );
}

/*
 * Gracefully close the connection
 */
void net_close( int fd )
{
    shutdown( fd, 2 );
    close( fd );
}

#endif
