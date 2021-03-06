#include "builtin.h"
#include "ajj-priv.h"
#include "util.h"
#include "vm.h"
#include "upvalue.h"
#include "object.h"
#include "bc.h"
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

#define EXEC_FAIL1(a,f,val) \
  do { \
    ajj_error(a,f,val); \
    return AJJ_EXEC_FAIL; \
  } while(0)

#define EXEC_FAIL2(a,f,v1,v2) \
  do { \
    ajj_error(a,f,v1,v2); \
    return AJJ_EXEC_FAIL; \
  } while(0)

#define RETURN_VOID \
  (*ret) = AJJ_NONE; \
  return AJJ_EXEC_OK

#define IS_A(val,T)  \
  (((val)->type == AJJ_VALUE_OBJECT) && \
   ((val)->value.object->tp = T))

#define OBJECT(V) ((V)->value.object->val.obj.data)

/* LIST */
#define LIST_TYPE (AJJ_VALUE_BUILTIN_TYPE+1)
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
    EXEC_FAIL1(a,"%s","list::__ctor__ cannot accept arguments!");
  } else {
    struct list* l;
    l = malloc(sizeof(*l));
    l->cap = 0;
    l->len = 0;
    l->entry = NULL;
    *ret = l;
    *type = LIST_TYPE;
    return AJJ_EXEC_OK;
  }
}

/* Dtor */
static
void list_dtor( struct ajj* a, void* udata /* NULL */,
    void* object ) {
  struct list* l;
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  l = (struct list*)object;
  free(l->entry);
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
  size_t i;
  if( arg_len == 0 )
    EXEC_FAIL1(a,"%s","list::append must have at least 1 arguments!");
  if( l->len == l->cap ) {
    /* grow the memory */
    l->entry = mem_grow(l->entry,sizeof(struct ajj_value),
        arg_len,
        &(l->cap));
  }
  assert(l->len < l->cap);

  /* move the target value to THIS gc scope */
  for( i = 0 ; i < arg_len ; ++i ) {
    l->entry[l->len++] = ajj_value_move_scope(a,
        obj->value.object->scp,arg+i);
  }

  *ret = *obj;
  return AJJ_EXEC_OK;
}

/* extend */
static
int list_extend( struct ajj* a ,
    struct ajj_value* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  struct list* l = LIST(obj);
  if( arg_len != 1 || !IS_A(arg,LIST_TYPE) )
    EXEC_FAIL1(a,"%s","list::extend can only accept 1 argument "
        "which must be a list!");
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
        ajj_value_move_scope(a,obj->value.object->scp,t->entry+i);
    }

    *ret = *obj;
    return AJJ_EXEC_OK;
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
    EXEC_FAIL1(a,"%s","list::pop_back cannot accept argument!");
  } else {
    struct list* l = LIST(obj);
    if( l->len == 0 ) {
      EXEC_FAIL1(a,"%s","list::pop_back cannot pop value from "
          "list has 0 elements!");
    } else {
      --l->len;
    }
    *ret = *obj;
    return AJJ_EXEC_OK;
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
    EXEC_FAIL1(a,"%s","list::count cannot accept argument!");
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
    EXEC_FAIL1(a,"%s","list::clear cannot accept argument!");
  } else {
    struct list* l = LIST(obj);
    l->len = 0;
    RETURN_VOID;
  }
}

/* slot protocol */
static
size_t list_length( struct ajj* a ,
    const struct ajj_value* l ) {
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  return LIST(l)->len;
}

static
int list_iter_start( struct ajj* a ,
    const struct ajj_value* l) {
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  return 0;
}

static
int list_iter_move( struct ajj* a ,
    const struct ajj_value* l, int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  return itr+1;
}

static
int list_iter_has( struct ajj* a ,
    const struct ajj_value* l , int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  return itr < LIST(l)->len;
}

static
struct ajj_value
list_iter_get_key( struct ajj* a ,
    const struct ajj_value* l , int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  return ajj_value_number(itr);
}

static
struct ajj_value
list_iter_get_val( struct ajj* a ,
    const struct ajj_value* l , int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  assert( itr < LIST(l)->len);
  return LIST(l)->entry[itr];
}

static
int list_empty( struct ajj* a ,
    const struct ajj_value* l ) {
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  return LIST(l)->len == 0;
}

static
struct ajj_value
list_attr_get( struct ajj* a ,
    const struct ajj_value* obj,
    const struct ajj_value* idx ) {
  int i = 0;
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
void list_attr_set( struct ajj* a,
    struct ajj_value* obj,
    const struct ajj_value* idx ,
    const struct ajj_value* val ) {
  int i = 0;
  UNUSE_ARG(a);
  assert( IS_A(obj,LIST_TYPE) );
  if( vm_to_integer(idx,&i) )
    return;
  else {
    struct list* l = LIST(obj);
    if( (size_t)i < l->len ) {
      l->entry[i] = ajj_value_move_scope(a,
          obj->value.object->scp,
          val);
    }
  }
}

static
void list_attr_push( struct ajj* a,
    struct ajj_value* obj,
    const struct ajj_value* val ) {
  struct ajj_value arg = *val;
  struct ajj_value ret;
  assert( IS_A(obj,LIST_TYPE) );
  CHECK(list_append(a,obj,&arg,1,&ret) == AJJ_EXEC_OK);
}

static
void list_move( struct ajj* a, struct ajj_value* obj ) {
  size_t i;
  struct list* l;

  assert( IS_A(obj,LIST_TYPE) );
  l = LIST(obj);
  for( i = 0 ; i < l->len ; ++i ) {
    ajj_value_move_scope(a,obj->value.object->scp,
        l->entry + i );
  }
}

static
const char*
list_display( struct ajj* a ,
    const struct ajj_value* obj,
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

/* comparison */
static
int list_in ( struct ajj* a , const struct ajj_value* l ,
    const struct ajj_value* val , int* result ) {
  size_t i;
  struct list* lst;
  UNUSE_ARG(a);
  assert( IS_A(l,LIST_TYPE) );
  lst = LIST(l);
  for( i = 0 ;  i < lst->len ; ++i ) {
    int cmp;
    if( ajj_value_eq( a , lst->entry+i, val, &cmp ) ) {
      return AJJ_EXEC_FAIL;
    }
    if(cmp) {
      *result = 1;
      return AJJ_EXEC_OK;
    }
  }
  *result = 0;
  return AJJ_EXEC_OK;
}

int list_eq(struct ajj* a, const struct ajj_value* l,
    const struct ajj_value* r , int* res ) {
  struct list* L;
  struct list* R;
  /* left value is always the list */
  assert( IS_A(l,LIST_TYPE) );
  L = LIST(l);

  if(!IS_A(r,LIST_TYPE)) {
    *res = 0; return AJJ_EXEC_OK;
  } else {
    R = LIST(r);
    if( R->len != L->len ) {
      *res = 0; return AJJ_EXEC_OK;
    } else {
      size_t i;
      for( i = 0 ; i < L->len ; ++i ) {
        struct ajj_value* lval = L->entry + i;
        struct ajj_value* rval = R->entry + i;
        int cmp;
        if( ajj_value_eq(a,lval,rval,&cmp) ) {
          return AJJ_EXEC_FAIL;
        }
        if(!cmp) {
          *res = 0; return AJJ_EXEC_OK;
        }
      }
      *res = 1; return AJJ_EXEC_OK;
    }
  }
}

int list_ne( struct ajj* a, const struct ajj_value* l,
    const struct ajj_value* r , int* result ) {
  if(list_eq(a,l,r,result))
    return AJJ_EXEC_FAIL;
  else {
    *result = !*result;
    return AJJ_EXEC_OK;
  }
}

void builtin_list_push( struct ajj* a, struct ajj_object* obj,
    struct ajj_value* val ) {
  struct ajj_value ret;
  struct ajj_value list = ajj_value_assign(obj);
  CHECK(list_append(a, &list, val, 1, &ret) == AJJ_EXEC_OK);
  (void)ret;
}

struct ajj_value
builtin_list_index( struct ajj* a, struct ajj_object* obj,
    int index ) {
  struct list* l;
  assert( obj->tp == LIST_TYPE );
  l = (struct list*)(obj->val.obj.data);
  if( index <0 || index >= l->len )
    return AJJ_NONE;
  else
    return l->entry[index];
}

void builtin_list_clear( struct ajj* a, struct ajj_object* obj ) {
  struct ajj_value ret;
  struct ajj_value list = ajj_value_assign(obj);
  CHECK(list_clear(a,&list,NULL,0,&ret) == AJJ_EXEC_OK);
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
    list_attr_get,
    list_attr_set,
    list_attr_push,
    list_move,
    list_display,
    list_in,
    list_eq,
    list_ne,
    NULL,
    NULL,
    NULL,
    NULL
  },
  NULL
};

/* DICT */
#define DICT_TYPE (AJJ_VALUE_BUILTIN_TYPE+2)
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
    EXEC_FAIL1(a,"%s","dict::__ctor__ cannot accept arguments!");
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
    EXEC_FAIL1(a,"%s","dict::get cannot convert argument to string as key!");
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
    EXEC_FAIL1(a,"%s","dict::set can only accept 2 arguments and the "
        "first one must be a string!");
  } else {
    struct map* m = DICT(obj);
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
    EXEC_FAIL1(a,"%s","dict::has_key can only accept 1 argument and it "
        "must be a string!");
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
    EXEC_FAIL1(a,"%s","dict::updatae can only accept 2 arguments and the first "
        "one must be a string!");
  } else {
    struct map* m = DICT(obj);
    struct ajj_value* val;
    if( (val = map_find(m,&key)) ) {
      *val = ajj_value_move_scope(a,obj->value.object->scp,arg+1);
      *ret = AJJ_TRUE;
    } else {
      *ret = AJJ_FALSE;
    }
    if(own) string_destroy(&key);
    return AJJ_EXEC_OK;
  }
}

