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
#define OBJECT(V) ((V)->value.object->val.obj.data)

/* LIST */
#define LIST_TYPE (AJJ_VALUE_EXTENSION+1)
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
    FAIL(a,"%s","list::__ctor__ cannot accept arguments!");
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
void list_dtor( struct ajj* a, void* udata /* NULL */,
    void* object ) {
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  free(object);
}

/* append */
static
int list_append( struct ajj* a ,
    struct ajj_value * obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  struct list* l = LIST(obj);
  if( arg_len > 1 )
    FAIL(a,"%s","list::append can only accept 1 argument!");
  if( l->len == l->cap ) {
    /* grow the memory */
    l->entry = mem_grow(l->entry,sizeof(struct ajj_value),
        0,
        &(l->cap));
  }
  assert(l->len < l->cap);
  /* move the target value to THIS gc scope */
  l->entry[l->len++] = ajj_value_move(
      obj->value.object->scp,arg);

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
  if( arg_len > 1 || !IS_A(arg,LIST_TYPE) )
    FAIL(a,"%s","list::extend can only accept 1 argument as a list!");
  else {
    struct list* t  = LIST(arg);
    size_t i;
    if( l->len + t->len > l->cap ) {
      l->cap = sizeof(struct ajj_value)*(l->len+t->len) + l->len;
      l->entry = realloc( l->entry,l->cap );
    }
    /* Unfortunately we cannot use memcpy since we need to move those
     * value to the new scope */
    for( i = 0 ; i < t->len ; ++i ) {
      l->entry[l->len++] =
        ajj_value_move(obj->value.object->scp,t->entry+i);
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
    FAIL(a,"%s","list::pop_back cannot accept argument!");
  } else {
    struct list* l = LIST(obj);
    if( l->len == 0 ) {
      FAIL(a,"%s","list::pop_back cannot pop value from \
          list has 0 elements!");
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
    FAIL(a,"%s","list::count cannot accept argument!");
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
    FAIL(a,"%s","list::clear cannot accept argument!");
  } else {
    struct list* l = LIST(obj);
    l->len = 0;
    RETURN_VOID;
  }
}

/* slot protocol */
static
size_t list_length( struct ajj* a , struct ajj_value* l ) {
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  return LIST(l)->len;
}

static
int list_iter_start( struct ajj* a , struct ajj_value* l) {
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  return 0;
}

static
int list_iter_move( struct ajj* a , struct ajj_value* l, int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  return itr+1;
}

static
int list_iter_has( struct ajj* a , struct ajj_value* l , int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  return itr < LIST(l)->len;
}

static
struct ajj_value
list_iter_get_key( struct ajj* a , struct ajj_value* l , int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  return ajj_value_number(itr);
}

static
struct ajj_value
list_iter_get_val( struct ajj* a , struct ajj_value* l , int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  assert( itr < LIST(l)->len);
  return LIST(l)->entry[itr];
}

static
int list_empty( struct ajj* a , struct ajj_value* l ) {
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  return LIST(l)->len == 0;
}

static
int list_in ( struct ajj* a , struct ajj_value* l ,
    const struct ajj_value* idx ) {
  int k;
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  if( vm_to_integer(idx,&k) ) {
    return 0;
  } else {
    return LIST(l)->len > (size_t)k && k >= 0;
  }
}

static
struct ajj_value
list_attr_get( struct ajj* a , struct ajj_value* obj,
    const struct ajj_value* idx ) {
  int i;
  UNUSE_ARG(a);
  if( vm_to_integer(idx,&i) )
    return AJJ_NONE;
  else {
    struct list* l = LIST(obj);
    if( l->len <= (size_t)i ) {
      return AJJ_NONE;
    } else {
      return l->entry[i];
    }
  }
}

static
void list_attr_set( struct ajj* a, struct ajj_value* obj,
    const struct ajj_value* idx ,
    const struct ajj_value* val ) {
  int i;
  UNUSE_ARG(a);
  assert( IS_A(obj,LIST_TYPE) );
  if( vm_to_integer(idx,&i) )
    return;
  else {
    struct list* l = LIST(obj);
    if( (size_t)i < l->len ) {
      l->entry[i] = ajj_value_move(
          obj->value.object->scp,
          val);
    }
  }
}

static
void list_attr_push( struct ajj* a, struct ajj_value* obj,
    const struct ajj_value* val ) {
  struct ajj_value arg = *val;
  struct ajj_value ret;
  assert( IS_A(obj,LIST_TYPE) );
  CHECK(list_append(a,obj,&arg,1,&ret) == AJJ_EXEC_OK);
}

static
const char*
list_display( struct ajj* a , const struct ajj_value* obj,
    size_t* len ) {
  struct list* lst;
  size_t i;
  struct strbuf sbuf;

  assert( IS_A(obj,LIST_TYPE) );
  lst = LIST(obj);
  strbuf_init(&sbuf);
  for( i = 0 ; i < lst->len ; ++i ) {
    struct ajj_value* val = lst->entry + i;
    int own;
    size_t l;
    const char* c;
    c = ajj_display(a,val,&l,&own);
    if(!c) continue; /* skip */
    strbuf_append(&sbuf,c,l);
    if(own) free((void*)c); /* free the memory */
    if( i < lst->len-1 )
      strbuf_append(&sbuf," ",1);
  }
  *len = sbuf.len;
  return sbuf.str;
}

/* list class */
struct ajj_class_method LIST_METHOD[] = {
  { list_append, "append" },
  { list_extend, "extend" },
  { list_pop_back, "pop_back" },
  { list_count, "count" },
  { list_clear, "clear" }
};

static struct ajj_class LIST_CLASS  = {
  "list",
  list_ctor,
  list_dtor,
  LIST_METHOD,
  ARRAY_SIZE(LIST_METHOD),
  {
    list_iter_start,
    list_iter_move,
    list_iter_has,
    list_iter_get_key,
    list_iter_get_val,
    list_length,
    list_empty,
    list_in,
    list_attr_get,
    list_attr_set,
    list_attr_push,
    list_display
  },
  NULL
};

/* DICT */
#define DICT_TYPE (AJJ_VALUE_EXTENSION+2)
#define DICT(val) ((struct map*)OBJECT(val))
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
    FAIL(a,"%s","dict::__ctor__ cannot accept arguments!");
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
    FAIL(a,"%s","dict::get cannot convert argument to string as key!");
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
    FAIL(a,"%s","dict::set can only accept 2 arguments and the\
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
    FAIL(a,"%s","dict::has_key can only accept 1 argument and it \
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
int dict_update( struct ajj* a ,
    struct ajj_value* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  struct string key;
  int own;

  if( arg_len != 2 || vm_to_string(arg,&key,&own) ) {
    FAIL(a,"%s","dict::updatae can only accept 2 arguments and the first \
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
    FAIL(a,"%s","dict::clear cannot accept argument!");
  } else {
    struct map* m = DICT(obj);
    map_clear(m);
  }
  RETURN_VOID;
}

static
int dict_iter_start( struct ajj* a ,
    struct ajj_value* obj ) {
  UNUSE_ARG(a);

  assert( IS_A(obj,DICT_TYPE) );
  return map_iter_start(DICT(obj));
}

static
int dict_iter_has( struct ajj* a ,
    struct ajj_value* obj , int itr ) {
  UNUSE_ARG(a);

  assert( IS_A(obj,DICT_TYPE) );
  return map_iter_has(DICT(obj),itr);
}

static
int dict_iter_move( struct ajj* a  ,
    struct ajj_value* obj , int itr ) {
  UNUSE_ARG(a);

  assert( IS_A(obj,DICT_TYPE) );
  return map_iter_move(DICT(obj),itr);
}

static
struct ajj_value
dict_iter_get_key( struct ajj* a ,
    struct ajj_value* obj , int itr ) {
  struct map_pair ret;
  UNUSE_ARG(a);
  assert( IS_A(obj,DICT_TYPE) );
  ret = map_iter_deref(DICT(obj),itr);
  /* return a copied string for key, we do the copy for safety reason
   * because it is possible this object is dropped but its returned key
   * should not dropped so we cannot return a constant string reference */
  return ajj_value_assign(
      ajj_object_create_string(
        a,
        ajj_cur_gc_scope(a),
        ret.key->str,
        ret.key->len,
        0
        ));
}

static
struct ajj_value
dict_iter_get_val( struct ajj* a ,
    struct ajj_value* obj , int itr ) {
  struct map_pair ret;
  UNUSE_ARG(a);
  assert( IS_A(obj,DICT_TYPE) );
  ret = map_iter_deref(DICT(obj),itr);
  return *(struct ajj_value*)(ret.val);
}

static
size_t dict_length( struct ajj* a ,
    struct ajj_value* obj ) {
  UNUSE_ARG(a);
  return DICT(obj)->len;
}

static
int dict_in( struct ajj* a ,
    struct ajj_value* obj , const struct ajj_value* key ) {
  struct string str;
  int own;
  UNUSE_ARG(a);
  assert( IS_A(obj,DICT_TYPE) );
  if( vm_to_string(key,&str,&own) )
    return 0;
  else {
    if( map_find( DICT(obj) , &str ) ) {
      if(own) string_destroy(&str);
      return 1;
    } else {
      if(own) string_destroy(&str);
      return 0;
    }
  }
}

static
int dict_empty( struct ajj* a , struct ajj_value* obj ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,DICT_TYPE) );
  return DICT(obj)->len == 0;
}

static
struct ajj_value
dict_attr_get( struct ajj* a ,
    struct ajj_value* obj, const struct ajj_value* key ) {
  struct ajj_value ret;
  struct ajj_value arg = *key;
  CHECK(dict_get(a,obj,&arg,1,&ret)==AJJ_EXEC_OK);
  return ret;
}

static
void
dict_attr_set( struct ajj* a, struct ajj_value* obj,
    const struct ajj_value* key,
    const struct ajj_value* val ) {
  struct ajj_value ret;
  struct ajj_value arg[2] = { *key , *val };
  CHECK(dict_set(a,obj,arg,2,&ret)==AJJ_EXEC_OK);
}

static
const char*
dict_display( struct ajj* a , const struct ajj_value* obj,
    size_t* len ) {
  struct strbuf sbuf;
  size_t i;
  struct map* mp;
  int itr;

  assert( IS_A(obj,DICT_TYPE) );
  strbuf_init(&sbuf);
  mp = DICT(obj);

  /* loop to dump all the content out */
  i = 0 ; itr = map_iter_start(mp);
  while( map_iter_has(mp,itr) ) {
    struct map_pair ret = map_iter_deref(mp,itr);
    size_t l;
    int own;
    const char* c;

    c = ajj_display(a,
        (struct ajj_value*)(ret.val),&l,&own);
    if(c) {
      /* append key */
      strbuf_append(&sbuf,ret.key->str,ret.key->len);
      strbuf_append(&sbuf,"=",1);
      strbuf_append(&sbuf,c,l);

      if(own) free((void*)(c));
      if( i < mp->len-1 ) {
        strbuf_append(&sbuf," ",1);
      }
    }
    ++i; itr = map_iter_move(mp,itr);
  }
  *len = sbuf.len;
  return sbuf.str;
}

static struct ajj_class_method DICT_METHOD[] = {
  { dict_set , "set" },
  { dict_get , "get" },
  { dict_update, "update" },
  { dict_has_key, "has_key" },
  { dict_clear, "clear" }
};

static struct ajj_class DICT_CLASS = {
  "dict",
  dict_ctor,
  dict_dtor,
  DICT_METHOD,
  ARRAY_SIZE(DICT_METHOD),
  {
    dict_iter_start,
    dict_iter_move,
    dict_iter_has,
    dict_iter_get_key,
    dict_iter_get_val,
    dict_length,
    dict_empty,
    dict_in,
    dict_attr_get,
    dict_attr_set,
    NULL,
    dict_display
  },
  NULL
};

/* XList
 * An easy way to craft a list that is used to do foreach. It basically doesn't have any
 * member and cannot be used to add and set member */
#define XLIST_TYPE (AJJ_VALUE_EXTENSION+3)
#define XLIST(val) ((struct xlist*)(OBJECT(val)))

struct xlist {
  size_t len;
};

static
int xlist_ctor( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    void** ret,
    int* tp ) {
  struct xlist* x;
  int val;

  if( arg_len != 1 || vm_to_integer(arg,&val) ) {
    FAIL(a,"%s","xlist::__ctor__ can only accept 1 \
        argument and it must be a integer!");
  } else {
    x = malloc(sizeof(*x));
    x->len = (size_t)(val);
    *ret = x;
    *tp = XLIST_TYPE;
    return AJJ_EXEC_OK;
  }
}

static
void xlist_dtor( struct ajj* a, void* udata,
    void* object ) {
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  free(object);
}

/* slots functions */
static
int xlist_iter_start( struct ajj* a,
    struct ajj_value* obj ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,XLIST_TYPE) );
  return 0;
}

static
int xlist_iter_has( struct ajj* a,
    struct ajj_value* obj , int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,XLIST_TYPE) );
  return itr < XLIST(obj)->len;
}

