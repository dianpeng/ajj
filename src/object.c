#include "ajj-priv.h"

/* ========================
 * List implementation
 * ======================*/

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

/* Function table */
struct function*
func_tabe_find_func( struct func_table* tb , const struct string* name ) {
  int i;
  for( i = 0 ; i < tb->func_len ; ++i ) {
    if( string_eq(name,&(tb->func_tb[i].name)) ) {
      return tb->func_tb + i;
    }
  }
  return NULL;
}