/* pop */
static
int dict_pop( struct ajj* a ,
    struct ajj_value* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  struct string key;
  int own;

  if( arg_len != 1 || vm_to_string(arg,&key,&own) ) {
    EXEC_FAIL1(a,"%s","dict::pop can only accept 1 argument and it "
        "must be a string!");
  } else {
    struct map* m = DICT(obj);
    int r = map_remove(m,&key,NULL);
    *ret = ajj_value_boolean( !r );
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
    EXEC_FAIL1(a,"%s","dict::clear cannot accept argument!");
  } else {
    struct map* m = DICT(obj);
    map_clear(m);
  }
  RETURN_VOID;
}

/* count */
static
int dict_count( struct ajj* a ,
    struct ajj_value* obj,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  if( arg_len != 0 ) {
    EXEC_FAIL1(a,"%s","dict::count cannot accept argument!");
  } else {
    struct map* m = DICT(obj);
    *ret = ajj_value_number(map_size(m));
    return AJJ_EXEC_OK;
  }
}

static
int dict_iter_start( struct ajj* a ,
    const struct ajj_value* obj ) {
  UNUSE_ARG(a);

  assert( IS_A(obj,DICT_TYPE) );
  return map_iter_start(DICT(obj));
}

static
int dict_iter_has( struct ajj* a ,
    const struct ajj_value* obj , int itr ) {
  UNUSE_ARG(a);

  assert( IS_A(obj,DICT_TYPE) );
  return map_iter_has(DICT(obj),itr);
}

static
int dict_iter_move( struct ajj* a  ,
    const struct ajj_value* obj , int itr ) {
  UNUSE_ARG(a);

  assert( IS_A(obj,DICT_TYPE) );
  return map_iter_move(DICT(obj),itr);
}

static
struct ajj_value
dict_iter_get_key( struct ajj* a ,
    const struct ajj_value* obj , int itr ) {
  struct map_pair ret;
  UNUSE_ARG(a);
  assert( IS_A(obj,DICT_TYPE) );
  ret = map_iter_deref(DICT(obj),itr);

  /* return a copied string for key, we do
   * the copy for safety reason  because it
   * is possible this object is dropped but
   * its returned key should not dropped so
   * we cannot return a constant string reference */

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
    const struct ajj_value* obj , int itr ) {
  struct map_pair ret;
  UNUSE_ARG(a);
  assert( IS_A(obj,DICT_TYPE) );
  ret = map_iter_deref(DICT(obj),itr);
  return *(struct ajj_value*)(ret.val);
}

static
size_t dict_length( struct ajj* a ,
    const struct ajj_value* obj ) {
  UNUSE_ARG(a);
  return DICT(obj)->len;
}

static
int dict_empty( struct ajj* a , const struct ajj_value* obj ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,DICT_TYPE) );
  return DICT(obj)->len == 0;
}

static
struct ajj_value
dict_attr_get( struct ajj* a ,
    const struct ajj_value* obj,
    const struct ajj_value* key ) {
  struct ajj_value ret;
  struct ajj_value arg = *key;
  CHECK(dict_get(a,(struct ajj_value*)obj,
        &arg,1,&ret)==AJJ_EXEC_OK);
  return ret;
}

static
void dict_attr_set( struct ajj* a,
    struct ajj_value* obj,
    const struct ajj_value* key,
    const struct ajj_value* val ) {
  struct ajj_value ret;
  struct ajj_value arg[2];
  arg[0] = *key;
  arg[1] = *val;

  CHECK(dict_set(a,obj,arg,2,&ret)==AJJ_EXEC_OK);
}

static
void dict_move( struct ajj* a, struct ajj_value* obj ) {
  int itr;
  struct map* d;
  assert( IS_A(obj,DICT_TYPE) );
  d = DICT(obj);
  itr = map_iter_start(d);
  while( map_iter_has(d,itr) ) {
    struct map_pair p =
      map_iter_deref(d,itr);
    ajj_value_move_scope(a,obj->value.object->scp,
        (struct ajj_value*)p.val);
    itr = map_iter_move(d,itr);
  }
}

static
const char*
dict_display( struct ajj* a ,
    const struct ajj_value* obj,
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
        strbuf_append(&sbuf,";",1);
      }
    }
    ++i; itr = map_iter_move(mp,itr);
  }
  *len = sbuf.len;
  return sbuf.str;
}

/* comparision */
static
int dict_in( struct ajj* a ,
    const struct ajj_value* obj ,
    const struct ajj_value* key ,
    int* result ) {
  struct string str;
  int own;
  UNUSE_ARG(a);
  assert( IS_A(obj,DICT_TYPE) );
  if( vm_to_string(key,&str,&own) ) {
    *result = 0;
    return AJJ_EXEC_OK;
  } else {
    if( map_find( DICT(obj) , &str ) ) {
      if(own) string_destroy(&str);
      *result = 1;
      return AJJ_EXEC_OK;
    } else {
      if(own) string_destroy(&str);
      *result = 0;
      return AJJ_EXEC_OK;
    }
  }
}

static
int dict_eq( struct ajj* a ,
    const struct ajj_value* l,
    const struct ajj_value* r,
    int* result ) {
  assert( IS_A(l,DICT_TYPE) );
  if( !IS_A(r,DICT_TYPE) ) {
    *result = 0; return AJJ_EXEC_OK;
  } else {
    struct map* L = DICT(l);
    struct map* R = DICT(r);
    if( L->len != R->len ) {
      *result = 0; return AJJ_EXEC_OK;
    } else {
      int litr , ritr;
      litr = map_iter_start(L);
      ritr = map_iter_start(R);
      while( map_iter_has(L,litr) ) {
        struct map_pair lval =
          map_iter_deref(L,litr);
        struct map_pair rval =
          map_iter_deref(R,ritr);
        /* check whether key and value are equal */
        if( string_eq(lval.key,rval.key) ) {
          int cmp;
          if( ajj_value_eq(a,
                (struct ajj_value*)lval.val,
                (struct ajj_value*)rval.val,
                &cmp)) {
            return AJJ_EXEC_FAIL;
          } else {
            if(!cmp) {
              *result = 0;
              return AJJ_EXEC_OK;
            }
          }
        }
        /* move the iterator */
        litr = map_iter_move(L,litr);
        ritr = map_iter_move(R,ritr);
      }
      *result = 1;
      return AJJ_EXEC_OK;
    }
  }
}

static
int dict_ne( struct ajj* a,
    const struct ajj_value* l,
    const struct ajj_value* r,
    int* result ) {
  if(dict_eq(a,l,r,result))
    return AJJ_EXEC_FAIL;
  else {
    *result = !(*result);
    return AJJ_EXEC_OK;
  }
}

int object_is_map( struct ajj_value* v ) {
  return IS_A(v,DICT_TYPE);
}

struct map* object_cast_to_map( struct ajj_value* v ) {
  assert(IS_A(v,DICT_TYPE));
  return DICT(v);
}

void builtin_dict_insert( struct ajj* a,
    struct ajj_object* obj,
    struct ajj_value* key,
    struct ajj_value* val ) {
  struct ajj_value ret;
  struct ajj_value dict = ajj_value_assign(obj);
  struct ajj_value arg[2];
  arg[0] = *key ; arg[1] = *val;
  CHECK(dict_set(a,&dict,arg,2,&ret) == AJJ_EXEC_OK);
}

struct ajj_value builtin_dict_find( struct ajj* a,
    struct ajj_object* obj,
    struct ajj_value* key ) {
  struct ajj_value ret;
  struct ajj_value dict = ajj_value_assign(obj);
  CHECK(dict_get(a,&dict,key,1,&ret) == AJJ_EXEC_OK);
  return ret;
}

int builtin_dict_remove( struct ajj* a,
    struct ajj_object* obj,
    struct ajj_value* key ) {
  struct ajj_value ret;
  struct ajj_value dict = ajj_value_assign(obj);
  CHECK(dict_pop(a,&dict,key,0,&ret) == AJJ_EXEC_OK);
  return ajj_value_to_boolean(&ret) ? 0 : -1;
}

void builtin_dict_clear( struct ajj* a,
    struct ajj_object* obj ) {
  struct ajj_value ret;
  struct ajj_value dict = ajj_value_assign(obj);
  CHECK(dict_clear(a,&dict,NULL,0,&ret) == AJJ_EXEC_OK);
}

