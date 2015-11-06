#include "util.h"

/*
 * Dictionary implementation
 * Our dictionary supports traditional : insert , find and remove. The general
 * implementation for this is using a chain resolution hash table. Recently I
 * saw Hotspot VM uses such way , for cache coherence , they uses an dynamic
 * array instead of a linked list to solve the collision. We gonna use the method
 * in LUA, open addressing, unlike the implement, we don't do main position swapping.
 * For deletion, we will just mark it as deleted. We assume deletion is not happened
 * frequently.
 */

static
int dict_hash( const struct string* key );


/* Insert a key into the hash table and return a slot entry to the caller.
 * This entry may be already in used ( return an existed one ) or a new one */
static
struct dict_entry* dict_insert_entry( struct dict* d , const struct string* key,
    int fullhash , int insert ) {
  int idx = fullhash & (d->cap-1);
  struct dict_entry* e;
  e = d->entry + idx;

  assert(d->len < d->cap);

  if( e->empty ) {
    return e;
  } else {
    struct dict_entry* ret = e->del ? e : NULL;
    struct dict_entry* ne = e;
    /* Looking froward through the collision chain */
    do {
      if( ne->del ) {
        if( ret == NULL )
          ret = ne;
      } else {
        if( ne->hash == fullhash && string_eq(&ne->key,key) ) {
          /* We found an existed one here */
          return ne;
        }
      }
      if( ne->next < 0 )
        break;
      ne = d->entry + ne->next;
    } while(1);

    /* We don't do insertion, so after searching in the chain,
     * if we cannot find any one, then just return NULL */
    if( !insert ) return NULL;

    if( ret == NULL ) {
      /* linear probing here */
      int h = fullhash;
      while(1) {
        ret = d->entry + (++h &(d->cap-1));
        if( ret->empty || ret->del )
          break;
      }
      assert(ne->next <0);
      ne->next = ret - d->entry;
    }
    return ret;
  }
}

/* rehashing */
static
void dict_rehash( struct dict* d ) {
  size_t new_cap = d->cap * 2; /* make sure power of 2 */
  void* new_buf = calloc(new_cap,sizeof(struct dict_entry));
  struct dict temp_d;
  int i;

  temp_d.entry = new_buf;
  temp_d.cap = new_cap;
  temp_d.len = 0;

  for( i = 0 ; i < d->cap ; ++i ) {
    struct dict_entry* o = d->entry + i;
    struct dict_entry* e;
    if( o->del || o->empty )
      continue;
    e = dict_insert_entry(d,o->key,o->hash,1);
    e->key = o->key;
    e->value= o->value;
    e->hash = o->hash;
    if(e->del) e->del = 0;
    if(e->empty) {
      e->empty = 1;
      e->next = -1;
    }
  }
  if(d->entry != d->lbuf)
    free(d->entry);
  temp_d.len = d->len;
  *d = temp_d;
}


int dict_insert( struct dict* d, const struct string* key, const struct ajj_value* val ) {
  int fh;
  struct dict_entry* e;

  if( d->cap == d->len )
    dict_rehash(d);

  fh = dict_hash(key);
  e = dict_insert_entry(d,key,fh,1);
  e->key = string_dup(key);
  if( e->del ) e->del = 0;
  if( e->empty ) {
    e->empty = 0;
    e->next = -1;
  }
  ++d->len;
  e->hash = fh;
  e->value = *val;
  return 0;
}

int dict_remove( struct dict* d , const struct string* key , struct ajj_value* output ) {
  struct dict_entry* e = dict_insert_entry(d,key,dict_hash(key),0);
  if( e == NULL )
    return -1;
  else {
    if( output )
      *output = e->value;
    e->del = 1;
    --d->cap;
    return 0;
  }
}

struct ajj_value* dict_find( struct dict* d , const struct string* key ) {
  struct dict_entry* e;
  return (e=dict_insert_entry(d,key,dict_hash(key),0)) ? &(e->value) : NULL;
}
