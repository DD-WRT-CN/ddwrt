/*
 * $Id: parser_f.c,v 1.2 2005/06/16 11:37:54 miconda Exp $
 *
 * parser helper  functions
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
 */


#include  "parser_f.h"
#include "../ut.h"

/* returns pointer to next line or after the end of buffer */
char* eat_line(char* buffer, unsigned int len)
{
	char* nl;

	/* jku .. replace for search with a library function; not conforming
 		  as I do not care about CR
	*/
	nl=(char *)q_memchr( buffer, '\n', len );
	if ( nl ) { 
		if ( nl + 1 < buffer+len)  nl++;
		if (( nl+1<buffer+len) && * nl=='\r')  nl++;
	} else  nl=buffer+len;
	return nl;
}