static struct ajj_class_method DICT_METHOD[] = {
  { dict_set , "set" },
  { dict_get , "get" },
  { dict_update, "update" },
  { dict_pop , "pop" },
  { dict_has_key, "has_key" },
  { dict_count , "count" },
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
    dict_attr_get,
    dict_attr_set,
    NULL,
    dict_move,
    dict_display,
    dict_in,
    dict_eq,
    dict_ne,
    NULL,
    NULL,
    NULL,
    NULL
  },
  NULL
};

/* XRange
 * An easy way to craft a list that is used to do basic for.
 * It basically doesn't have any member and cannot be used
 * to add and set member */

#define XRANGE_TYPE (AJJ_VALUE_BUILTIN_TYPE+3)
#define XRANGE(val) ((struct xrange*)(OBJECT(val)))

struct xrange {
  size_t len;
};

static
int xrange_ctor( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    void** ret,
    int* tp ) {
  struct xrange* x;
  int val = 0;

  if( arg_len != 1 || vm_to_integer(arg,&val) ) {
    EXEC_FAIL1(a,"%s","xrange::__ctor__ can only accept 1 "
        "argument and it must be a integer!");
  } else {
    x = malloc(sizeof(*x));
    x->len = (size_t)(val);
    *ret = x;
    *tp = XRANGE_TYPE;
    return AJJ_EXEC_OK;
  }
}

static
void xrange_dtor( struct ajj* a, void* udata,
    void* object ) {
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  free(object);
}

/* slots functions */
static
int xrange_iter_start( struct ajj* a,
    const struct ajj_value* obj ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,XRANGE_TYPE) );
  return 0;
}

static
int xrange_iter_has( struct ajj* a,
    const struct ajj_value* obj , int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,XRANGE_TYPE) );
  return itr < XRANGE(obj)->len;
}

static
int xrange_iter_move( struct ajj* a,
    const struct ajj_value* obj , int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,XRANGE_TYPE) );
  return ++itr;
}

static
struct ajj_value
xrange_iter_get_key( struct ajj* a,
    const struct ajj_value* obj , int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,XRANGE_TYPE) );
  return ajj_value_number(itr);
}

static
struct ajj_value
xrange_iter_get_val( struct ajj* a,
    const struct ajj_value* obj , int itr ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,XRANGE_TYPE) );
  return ajj_value_number(itr);
}

static
size_t xrange_length( struct ajj* a,
    const struct ajj_value* obj ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,XRANGE_TYPE) );
  return XRANGE(obj)->len;
}

static
int xrange_empty( struct ajj* a ,
    const struct ajj_value* obj ) {
  UNUSE_ARG(a);
  assert( IS_A(obj,XRANGE_TYPE) );
  return XRANGE(obj)->len == 0;
}

static
const char*
xrange_display( struct ajj* a ,
    const struct ajj_value* val,
    size_t* len ) {
  char buf[256];
  UNUSE_ARG(a);
  assert( IS_A(val,XRANGE_TYPE) );
  *len = (size_t)sprintf(buf,"xrange(" SIZEF ")",
      SIZEP(XRANGE(val)->len));
  return strdup(buf);
}

/* comparison */
#define DEFINE_XRANGE_CMP(T,O) \
  static \
  int xrange_##T( struct ajj* a, \
      const struct ajj_value* l, \
      const struct ajj_value* r, \
      int* result ) { \
    assert( IS_A(l,XRANGE_TYPE) ); \
    if( !IS_A(r,XRANGE_TYPE) ) { \
      *result = 0; return AJJ_EXEC_FAIL; \
    } else { \
      struct xrange* L = XRANGE(l); \
      struct xrange* R = XRANGE(r); \
      *result = L->len O R->len; \
      return AJJ_EXEC_OK; \
    } \
  }

DEFINE_XRANGE_CMP(eq,==)
DEFINE_XRANGE_CMP(ne,!=)
DEFINE_XRANGE_CMP(lt,<)
DEFINE_XRANGE_CMP(le,<=)
DEFINE_XRANGE_CMP(gt,>)
DEFINE_XRANGE_CMP(ge,>=)

#undef DEFINE_XRANGE_CMP

static
struct ajj_class XRANGE_CLASS = {
  "xrange",
  xrange_ctor,
  xrange_dtor,
  NULL,
  0,
  {
    xrange_iter_start,
    xrange_iter_move,
    xrange_iter_has,
    xrange_iter_get_key,
    xrange_iter_get_val,
    xrange_length,
    xrange_empty,
    NULL,
    NULL,
    NULL,
    NULL,
    xrange_display,
    NULL,
    xrange_eq,
    xrange_ne,
    xrange_lt,
    xrange_le,
    xrange_gt,
    xrange_ge
  },
  NULL,
};

/* =========================================
 * Loop Object
 * =======================================*/
struct loop {
  size_t index;
  size_t index0;

  size_t revindex;
  size_t revindex0;

  int first;
  int last;
  size_t length;
  /* we don't support depth/depth0 */
};

#define LOOP_TYPE (AJJ_VALUE_BUILTIN_TYPE + 4)
#define LOOP(V) ((struct loop*)(OBJECT(V)))

static
int loop_ctor( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    void** ret,
    int* tp ) {
  UNUSE_ARG(udata);
  if(arg_num != 1)
    EXEC_FAIL1(a,"%s","__loop__::__ctor__ can only accept 1 argument and "
        "it must be an integer!");
  else {
    int len = 0;
    if( vm_to_integer(arg,&len) ) {
      EXEC_FAIL1(a,"__loop__::__ctpr__ can only convert first argument:%s "
          "to integer!",ajj_value_get_type_name(arg));
    } else {
      struct loop* lp = malloc(sizeof(*lp));
      lp->index = 1;
      lp->index0= 0;
      lp->revindex= (size_t)(len);
      lp->revindex0=(size_t)(len-1);
      lp->first = 1;
      lp->last = 0;
      lp->length = (size_t)len;
      *ret = lp;
      *tp = LOOP_TYPE;
      return AJJ_EXEC_OK;
    }
  }
}

static
void loop_dtor( struct ajj* a,
    void* udata,
    void* object ) {
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  free(object);
}

static
struct ajj_value loop_attr_get( struct ajj* a,
    const struct ajj_value* obj,
    const struct ajj_value* val ) {
  struct string key;
  int own;
  if( vm_to_string(val,&key,&own) ) {
    return AJJ_NONE;
  } else {
    struct loop* l = LOOP(obj);
    if( string_eqc(&key,"index") ) {
      return ajj_value_number(l->index);
    } else if( string_eqc(&key,"index0") ) {
      return ajj_value_number(l->index0);
    } else if( string_eqc(&key,"revindex") ) {
      return ajj_value_number(l->revindex);
    } else if( string_eqc(&key,"revindex0") ) {
      return ajj_value_number(l->revindex0);
    } else if( string_eqc(&key,"first") ) {
      return ajj_value_number(l->first);
    } else if( string_eqc(&key,"last") ) {
      return ajj_value_number(l->last);
    } else if( string_eqc(&key,"length") ) {
      return ajj_value_number(l->length);
    } else {
      return AJJ_NONE;
    }
  }
}

const char*
loop_display( struct ajj* a, const struct ajj_value* obj,
    size_t* len ) {
  char buf[1024];
  struct loop* l;
  assert(IS_A(obj,LOOP_TYPE));
  l = LOOP(obj);
  *len = (size_t)sprintf(buf,"loop(index:"
      SIZEF
      ";index0:"
      SIZEF
      ";revindex:"
      SIZEF
      ";revindex0:"
      SIZEF
      ";first:%d;last:%d;length:"
      SIZEF")",
      SIZEP(l->index),
      SIZEP(l->index0),
      SIZEP(l->revindex),
      SIZEP(l->revindex0),
      l->first,
      l->last,
      SIZEP(l->length));
  return strdup(buf);
}

void builtin_loop_move( struct ajj_value* loop ) {
  struct loop* l;
  assert( IS_A(loop,LOOP_TYPE) );
  l = LOOP(loop);
  ++(l->index);
  ++(l->index0);
  --(l->revindex);
  --(l->revindex0);
  l->first = 0;
  if( l->index == l->length ) {
    l->last = 1;
  }
}

static
struct ajj_class LOOP_CLASS = {
  "__loop__",
  loop_ctor,
  loop_dtor,
  NULL,
  0,
  {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    loop_attr_get,
    NULL,
    NULL,
    NULL,
    loop_display,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  },
  NULL
};

/* Cycler object */
struct cycler {
  struct ajj_value data[ AJJ_FUNC_ARG_MAX_SIZE ];
  size_t len;
  int cur; /* current cursor position. Use int is
            * because we could assign value -1 to
            * it initially, then we will not lose
            * the very first element at very first
            * shot */
};

#define CYCLER_TYPE (AJJ_VALUE_BUILTIN_TYPE+5)
#define CYCLER(V) ((struct cycler*)(OBJECT(V)))

static
int cycler_ctor( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    void** ret,
    int* tp ) {
  struct cycler* c;
  size_t i;
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  assert(arg_num <= AJJ_FUNC_ARG_MAX_SIZE);
  c = malloc(sizeof(*c));
  c->len = arg_num;
  for( i = 0 ; i < arg_num ; ++i ) {
    /* we don't care about the scope rules
     * because after the ctor gets called,
     * the our move function will be called
     * to ensure we have correct gc scope */
    c->data[i] = arg[i];
  }

  c->cur = -1; /* ensure we don't miss the first
                * shot if user try to put a next
                * function in the loop */
  *ret = c;
  *tp = CYCLER_TYPE;
  return AJJ_EXEC_OK;
}

