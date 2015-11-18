#ifndef _UTIL_H_
#define _UTIL_H_

#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>

#ifndef UNREACHABLE
#define UNREACHABLE() assert(!"UNREACHABLE!")
#endif /* UNREACHABLE */

#ifndef UNUSE_ARG
#define UNUSE_ARG(X) (void)(X)
#endif /* UNUSE_ARG */

#define STRBUF_MOVE_THRESHOLD 1024

#define STRING_HASH_SEED 1771
#define DICT_MAX_SIZE (1<<29)
#define STRBUF_INIT_SIZE 64

#define ARRAY_SIZE(X) (sizeof(X)/sizeof((X)[0]))

/* Helper macro to insert into double linked list */
#define LINIT(X) \
  do { \
    (X)->next = (X); \
    (X)->prev = (X); \
  } while(0)

#define LINSERT(X,T) \
  do { \
    (X)->next = (T); \
    (X)->prev = (T)->prev; \
    (T)->prev->next = (X); \
    (T)->prev = (X); \
  } while(0)

#define LREMOVE(X) \
  do { \
    (X)->prev->next = (X)->next; \
    (X)->next->prev = (X)->prev; \
  } while(0)


/* =================================
 * String , it supports value smeantic
 * ===============================*/
struct string {
  const char* str;
  size_t len;
};

extern struct string NULL_STRING;
extern struct string TRUE_STRING;
extern struct string FALSE_STRING;
extern struct string NONE_STRING;

#define CONST_STRBUF(X) { X , ARRAY_SIZE(X) }

static inline
struct string string_dup( const struct string* str ) {
  struct string ret;
  ret.str = strdup(str->str);
  ret.len = str->len;
  return ret;
}

static inline
struct string string_dupc( const char* str ) {
  struct string ret;
  ret.str = strdup(str);
  ret.len = strlen(str);
  return ret;
}

/* Do not call string_destroy on constant string */
static inline
struct string string_const( const char* str , size_t len ) {
  struct string ret;
  ret.str = str;
  ret.len = len;
  return ret;
}

static inline
int string_eq( const struct string* l , const struct string* r ) {
  assert( !string_null(l) );
  assert( !string_null(r) );
  return l->len == r->len && strcmp(l->str,r->str) == 0 ;
}

static inline
int string_eqc( const struct string* l , const char* str ) {
  assert(!string_null(l));
  return strcmp(l->str,str) == 0;
}

static inline
void string_destroy( struct string* str ) {
  if( str->str ) free(str->str);
}

static inline
int string_null( struct string* str ) {
  return str->str == NULL;
}

/* ====================================================
 * String buffer
 * Support pushing back and buffer manipulation
 * ===================================================*/
struct strbuf {
  char* str;
  size_t len;
  size_t cap;
};


static inline
void strbuf_reserve( struct strbuf* buf , size_t cap ) {
  char* nbuf = malloc(cap);
  if( buf->str ) {
    memcpy(nbuf,buf->str,buf->len);
    free(buf->str);
  }
  buf->str = nbuf;
  buf->cap = cap;
}

static inline
void strbuf_create( struct strbuf* buf ) {
  buf->str = NULL;
  buf->len = 0;
  strbuf_reserve(buf,STRBUF_INIT_SIZE);
}

static inline
void strbuf_push( struct strbuf* buf , char c ) {
  if( buf->cap == 0 || buf->cap == buf->len+1 ) {
    strbuf_reserve( buf ,
        buf->cap == 0 ? STRBUF_MINIMUM : buf->cap*2 );
  }
  buf->str[buf->len] = c;
  buf->str[buf->len+1]=0; /* always make sure it is ended with null terminator */
  ++(buf->len);
}

static inline
void strbuf_append( struct strbuf* buf , const char* str , size_t len ) {
  if( buf->cap == 0 || buf->cap == buf->len + 1 ) {
    strbuf_reserve( buf,
        buf->cap == 0 ? len +STRBUF_MINIMUM : len+buf->cap* 2);
  }
  memcpy(buf->str+buf->len,str,len);
  buf->len += len;
}

