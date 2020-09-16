/* 
 * $Id: case_via.h,v 1.2 2005/06/16 11:37:54 miconda Exp $ 
 *
 * Via Header Field Name Parsing Macros
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
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 */


#ifndef CASE_VIA_H
#define CASE_VIA_H


#define via1_CASE            \
        hdr->type = HDR_VIA_T; \
        hdr->name.len = 3;   \
        return (p + 4)        

#define via2_CASE            \
        hdr->type = HDR_VIA_T; \
        p += 4;              \
        goto dc_end


#endif /* CASE_VIA_H */