static
void cycler_dtor( struct ajj* a,
    void* udata,
    void* obj ) {
  UNUSE_ARG(a);
  UNUSE_ARG(udata);
  free(obj);
}

/* member functions */
static
int cycler_reset( struct ajj* a,
    struct ajj_value* obj,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  struct cycler* c;
  UNUSE_ARG(a);
  assert(IS_A(obj,CYCLER_TYPE));
  if(arg_num != 0) {
    EXEC_FAIL1(a,"%s","cycler::reset cannot accept any arguments!");
  }

  c = CYCLER(obj);
  c->cur = 0;
  return AJJ_EXEC_OK;
}

static
int cycler_next( struct ajj* a,
    struct ajj_value* obj,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  struct cycler* c;

  UNUSE_ARG(a);
  assert(IS_A(obj,CYCLER_TYPE));

  if(arg_num != 0) {
    EXEC_FAIL1(a,"%s","cycler::next cannot accept any arguments!");
  }

  c = CYCLER(obj);
  ++(c->cur);
  if(c->cur == (int)(c->len))
    c->cur = 0;
  /* return current item */
  *ret = c->data[c->cur];
  return AJJ_EXEC_OK;
}

/* slot */

static
struct ajj_value
cycler_attr_get( struct ajj* a,
    const struct ajj_value* obj ,
    const struct ajj_value* key ) {
  struct string str;
  int own;

  UNUSE_ARG(a);
  assert( IS_A(obj,CYCLER_TYPE) );

  if(vm_to_string(key,&str,&own))
    return AJJ_NONE;
  else {
    if(string_eqc(&str,"current")) {
      struct cycler* c = CYCLER(obj);
      int idx = c->cur <0 ? 0 : c->cur;
      return c->data[idx];
    }
    return AJJ_NONE;
  }
}

static
void cycler_move( struct ajj* a, struct ajj_value* obj ) {
  size_t i;
  struct cycler* c;
  UNUSE_ARG(a);
  assert( IS_A(obj,CYCLER_TYPE) );
  c = CYCLER(obj);
  for( i = 0 ; i < c->len ; ++i ) {
    ajj_value_move_scope(a,obj->value.object->scp,
        c->data+i);
  }
}

static
const char* cycler_display( struct ajj* a,
    const struct ajj_value* obj ,
    size_t* len ) {
  char buf[1024];
  struct cycler* c;

  UNUSE_ARG(a);
  assert( IS_A(obj,CYCLER_TYPE) );
  c = CYCLER(obj);

  *len = (size_t)sprintf(buf,
      "cycler(current:%d"
      ";length:" SIZEF
      ")", c->cur,c->len);

  return strdup(buf);
}

static
struct ajj_class_method AJJ_METHOD[] = {
  { cycler_reset , "reset" },
  { cycler_next , "next" }
};

static
struct ajj_class CYCLER_CLASS = {
  "cycler",
  cycler_ctor,
  cycler_dtor,
  AJJ_METHOD,
  ARRAY_SIZE(AJJ_METHOD),
  {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    cycler_attr_get,
    NULL,
    NULL,
    cycler_move,
    cycler_display,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  },
  NULL
};


/* Builtin Test -------------------------------- */
static
int test_true( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  const char* fn = (const char*)udata;
  if(arg_num != 1) {
    EXEC_FAIL1(a,"Test function:%s cannot accept extra arguments!",fn);
  } else {
    if(arg->type == AJJ_VALUE_BOOLEAN &&
       arg->value.boolean ) {
      *ret = AJJ_TRUE;
    } else {
      *ret = AJJ_FALSE;
    }
    return AJJ_EXEC_OK;
  }
}

static
int test_false( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  const char* fn = (const char*)udata;
  if(arg_num != 1) {
    EXEC_FAIL1(a,"Test function:%s cannot accept extra arguments!",fn);
  } else {
    if(arg->type == AJJ_VALUE_BOOLEAN &&
       !arg->value.boolean) {
      *ret = AJJ_TRUE;
    } else {
      *ret = AJJ_FALSE;
    }
    return AJJ_EXEC_OK;
  }
}

static
int test_none( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  const char* fn = (const char*)udata;
  if(arg_num != 1) {
    EXEC_FAIL1(a,"Test function:%s cannot accept extra arguments!",fn);
  } else {
    if(arg->type == AJJ_VALUE_NONE) {
      *ret = AJJ_TRUE;
    } else {
      *ret = AJJ_FALSE;
    }
    return AJJ_EXEC_OK;
  }
}

static
int test_divisableby( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if(arg_num != 2) {
    EXEC_FAIL1(a,"%s","Test function:divisableby can only accept 2 argument!");
  } else {
    if(arg[1].type != AJJ_VALUE_NUMBER ||
       arg[0].type != AJJ_VALUE_NUMBER) {
      EXEC_FAIL2(a,"Test function:divisableby's all arguments "
          "must be a number, but only get:(%s,%s)!",
          ajj_value_get_type_name(arg),
          ajj_value_get_type_name(arg+1));
    } else {
      int arg1,arg2;
      CHECK(!vm_to_integer(arg+1,&arg2));
      CHECK(!vm_to_integer(arg,&arg1));
      if( arg1 % arg2 == 0 )
        *ret = AJJ_TRUE;
      else
        *ret = AJJ_FALSE;
    }
    return AJJ_EXEC_OK;
  }
}

static
int test_even( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if(arg_num != 1) {
    EXEC_FAIL1(a,"%s","Test function:even cannot accept extra arguments!");
  } else {
    if(arg[0].type != AJJ_VALUE_NUMBER ) {
      EXEC_FAIL1(a,"Test function:even's argument must be number,"
          "but get:%s!",
          ajj_value_get_type_name(arg));
    } else {
      int arg1;
      CHECK(!vm_to_integer(arg,&arg1));
      if( arg1 % 2 == 0 ) {
        *ret = AJJ_TRUE;
      } else {
        *ret = AJJ_FALSE;
      }
    }
    return AJJ_EXEC_OK;
  }
}

static
int test_iterator( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if(arg_num != 1) {
    EXEC_FAIL1(a,"%s","Test function:iterator cannot "
        "accept extra arguments!");
  } else {
    switch(arg[0].type) {
      case AJJ_VALUE_NONE:
      case AJJ_VALUE_NUMBER:
      case AJJ_VALUE_BOOLEAN:
      case AJJ_VALUE_STRING:
        *ret = AJJ_FALSE;
        break;
      case AJJ_VALUE_OBJECT:
        {
          const struct object* o =
            ajj_value_to_obj(arg);
          if(o->fn_tb->slot.iter_start) {
            *ret = AJJ_TRUE;
          } else {
            *ret = AJJ_FALSE;
          }
          break;
        }
      default:
        break;
    }
    return AJJ_EXEC_OK;
  }
}

static
int test_mapping( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  const char* fn = (const char*)udata;
  if(arg_num != 1) {
    EXEC_FAIL1(a,"Test function:%s cannot accept "
        "extra arguments!",
        fn);
  } else {
    if( IS_A(arg,DICT_TYPE) ) {
      *ret = AJJ_TRUE;
    } else {
      *ret = AJJ_FALSE;
    }
  }
  return AJJ_EXEC_OK;
}

static
int test_number( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  if(arg_num != 1) {
    EXEC_FAIL1(a,"%s","Test function:number cannot accept "
        "extra arguments!");
  } else {
    if( arg->type == AJJ_VALUE_NUMBER ) {
      *ret = AJJ_TRUE;
    } else {
      *ret = AJJ_FALSE;
    }
  }
  return AJJ_EXEC_OK;
}

static
int test_odd( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  if(arg_num != 1) {
    EXEC_FAIL1(a,"%s","Test function:odd cannot accept "
        "extra arguments!");
  } else {
    if(arg->type != AJJ_VALUE_NUMBER){
      EXEC_FAIL1(a,"Test function:odd's argument must be number,"
          "but get:%s!",
          ajj_value_get_type_name(arg));
    } else {
      int val;
      CHECK(!vm_to_integer(arg,&val));
      if( val % 2 == 1 ) {
        *ret = AJJ_TRUE;
      } else {
        *ret = AJJ_FALSE;
      }
    }
  }
  return AJJ_EXEC_OK;
}

static
int test_object( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  if(arg_num !=1) {
    EXEC_FAIL1(a,"%s","Test function:object cannot accept "
        "extra arguments!");
  } else {
    if(arg->type == AJJ_VALUE_OBJECT)
      *ret = AJJ_TRUE;
    else
      *ret = AJJ_FALSE;
    return AJJ_EXEC_OK;
  }
}

