/*
 * $Id: group.h,v 1.2 2005/07/26 16:27:39 miconda Exp $
 *
 * Group membership
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
 * --------
 * 2003-02-25 - created by janakj
 *
 */


#ifndef GROUP_H
#define GROUP_H

#include "../../parser/msg_parser.h"
#include "../../items.h"

typedef struct _group_check
{
	int id;
	xl_spec_t sp;
} group_check_t, *group_check_p;
/*
 * Check if username in specified header field is in a table
 */
int is_user_in(struct sip_msg* _msg, char* _hf, char* _grp);


int group_db_init(char* db_url);
int group_db_bind(char* db_url);
void group_db_close();
int group_db_ver(char* db_url, str* name);

#endif /* GROUP_H */
