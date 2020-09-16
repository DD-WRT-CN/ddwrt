/*
 * $Id: rd_filter.h,v 1.1 2005/06/23 18:30:36 bogdan_iancu Exp $
 *
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2005-06-22  first version (bogdan)
 */


#ifndef _REDIRECT_FILTER_H
#define _REDIRECT_FILTER_H

#include <sys/types.h> /* for regex */
#include <regex.h>

#define ACCEPT_FILTER   0
#define DENY_FILTER     1
#define NR_FILTER_TYPES 2

#define ACCEPT_RULE    11
#define DENY_RULE      12

#define RESET_ADDED    (1<<0)
#define RESET_DEFAULT  (1<<1)

void init_filters();
void set_default_rule( int type );
void reset_filters();
void add_default_filter( int type, regex_t *filter);
int add_filter( int type, regex_t *filter, int flags);
int run_filters(char *s);

#endif
