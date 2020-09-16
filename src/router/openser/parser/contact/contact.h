/*
 * $Id: contact.h,v 1.1.1.1 2005/06/13 16:47:51 bogdan_iancu Exp $
 *
 * Contact data type
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
 * History:
 * -------
 *  2003-030-25 Adapted to use new parameter parser (janakj)
 */


#ifndef CONTACT_H
#define CONTACT_H

#include <stdio.h>
#include "../../str.h"
#include "../parse_param.h"


/*
 * Structure representing a Contact HF body
 */
typedef struct contact {
	str name;               /* Name part */
	str uri;                /* contact uri */
	param_t* q;             /* q parameter hook */
	param_t* expires;       /* expires parameter hook */
	param_t* method;        /* method parameter hook */
	param_t* received;      /* received parameter hook */
	param_t* params;        /* List of all parameters */
	int len;                /* Total length of the element */
        struct contact* next; /* Next contact in the list */
} contact_t;


/*
 * Parse contacts in a Contact HF
 */
int parse_contacts(str* _s, contact_t** _c);


/*
 * Free list of contacts
 * _c is head of the list
 */
void free_contacts(contact_t** _c);


/*
 * Print list of contacts, just for debugging
 */
void print_contacts(FILE* _o, contact_t* _c);


#endif /* CONTACT_H */
