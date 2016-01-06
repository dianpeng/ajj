#ifndef _UPVALUE_H_
#define _UPVALUE_H_
#include "conf.h"
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
  UPVALUE_FUNCTION
};

struct upvalue {
  struct upvalue* prev;      /* previous upvalue */
  union {
    struct ajj_value val;    /* value , could contain whatever */
    struct function gfunc;   /* function, global executable entry */
  } gut;
  int type : 31;
  int fixed: 1; /* indicate whether this upvalue is not
                 * chainable. */
};

struct upvalue_table {
  struct map d; /* dictionary of the map */
  struct upvalue_table* prev; /* previous table */
};

struct upvalue_table*
upvalue_table_init( struct upvalue_table* ret ,
    struct upvalue_table* p );

/* Global varialbes table. Wrapper around map */
struct upvalue_table*
upvalue_table_create( struct upvalue_table* p );

struct upvalue_table*
upvalue_table_destroy_one( struct ajj* a,
    struct upvalue_table* m );

void
upvalue_table_clear( struct ajj* , struct upvalue_table * );

/* this function will recursively clean each
 * upvalue table layer and delete the table
 * itself. */
void
upvalue_table_destroy( struct ajj* a,
    struct upvalue_table* m ,
    const struct upvalue_table* until );

/* add a upvalue into the upvalue table chain */
struct upvalue*
upvalue_table_add( struct ajj* a ,
    struct upvalue_table* ,
    const struct string* ,
    int own ,
    int force,
    int fixed );

struct upvalue*
upvalue_table_add_c( struct ajj* a,
    struct upvalue_table* ,
    const char* key ,
    int force,
    int fixed );

struct upvalue*
upvalue_table_overwrite( struct ajj* a,
    struct upvalue_table* ,
    const struct string* ,
    int own ,
    int fixed );

struct upvalue*
upvalue_table_overwrite_c( struct ajj* a,
    struct upvalue_table* ,
    const char* key ,
    int fixed );

/* delete a upvalue inside of the table chain */
int
upvalue_table_del( struct ajj* a,
    struct upvalue_table* ,
    const struct string*  ,
    const struct upvalue_table* util );

int
upvalue_table_del_c( struct ajj* a,
    struct upvalue_table* ,
    const char* key ,
    const struct upvalue_table* util );

struct upvalue*
upvalue_table_find( struct upvalue_table* ,
    const struct string* ,
    const struct upvalue_table* util );

struct upvalue*
upvalue_table_find_c( struct upvalue_table* ,
    const char* ,
    const struct upvalue_table* util );

#endif /* _UPVALUE_H_ */
