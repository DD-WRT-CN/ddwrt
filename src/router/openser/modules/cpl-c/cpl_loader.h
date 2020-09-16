/*
 * $Id: cpl_loader.h,v 1.1.1.1 2005/06/13 16:47:35 bogdan_iancu Exp $
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
 *
 * History:
 * -------
 * 2003-06-24: file created (bogdan)
 */

#ifndef _CPL_LOADER_H
#define _CPL_LOADER_H


int cpl_load( FILE *fifo_stream, char *response_file );
int cpl_remove( FILE *fifo_stream, char *response_file );
int cpl_get( FILE *fifo_stream, char *response_file );

#endif