static inline
void strbuf_destroy( struct strubuf* buf ) {
  if( buf->str ) {
    free(buf->str);
    buf->cap = buf->len = 0;
  }
}

static inline
char strbuf_index( const struct strbuf* buf , int idx ){
  assert( idx < buf->len );
  return buf->str[idx];
}

static inline
void strbuf_reset( struct strbuf* buf ) {
  buf->len = 0;
}

static inline
void strbuf_move( struct strbuf* buf , struct string* output ) {
  /* If the occupied the buffer is larger than 1/2 of string buffer
   * and its length is smaller than 1KB( small text ). We just return
   * the buffer directly. Otherwise, we do a deep copy */

  if( buf->len > STRBUF_MOVE_THRESHOLD ) {
    if( buf->len == buf->cap ) {
      output->str = buf->str;
      output->len = buf->len;
      buf->cap = buf->len = 0;
    } else {
      output->str = strdup(buf->str);
      output->len = buf->len;
    }
  } else {
    if( buf->len >= buf->cap/2 ) {
      output->str = buf->str;
      output->len = buf->len;
      buf->cap = buf->len = 0;
    } else {
      output->str = strdup(buf->str);
      output->len = buf->len;
      return ret;
    }
  }
}

static inline
struct string strbuf_tostring( struct strbuf* buf ) {
  struct string ret;
  ret.str = buf->str;
  ret.len = buf->len;
  return ret;
}


/* ===========================================
 * Map
 * =========================================*/

struct map_entry {
  struct string key;
  unsigned int hash; /* fullhash for this key */
  unsigned int next : 29; /* next resolved collision */
  unsigned int end  : 1 ; /* end of the collision chain */
  unsigned int empty: 1 ; /* whether this one is empty */
  unsigned int del  : 1 ; /* whether this one is deleted
                           * Please be sure that , if the empty is set to 0,
                           * it will never be reset back to 1 even if it is
                           * deleted somehow. The delete is just a marker says
                           * that in the future, this piece of memory could be
                           * used */
};

struct map {
  struct map_entry* entry;
  void* value;

  size_t cap;
  size_t len;
  size_t obj_sz;
};

struct map_pair {
  const struct string* key;
  const void* val;
};

void map_create( struct map* d , size_t obj_sz ,
    size_t cap );

void map_destroy(struct map* d);

/* XXX_c APIs are used to help dealing with the public interfaces ,since public
 * interfaces are not using struct string objects */
int map_insert( struct map* , const struct string* , int own , const void* val );
int map_insert_c( struct map* , const char* key , const void* val );
int map_remove( struct map* , const struct string* , void* val );
int map_remove_c( struct map* , const char* key , void* val );
const void* map_find  ( struct map* , const struct string* );
const void* map_find_c( struct map* , const char* key );
void map_clear( struct map* );
#define map_size(d) ((d)->len)

/* iterator for mapionary */
int map_iter_start( const struct map* );
static inline
int map_iter_has  ( const struct map* d, int itr ) {
  return itr < d->cap;
}
int map_iter_move ( const struct map* , int itr );

static inline
struct map_pair map_iter_deref( struct map* d, int itr ) {
  struct map_entry* e = d->entry + itr;
  struct map_pair ret;
  assert( !e->empty && !e->del );
  ret.key = &(e->key);
  ret.val = ((char*)(d->value)) + (d->obj_sz*(e-(d->entry)));
  return ret;
}

/* ========================================
 * Slab
 * ======================================*/
struct chunk {
  struct chunk* next;
};

struct freelist {
  struct freelist* next;
};

struct slab {
  struct chunk* ck;
  struct freelist* fl;
  size_t cur_cap;
  size_t obj_sz;
};

void slab_create( struct slab* , size_t cap , size_t obj_sz );
void slab_destroy(struct slab* );
void* slab_malloc( struct slab* );
void slab_free( struct slab* , void* );

#endif /* _UTIL_H_ */
