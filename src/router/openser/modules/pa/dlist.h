/*
 * $Id: dlist.h,v 1.1.1.1 2005/06/13 16:47:41 bogdan_iancu Exp $
 *
 * Presence Agent, domain list
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


#ifndef DLIST_H
#define DLIST_H

#include "../../str.h"
#include "pdomain.h"


/*
 * List of all domains registered with usrloc
 */
typedef struct dlist {
	str name;            /* Name of the domain */
	pdomain_t* d;        /* Payload */
	struct dlist* next;  /* Next element in the list */
} dlist_t;


extern dlist_t* root;


/*
 * Function registers a new domain with presence agent
 * if the domain exists, pointer to existing structure
 * will be returned, otherwise a new domain will be
 * created
 */
int register_pdomain(const char* _n, pdomain_t** _d);


/*
 * Free all registered domains
 */
void free_all_pdomains(void);


/*
 * Just for debugging
 */
void print_all_pdomains(FILE* _f);


/*
 * Called from timer
 */
int timer_all_pdomains(void);

#endif /* DLIST_H */
