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
unsigned int dict_hash( const struct string* key ) {
  /* This hash function implementation is taken from LUA */
  size_t i;
  const size_t sz = key->len;
  unsigned int h = STRING_HASH_SEED;
  for( i = 0 ; i < sz ; ++i ) {
    h = h ^((h<<5)+(h>>2)) + (unsigned int)(key->str[i]);
  }
  return h;
}

/* Insert a key into the hash table and return a slot entry to the caller.
 * This entry may be already in used ( return an existed one ) or a new one */
static
struct dict_entry* dict_insert_entry( struct dict* d , const struct string* key,
    unsigned int fullhash , int insert ) {
  unsigned int idx = fullhash & (d->cap-1);
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
      if( ne->end )
        break;
      ne = d->entry + ne->next;
    } while(1);

    /* We don't do insertion, so after searching in the chain,
     * if we cannot find any one, then just return NULL */
    if( !insert ) return NULL;

    if( ret == NULL ) {
      /* linear probing here */
      unsigned int h = fullhash;
      while(1) {
        ret = d->entry + (++h &(d->cap-1));
        if( ret->empty || ret->del )
          break;
      }
      assert( ne->end );
      ne->next = ret - d->entry;
      ne->end = 0;
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
      e->end = 1;
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
    e->end = 1;
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
    assert(!e->empty);
    assert(!e->del);
    /* destroy the key */
    string_destroy(&(e->key));
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

void dict_clear( struct dict* d ) {
  int i;
  /* We need to traversal through the dictionary to release all the key
   * since they all are on the heap */
  for( i = 0 ; i < d->cap ; ++i ) {
    struct dict_entry* e = d->entry + i;
    if( !e->empty && !e->del ) {
      string_destroy(&(e->key));
    }
  }
  if( d->cap > DICT_LOCAL_BUF_SIZE ) {
    free(d->entry);
  }
  d->cap = DICT_LOCAL_BUF_SIZE;
  d->len = 0;
}

int dict_iter_start( const struct dict* d ) {
  int ret = 0;
  for( ; ret < d->cap ; ++ret ) {
    struct dict_entry* e = d->entry + ret;
    if( !e->empty && !e->del )
      return ret;
  }
  return ret;
}

int dict_iter_move( const struct dict* d , int itr ) {
  for( ++itr ; itr < d->cap ; ++itr ) {
    struct dict_entry* e = d->entry + itr;
    if( !e->empty && !e->del )
      return itr;
  }
}

static
void list_reserve( struct list* l ) {
  void* mem;
  assert( l->cap >= LIST_LOCAL_BUF_SIZE );
  mem = malloc(sizeof(struct ajj_value)*2*l->cap);
  memcpy(mem,l->entry,l->len*sizeof(struct ajj_value));
  if( l->lbuf != l->entry )
    free(l->entry);
  l->entry = mem;
  l->cap *= 2;
}

void list_push( struct list* l , const struct ajj_value* val ) {
  if( l->cap == l->len )
    list_reserve(l);
  l->entry[l->len] = *val;
  ++(l->len);
}

void list_destroy( struct list* l ) {
  if( l->lbuf != l->entry )
    free(l->entry);
  l->cap = LIST_LOCAL_BUF_SIZE;
  l->entry = l->lbuf;
  l->len = 0;
}

static
void slab_reserve( struct slab* sl ) {
  const size_t cap = sl->cur_cap * 2;
  void* mem = malloc(sizeof(struct chunk) + sl->cur_cap*sl->obj_sz*2);
  void* h;
  size_t i;

  ((struct chunk*)mem)->next = sl->ck;
  sl->ck = mem;
  h = mem = (char*)(mem) + sizeof(struct chunk);

  for( i = 0 ; i < cap-1 ; ++i ) {
    ((struct freelist*)(mem))->next =
      ((char*)mem) + sl->obj_sz;
    mem = (char*)mem + sl->obj_sz;
  }
  ((struct freelist*)(mem))->next = NULL;
  sl->fl = h;
  sl->cur_cap = cap;
}

void slab_create( struct slab* sl , size_t cap , size_t obj_sz ) {
  cap = cap < 32 ? 16 : cap/2;
  sl->obj_sz = obj_sz;
  sl->fi = sl->ck = NULL;
  sl->cur_cap = cap;
  slab_reserve( sl );
}

void* slab_malloc( struct slab* sl ) {
  void* ret;
  if( sl->fl == NULL ) {
    slab_reserve(sl);
  }
  ret = sl->fl;
  sl->fl = sl->fl->next;
  return ret;
}

void slab_free( struct slab* sl , void* ptr ) {
  ((struct freelist*)ptr)->next = sl->fl;
  sl->fl = ptr;
}


void slab_destroy( struct slab* sl ) {
  struct chunk* c = sl->ck;
  struct chunk* n;
  while( c ) {
    n = c->next;
    free(c);
    c = n;
  }
  sl->ck = sl->fl = NULL;
  sl->cur_cap = sl->obj_sz = 0;
}