static
int xlist_iter_move( struct ajj* a,
    struct ajj_value* obj , int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,XLIST_TYPE) );
  return ++itr;
}

static
struct ajj_value
xlist_iter_get_key( struct ajj* a,
    struct ajj_value* obj , int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,XLIST_TYPE) );
  return ajj_value_number(itr);
}

static
struct ajj_value
xlist_iter_get_val( struct ajj* a,
    struct ajj_value* obj , int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,XLIST_TYPE) );
  return ajj_value_number(itr);
}

static
size_t xlist_length( struct ajj* a,
    struct ajj_value* obj ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,XLIST_TYPE) );
  return XLIST(obj)->len;
}

static
int xlist_empty( struct ajj* a ,
    struct ajj_value* obj ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,XLIST_TYPE) );
  return XLIST(obj)->len == 0;
}

static
const char*
xlist_display( struct ajj* a , const struct ajj_value* val,
    size_t* len ) {
  UNUSE_ARG(a);
  assert( IS_A(val,XLIST_TYPE) );
  *len = 1;
  return "";
}

static
struct ajj_class XLIST_CLASS = {
  "xlist",
  xlist_ctor,
  xlist_dtor,
  NULL,
  0,
  {
    xlist_iter_start,
    xlist_iter_move,
    xlist_iter_has,
    xlist_iter_get_key,
    xlist_iter_get_val,
    xlist_length,
    xlist_empty,
    NULL,
    NULL,
    NULL,
    NULL,
    xlist_display
  },
  NULL,
};

void ajj_builtin_load( struct ajj* a ) {
  /* list */
  a->list = ajj_add_value(a,&(a->builtins),NULL,
      AJJ_VALUE_CLASS,&LIST_CLASS);
  /* dict */
  a->dict = ajj_add_value(a,&(a->builtins),NULL,
      AJJ_VALUE_CLASS,&DICT_CLASS);
  /* xlist */
  ajj_add_value(a,&(a->builtins),NULL,
      AJJ_VALUE_CLASS,&XLIST_CLASS);
}
