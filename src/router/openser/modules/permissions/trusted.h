/*
 * $Id: trusted.h,v 1.1.1.1 2005/06/13 16:47:43 bogdan_iancu Exp $
 *
 * Header file for trusted.c implementing allow_trusted function
 *
 * Copyright (C) 2003 Juha Heinanen
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

#ifndef TRUSTED_H
#define TRUSTED_H
		
#include "../../parser/msg_parser.h"


extern struct trusted_list ***hash_table;     /* Pointer to current hash table pointer */
extern struct trusted_list **hash_table_1;   /* Pointer to hash table 1 */
extern struct trusted_list **hash_table_2;   /* Pointer to hash table 2 */


/*
 * Initialize data structures
 */
int init_trusted(void);


/*
 * Open database connections if necessary
 */
int init_child_trusted(int rank);


/*
 * Close connections and release memory
 */
void clean_trusted(void);


/*
 * Check if request comes from trusted ip address with matching from URI
 */
int allow_trusted(struct sip_msg* _msg, char* _s1, char* _s2);


#endif /* TRUSTED_H */
