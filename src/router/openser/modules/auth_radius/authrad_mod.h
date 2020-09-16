/*
 * $Id: authrad_mod.h,v 1.3 2005/07/08 11:00:32 bogdan_iancu Exp $
 *
 * Digest Authentication - Radius support
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
 * 2003-03-09: Based on auth_mod.h from radius_authorize (janakj)
 */


#ifndef AUTHRAD_MOD_H
#define AUTHRAD_MOD_H

#include "../auth/api.h"
#include "../acc/dict.h"

extern struct attr attrs[];
extern struct val vals[];
extern void *rh;

extern auth_api_t auth_api;

#endif /* AUTHRAD_MOD_H */
