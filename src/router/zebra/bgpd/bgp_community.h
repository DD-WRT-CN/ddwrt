/*
 * Community attribute related functions.
 * Copyright (C) 1998 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#ifndef _ZEBRA_BGP_COMMUNITY_H
#define _ZEBRA_BGP_COMMUNITY_H

/* Community attribute. */
struct community 
{
  unsigned long refcnt;
  int size;
  u_int32_t *val;
};

/* Community pre-defined values definition. */
#define COMMUNITY_NO_EXPORT             0xFFFFFF01
#define COMMUNITY_NO_ADVERTISE          0xFFFFFF02
#define COMMUNITY_NO_EXPORT_SUBCONFED   0xFFFFFF03
#define COMMUNITY_LOCAL_AS              0xFFFFFF03

/* Macros of community attribute. */
#define com_length(X)    ((X)->size * 4)
#define com_lastval(X)   ((X)->val + (X)->size - 1)
#define com_nthval(X,n)  ((X)->val + (n))

/* Prototypes of community attribute functions. */
void community_init ();
void community_free (struct community *);
struct community *community_uniq_sort (struct community *);
struct community *community_parse (char *, u_short);
struct community *community_intern (struct community *);
void community_unintern (struct community *);
const char *community_print (struct community *);
void community_print_vty (struct vty *, struct community *);
void community_print_all_vty (struct vty *);
unsigned int community_hash_make (struct community *);
struct community *community_str2com (char *);
int community_match (struct community *, struct community *);
int community_cmp (struct community *, struct community *);
struct community *community_merge (struct community *, struct community *);
struct community *community_delete (struct community *, struct community *);
struct community *community_dup (struct community *);
int community_include (struct community *, u_int32_t);
void community_del_val (struct community *, u_int32_t *);
unsigned long community_count ();
#endif /* _ZEBRA_BGP_COMMUNITY_H */
