#include "ajj-priv.h"

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
