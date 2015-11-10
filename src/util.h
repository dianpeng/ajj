#ifndef _UTIL_H_
#define _UTIL_H_
#include <string.h>
#include <stddef.h>
#include <assert.h>

/* Forward =================================== */
struct ajj_value;

#ifndef UNREACHABLE
#define UNREACHABLE() assert(!"UNREACHABLE!")
#endif /* UNREACHABLE */

#ifndef UNUSE_ARG
#define UNUSE_ARG(X) (void)(X)
#endif /* UNUSE_ARG */

#define STRBUF_MOVE_THRESHOLD 1024

#define DICT_LOCAL_BUF_SIZE 4
#define LIST_LOCAL_BUF_SIZE 4

#define STRING_HASH_SEED 1771
#define DICT_MAX_SIZE (1<<29)
#define STRBUF_INIT_SIZE 64

/* String implementation. All the data owned by this
 * string are on heap. We don't SSO for string object */
struct string {
  const char* str;
  size_t len;
};

extern struct string NULL_STRING;

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
void string_destroy( struct string* str ) {
  if( str->str ) free(str->str);
}

static inline
int string_null( struct string* str ) {
  return str->str == NULL;
}

/* String buffer ========================== */

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

/* dictionary */
struct dict_entry {
  struct ajj_value value;
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

struct dict {
  struct dict_entry lbuf[DICT_LOCAL_BUF_SIZE];
  struct dict_entry* entry;
  size_t cap;
  size_t len;
};

static inline void dict_create( struct dict* d ) {
  d->cap = DICT_LOCAL_BUF_SIZE;
  d->entry = d->lbuf;
  d->len = 0;
}

static inline void dict_destroy(struct dict* d) {
  dict_clear(d);
}

/* XXX_c APIs are used to help dealing with the public interfaces ,since public
 * interfaces are not using struct string objects */
int dict_insert( struct dict* , const struct string* , const struct ajj_value* val );
int dict_insert_c( struct dict* , const char* key , const struct ajj_value* val );
int dict_remove( struct dict* , const struct string* , struct ajj_value* output );
int dict_remove_c( struct dict* , const char* key , const struct ajj_value* val );
struct ajj_value* dict_find  ( struct dict* , const struct string* );
struct ajj_value* dict_find_c( struct dict* , const char* key );
void dict_clear( struct dict* );
#define dict_size(d) ((d)->len)

/* iterator for dictionary */
int dict_iter_start( const struct dict* );
static inline
int dict_iter_has  ( const struct dict* d, int itr ) {
  return itr < d->cap;
}
int dict_iter_move ( const struct dict* , int itr );

static inline
struct ajj_value* dict_iter_deref( struct dict* , int itr ) {
  struct dict_entry* e = d->entry + itr;
  assert( !e->empty && !e->del );
  return &(e->value);
}

/* list */
struct list {
  struct ajj_value lbuf[LIST_LOCAL_BUF_SIZE];
  struct ajj_value* entry;
  size_t cap;
  size_t len;
};

static inline
void list_create( struct list* l ) {
  l->entry = l->lbuf;
  l->cap = LIST_LOCAL_BUF_SIZE;
  l->len = 0;
}

void list_destroy(struct list* );
void list_push( struct list* , const struct ajj_value* val );
#define list_size(l) ((l)->len)
static inline
const struct ajj_value*
list_index( struct list* l , size_t i ) {
  assert(i < l->len);
  return l->entry[i];
}
void list_clear();

/* iterator for list */
static inline
int list_iter_begin( const struct list* ) {
  return 0;
}
static inline
int list_iter_has( const struct list* l , int itr ) {
  return itr < l->len;
}
static inline
int list_iter_move( const struct list* l , int itr ) {
  return itr+1;
}
static inline
void* list_iter_deref( const struct list* l , int itr ) {
  return list_index(l,itr);
}

/* slab memory pool */
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
