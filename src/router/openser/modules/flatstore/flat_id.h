/* 
 * $Id: flat_id.h,v 1.1.1.1 2005/06/13 16:47:38 bogdan_iancu Exp $
 *
 * Flatstore connection identifier
 *
 * Copyright (C) 2004 FhG Fokus
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

#ifndef _FLAT_ID_H
#define _FLAT_ID_H

#include "../../str.h"


struct flat_id {
	str dir;   /* Database directory */ 
	str table; /* Name of table */
};


/*
 * Create a new connection identifier
 */
struct flat_id* new_flat_id(char* dir, char* table);


/*
 * Compare two connection identifiers
 */
unsigned char cmp_flat_id(struct flat_id* id1, struct flat_id* id2);


/*
 * Free a connection identifier
 */
void free_flat_id(struct flat_id* id);


#endif /* _FLAT_ID_H */
