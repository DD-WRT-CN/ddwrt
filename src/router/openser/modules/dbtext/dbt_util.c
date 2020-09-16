/*
 * $Id: dbt_util.c,v 1.1.1.1 2005/06/13 16:47:37 bogdan_iancu Exp $
 *
 * DBText library
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
 * 2003-01-30 created by Daniel
 * 
 */

#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include "dbt_util.h"

/**
 *
 */
int dbt_is_database(str *_s)
{
	DIR *dirp = NULL;
	char buf[512];
	
	if(!_s || !_s->s || _s->len <= 0 || _s->len > 510)
		return 0;
	strncpy(buf, _s->s, _s->len);
	buf[_s->len] = 0;
	dirp = opendir(buf);
	if(!dirp)
		return 0;
	closedir(dirp);

	return 1;
}

