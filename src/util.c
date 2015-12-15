#include "util.h"

struct string NULL_STRING = { NULL , 0 };
struct string EMPTY_STRING = CONST_STRING("");
struct string TRUE_STRING = CONST_STRING("true");
struct string FALSE_STRING= CONST_STRING("false");
struct string NONE_STRING = CONST_STRING("none");

struct string
string_concate( const struct string* l , const struct string* r ) {
  struct strbuf sbuf;
  strbuf_init_cap(&sbuf,l->len+r->len+1);
  strbuf_append(&sbuf,l->str,l->len);
  strbuf_append(&sbuf,r->str,r->len);
  return strbuf_tostring(&sbuf);
}

struct string
string_multiply( const struct string*  l , int times ) {
  struct strbuf sbuf;
  strbuf_init_cap(&sbuf,l->len*times + 1);
  while(times--) {
    strbuf_append(&sbuf,l->str,l->len);
  }
  return strbuf_tostring(&sbuf);
}

/* Yet another open addressing hash table */
static
unsigned int map_hash( const struct string* key ) {
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
struct map_entry* map_insert_entry_c( struct map* d ,
    const char* key , unsigned int fullhash , int insert ) {
  unsigned int idx = fullhash & (d->cap-1);
  struct map_entry* e;
  e = d->entry + idx;

  if( !e->used ) {
    return insert ? e : NULL;
  } else {
    struct map_entry* ret = e->del ? e : NULL;
    struct map_entry* ne = e;
    /* Looking froward through the collision chain */
    do {
      if( ne->del ) {
        if( ret == NULL )
          ret = ne;
      } else {
        if( ne->hash == fullhash && (strcmp(key,ne->key.str)==0) ) {
          /* We found an existed one here */
          return ne;
        }
      }
      if( !ne->more )
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
        if( !(ret->used) || ret->del )
          break;
      }
      assert( !ne->more );
      ne->next = ret - d->entry;
      ne->more = 1;
    }
    return ret;
  }
}

#define MAP_VALUE(D,E) (((char*)((D)->value)) + ((E)-((D)->entry))*(D)->obj_sz)

static inline
void map_value_store( struct map* d, struct map_entry* e , const void* val ) {
  void* pos = MAP_VALUE(d,e);
  memcpy(pos,val,d->obj_sz);
}

static inline
void map_value_load( struct map* d , struct map_entry* e , void* val ) {
  void* pos = MAP_VALUE(d,e);
  memcpy(val,pos,d->obj_sz);
}

static
struct map_entry* map_insert_entry(struct map* d,
    const struct string* key,
    unsigned int fullhash, int insert ) {
  return map_insert_entry_c(d,
      key->str,
      fullhash,
      insert);
}

/* rehashing */
static
void map_rehash( struct map* d ) {
  size_t new_cap = d->cap * 2; /* make sure power of 2 */
  void* new_buf = calloc(new_cap, sizeof(struct map_entry) + d->obj_sz);
  struct map temp_d;
  int i;

  temp_d.entry = new_buf;
  temp_d.cap = new_cap;
  temp_d.len = 0;
  temp_d.obj_sz = d->obj_sz;
  temp_d.value = (char*)(new_buf) + sizeof(struct map_entry)*new_cap;

  for( i = 0 ; i < d->cap ; ++i ) {
    struct map_entry* o = d->entry + i;
    struct map_entry* e;
    if( o->del || !(o->used) )
      continue;
    e = map_insert_entry(&temp_d,&(o->key),o->hash,1);
    e->key = o->key;
    e->hash = o->hash;
    map_value_store(&temp_d,e,MAP_VALUE(d,o));
    if(e->del) e->del = 0;
    if(!(e->used)) {
      e->used = 1;
      e->more = 0;
    }
  }

  /* free old memory if we have to */
  free(d->entry);

  temp_d.len = d->len;
  *d = temp_d;
}

int map_insert( struct map* d, const struct string* key , int own ,
    const void* val ) {
  int fh;
  struct map_entry* e;

  if( d->len == DICT_MAX_SIZE )
    return -1;

  if( d->cap == d->len )
    map_rehash(d);

  fh = map_hash(key);
  e = map_insert_entry(d,key,fh,1);
  e->key = own ? *key : string_dup(key);
  if( e->del ) e->del = 0;
  if( !(e->used) ) {
    e->used = 1;
    e->more = 0;
  }
  ++d->len;
  e->hash = fh;
  map_value_store(d,e,val);
  return 0;
}

int map_insert_c( struct map* d, const char* key ,
    const void* val ) {
  int fh;
  struct map_entry* e;
  struct string k;

  if( d->len == DICT_MAX_SIZE )
    return -1;

  if( d->cap == d->len )
    map_rehash(d);
  k.str = key;
  k.len = strlen(key);

  fh = map_hash(&k);
  e = map_insert_entry_c(d,key,fh,1);
  e->key = string_dup(&k);
  if( e->del ) e->del = 0;
  if( !(e->used) ) {
    e->used = 1;
    e->more = 0;
  }
  ++d->len;
  e->hash = fh;
  map_value_store(d,e,val);
  return 0;
}