static
int test_sameas( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  if(arg_num != 2) {
    EXEC_FAIL1(a,"%s","Test function:sameas must accept one extra "
        "arguments!");
  } else {
    switch(arg->type) {
      case AJJ_VALUE_NONE:
        if(arg[1].type == AJJ_VALUE_NONE) {
          *ret = AJJ_TRUE;
        } else {
          *ret = AJJ_FALSE;
        }
        break;
      case AJJ_VALUE_BOOLEAN:
        if(arg[1].type == AJJ_VALUE_BOOLEAN &&
           arg[1].value.boolean == arg->value.boolean)
          *ret = AJJ_TRUE;
        else
          *ret = AJJ_FALSE;
        break;
      case AJJ_VALUE_NUMBER:
        if(arg[1].type == AJJ_VALUE_NUMBER &&
           arg[1].value.number == arg->value.number)
          *ret = AJJ_TRUE;
        else
          *ret = AJJ_FALSE;
        break;
      case AJJ_VALUE_STRING:
      case AJJ_VALUE_OBJECT:
        if(arg[1].type == AJJ_VALUE_STRING &&
           arg[1].value.object == arg->value.object)
          *ret = AJJ_TRUE;
        else
          *ret = AJJ_FALSE;
        break;
      default:
        UNREACHABLE();
        break;
    }
    return AJJ_EXEC_OK;
  }
}

static
int test_string( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  if( arg_num != 1 )
    EXEC_FAIL1(a,"%s","Test function:string cannot accept "
        "extra arguments!");
  else {
    if(arg->type == AJJ_VALUE_STRING)
      *ret = AJJ_TRUE;
    else
      *ret = AJJ_FALSE;
    return AJJ_EXEC_OK;
  }
}

static
int test_defined( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  if(arg_num != 1) {
    EXEC_FAIL1(a,"%s","Test function:defined cannot accept "
        "extra arguments!");
  } else {
    if(arg->type != AJJ_VALUE_NONE)
      *ret = AJJ_TRUE;
    else
      *ret = AJJ_FALSE;
    return AJJ_EXEC_OK;
  }
}

/* =====================================
 * Builtin Filter and Function
 * ===================================*/

/* JSON ---------------------------------------------------------
 * This provides a builtin json parser that is used for two
 * purpose:
 * 1. Help setup environment which is used for rendering some
 * extra documents.
 * 2. Provide builtin function for user to parse a json document
 * but it won't be very useful.
 * --------------------------------------------------------------*/

struct json_lexer {
  size_t pos;
  const char* src;
};

#define JSON_ERROR_CODE_SNIPPET_SIZE 32

static
void json_report_error(struct ajj* a, struct json_lexer* jl,
    const char* fmt , ... ) {
  /* get the code snippet */
  size_t start = jl->pos;
  size_t end = strlen(jl->pos+jl->src);
  char cbuf[ JSON_ERROR_CODE_SNIPPET_SIZE + 1 ];
  char* buf = a->err;
  char* bend= a->err + ERROR_BUFFER_SIZE;
  va_list vl;
  int lnum;
  int ccnt;
  size_t i;

  va_start(vl,fmt);

  start = MIN(JSON_ERROR_CODE_SNIPPET_SIZE/2,
      start);

  end = MIN(JSON_ERROR_CODE_SNIPPET_SIZE/2,
      end);

  strncpy(cbuf,jl->src+jl->pos-start,end+start);
  cbuf[end+start] = 0;

  /* grab the linenum and ccount */
  lnum = 1;
  ccnt = 1;
  i =0;

  while(i<jl->pos) {
    Rune c;
    int off;

    off = chartorune(&c,jl->src+i);
    assert(c != Runeerror);
    if(c == '\n') {
      lnum++; ccnt = 1;
    } else {
      ++ccnt;
    }
    i += off;
  }


  buf += snprintf(buf,ERROR_BUFFER_SIZE,
      "Json parsing error(%d|%d):(... %s ...):",
      lnum,
      ccnt,
      cbuf);

  vsnprintf(buf, bend-buf, fmt,vl);
}

/* Compare the left hand side (UTF encode) with right hand side
 * (Pure ansi string) in a safe way */
static
int json_ustr_equal( const char* left , const char* right ) {
  int off;
  Rune c;
#define CHECK_CHAR(OFF) \
  do { \
    off = chartorune(&c,left); \
    if(c == Runeerror) goto fail; \
    if( c != right[OFF] )goto fail; \
    if(!right[OFF+1]) return 0; \
    left += off; \
  } while(0)

  CHECK_CHAR(0);
  CHECK_CHAR(1);
  CHECK_CHAR(2);
  CHECK_CHAR(3);
  CHECK_CHAR(4);
  CHECK_CHAR(5);

#undef CHECK_CHAR

  assert(0);
  return -1;

fail:
  return 1;
}

static int
json_parse_array( struct ajj* a, struct gc_scope* scp,
    struct json_lexer* jl, int rec , struct ajj_value* out );

static int
json_parse_object( struct ajj* a, struct gc_scope* scp,
    struct json_lexer* jl, int rec , struct ajj_value* out );

static int
json_parse_string( struct ajj* a, struct gc_scope* scp,
    struct json_lexer* jl , struct ajj_value* out );

static int
json_parse_number( struct ajj* a, struct json_lexer* jl,
    struct ajj_value* out );

/* subroutine for parsing the json in a recursive way.
 *
 * The recursive parser is easy to understand but has
 * some flaws while dealing unknown input. Since if the
 * attacker crafts an input with too much nested json
 * format then we may run out of stack. Here to impose
 * some limits, we put a constant value called
 * JSON_MAX_NESTED_SIZE which indicates how many nested
 * layer of the input json should have. This limitation
 * some how help us dealing with purposely crafted json
 * input */

#define JSON_MAX_NESTED_SIZE 128
enum {
  JSON_ERROR = -1,
  JSON_OBJECT,
  JSON_ARRAY,
  JSON_TRUE,
  JSON_FALSE,
  JSON_NULL,
  JSON_STRING,
  JSON_NUMBER
};

#define JSON_TRY_PARSE(X,TAG) \
  do { \
    if(X) { \
      goto fail; \
    } else { \
      ret = TAG; \
      goto done; \
    } \
  } while(0)

#define JSON_CHECK_NEST() \
  do { \
    if(rec+1 == JSON_MAX_NESTED_SIZE) {\
      json_report_error(a,jl,"Too much nested scope in " \
          "json file,more than:%d!",JSON_MAX_NESTED_SIZE); \
      goto fail; \
    } \
  } while(0)

/* Helper function for skipping those white spaces and get the
 * deliminator. Only support find 2 because json only needs 2 */
static int json_next_char( struct ajj* a, struct json_lexer* jl , Rune* out ) {
  const char* src = jl->src;
  size_t i = jl->pos;
  Rune c;

  while(1) {
    chartorune(&c,src+i);
    switch(c) {
      case ' ':case '\t':case '\n':
      case '\r':case '\f':case '\v':
        ++i;
        continue;
      case 0: /* EOF */
        json_report_error(a,jl,"Unexpected EOF!");
        goto fail;
      case Runeerror:
        json_report_error(a,jl,"UTF decode error!");
        goto fail;
      default:
        *out = c;
        jl->pos = i;
        return 0;
    }
  }

fail:
  jl->pos = i;
  return -1;
}

static
int json_parse_value( struct ajj* a ,
    struct gc_scope* scp,
    struct json_lexer* jl,
    int rec ,
    struct ajj_value* out) {

  size_t i = jl->pos;
  const char* src = jl->src;
  int ret;
  int off;
  Rune c;

  assert( rec < JSON_MAX_NESTED_SIZE );

  while(1) {
    off = chartorune(&c,src+i);

    if(c == Runeerror) {
      json_report_error(a,jl,"UTF decode error!");
      goto fail;
    } else if(!c) {
      json_report_error(a,jl,"Unexpected EOF!");
      goto fail;
    }

    switch(c) {
      case '[':
        JSON_CHECK_NEST();
        i += off; jl->pos = i;
        JSON_TRY_PARSE(json_parse_array(a,scp,jl,rec+1,out),JSON_ARRAY);
      case '{':
        JSON_CHECK_NEST();
        i += off; jl->pos = i;
        JSON_TRY_PARSE(json_parse_object(a,scp,jl,rec+1,out),JSON_OBJECT);
      case '\"':
        /* parsing the string literal */
        JSON_CHECK_NEST();
        i += off; jl->pos = i;
        JSON_TRY_PARSE(json_parse_string(a,scp,jl,out),JSON_STRING);
      case 't':
        ret = json_ustr_equal(src+i+1,"rue");
        if(ret) {
          json_report_error(a,jl,"Unrecognize token here,maybe \"true\"?");
          goto fail;
        } else {
          *out = AJJ_TRUE;
          ret = JSON_TRUE;
          jl->pos = i + 4;
          goto done;
        }
      case 'f':
        ret = json_ustr_equal(src+i+1,"alse");
        if(ret) {
          json_report_error(a,jl,"Unrecognize token here,maybe \"false\"?");
          goto fail;
        } else {
          *out = AJJ_FALSE;
          ret = JSON_FALSE;
          jl->pos = i + 5;
          goto done;
        }
      case 'n':
        ret = json_ustr_equal(src+i+1,"ull");
        if(ret) {
          json_report_error(a,jl,"Unrecognize token here,maybe \"null\"?");
          goto fail;
        } else {
          *out = AJJ_NONE;
          ret = JSON_NULL;
          jl->pos = i + 4;
          goto done;
        }
      case '-': case '+':
      case '0':case '1':case '2':case '3':case '4':
      case '5':case '6':case '7':case '8':case '9':
        /* parse the number */
        jl->pos = i;
        if(json_parse_number(a,jl,out))
          goto fail;
        else {
          ret = JSON_NUMBER;
          goto done;
        }
      case ' ':case '\t':case '\n':
      case '\r':case '\f':case '\v':
        i += off;
        continue;
      default:
        json_report_error(a,jl,"Unrecognize token!");
        goto fail;
    }
  }

done:

  return ret;

fail:
  return JSON_ERROR;
}

