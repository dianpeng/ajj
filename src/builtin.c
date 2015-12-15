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
#define OBJECT(val) ((val)->value.object->val.obj.data)

/* LIST */
#define LIST_TYPE 1
#define LIST(val) ((struct list*)OBJECT(val))

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
        0,
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
    struct ajj_value* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  struct list* l = LIST(obj);
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
      l->entry[l->len++] = ajj_value_move(obj->value.object->scp,
          t->entry+i);
    }

    RETURN_VOID;
  }
}

/* pop_back */
static
int list_pop_back( struct ajj* a,
    struct ajj_value* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  if( arg_len > 0 ) {
    FAIL(a,"list::pop_back cannot accept argument!");
  } else {
    struct list* l = LIST(obj);
    if( l->len == 0 ) {
      FAIL(a,"list::pop_back cannot pop value from list has 0 elements!");
    } else {
      --l->len;
    }
    RETURN_VOID;
  }
}

/* count */
static
int list_count( struct ajj* a,
    struct ajj_value* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  if( arg_len > 0 )
    FAIL(a,"list::count cannot accept argument!");
  else {
    struct list* l = LIST(obj);
    *ret = ajj_value_number(l->len);
    return AJJ_EXEC_OK;
  }
}

/* clear */
static
int list_clear( struct ajj* a,
    struct ajj_value* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  if( arg_len > 0 ) {
    FAIL(a,"list::clear cannot accept argument!");
  } else {
    struct list* l = LIST(obj);
    l->len = 0;
    RETURN_VOID;
  }
}

/* slot protocol */
static
size_t list_length( struct ajj* a , void* udata , struct ajj_value* l ) {
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  assert( IS_A(l,LIST_TYPE) );
  return LIST(l)->len;
}

static
int list_iter_start( struct ajj* a , void* udata , struct ajj_value* l) {
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  assert( IS_A(l,LIST_TYPE) );
  return 0;
}

static
int list_iter_move( struct ajj* a , void* udata , struct ajj_value* l, int itr ) {
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  assert( IS_A(l,LIST_TYPE) );
  return itr+1;
}

static
int list_iter_has( struct ajj* a , void* udata , struct ajj_value* l , int itr ) {
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  assert( IS_A(l,LIST_TYPE) );
  return itr < LIST(l)->len;
}

static
struct ajj_value
list_iter_get_key( struct ajj* a , void* udata , struct ajj_value* l , int itr ) {
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  assert( IS_A(l,LIST_TYPE) );
  return ajj_value_number(itr);
}

static
struct ajj_value
list_iter_get_val( struct ajj* a , void* udata , struct ajj_value* l , int itr ) {
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  assert( IS_A(l,LIST_TYPE) );
  assert( itr < LIST(l)->len);
  return LIST(l)->entry[itr];
}

static
int list_empty( struct ajj* a , void* udata , struct ajj_value* l ) {
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  assert( IS_A(l,LIST_TYPE) );
  return LIST(l)->len == 0;
}

static
int list_in ( struct ajj* a , void* udata , struct ajj_value* l , const struct ajj_value* idx ) {
  int k;
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  assert( IS_A(l,LIST_TYPE) );
  if( vm_to_integer(a,idx,&k) ) {
    FAIL(a,"list::__in__ cannot convert index to integer!");
  } else {
    return LIST(l)->len > idx && idx >= 0;
  }
}

static
struct ajj_value
list_index( struct ajj* a , void* udata , struct ajj_value* l , const struct ajj_value* idx ) {
  int i;
  if( vm_to_integer(a,idx,&i) )
    return AJJ_NONE;
  else {
    struct list* l = LIST(l);
    if( l->len <= idx ) {
      return AJJ_NONE;
    } else {
      return l->entry[i];
    }
  }
}

/* list class */
static struct ajj_class LIST_CLASS  = {
  "list",
  list_ctor,
  list_dtor,
  {
    { list_append, "append" },
    { list_extend, "extend" },
    { list_pop_back, "pop_back" },
    { list_count, "count" },
    { list_clear, "clear" }
  },
  5,
  {
    list_iter_start,
    list_iter_move,
    list_iter_has,
    list_iter_get_key,
    list_iter_get_val,
    list_length,
    list_empty,
    list_in,
    list_index
  },
  NULL
};

