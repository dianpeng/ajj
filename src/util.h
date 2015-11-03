#ifndef _UTIL_H_
#define _UTIL_H_
#include <string.h>
#include <stddef.h>
#include <assert.h>

#define UNREACHABLE() assert(!"UNREACHABLE!")
#define UNUSE_ARG(X) (void)(X)

/* string */
struct string {
  const char* str;
  size_t len;
};

struct strbuf {
  char* str;
  size_t len;
  size_t cap;
};

#define STRBUF_MOVE_THRESHOLD 1024

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
void strbuf_push( struct strbuf* buf , char c ) {
  if( buf->cap == 0 || buf->cap == buf->len+1 ) {
    strbuf_reserve( buf ,
        buf->cap == 0 ? STRBUF_MINIMUM : buf->cap*2 );
  }
  buf->str[buf->len] = c;
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
struct string strbuf_move( struct strbuf* buf ) {
  /* If the occupied the buffer is larger than 1/2 of string buffer
   * and its length is smaller than 1KB( small text ). We just return
   * the buffer directly. Otherwise, we do a deep copy */
  struct string ret;

  if( buf->len > STRBUF_MOVE_THRESHOLD ) {
    if( buf->len == buf->cap ) {
      ret.str = buf->str;
      ret.len = buf->len;
      buf->cap = buf->len = 0;
      return ret;
    } else {
      ret.str = strdup(buf->str);
      ret.len = buf->len;
      return ret;
    }
  } else {
    if( buf->len >= buf->cap/2 ) {
      ret.str = buf->str;
      ret.len = buf->len;
      buf->cap = buf->len = 0;
      return ret;
    } else {
      ret.str = strdup(buf->str);
      ret.len = buf->len;
      return ret;
    }
  }
}

struct dict {
  void* entry;
  size_t cap;
  size_t len;
};

struct list {
  void** entry;
  size_t cap;
  size_t len;
};

struct slab {

};

#endif /* _UTIL_H_ */
