#include "builtin.h"
#include "ajj-priv.h"
#include "util.h"
#include "vm.h"

#define DICT_DEFAULT_BUF_SIZE 4

#define FAIL(a,f,...) \
  do { \
    ajj_error(a,f,__VA_ARGS__); \
    return AJJ_EXEC_FAIL; \
  } while(0)

#define RETURN_VOID \
  (*ret) = AJJ_NONE; \
  return AJJ_EXEC_OK

#define IS_A(val,T) (((val)->type == AJJ_VALUE_OBJECT) && ((val)->value.object->tp = T))

/* LIST */
#define LIST_TYPE 1
#define LIST(val) ((struct list*)((val)->value.object->val.obj.data))

/* List core data structure and it will be embeded inside of the
 * struct object to be used as an builtin object */
struct list {
  struct ajj_value* entry;
  size_t cap;
  size_t len;
};

/* Ctor */
static
int list_ctor( struct ajj* a, void* udata /* NULL */,
    struct ajj_value* val,
    size_t len,
    void** ret, int* type ) {
  if( len > 0 ) {
    FAIL(a,"list::__ctor__ cannot accept arguments!");
  } else {
    struct list* l;
    l = malloc(sizeof(*l));
    l->cap = 0;
    l->len = 0;
    l->entry = NULL;
    *ret = l;
    return AJJ_EXEC_OK;
  }
}

/* Dtor */
static
int list_dtor( struct ajj* a, void* udata /* NULL */,
    void* object ) {
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  free(object);
}

/* append */
static
int list_append( struct ajj* a ,
    struct ajj_object* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  struct list* l = obj->val.obj.data;
  if( arg_len > 1 )
    FAIL(a,"list::append can only accept 1 argument!");
  if( l->len == l->cap ) {
    /* grow the memory */
    l->entry = mem_grow(l->entry,sizeof(struct ajj_value),
        &(l->cap));
  }
  assert(l->len < l->cap);
  /* move the target value to THIS gc scope */
  l->entry[l->len++] = ajj_value_move(obj->scp,arg);

  RETURN_VOID;
}

/* extend */
static
int list_extend( struct ajj* a ,
    struct ajj_object* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  struct list* l = obj->val.obj.data;
  if( arg_len > 1 || IS_A(arg,LIST_TYPE) )
    FAIL(a,"list::extend can only accept 1 argument!");
  else {
    struct list* t  = arg->value.object->val.obj.data;
    size_t i;
    if( l->len + t->len > l->cap ) {
      l->cap = sizeof(struct ajj_value)*(l->len+t->len) + l->len;
      l->entry = realloc( l->entry,l->cap );
    }
    /* Unfortunately we cannot use memcpy since we need to move those
     * value to the new scope */
    for( i = 0 ; i < t->len ; ++i ) {
      l->entry[l->len++] = ajj_value_move(obj->scp,
          t->entry+i);
    }

    RETURN_VOID;
  }
}

/* pop_back */
static
int list_pop_back( struct ajj* a,
    struct ajj_object* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  if( arg_len > 0 ) {
    FAIL(a,"list::pop_back cannot accept argument!");
  } else {
    struct list* l = obj->val.obj.data;
    if( l->len == 0 ) {
      FAIL(a,"list::pop_back cannot pop value from list has 0 elements!");
    } else {
      --l->len;
    }
    RETURN_VOID;
}

/* index */
static
int list_index( struct ajj* a ,
    struct ajj_object* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  int idx;
  if( arg_len > 1 && vm_to_integer(a,arg,&idx) )
    FAIL(a,"list::index can only accept 1 integer argument!");
  else {
    struct list* l = obj->val.obj.data;
    if( l->len <= idx ) {
      FAIL(a,"list::index index out of range!");
    } else {
      *ret = l->entry[idx];
    }
    return AJJ_EXEC_OK;
  }
}

/* count */
static
int list_count( struct ajj* a,
    struct ajj_object* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  if( arg_len > 0 )
    FAIL(a,"list::count cannot accept argument!");
  else {
    struct list* l = obj->val.obj.data;
    *ret = ajj_value_number(l->len);
    return AJJ_EXEC_OK;
  }
}

/* clear */
static
int list_clear( struct ajj* a,
    struct ajj_object* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  if( arg_len > 0 ) {
    FAIL(a,"list::clear cannot accept argument!");
  } else {
    struct list* l = obj->val.obj.data;
    l->len = 0;
    RETURN_VOID;
  }
}

/* slot protocol */
static
size_t list_slot_length( void* udata , struct ajj_value* l ) {
  UNUSE_ARG(udata);
  assert( IS_A(l,LIST_TYPE) );
  return LIST(l)->len;
}

int list_iter_start( void* udata , struct ajj_value* l) {
  UNUSE_ARG(udata);
  assert( IS_A(l,LIST_TYPE) );
  return 0;
}

int list_iter_move( void* udata , struct ajj_value* l, int itr ) {
  UNUSE_ARG(udata);
  assert( IS_A(l,LIST_TYPE) );
  return itr+1;
}

int list_iter_has( void* udata , struct ajj_value* l , int itr ) {
  UNUSE_ARG(udata);
  assert( IS_A(l,LIST_TYPE) );
  return itr < LIST(l)->len;
}

struct ajj_value
list_iter_get_key( void* udata , struct ajj_value* l , int itr ) {
  UNUSE_ARG(udata);
  assert( IS_A(l,LIST_TYPE) );
  return ajj_value_number(itr);
}

struct ajj_value
list_iter_get_val( void* udata , struct ajj_value* l , int itr ) {
  UNUSE_ARG(udata);
  assert( IS_A(l,LIST_TYPE) );
  assert( itr < LIST(l)->len);
  return LIST(l)->entry[itr];
}










