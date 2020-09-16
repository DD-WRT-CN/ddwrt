/*
 * $Id: usr_avp.h,v 1.3 2005/06/30 16:47:31 anomarme Exp $
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
 * ---------
 *  2004-07-21  created (bogdan)
 *  2004-11-14  global aliases support added (bogdan)
 *  2005-02-14  list with FLAGS USAGE added (bogdan)
 */

#ifndef _SER_URS_AVP_H_
#define _SER_URS_AVP_H_

/*
 *   LIST with the allocated flags, their meaning and owner
 *   flag no.    owner            description
 *   -------------------------------------------------------
 *     0        avp_core          avp has a string name
 *     1        avp_core          avp has a string value
 *     3        avpops module     avp was loaded from DB
 *     4        lcr module        contact avp qvalue change
 *
 */
 

#include "str.h"

typedef union {
	int  n;
	str *s;
} int_str;


struct usr_avp {
	unsigned short id;
	unsigned short flags;
	struct usr_avp *next;
	void *data;
};


#define AVP_NAME_STR     (1<<0)
#define AVP_VAL_STR      (1<<1)

#define is_avp_str_name(a)	(a->flags&AVP_NAME_STR)
#define is_avp_str_val(a)	(a->flags&AVP_VAL_STR)

#define GALIAS_CHAR_MARKER  '$'

/* add functions */
int add_avp( unsigned short flags, int_str name, int_str val);

/* search functions */
struct usr_avp *search_first_avp( unsigned short name_type, int_str name,
															int_str *val );
struct usr_avp *search_next_avp( struct usr_avp *avp, int_str *val  );

/* free functions */
void reset_avps( );
void destroy_avp( struct usr_avp *avp);
void destroy_avp_list( struct usr_avp **list );
void destroy_avp_list_unsafe( struct usr_avp **list );

/* get func */
void get_avp_val(struct usr_avp *avp, int_str *val );
str* get_avp_name(struct usr_avp *avp);
struct usr_avp** set_avp_list( struct usr_avp **list );
struct usr_avp** get_avp_list( );

/* global alias functions (manipulation and parsing)*/
int add_avp_galias_str(char *alias_definition);
int lookup_avp_galias(str *alias, int *type, int_str *avp_name);
int add_avp_galias(str *alias, int type, int_str avp_name);
int parse_avp_name( str *name, int *type, int_str *avp_name);
int parse_avp_spec( str *name, int *type, int_str *avp_name);

#endif