#undef JSON_TRY_PARSE
#undef JSON_CHECK_NEST

/* Try parse an object */
static int
json_parse_object( struct ajj* a, struct gc_scope* scp,
    struct json_lexer* jl,int rec, struct ajj_value* out ) {
  struct ajj_object* obj = ajj_object_create_dict(a,scp);
  struct ajj_value key;
  struct ajj_value val;
  Rune c;

  assert( rec < JSON_MAX_NESTED_SIZE );

  /* Check if it is an empty object */
  if( json_next_char(a,jl,&c) )
    goto fail;
  else {
    if( c == '}' ) {
      ++jl->pos;
      goto done;
    }
  }

  while(1) {
    /* Get the key out */
    int ret;

    ret = json_parse_value(a,scp,jl,rec,&key);

    if(ret != JSON_STRING) {
      if(ret != JSON_ERROR)
        json_report_error(a,jl,"Expect a string as key!");
      goto fail;
    }

    /* Expect a colon here */
    ret = json_next_char(a,jl,&c);
    if(ret) goto fail;
    if(c == ':') ++jl->pos; /* Bump one character */
    else {
      json_report_error(a,jl,"Expect \":\" here!");
      goto fail;
    }

    /* Parse the value */
    ret = json_parse_value(a,scp,jl,rec,&val);
    if(ret == JSON_ERROR)
      goto fail;
    else {
      builtin_dict_insert(a,obj,&key,&val);
    }

    /* Check if we need to go away or continue */
    ret = json_next_char(a,jl,&c);
    if(ret) goto fail;

    if(c == '}') {
      ++jl->pos;
      break;
    } else {
      if(c != ',') {
        json_report_error(a,jl,"Object expect \",\" or \"}\" here!");
        goto fail;
      }
      ++jl->pos; /* When we have a , */
    }
  }

done:

  out->type = AJJ_VALUE_OBJECT;
  out->value.object = obj;
  return 0;

fail:
  return -1;
}

static int
json_parse_array( struct ajj* a, struct gc_scope* scp,
    struct json_lexer* jl, int rec , struct ajj_value* out) {
  struct ajj_object* ls = ajj_object_create_list(a,scp);
  struct ajj_value val;
  Rune c;

  assert( rec < JSON_MAX_NESTED_SIZE );

  if(json_next_char(a,jl,&c))
    goto fail;
  else {
    if(c == ']') {
      ++jl->pos;
      goto done;
    }
  }

  while(1) {
    int ret = json_parse_value(a,scp,jl,rec,&val);
    if(ret == JSON_ERROR)
      goto fail;

    builtin_list_push(a,ls,&val);

    ret = json_next_char(a,jl,&c);
    if(ret) goto fail;

    if(c == ']') {
      ++jl->pos;
      break;
    } else {
      if(c != ',') {
        json_report_error(a,jl,"Array expect \",\" or \"]\" here!");
        goto fail;
      }
      ++jl->pos; /* When we have a , */
    }
  }

done:

  out->type = AJJ_VALUE_OBJECT;
  out->value.object = ls;
  return 0;

fail:
  return -1;
}

static int
json_parse_string( struct ajj* a, struct gc_scope* scp,
    struct json_lexer* jl, struct ajj_value* out) {
  struct strbuf sbuf;
  struct string str;
  size_t i = jl->pos;
  const char* src = jl->src;
  strbuf_init_cap(&sbuf,128);

  while(1) {
    Rune c;
    int off;

    off = chartorune(&c,src+i);
    if(c == Runeerror) {
      json_report_error(a,jl,"UTF decode error!");
      goto fail;
    }

    switch(c) {
      case '\\':
        i += off;
        off = chartorune(&c,src+i);
        switch(c) {
          case Runeerror:
            json_report_error(a,jl,"UTF decode error!");
            goto fail;
          case '\n':
            json_report_error(a,jl,"Escape character in string followed"
                " by line break!");
            goto fail;
          case 'U': case 'u':
            /* TODO:: Add support for unicode
             * Currently just push the origin string into buffer */
            strbuf_push_rune(&sbuf,'\\');
            break;
          case 'n': c = '\n'; break;
          case 't': c = '\t'; break;
          case 'r': c = '\r'; break;
          case '"': c = '"'; break;
          case '\\': c = '\\'; break;
          case 'f': c = '\f'; break;
          case 'v': c = '\v'; break;
          case 'b': c = '\b'; break;
          case '/': c = '/'; break;
          default:
            if(c < 128)
              json_report_error(a,jl,"Unrecognized escape character:%c\n",(char)c);
            else
              json_report_error(a,jl,"Unrecognized escape character as Rune:%d\n",c);
            goto fail;
        }
        break;
      case '\"':
        i += off;
        goto done;
      case 0:
        json_report_error(a,jl,"Unexpected EOF!");
        goto fail;
      case '\n':
        json_report_error(a,jl,"Linebreak is not allowed in string literal!");
        goto fail;
      case '\t':
        json_report_error(a,jl,"Tab is not allowed in string literal!");
        goto fail;
      default:
        break;
    }
    strbuf_push_rune(&sbuf,c);
    i += off;
  }

done:
  jl->pos = i;
  /* Although this may COPY the string but it will prevent
   * waste too much memory which is safe */
  strbuf_move(&sbuf,&str);

  out->value.object = ajj_object_create_string(a,scp,
      str.str,str.len,1);
  out->type = AJJ_VALUE_STRING;

  strbuf_destroy(&sbuf);
  return 0;

fail:
  jl->pos = i;
  strbuf_destroy(&sbuf);
  return -1;
}

static int
json_parse_number( struct ajj* a, struct json_lexer* jl,
    struct ajj_value* out ) {
  char* endp;
  double val;

  errno = 0;
  val = strtod(jl->src+jl->pos,&endp);
  if(val == 0.0) {
    if(endp == jl->pos+jl->src) {
      /* not a valid number */
      json_report_error(a,jl,"Cannot parse number!");
      goto fail;
    } else {
      if(errno) {
        json_report_error(a,jl,"Cannot parse number,because %s!",
            strerror(errno));
        goto fail;
      }
    }
  }
  jl->pos = (endp-jl->src);
  *out = ajj_value_number(val);
  return 0;

fail:
  return -1;
}

#ifndef DISABLE_JSON_FILE_TAIL_CHECK
/* After parsing done, we need to check whether
 * the file still contains significant characters
 * which is not empty */
static int json_check_tail( struct ajj* a , struct json_lexer* jl ) {
  Rune c;
  int off;
  const char* src = jl->src;
  size_t i = jl->pos;

  while(1) {
    off = chartorune(&c,src+i);
    switch(c) {
      case ' ':case '\r':case '\n':
      case '\v':case '\f':case '\t':
        i += off;
        continue;
      case 0: goto done;
      default:
        return -1;
    }
  }
done:
  return 0;
}
#endif /* DISABLE_JSON_FILE_TAIL_CHECK */

/* This is the function and entry for parsing a json file in a safe
 * manner. This function will clear all memory allocated if the parsing
 * failed.*/
struct ajj_object*
json_parsec( struct ajj* a , struct gc_scope* scp,
    const char* str ) {
  struct json_lexer jl;
  int ret;
  struct ajj_value root;
  struct gc_scope temp_scp; /* Temporary gc scope , if we encounter errro,
                               just relcaim this gc scope; otherwise merge
                               it back to the correct gc scope */
  gc_init_temp(&temp_scp,scp);

  jl.src = str; jl.pos = 0;

  /* Start parsing the json document */
  ret = json_parse_value(a,&temp_scp,&jl,0,&root);
  if(ret == JSON_ERROR) {
    goto fail;
  } else {
    if(ret == JSON_ARRAY ||
       ret == JSON_OBJECT ) {
#ifndef DISABLE_JSON_FILE_TAIL_CHECK
      if(json_check_tail(a,&jl)) {
        json_report_error(a,&jl,"Unexpected token here " \
            "after finishing parsing json object!");
        goto fail;
      }
#endif /* DISABLE_JSON_FILE_TAIL_CHECK */
      gc_scope_merge(scp,&temp_scp);
      return root.value.object;
    } else {
      json_report_error(a,&jl,"Parsing json error:Root "
          "must be array or object!");
      goto fail;
    }
  }
fail:
  gc_scope_exit(a,&temp_scp);
  return NULL;
}

struct ajj_object*
json_parse( struct ajj* a, struct gc_scope* scp,
    const char* filename , const char* func ) {
  size_t len;
  /* Use vfs api to load the file *DO NOT* use underlying C API */
  const char* src = a->vfs.vfs_load(a,filename,&len,NULL,a->vfs_udata);
  if(src == NULL) {
    return NULL;
  } else {
    struct ajj_object* ret = json_parsec(a,scp,src);
    free((void*)src);
    return ret;
  }
}

