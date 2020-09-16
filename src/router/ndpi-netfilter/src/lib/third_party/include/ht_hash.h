/* Based on https://gist.github.com/tonious/1377667 */

#ifndef _HASH_H_
#define _HASH_H_

#include "ndpi_api.h"

struct entry_s {
  char *key;
  u_int16_t value;
  struct entry_s *next;
};

typedef struct entry_s entry_t;

struct hashtable_s {
  int size;
  struct entry_s **table;
};

typedef struct hashtable_s hashtable_t;

static  hashtable_t *ht_create( int size );
static  int ht_hash( hashtable_t *hashtable, char *key );
static  entry_t *ht_newpair( char *key, u_int16_t value );
static  void ht_set( hashtable_t *hashtable, char *key, u_int16_t value );
static  u_int16_t ht_get( hashtable_t *hashtable, char *key );
static  void ht_free( hashtable_t *hashtable );

#endif /* _HASH_H_ */
