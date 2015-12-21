#ifndef _UTIL_H_
#define _UTIL_H_

#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>

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

#define INITIAL_MEMORY_SIZE 512
#define BOUNDED_MEMORY_SIZE 1024*64

#ifndef NDEBUG
#define CHECK assert
#else
#define CHECK(X) \
  do { \
    if( !(X) ) { \
      fprintf(stderr,"assertion failed:"#X); \
      abort(); \
    } \
  } while(0)
#endif /* NDEBUG */

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


/* =======================================================
 * String
 * It is a one time composes object,
 * you could create one by using several routines,
 * however , after creating it, you are not supposed
 * to modify it or recreate it on same string slot.
 * String doesn't support modification at all. If you
 * want to modify , use strbuf
 * ======================================================*/

struct string {
  const char* str;
  size_t len;
};

extern struct string NULL_STRING;
extern struct string EMPTY_STRING; /* contains a "" string , good for display */
extern struct string TRUE_STRING;
extern struct string FALSE_STRING;
extern struct string NONE_STRING;

#define CONST_STRING(X) { X , ARRAY_SIZE(X)-1 }

static int string_null( const struct string* );

static
struct string string_dup( const struct string* str ) {
  struct string ret;
  if( string_null(str) ) return NULL_STRING;
  ret.str = strdup(str->str);
  ret.len = str->len;
  return ret;
}

static
struct string string_dupc( const char* str ) {
  struct string ret;
  ret.str = strdup(str);
  ret.len = strlen(str);
  return ret;
}

/* Do not call string_destroy on constant string */
static
struct string string_const( const char* str , size_t len ) {
  struct string ret;
  ret.str = str;
  ret.len = len;
  return ret;
}

static
int string_eq( const struct string* l , const struct string* r ) {
  assert( !string_null(l) );
  assert( !string_null(r) );
  return l->len == r->len && strcmp(l->str,r->str) == 0 ;
}

static
int string_eqc( const struct string* l , const char* str ) {
  assert(!string_null(l));
  return strcmp(l->str,str) == 0;
}

static
int string_cmp( const struct string* l , const struct string* r ) {
  assert( !string_null(l) );
  assert( !string_null(r) );
  return strcmp(l->str,r->str);
}

static
void string_destroy( struct string* str ) {
  free((void*)str->str);
  *str = NULL_STRING;
}

static
int string_null( const struct string* str ) {
  return str->str == NULL || str->len == 0;
}

struct string
string_concate( const struct string* l , const struct string* r );

struct string
string_multiply( const struct string* l , int times );


/* ====================================================
 * String buffer
 * Support pushing back and buffer manipulation
 * ===================================================*/
struct strbuf {
  char* str;
  size_t len;
  size_t cap;
};

static
void strbuf_reserve( struct strbuf* buf , size_t cap ) {
  char* nbuf = malloc(cap);
  if( buf->str ) {
    memcpy(nbuf,buf->str,buf->len);
    buf->str[buf->len] = 0;
    free(buf->str);
  }
  buf->str = nbuf;
  buf->cap = cap;
}

static
void strbuf_init( struct strbuf* buf ) {
  buf->str = NULL;
  buf->len = 0;
  strbuf_reserve(buf,STRBUF_INIT_SIZE);
}

static
void strbuf_init_cap( struct strbuf* buf , size_t cap ) {
  buf->str = NULL;
  buf->len = 0;
  assert(cap>0);
  strbuf_reserve(buf,cap);
}

static
void strbuf_push( struct strbuf* buf , char c ) {
  if( buf->cap == 0 || buf->cap == buf->len+1 ) {
    size_t c = buf->cap == 0 ? STRBUF_INIT_SIZE : buf->cap*2;
    strbuf_reserve( buf , c );
  }
  buf->str[buf->len] = c;
  ++(buf->len);
  buf->str[buf->len]=0; /* always end with a null terminator */
}

static
void strbuf_append( struct strbuf* buf , const char* str , size_t len ) {
  if( buf->cap == 0 || buf->cap == buf->len + len + 1 ) {
    size_t c = buf->cap == 0 ? len + 1 + STRBUF_INIT_SIZE :
      (buf->len+len+1)*2;
    strbuf_reserve( buf, c );
  }
  memcpy(buf->str+buf->len,str,len);
  buf->len += len;
  buf->str[buf->len] = 0; /* always end with a null terminator */
}

static
void strbuf_destroy( struct strbuf* buf ) {
  free(buf->str);
  buf->cap = buf->len = 0;
}

static
char strbuf_index( const struct strbuf* buf , int idx ){
  assert( idx < buf->len );
  return buf->str[idx];
}

static
void strbuf_reset( struct strbuf* buf ) {
  buf->len = 0;
  if(buf->str) buf->str[0] == 0;
}

#define strbuf_clear strbuf_reset

static
void strbuf_move( struct strbuf* buf , struct string* output ) {
  /* If the occupied the buffer is larger than 1/2 of string buffer
   * and its length is smaller than 1KB( small text ). We just return
   * the buffer directly. Otherwise, we do a deep copy */

  if( buf->len > STRBUF_MOVE_THRESHOLD ) {
    if( buf->len == buf->cap ) {
      output->str = buf->str;
      output->len = buf->len;
      buf->cap = buf->len = 0;
      buf->str = NULL;
    } else {
      output->str = strdup(buf->str);
      output->len = buf->len;
      buf->len = 0;
    }
  } else {
    if( buf->len >= buf->cap/2 ) {
      output->str = buf->str;
      output->len = buf->len;
      buf->cap = buf->len = 0;
      buf->str = NULL;
    } else {
      output->str = strdup(buf->str);
      output->len = buf->len;
      buf->len = 0;
    }
  }
}

static
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
  unsigned int more: 1 ;  /* more collision ? */
  unsigned int used: 1 ;  /* whether this one is empty */
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
void* map_find  ( struct map* , const struct string* );
void* map_find_c( struct map* , const char* key );
void map_clear( struct map* );
#define map_size(d) ((d)->len)

/* iterator for mapionary */
int map_iter_start( const struct map* );
static
int map_iter_has  ( const struct map* d, int itr ) {
  return (size_t)itr < d->cap;
}
int map_iter_move ( const struct map* , int itr );

static
struct map_pair map_iter_deref( struct map* d, int itr ) {
  struct map_entry* e = d->entry + itr;
  struct map_pair ret;
  assert( e->used && !e->del );
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

void slab_init( struct slab* , size_t cap , size_t obj_sz );
void slab_destroy(struct slab* );
void* slab_malloc( struct slab* );
void slab_free( struct slab* , void* );

/* =========================================
 * Other helper functions
 * =======================================*/

/* grow a piece of memory to a new memory. The size_t obj_sz is used represent
 * the length for each elements and the length is the size of the current array,
 * the new_cap is used to indicate the target capacity of the memory.
 * assert new_cap > len
 * and the size for the new memory is obj_sz * length
 * After calling this function, the input mem will be freeed */

void* mem_grow( void* , size_t obj_sz , size_t append, size_t* old_cap );

static int is_int( double val ) {
  double i;
  modf(val,&i);
  if( i < INT_MAX && i > INT_MIN )
    return i == val;
  else
    return 0;
}

#endif /* _UTIL_H_ */