static
int to_json( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if( arg_len != 1 ) {
    EXEC_FAIL1(a,"%s","Function::to_json can only accept one argument,"
        "and it must be a string!");
    return AJJ_EXEC_FAIL;
  } else {
    if(arg->type != AJJ_VALUE_STRING) {
      EXEC_FAIL1(a,"Function::to_json can only accept one string "
          "argument,but get type:%s!",ajj_value_get_type_name(arg));
    } else {
      struct ajj_object* r;
      if( (r = json_parse(a,ajj_cur_gc_scope(a),
              ajj_value_to_cstr(arg), "to_json")) == NULL ) {
        *ret = AJJ_NONE;
        return AJJ_EXEC_FAIL;
      } else {
        *ret = ajj_value_assign(r);
        return AJJ_EXEC_OK;
      }
    }
  }
}

static
int to_jsonc( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if( arg_len != 1 ) {
    EXEC_FAIL1(a,"%s","Function::to_jsonc can only accept one argument,"
        "and it must be a string!");
    return AJJ_EXEC_FAIL;
  } else {
    if(arg->type != AJJ_VALUE_STRING) {
      EXEC_FAIL1(a,"Function::to_jsonc can only accept one string "
          "argument,but get type:%s!",ajj_value_get_type_name(arg));
    } else {
      struct ajj_object* r;
      if( (r = json_parsec(a,ajj_cur_gc_scope(a),
              ajj_value_to_cstr(arg))) == NULL ) {
        *ret = AJJ_NONE;
        return AJJ_EXEC_FAIL;
      } else {
        *ret = ajj_value_assign(r);
        return AJJ_EXEC_OK;
      }
    }
  }
}

/* Run a shell command and capture its output */
static
int shell( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if( arg_len != 1 ) {
    EXEC_FAIL1(a,"%s","Function::shell can only accept one argument,"
        "and it must be a string!");
    return AJJ_EXEC_FAIL;
  } else {
    if(arg->type != AJJ_VALUE_STRING) {
      EXEC_FAIL1(a,"Function::shell can only accept one string "
          "argument,but get type:%s!",ajj_value_get_type_name(arg));
    } else {
      FILE* p;
      p = popen(ajj_value_to_cstr(arg),"r");
      if(!p) {
        *ret = AJJ_NONE;
        return AJJ_EXEC_FAIL;
      } else {
        struct strbuf buf;
        strbuf_init(&buf);
        strbuf_append_file(&buf,p);
        pclose(p);
        *ret = ajj_value_assign(
            ajj_object_create_string(a,
            ajj_cur_gc_scope(a),
            buf.str,buf.len,1));
        return AJJ_EXEC_OK;
      }
    }
  }
}


static
int lstrip( struct ajj* a ,
    void* udata ,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if( arg_len != 1 ) {
    EXEC_FAIL1(a,"%s","Function::lstrip can only accept one argument!");
  } else {
    if(arg->type != AJJ_VALUE_STRING) {
      EXEC_FAIL1(a,"%s","Funcion::lstrip can only accept one string argument!");
    } else {
      struct strbuf sbuf;
      const struct string* tar = ajj_value_to_string(arg);
      size_t i;

      strbuf_init_cap(&sbuf,tar->len);
      for( i = 0 ; i < tar->len ; ) {
        Rune r;
        int len;

        len = chartorune(&r,tar->str + i);
        if(r == Runeerror) {
          EXEC_FAIL1(a,"%s","Function::lstrip argument is not properly UTF "
              "encoded!");
        }
        switch(r) {
          case ' ':
          case '\t':
          case '\r':
          case '\v':
          case '\n':
            break;
          default:
            strbuf_push_rune(&sbuf,r);
            break;
        }
        i += len;
      }
      *ret = ajj_value_assign(
          ajj_object_create_string(a,
            ajj_cur_gc_scope(a),
            sbuf.str,sbuf.len,1));
      return AJJ_EXEC_OK;
    }
  }
}

static
int rstrip( struct ajj* a ,
    void* udata ,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if( arg_len != 1 ) {
    EXEC_FAIL1(a,"%s","Function::rstrip can only accept one argument!");
  } else {
    if(arg->type != AJJ_VALUE_STRING) {
      EXEC_FAIL1(a,"%s","Funcion::rstrip can only accept one string argument!");
    } else {
      const struct string* tar = ajj_value_to_string(arg);
      size_t i;
      int start = -1;

      /* This loop tries to find out the start of the trailing empty spaces */
      for( i = 0 ; i < tar->len ; ) {
        Rune r;
        int len;

        len = chartorune(&r,tar->str + i);
        if(r == Runeerror) {
          EXEC_FAIL1(a,"%s","Function::rstrip argument is not properly UTF "
              "encoded!");
        }
        switch(r) {
          case ' ':
          case '\t':
          case '\r':
          case '\v':
          case '\n':
            if(start <0) start = i;
            break;
          default:
            start = -1;
            break;
        }
        i += len;
      }

      /* Now check if we really have trailing empty spaces */
      if(start >=0) {
        /* Here we do a ugly hack for avoiding one string copy */
        int c = tar->str[start];
        ((char*)(tar->str))[start] = 0;
        *ret = ajj_value_assign(
            ajj_object_create_string(a,
              ajj_cur_gc_scope(a),
              tar->str,(size_t)(start),0));
        ((char*)(tar->str))[start] = c;
      } else {
        *ret = ajj_value_assign(
            ajj_object_create_string(a,
              ajj_cur_gc_scope(a),
              tar->str,tar->len,0));
      }
      return AJJ_EXEC_OK;
    }
  }
}

static
int number_floor( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if( arg_len != 1 && arg->type != AJJ_VALUE_NUMBER ) {
    EXEC_FAIL1(a,"%s","Function::floor must accept one number!");
  } else {
    double val = floor( ajj_value_to_number(arg) );
    *ret = ajj_value_number(val);
    return AJJ_EXEC_OK;
  }
}

static
int number_ceil( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if( arg_len != 1 && arg->type != AJJ_VALUE_NUMBER ) {
    EXEC_FAIL1(a,"%s","Function::ceil must accept one number!");
  } else {
    double val = ceil( ajj_value_to_number(arg) );
    *ret = ajj_value_number(val);
    return AJJ_EXEC_OK;
  }
}

/* Useful function for testing the AJJ itself */
static
int assert_expr( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if(arg_len <=0 || arg_len > 2) {
    EXEC_FAIL1(a,"%s","Function::assert_expr can only accept 1 or 2 arguments!");
  } else {
    if( arg_len == 2 && arg[1].type != AJJ_VALUE_STRING ) {
      EXEC_FAIL1(a,"%s","Function::assert_expr's second argument must be a "
          "string!");
    }
    if( vm_is_true(arg) ) {
      *ret = AJJ_TRUE;
      return AJJ_EXEC_OK;
    } else {
      if( arg_len == 1 ) {
        ajj_error(a,"Function::assert_expr failed!");
      } else {
        ajj_error(a,"Function::assert_expr failed with message:%s!",
            ajj_value_to_cstr(arg+1));
      }
      return AJJ_EXEC_FAIL;
    }
  }
}

/* get the type string of a value */
static
int type_of( struct ajj* a ,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if(arg_len != 1) {
    EXEC_FAIL1(a,"%s","Function::typeof can only accept 1 argument!");
  } else {
    const char* str = ajj_value_get_type_name(arg);
    struct string s;
    s.str = str;
    s.len = strlen(str);

    *ret = ajj_value_assign(
        ajj_object_create_const_string(
          a,
          ajj_cur_gc_scope(a),
          &s));

    return AJJ_EXEC_OK;
  }
}

/* ===================================
 * Filters
 * =================================*/

static
int filter_abs( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if(arg_len != 1) {
    EXEC_FAIL1(a,"%s","Function::abs can only accept one argument,"
        "and it must be a number!");
  } else {
    double number;
    if( vm_to_number(arg,&number) )
      EXEC_FAIL1(a,"Function::abs can only get one number, but get:%s!",
          ajj_value_get_type_name(arg));
    *ret = ajj_value_number( fabs(number) );
    return AJJ_EXEC_OK;
  }
}

static
int filter_attr( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if(arg_len != 2) {
    EXEC_FAIL1(a,"%s","Function::attr can accept 2 arguments!");
  } else {
    /* redispatch to ajj_value_attr_get */
    return ajj_value_attr_get(a,arg,arg+1,ret);
  }
}