int map_remove( struct map* d , const struct string* key , void* output ) {
  struct map_entry* e = map_insert_entry(d,key,map_hash(key),0);
  if( e == NULL )
    return -1;
  else {
    assert(e->used);
    assert(!e->del);
    assert(string_eq(&(e->key),key));
    /* destroy the key */
    string_destroy(&(e->key));
    if( output )
      map_value_load(d,e,output);
    e->del = 1;
    --d->len;
    return 0;
  }
}

int map_remove_c( struct map* d , const char* key , void* output ) {
  struct string k;
  struct map_entry* e;
  k.str = key;
  k.len = strlen(key);
  e = map_insert_entry_c(d,key,map_hash(&k),0);
  if( e == NULL )
    return -1;
  else {
    assert(e->used);
    assert(!e->del);
    assert(string_eqc(&(e->key),key));
    /* destroy string */
    string_destroy(&(e->key));
    if( output )
      map_value_load(d,e,output);
    e->del = 1;
    --d->len;
    return 0;
  }
}

void* map_find( struct map* d , const struct string* key ) {
  struct map_entry* e = map_insert_entry(d,key,map_hash(key),0);
  if( e ) {
    return MAP_VALUE(d,e);
  } else {
    return NULL;
  }
}

void* map_find_c( struct map* d , const char* key ) {
  struct string k;
  struct map_entry* e;
  k.str = key;
  k.len = strlen(key);
  e = map_insert_entry_c(d,key,map_hash(&k),0);
  if( e ) {
    return MAP_VALUE(d,e);
  } else {
    return NULL;
  }
}

void map_create( struct map* d , size_t obj_sz , size_t cap ) {
  assert( cap >= 2 && !((cap&(cap-1))) );
  assert( obj_sz > 0 );
  d->obj_sz = obj_sz;
  d->cap = cap;
  d->len = 0;
  d->entry = calloc(cap,sizeof(struct map_entry)+obj_sz);
  d->value = ((char*)(d->entry)) + cap*sizeof(struct map_entry);
}

void map_destroy( struct map* d ) {
  int i;
  /* We need to traversal through the mapionary to release all the key
   * since they all are on the heap */
  for( i = 0 ; i < d->cap ; ++i ) {
    struct map_entry* e = d->entry + i;
    if( e->used && !e->del ) {
      string_destroy(&(e->key));
    }
  }
  free(d->entry);
  d->entry = d->value = NULL;
  d->cap = d->len = 0;
}

void map_clear( struct map* d ) {
  int i;
  for( i = 0 ; i < d->cap ; ++i ) {
    struct map_entry* e = d->entry + i;
    if( e->used ) {
      if( !e->del ) {
        string_destroy(&(e->key));
      }
      memset(e,0,sizeof(*e));
    }
  }
  d->len = 0;
}

int map_iter_start( const struct map* d ) {
  int ret = 0;
  for( ; ret < d->cap ; ++ret ) {
    struct map_entry* e = d->entry + ret;
    if( e->used && !e->del )
      return ret;
  }
  return ret;
}

int map_iter_move( const struct map* d , int itr ) {
  for( ++itr ; itr < d->cap ; ++itr ) {
    struct map_entry* e = d->entry + itr;
    if( e->used && !e->del )
      return itr;
  }
  return itr;
}


/* =====================
 * Slab implementation
 * =====================*/

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
      (struct freelist*)(((char*)mem) + sl->obj_sz);
    mem = (char*)mem + sl->obj_sz;
  }
  ((struct freelist*)(mem))->next = NULL;
  sl->fl = h;
  sl->cur_cap = cap;
}

void slab_init( struct slab* sl , size_t cap , size_t obj_sz ) {
  cap = cap < 32 ? 16 : cap/2;
  sl->obj_sz = obj_sz < sizeof(void*) ?
    sizeof(void*) : obj_sz;
  sl->fl = NULL;
  sl->ck = NULL;
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
  sl->ck = NULL;
  sl->fl = NULL;
  sl->cur_cap = sl->obj_sz = 0;
}

/* ===============================
 * Other
 * =============================*/
void* mem_grow( void* ptr , size_t obj_sz,
    size_t append ,
    size_t* old_cap ) {
  size_t cap = *old_cap;
  /* We an use realloc safe even if the input ptr is not
   * filled up with data. We just copy garbage bytes and
   * it is no harm */
  if(cap == 0) {
    cap = INITIAL_MEMORY_SIZE > append ? INITIAL_MEMORY_SIZE : append;
  } else {
    if( cap >= BOUNDED_MEMORY_SIZE ) {
      /* now we fallback to linear growth instead of exponential
       * growth which avoids large memory allocation and waste */
      cap += BOUNDED_MEMORY_SIZE > append ? BOUNDED_MEMORY_SIZE : append;
    } else {
      cap += (append > cap) ? append+cap : cap;
    }
  }
  *old_cap = cap;
  return realloc(ptr,obj_sz*cap);
}
