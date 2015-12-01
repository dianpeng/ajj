#ifndef _UPVALUE_H_
#define _UPVALUE_H_
#include "util.h"
#include "object.h"

/* UPVALUE
 * The upvalue is used as a context. User could change it as they
 * want. Upvalue are stored inside of upvalue table, but user could
 * have multiple upvalue table linked together, the most recent linked
 * upvalue will be choosed at first to resolve a name lookup until
 * we hit the parent most upvalue. Inside of each upvalue table. The
 * upvalue entry is actually a stack. Therefore user is allowed to push
 * multiple upvalue associated with a certain key and then pop it.
 * To have the table as a linked stack is basically because this makes
 * code easier to write when we need to set up lots of upvalue and tear
 * them down at once.
 *
 * [ upvalue_table ] <--- [ upvalue_table ] <--- [ upvalue_table ]
 *   | key1: upvalue <-- upvalue <-- upvalue
 *   | key2: upvalue <-- upvalue <-- upvalue
 *
 */

#define UPVALUE_DEFAULT_BUF_SIZE 32

enum {
  UPVALUE_VALUE,
  UPVALUE_FUNCTION,
  UPVALUE_OBJECT
};

struct upvalue {
  struct upvalue* prev;      /* previous upvalue */
  union {
    struct ajj_value val;    /* value , could contain whatever */
    struct c_closure func;   /* global function */
    struct func_table* obj;  /* user registered object */
  } gut;
  int type;
};

struct upvalue_table {
  struct map d; /* dictionary of the map */
  struct upvalue_table* prev; /* previous table */
};

/* Global varialbes table. Wrapper around map */
static
struct upvalue_table*
upvalue_table_create( struct upvalue_table* p ) {
  struct upvalue_table* ret = malloc(sizeof(*p));
  map_create(&(ret->d),sizeof(struct upvalue),
      UPVALUE_DEFAULT_BUF_SIZE );
  ret->prev = p;
  return ret;
}

void upvalue_table_clear( struct upvalue_table* m );

struct upvalue_table*
upvalue_table_destroy_one( struct upvalue_table* m );

struct upvalue_table*
upvalue_table_destroy( struct upvalue_table* m );

/* add a upvalue into the upvalue table chain */
int upvalue_table_add( struct upvalue_table* ,
    const struct string* ,
    int own,
    const struct upvalue* );

int upvalue_table_add_c( struct upvalue_table* ,
    const char* key,
    const struct upvalue* );

int upvalue_table_del( struct upvalue_table* ,
    const struct string* ,
    struct upvalue* );

int upvalue_table_del_c( struct upvalue_table* ,
    const char* key,
    struct upvalue* );

struct upvalue*
upvalue_table_find( struct upvalue_table* ,
    const struct string* );

struct upvalue*
upvalue_table_find_c( struct upvalue_table* ,
    const char* );

#endif /* _UPVALUE_H_ */
