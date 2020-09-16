/**
 * $Id: dispatch.h,v 1.4 2005/08/08 15:44:28 miconda Exp $
 *
 * dispatcher module
 * 
 * Copyright (C) 2004-2006 FhG Fokus
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
 * -------
 * 2004-07-31  first version, by daniel
 */

#ifndef _DISPATCH_H_
#define _DISPATCH_H_

#include "../../parser/msg_parser.h"


#define DS_HASH_USER_ONLY 1  /* use only the uri user part for hashing */
extern int ds_flags; 

int ds_load_list(char *lfile);
int ds_destroy_list();
int ds_select_dst(struct sip_msg *msg, char *set, char *alg, int mode);

#endif

