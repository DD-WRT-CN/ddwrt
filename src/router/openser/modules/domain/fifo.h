/*
 * $Id: fifo.h,v 1.1.1.1 2005/06/13 16:47:37 bogdan_iancu Exp $
 *
 * Header file for domain fifo functions
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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


#ifndef _FIFO_H
#define _FIFO_H


#define DOMAIN_RELOAD "domain_reload"
#define DOMAIN_DUMP "domain_dump"


int init_domain_fifo( void );
int reload_domain_table( void );


#endif
