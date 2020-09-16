/*
 * $Id: lcr_mod.h,v 1.1 2005/06/16 13:08:33 bogdan_iancu Exp $
 *
 * Various lcr related functions
 *
 * Copyright (C) 2005 Juha Heinanen
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
 * 2005-02-06: created by jh
 */


#ifndef LCR_MOD_H
#define LCR_MOD_H

#include <stdio.h>

void print_gws (FILE *reply_file);
int reload_gws (void);

#endif /* LCR_MOD_H */