static
int filter_slice( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if(arg_len != 3) {
    EXEC_FAIL1(a,"%s","Function::slice can accept 3 arguments!");
  } else {
    int i;
    int start , end;
    int start_idx , end_idx;
    int ccnt;
    struct string* str;

    /* Handle the arguments */
    if(arg[0].type != AJJ_VALUE_STRING) {
      EXEC_FAIL1(a,"%s","Function::slice first argument must be string!");
    }
    if(arg[1].type != AJJ_VALUE_NUMBER) {
      EXEC_FAIL1(a,"%s","Function::slice second argument must be number!");
    }
    if(arg[2].type != AJJ_VALUE_NUMBER) {
      EXEC_FAIL1(a,"%s","Function::slice third argument must be number!");
    }
    if(vm_to_integer(arg+1,&start) || vm_to_integer(arg+2,&end)) {
      EXEC_FAIL1(a,"%s","Function::slice 2nd and 3rd arguments must be "
          "convertable to integer!");
    }
    if(start > end) {
      EXEC_FAIL1(a,"%s","Function::slice 2nd must "
          "be less than 3rd argument!");
    }

    str = ajj_value_to_string(arg);

    if( (start < 0 || start >=  str->len) ||
        (end < 0 || end >= str->len) ) {
      EXEC_FAIL1(a,"Function::slice start and end index must be larger than "
          "0 and less than size of the string , which is %d",(int)str->len);
    }

    /* Decode the whole string */
    ccnt= 0 ; start_idx = end_idx = -1;
    for( i = 0 ; i < str->len ; ) {
      Rune r;
      int ret = chartorune(&r,str->str + i);
      if(ret == Runeerror) {
        EXEC_FAIL1(a,"%s","Function::slice cannot decode UTF string!");
      }
      if(ccnt == start) {
        start_idx = i;
      }
      if(ccnt == end) {
        end_idx = i;
        break;
      }
      ++ccnt;
      i += ret;
    }
    assert(start_idx >=0);
    assert(end_idx >=0);

    /* Now slice one string out to the return */
    *ret = ajj_value_assign( ajj_object_create_string(a,ajj_cur_gc_scope(a),
          str->str+start_idx,end_idx-start_idx,0));
    return AJJ_EXEC_OK;
  }
}

static
int filter_bslice( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if(arg_len != 3) {
    EXEC_FAIL1(a,"%s","Function::bslice can accept 3 arguments!");
  } else {
    int start , end;
    struct string* str;

    /* Handle the arguments */
    if(arg[0].type != AJJ_VALUE_STRING) {
      EXEC_FAIL1(a,"%s","Function::bslice first argument must be string!");
    }
    if(arg[1].type != AJJ_VALUE_NUMBER) {
      EXEC_FAIL1(a,"%s","Function::bslice second argument must be number!");
    }
    if(arg[2].type != AJJ_VALUE_NUMBER) {
      EXEC_FAIL1(a,"%s","Function::bslice third argument must be number!");
    }
    if(vm_to_integer(arg+1,&start) || vm_to_integer(arg+2,&end)) {
      EXEC_FAIL1(a,"%s","Function::bslice 2nd and 3rd arguments must be "
          "convertable to integer!");
    }
    if(start > end) {
      EXEC_FAIL1(a,"%s","Function::bslice 2nd must "
          "be less than 3rd argument!");
    }

    str = ajj_value_to_string(arg);

    if( (start < 0 || start >=  str->len) ||
        (end < 0 || end >= str->len) ) {
      EXEC_FAIL1(a,"Function::bslice start and end index must be larger than "
          "0 and less than size of the string , which is %d",(int)str->len);
    }

    *ret = ajj_value_assign( ajj_object_create_string(a,ajj_cur_gc_scope(a),
          str->str+start,end-start,0));
    return AJJ_EXEC_OK;
  }
}

static
int filter_do_string_manipulate(
    struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret,
    Rune (*conv)(Rune),
    const char* funcname ) {
  UNUSE_ARG(udata);
  if(arg_len !=1) {
    EXEC_FAIL1(a,"Function::%s requires 1 argument!",funcname);
  } else {
    struct strbuf sbuf;
    struct string* str;
    int i;

    /* Handle the arguments for this function */
    if(arg->type != AJJ_VALUE_STRING) {
      EXEC_FAIL1(a,"Function:%s 1st argument must be string!",funcname);
    }
    str = ajj_value_to_string(arg);

    strbuf_init(&sbuf);
    /* Decode the UTF string and then upper case each Rune */
    for( i = 0 ; i <  str->len ; ) {
      Rune r;
      Rune cr;
      int ret = chartorune(&r,str->str+i);
      if(ret == Runeerror) {
        strbuf_destroy(&sbuf);
        EXEC_FAIL1(a,"Function:%s has UTF encoding error!",funcname);
      }
      /* Perform the conversion */
      cr = (*conv)(r);
      /* Push rune into the strbuf */
      i += strbuf_push_rune(&sbuf,cr);
    }

    /* ignore whether this string is correct UTF or not */
    {
      struct string nstr = strbuf_fitstring(&sbuf);
      *ret = ajj_value_assign(
          ajj_object_create_string(a,
            ajj_cur_gc_scope(a),nstr.str,nstr.len,1));
    }
    strbuf_destroy(&sbuf);
    return AJJ_EXEC_OK;
  }
}

static
int filter_upper( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  return filter_do_string_manipulate(a,
      udata,arg,arg_len,ret,toupperrune,"upper");
}

static 
int filter_lower( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  return filter_do_string_manipulate(a,
      udata,arg,arg_len,ret,tolowerrune,"lower");
}

static
int filter_default( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  UNUSE_ARG(udata);
  if(arg_len != 2) {
    EXEC_FAIL1(a,"%s","Function::default requires 2 arguments!");
  } else {
    if( arg->type == AJJ_VALUE_NONE ) {
      *ret = arg[1];
    } else {
      *ret = arg[0];
    }
    return AJJ_EXEC_OK;
  }
}

/* type conversion */

void ajj_builtin_load( struct ajj* a ) {
  /* list */
  a->list =  ajj_add_class(a,&(a->builtins),
      &LIST_CLASS);
  /* dict */
  a->dict = ajj_add_class(a,&(a->builtins),
      &DICT_CLASS);
  /* xrange */
  ajj_add_class(a,&(a->builtins),
      &XRANGE_CLASS);
  /* loop */
  a->loop = ajj_add_class(a,&(a->builtins),
      &LOOP_CLASS);
  /* cycler */
  ajj_add_class(a,&(a->builtins),
      &CYCLER_CLASS);

  /* builtin functions */
  ajj_add_function(a,&(a->builtins),
      CALLER.str,
      vm_caller,
      NULL);

  ajj_add_function(a,&(a->builtins),
      SUPER.str,
      vm_super,
      NULL);

  ajj_add_function(a,&(a->builtins),
      "to_json",
      to_json,
      NULL);

  ajj_add_function(a,&(a->builtins),
      "to_jsonc",
      to_jsonc,
      NULL);

  ajj_add_function(a,&(a->builtins),
      "shell",
      shell,
      NULL);

  ajj_add_function(a,&(a->builtins),
      "lstrip",
      lstrip,
      NULL);

  ajj_add_function(a,&(a->builtins),
      "rstrip",
      rstrip,
      NULL);

  ajj_add_function(a,&(a->builtins),
      "typeof",
      type_of,
      NULL);

  ajj_add_function(a,&(a->builtins),
      "assert_expr",
      assert_expr,
      NULL);

  ajj_add_function(a,&(a->builtins),
      "floor",
      number_floor,
      NULL);

  ajj_add_function(a,&(a->builtins),
      "ceil",
      number_ceil,
      NULL);

  ajj_add_filter(a,&(a->builtins),
      "abs",
      filter_abs,
      NULL);

  ajj_add_filter(a,&(a->builtins),
      "attr",
      filter_attr,
      NULL);

  ajj_add_filter(a,&(a->builtins),
      "default",
      filter_default,
      NULL);

  ajj_add_filter(a,&(a->builtins),
      "slice",
      filter_slice,
      NULL);

  ajj_add_filter(a,&(a->builtins),
      "bslice",
      filter_bslice,
      NULL);

  ajj_add_filter(a,&(a->builtins),
      "upper",
      filter_upper,
      NULL);

  ajj_add_filter(a,&(a->builtins),
      "lower",
      filter_lower,
      NULL);

  /* builtin test */
  ajj_add_test(a,&(a->builtins),
      "True",test_true,"True");

  ajj_add_test(a,&(a->builtins),
      "true",test_true,"true");

  ajj_add_test(a,&(a->builtins),
      "False",test_false,"False");

  ajj_add_test(a,&(a->builtins),
      "false",test_false,"false");

  ajj_add_test(a,&(a->builtins),
      "None",test_none,"None");

  ajj_add_test(a,&(a->builtins),
      "none",test_none,"none");

  ajj_add_test(a,&(a->builtins),
      "undefined",test_none,"undefined");

  ajj_add_test(a,&(a->builtins),
      "defined",test_defined,NULL);

  ajj_add_test(a,&(a->builtins),
      "divisableby",test_divisableby,NULL);

  ajj_add_test(a,&(a->builtins),
      "even",test_even,NULL);

  ajj_add_test(a,&(a->builtins),
      "iterable",test_iterator,NULL);

  ajj_add_test(a,&(a->builtins),
      "mapping",test_mapping,NULL);

  ajj_add_test(a,&(a->builtins),
      "number",test_number,NULL);

  ajj_add_test(a,&(a->builtins),
      "odd",test_odd,NULL);

  ajj_add_test(a,&(a->builtins),
      "sameas",test_sameas,NULL);

  ajj_add_test(a,&(a->builtins),
      "string",test_string,NULL);

  ajj_add_test(a,&(a->builtins),
      "object",test_object,NULL);
}
