/*
 * $Id: mdefines.h,v 1.1.1.1 2005/06/13 16:47:38 bogdan_iancu Exp $
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



/***************************************************************************
                          mdefines.h  -  description
                             -------------------
    author               : Daniel-Constantin MIERLA
    email                : mierla@fokus.fhg.de
    organization         : FhI FOKUS, BERLIN
 ***************************************************************************/

#ifndef _mdefines_h_
#define _mdefines_h_

#define _M_PRINTF	printf
#define _M_CALLOC 	calloc
#define _M_REALLOC	realloc

#define _M_MALLOC 	pkg_malloc
#define _M_FREE		pkg_free

#define _M_SHM_MALLOC 	shm_malloc
#define _M_SHM_FREE		shm_free

#endif
