/* 
 * $Id: parse_nameaddr.h,v 1.2 2005/06/16 11:37:54 miconda Exp $
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 * History
 * --------
 * 2003-03-24 Created by janakj
 */

#ifndef PARSE_NAMEADDR_H
#define PARSE_NAMEADDR_H

#include <stdio.h>
#include "../str.h"

/*
 * Name-addr structure, see RFC3261 for more details
 */
typedef struct name_addr {
	str name;   /* Display name part */
	str uri;    /* Uri part without surrounding <> */
	int len;    /* Total length of the field (including all
		    * whitechars present in the parsed message */
} name_addr_t;


/*
 * Parse name-addr part, the given string can be longer,
 * parsing will stop when closing > is found
 */
int parse_nameaddr(str* _s, name_addr_t* _a);


/*
 * Print a name-addr structure, just for debugging
 */
void print_nameaddr(FILE* _o, name_addr_t* _a);


#endif /* PARSE_NAMEADDR_H */
