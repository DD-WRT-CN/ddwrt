/* 
 * $Id: flatstore.h,v 1.1.1.1 2005/06/13 16:47:38 bogdan_iancu Exp $ 
 *
 * Flatstore module interface
 *
 * Copyright (C) 2004 FhG Fokus
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
/*
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 */

#ifndef _FLATSTORE_H
#define _FLATSTORE_H

#include "../../db/db_val.h"
#include "../../db/db_key.h"
#include "../../db/db_con.h"


/*
 * Initialize database module
 * No function should be called before this
 */
db_con_t* flat_db_init(const char* _url);


/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int flat_use_table(db_con_t* h, const char* t);


void flat_db_close(db_con_t* h);


/*
 * Insert a row into specified table
 * h: structure representing database connection
 * k: key names
 * v: values of the keys
 * n: number of key=value pairs
 */
int flat_db_insert(db_con_t* h, db_key_t* k, db_val_t* v, int n);


#endif /* _FLATSTORE_H */
