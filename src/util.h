#ifndef _UTIL_H_
#define _UTIL_H_
#include <string.h>
#include <stddef.h>

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

#endif /* _UTIL_H_ */
