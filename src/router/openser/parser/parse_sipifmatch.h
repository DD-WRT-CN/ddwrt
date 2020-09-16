/*
 * $Id: parse_sipifmatch.h,v 1.2 2005/06/16 11:37:54 miconda Exp $
 * 
 * Copyright (C) 2001-2003 FhG FOKUS
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

#ifndef PARSE_SIPIFMATCH_H
#define PARSE_SIPIFMATCH_H

#include "../str.h"
#include "hf.h"

typedef struct etag {
	str text;       /* Original string representation */
} etag_t;


/*
 * Parse Sipifmatch HF body
 */
int parse_sipifmatch(struct hdr_field* _h);


/*
 * Release memory
 */
void free_sipifmatch(str** _e);


#endif /* PARSE_SIPIFMATCH_H */
