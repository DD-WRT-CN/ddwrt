/* lib/version.h.  Generated from version.h.in by configure.
 *
 * Quagga version
 * Copyright (C) 1997, 1999 Kunihiro Ishiguro
 * 
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#ifndef _ZEBRA_VERSION_H
#define _ZEBRA_VERSION_H

#ifdef GIT_VERSION
#include "gitversion.h"
#endif

#ifndef GIT_SUFFIX
#define GIT_SUFFIX ""
#endif
#ifndef GIT_INFO
#define GIT_INFO ""
#endif

#define QUAGGA_PROGNAME   "Quagga"

#define QUAGGA_VERSION     "1.2.4" GIT_SUFFIX

#define ZEBRA_BUG_ADDRESS "https://bugzilla.quagga.net"

#define QUAGGA_URL "http://www.quagga.net"

#define QUAGGA_COPYRIGHT "Copyright 1996-2005 Kunihiro Ishiguro, et al."

#define QUAGGA_CONFIG_ARGS "--host=arm-uclibc-linux --localstatedir=/var/run --libdir=/usr/tmp --enable-opaque-lsa --disable-nhrpd --enable-ospf-te --disable-ospfclient --enable-multipath=32 --enable-ipv6 --prefix=/usr --sysconfdir=/tmp --disable-ospf6d --enable-vtysh --enable-user=root --enable-group=root --disable-ospfapi --disable-isisd --disable-pimd --disable-nhrpd --enable-pie=no --with-libreadline=/home/paldier/ddwrt/src/router/readline CFLAGS=-fno-strict-aliasing -I/home/paldier/ddwrt/src/router/ -DNEED_PRINTF -fcommon -Drpl_malloc=malloc -Drpl_realloc=realloc -Os -pipe -mcpu=cortex-a9 -mtune=cortex-a9  -msoft-float -mfloat-abi=soft -fno-caller-saves -fno-plt   LDFLAGS=-L/home/paldier/ddwrt/src/router/readline/shlib -L/home/paldier/ddwrt/src/router/ncurses/lib -lncurses"

pid_t pid_output (const char *);

#ifndef HAVE_DAEMON
int daemon(int, int);
#endif

#endif /* _ZEBRA_VERSION_H */