/* XList
 * An easy way to craft a list that is used to do foreach. It basically doesn't have any
 * member and cannot be used to add and set member */
struct xlist {
  size_t len;
};


/* DICT */
#define DICT_TYPE 2
#define DICT(val) ((struct map*)UDATA(val))
#define DEFAULT_DICT_CAP 8

static
int dict_ctor( struct ajj* a ,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    void** ret,
    int* tp ) {
  UNUSE_ARG(udata);

  if( arg_len != 0 ) {
    FAIL(a,"dict::__ctor__ cannot accept arguments!");
  } else {
    struct map* m = malloc(sizeof(*m));
    map_create(m,sizeof(struct ajj_value),DEFAULT_DICT_CAP);
    *tp = DICT_TYPE;
    *ret = m;
    return AJJ_EXEC_OK;
  }
}

static
void dict_dtor( struct ajj* a , void* udata, void* object ) {
  struct map* m = (struct map*)object;
  UNUSE_ARG(udata);

  map_destroy(m);
  free(m);
}

/* get */
static
int dict_get( struct ajj* a ,
    struct ajj_value* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  struct string key ;
  int own;

  if( arg_len != 1 || vm_to_string(arg,&key,&own) ) {
    FAIL(a,"dict::get cannot convert argument to string as key!");
  } else {
    struct map* m = DICT(obj);
    struct ajj_value* val;
    if((val=map_find(m,&key)) == NULL) {
      if(own) string_destroy(&key);
      *ret = AJJ_NONE;
    } else {
      if(own) string_destroy(&key);
      *ret = *val;
    }
    return AJJ_EXEC_OK;
  }
}

/* set */
static
int dict_set( struct ajj* a ,
    struct ajj_value* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  struct string key;
  int own;

  if( arg_len != 2 || vm_to_string(arg,&key,&own) ) {
    FAIL(a,"dict::set can only accept 2 arguments and the\
        first one must be a string!");
  } else {
    struct map* m = DICT(obj);
    struct ajj_value* val;
    if( map_insert(m,&key,own,arg+1) ) {
      if( own ) string_destroy(&key);
      *ret = AJJ_FALSE;
    }
    *ret = AJJ_TRUE;
    return AJJ_EXEC_OK;
  }
}

/* has_key */
static
int dict_has_key( struct ajj* a ,
    struct ajj_value* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  struct string key;
  int own;

  if( arg_len != 1 || vm_to_string(arg,&key,&own) ) {
    FAIL(a,"dict::has_key can only accept 1 argument and it \
        must be a string!");
  } else {
    struct map* m = DICT(obj);
    struct ajj_value* val;
    if( (val = map_find(m,&key)) ) {
      *ret = AJJ_TRUE;
    } else {
      *ret = AJJ_FALSE;
    }
    if(own) string_destroy(&key);
    return AJJ_EXEC_OK;
  }
}

/* udpate */
static
int dict_udpate( struct ajj* a ,
    struct ajj_value* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  struct string key;
  int own;

  if( arg_len != 2 || vm_to_string(arg,&key,&own) ) {
    FAIL(a,"dict::updatae can only accept 2 arguments and the first \
        one must be a string!");
  } else {
    struct map* m = DICT(obj);
    struct ajj_value* val;
    if( (val = map_find(m,&key)) ) {
      *val = ajj_value_move(obj->value.object->scp,arg+1);
      *ret = AJJ_TRUE;
    } else {
      *ret = AJJ_FALSE;
    }
    if(own) string_destroy(&key);
    return AJJ_EXEC_OK;
  }
}

/* clear */
static
int dict_clear( struct ajj* a ,
    struct ajj_value* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  if( arg_len != 0 ) {
    FAIL(a,"dict::clear cannot accept argument!");
  } else {
    struct map* m = DICT(obj);
    map_clear(m);
  }
  RETURN_VOID;
}

static
int dict_iter_start( struct ajj* a , void* udata ,
    struct ajj_value* obj ) {
  struct map* m = DICT(obj);
  UNUSE_ARG(a);
  UNUSE_ARG(udata);

  assert( IS_A(obj,DICT_TYPE) );
  return map_iter_start(m);
}
















