/*
 * $Id: xjab_load.c,v 1.1.1.1 2005/06/13 16:47:39 bogdan_iancu Exp $
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


#include "xjab_load.h"

#define LOAD_ERROR "ERROR:XJAB:xjab_bind: module function "

int load_xjab(struct xjab_binds *xjb)
{
	if(!( xjb->register_watcher=(pa_register_watcher_f)
			find_export("jab_register_watcher", XJ_NO_SCRIPT_F, 0)) ) 
	{
		LOG(L_ERR, LOAD_ERROR "'jab_register_watcher' not found!\n");
		return -1;
	}
	if(!( xjb->unregister_watcher=(pa_unregister_watcher_f)
			find_export("jab_unregister_watcher", XJ_NO_SCRIPT_F, 0)) ) 
	{
		LOG(L_ERR, LOAD_ERROR "'jab_unregister_watcher' not found!\n");
		return -1;
	}
	return 1;
}
