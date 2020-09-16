/*
 * $Id: unixsock.h,v 1.1.1.1 2005/06/13 16:47:43 bogdan_iancu Exp $
 *
 * UNIX Domain Socket Interface
 *
 * Copyright (C) 2003 Juha Heinanen
 * Copyright (C) 2004 FhG FOKUS
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef _UNIXSOCK_H
#define _UNIXSOCK_H 1

/*
 * Register domain fifo functions
 */
int init_trusted_unixsock(void);


#endif /* _UNIXSOCK_H */
