#ifndef _OBJECT_H_
#define _OBJECT_H_
#include "ajj.h"
#include "util.h"
#include "vm.h"

struct gc_scope;
struct ajj;

extern struct string MAIN; /* For __main__ */

/* Internally we separete a const string or a heap based string by
 * using this tag. Since if it is a AJJ_VALUE_CONST_STRING, then we
 * will never need to release the string memory */
#define AJJ_VALUE_CONST_STRING (AJJ_VALUE_SIZE+1)

/* Internal representation of iterators */
#define AJJ_VALUE_ITERATOR     (AJJ_VALUE_SIZE+2)

/* Internal representation of JINJA template */
#define AJJ_VALUE_JINJA  (AJJ_VALUE_SIZE+3)

#define AJJ_IS_PRIMITIVE(V) \
  ((V)->type==AJJ_VALUE_NUMBER||\
   (V)->type==AJJ_VALUE_NONE|| \
   (V)->type==AJJ_VALUE_NONE||\
   (V)->type==AJJ_VALUE_ITERATOR|| \
   (V)->type==AJJ_VALUE_BOOLEAN)

#define AJJ_IS_REFERENCE(V) (!(AJJ_IS_PRIMITIVE(V)))


#define LIST_LOCAL_BUF_SIZE 4
#define DICT_DEFAULT_BUF_SIZE 4

struct c_closure {
  void* udata; /* user data */
  ajj_function func; /* function */
};

static
void c_closure_init( struct c_closure* cc ) {
  cc->udata = NULL;
  cc->func = NULL;
}

enum {
  C_FUNCTION,
  C_METHOD,
  JJ_BLOCK,
  JJ_MACRO,
  JJ_MAIN
};

static
const char* function_get_type_name( int tp ) {
  switch(tp) {
    case C_FUNCTION: return "c-function";
    case C_METHOD: return "c-method";
    case JJ_BLOCK: return "jinja-block";
    case JJ_MACRO: return "jinja-macro";
    case JJ_MAIN: return "jinja-main";
    default: UNREACHABLE(); return NULL;
  }
}

struct function {
  union {
    struct c_closure c_fn; /* C function */
    ajj_method c_mt; /* C method */
    struct program jj_fn; /* JJ_BLOCK/JJ_MACRO */
  } f;
  struct string name;
  int tp;
};

#define IS_CFUNCTION(f) ((f)->tp == C_FUNCTION)
#define IS_CMETHOD(f) ((f)->tp == C_METHOD)
#define IS_JJBLOCK(f) ((f)->tp == JJ_BLOCK)
#define IS_JJMACRO(f) ((f)->tp == JJ_MACRO)
#define IS_JJMAIN(f) ((f)->tp == JJ_MAIN)
#define IS_JINJA(f) ((f)->tp == JJ_BLOCK || (f)->tp == JJ_MACRO || (f)->tp == JJ_MAIN)
#define IS_C(f) (!IS_JINJA(f))
#define GET_CCLOSURE(F) (&((F)->f.c_fn))
#define GET_CMETHOD(F) (&((F)->f.c_mt))
#define GET_JINJAFUNC(F) (&((F)->f.jj_fn))

struct func_table {
  struct function func_buf[ AJJ_FUNC_LOCAL_BUF_SIZE ];
  struct function* func_tb; /* function table */
  size_t func_len;
  size_t func_cap;
  ajj_class_dtor dtor;
  ajj_class_ctor ctor;
  void* udata;
  struct string name; /* object's name */
};

struct object {
  struct map  prop; /* properties of this objects */
  struct func_table* fn_tb; /* This function table can be NULL which
                             * simply represents this object doesn't have
                             * any defined function related to it */
  void* data; /* object's data */

  /* Field only used when the object is a JINJA object */
  const char* src;     /* source file */
};


/* ======================================
 * List
 * ====================================*/

struct list {
  struct ajj_value lbuf[LIST_LOCAL_BUF_SIZE];
  struct ajj_value* entry;
  size_t cap;
  size_t len;
};

static
void list_create( struct list* l ) {
  l->entry = l->lbuf;
  l->cap = LIST_LOCAL_BUF_SIZE;
  l->len = 0;
}

void list_destroy(struct list* );

void list_push( struct list* , const struct ajj_value* val );

static
size_t list_size( struct list* l ) {
  return l->len;
}
static
struct ajj_value*
list_index( const struct list* l , size_t i ) {
  assert(i < l->len);
  return l->entry + i;
}
void list_clear();

/* iterator for list */
static
int list_iter_start( const struct list* l ) {
  UNUSE_ARG(l);
  return 0;
}
static
int list_iter_has( const struct list* l , int itr ) {
  return itr < l->len;
}
static
int list_iter_move( const struct list* l , int itr ) {
  return itr+1;
}
static
struct ajj_value*
list_iter_deref( const struct list* l , int itr ) {
  return list_index(l,itr);
}

/* =====================================
 * Dict
 * just wrapper around map structure
 * ===================================*/
static
void dict_create( struct map* d ) {
  map_create(d,sizeof(struct ajj_value),DICT_DEFAULT_BUF_SIZE);
}

static
void dict_clear( struct map* d ) {
  map_clear(d);
}

static
void dict_destroy( struct map* d ) {
  map_destroy(d);
}

static
int dict_insert( struct map* d , const struct string* k,
    int own, const struct ajj_value* val ) {
  return map_insert(d,k,own,val);
}

static
int dict_insert_c( struct map* d , const char* k,
    const struct ajj_value* val ) {
  return map_insert_c(d,k,val);
}

static
int dict_remove( struct map* d , const struct string* k,
    void* val ) {
  return map_remove(d,k,val);
}

static
int dict_remove_c( struct map* d , const char* k, void* val ) {
  return map_remove_c(d,k,val);
}

static
struct ajj_value*
dict_find( struct map* d , const struct string* k ) {
  return map_find(d,k);
}

static
struct ajj_value*
dict_find_c( struct map* d , const char* k ) {
  return map_find_c(d,k);
}

/* iterator wrapper */
static
int dict_iter_start( const struct map* d ) {
  return map_iter_start(d);
}

static
int dict_iter_move( const struct map* d , int itr ) {
  return map_iter_move(d,itr);
}

static
int dict_iter_has( const struct map* d , int itr ) {
  return map_iter_has(d,itr);
}

static
struct map_pair
dict_iter_deref( struct map* d , int itr ) {
  return map_iter_deref(d,itr);
}

/* ======================================
 * AJJ_OBJECT
 * ====================================*/
struct ajj_object {
  struct ajj_object* prev;
  struct ajj_object* next;
  struct ajj_object* parent[AJJ_EXTENDS_MAX_SIZE]; /* for extends */
  unsigned short parent_len;
  unsigned short tp; /* Type for this object. This type CAN be different
                      * with the type in its holder ajj_value. A AJJ_VALUE_STRING
                      * could internally represent as 1. AJJ_VALUE_STRING (heap)
                      * 2. AJJ_VALUE_CONST_STRING (reference).
                      * Also this tp can be AJJ_VALUE_JINJA, which represents a
                      * jinja template, but its holder's type is AJJ_VALUE_OBJECT. */
  union {
    struct string str; /* string */
    struct map  d;     /* dictionary */
    struct list l;     /* list */
    struct object obj; /* object */
  } val;
  struct gc_scope* scp;
};

#define IS_OBJECT_OWNED(obj) (((obj)->scp) == NULL)
#define IS_VALUE_OWNED(val) ((AJJ_IS_PRIMITIVE(val))||((val)->value.object->scp!=NULL))


/* This function will initialize an existed function table */
static
void func_table_init( struct func_table* tb ,
    ajj_class_ctor ctor ,
    ajj_class_dtor dtor ,
    struct string* name , int own ) {
  tb->func_tb = tb->func_buf;
  tb->func_len = 0;
  tb->func_cap = AJJ_FUNC_LOCAL_BUF_SIZE;
  tb->dtor = dtor ;
  tb->ctor = ctor ;
  tb->name = own ? *name : string_dup(name);
}

/* Clear the GUT of func_table object */
void func_table_clear( struct ajj* a , struct func_table* tb );

/* Add a new function into the func_table */
static
struct function* func_table_add_func( struct func_table* tb ) {
  if( tb->func_len == tb->func_cap ) {
   void* nf;
   if( tb->func_tb == tb->func_buf ) {
     nf = malloc( sizeof(struct function)*(tb->func_cap)*2 );
     memcpy(nf,tb->func_tb,tb->func_len*sizeof(struct function));
   } else {
     nf = realloc(tb->func_tb,sizeof(struct function)*(tb->func_cap)*2);
   }
   tb->func_tb = nf;
   tb->func_cap *= 2;
  }
  return tb->func_tb + (tb->func_len++);
}

void func_table_destroy( struct ajj* a , struct func_table* tb );

/* Find a new function in func_table */
struct function* func_table_find_func( struct func_table* tb,
    const struct string* name );

static
struct c_closure*
func_table_add_c_clsoure( struct func_table* tb , struct string* name , int own ) {
  struct function* f = func_table_find_func(tb,name);
  if( f != NULL )
    return NULL;
  f = func_table_add_func(tb);
  f->name = own ? *name : string_dup(name);
  f->tp = C_FUNCTION;
  c_closure_init(&(f->f.c_fn));
  return &(f->f.c_fn);
}

static
ajj_method*
func_table_add_c_method( struct func_table* tb , struct string* name , int own ) {
  struct function* f = func_table_find_func(tb,name);
  if( f != NULL )
    return NULL;
  f = func_table_add_func(tb);
  f->name = own ? *name : string_dup(name);
  f->tp = C_METHOD;
  return &(f->f.c_mt);
}

static
struct function*
func_table_add_jinja_func( struct func_table* tb, struct string* name, int own ) {
  struct function* f = func_table_find_func(tb,name);
  if( f != NULL )
    return NULL;
  f = func_table_add_func(tb);
  f->name = own ? *name : string_dup(name);
  program_init(&(f->f.jj_fn));
  return f;
}

static
struct program*
func_table_add_jj_block( struct func_table* tb, struct string* name , int own ) {
  struct function* f = func_table_add_jinja_func(tb,name,own);
  if(f) f->tp = JJ_BLOCK;
  else return NULL;
  return &(f->f.jj_fn);
}

static
struct program*
func_table_add_jj_macro( struct func_table* tb, struct string* name , int own ) {
  struct function* f = func_table_add_jinja_func(tb,name,own);
  if(f) f->tp = JJ_MACRO;
  else return NULL;
  return &(f->f.jj_fn);
}

static
struct program*
func_table_add_jj_main( struct func_table* tb, struct string* name , int own ) {
  struct function* f = func_table_add_jinja_func(tb,name,own);
  if(f) f->tp = JJ_MAIN;
  else return NULL;
  return &(f->f.jj_fn);
}

/* Create a single ajj_object which is NOT INITIALZIED with any type
 * under the scope object "scope" */
struct ajj_object*
ajj_object_create ( struct ajj* , struct gc_scope* scope );

struct ajj_object*
ajj_object_move( struct gc_scope* scp , struct ajj_object* obj );

/* Initialize an created ajj_object to a certain type */
static
struct ajj_object*
ajj_object_string( struct ajj_object* obj,
    const char* str , size_t len , int own ) {
  obj->val.str.str = own ? str : strdup(str);
  obj->val.str.len = len;
  obj->tp = AJJ_VALUE_STRING;
  return obj;
}

static
struct ajj_object*
ajj_object_create_string( struct ajj* a, struct gc_scope* scp,
    const char* str, size_t len , int own ) {
  return ajj_object_string( ajj_object_create(a,scp),
      str,len,own);
}

static
struct ajj_object*
ajj_object_const_string( struct ajj_object* obj,
    const struct string* str ) {
  obj->val.str = *str;
  obj->tp = AJJ_VALUE_CONST_STRING;
  return obj;
}

static
struct ajj_object*
ajj_object_create_const_string( struct ajj*a , struct gc_scope* scp,
    const struct string* str ) {
  return ajj_object_const_string( ajj_object_create(a,scp),
      str);
}

static
struct ajj_object*
ajj_object_dict( struct ajj_object* obj ) {
  dict_create(&(obj->val.d));
  obj->tp = AJJ_VALUE_DICT;
  return obj;
}

static
struct ajj_object*
ajj_object_create_dict( struct ajj* a, struct gc_scope* scp ) {
  return ajj_object_dict(ajj_object_create(a,scp));
}

static
struct ajj_object*
ajj_object_list( struct ajj_object* obj ) {
  list_create(&(obj->val.l));
  obj->tp = AJJ_VALUE_LIST;
  return obj;
}

static
struct ajj_object*
ajj_object_create_list( struct ajj* a, struct gc_scope* scp ) {
  return ajj_object_list(ajj_object_create(a,scp));
}

static
struct ajj_object*
ajj_object_obj( struct ajj_object* obj , struct func_table* fn_tb, void* data ) {
  struct object* o = &(obj->val.obj);
  dict_create(&(o->prop));
  o->fn_tb = fn_tb;
  o->data = data;
  o->src = NULL;
  obj->tp = AJJ_VALUE_OBJECT;
  return obj;
}

static
struct ajj_object*
ajj_object_create_obj( struct ajj* a, struct gc_scope* scp,
    struct func_table* fn_tb, void* data ) {
  return ajj_object_obj(ajj_object_create(a,scp),fn_tb,data);
}

/* Creating a jinja object. A jinja object is an object represents a
 * compiled jinja source file. It is only used internally by the parser
 */
struct ajj_object*
ajj_object_jinja( struct ajj* , struct ajj_object* obj ,
    const char* name , const char* src , int own );

struct ajj_object*
ajj_object_create_jinja( struct ajj* a , const char* name ,
    const char* src , int own );

static
const struct program*
ajj_object_jinja_main( const struct ajj_object* obj ) {
  struct function* f;
  assert(obj->tp == AJJ_VALUE_JINJA);
  if( (f=func_table_find_func(obj->val.obj.fn_tb,&MAIN)) ) {
    assert(IS_JINJA(f));
    return &(f->f.jj_fn);
  }
  return NULL;
}

static
const struct program*
ajj_object_get_jinja_macro( const struct ajj_object* obj ,
    const struct string* name ) {
  struct function* f;
  assert(obj->tp == AJJ_VALUE_JINJA);
  if( (f=func_table_find_func(obj->val.obj.fn_tb,name)) ) {
    if( IS_JJMACRO(f) ) {
      return &(f->f.jj_fn);
    }
  }
  return NULL;
}

static
const struct program*
ajj_object_get_jinja_block( const struct ajj_object* obj ,
    const struct string* name ) {
  struct function* f;
  assert(obj->tp == AJJ_VALUE_JINJA);
  if( (f=func_table_find_func(obj->val.obj.fn_tb,name)) ) {
    if( IS_JJBLOCK(f) ) {
      return &(f->f.jj_fn);
    }
  }
  return NULL;
}

static
const struct c_closure*
ajj_object_get_c_closure( const struct ajj_object* obj ,
    const struct string* name ) {
  struct function* f;
  assert(obj->tp == AJJ_VALUE_OBJECT);
  if( (f=func_table_find_func(obj->val.obj.fn_tb,name)) ) {
    if( IS_CFUNCTION(f) ) {
      return &(f->f.c_fn);
    }
  }
  return NULL;
}

static
const ajj_method*
ajj_object_get_c_method( const struct ajj_object* obj ,
    const struct string* name ) {
  struct function* f;
  assert(obj->tp == AJJ_VALUE_OBJECT);
  if( (f=func_table_find_func(obj->val.obj.fn_tb,name)) ) {
    if( IS_CFUNCTION(f) ) {
      return &(f->f.c_mt);
    }
  }
  return NULL;
}

/* ===================================================
 * Value wrapper for internal use
 * =================================================*/

static
const struct string* ajj_value_to_string( const struct ajj_value* val ) {
  assert(val->type == AJJ_VALUE_STRING);
  return &(val->value.object->val.str);
}

static
const char* ajj_value_to_cstr( const struct ajj_value* val ) {
  return ajj_value_to_string(val)->str;
}

static
const struct map*
ajj_value_to_dict( const struct ajj_value* val ) {
  assert(val->type == AJJ_VALUE_DICT);
  return &(val->value.object->val.d);
}

static
const struct list*
ajj_value_to_list( const struct ajj_value* val ) {
  assert(val->type == AJJ_VALUE_LIST);
  return &(val->value.object->val.l);
}

static
const struct object*
ajj_value_to_obj( const struct ajj_value* val ) {
  assert(val->type == AJJ_VALUE_OBJECT);
  return &(val->value.object->val.obj);
}

static
int ajj_value_to_iter( const struct ajj_value* val ) {
  assert(val->type == AJJ_VALUE_ITERATOR);
  return val->value.boolean;
}

static
struct ajj_value
ajj_value_boolean( int boolean ) {
  struct ajj_value ret;
  assert(boolean ==0 || boolean ==1);
  ret.type = AJJ_VALUE_BOOLEAN;
  ret.value.boolean = boolean;
  return ret;
}

static
struct ajj_value
ajj_value_iter( int itr ) {
  struct ajj_value ret;
  ret.type = AJJ_VALUE_ITERATOR;
  ret.value.boolean = itr; /* Use boolean to store the iterator */
  return ret;
}

static
struct ajj_value ajj_value_assign( struct ajj_object* obj ) {
  struct ajj_value val;
  assert(obj->tp != AJJ_VALUE_NOT_USE);
  val.type = obj->tp;

  /* rewrite the internal object type into public type */
  if( val.type == AJJ_VALUE_CONST_STRING )
    val.type = AJJ_VALUE_STRING;
  else if( val.type == AJJ_VALUE_JINJA )
    val.type = AJJ_VALUE_OBJECT;

  val.value.object = obj;
  return val;
}

/* This is the only safe way to copy an ajj_value to another value
 * holder. Internally it will copy the primitive type or MOVE the
 * none-primitive type to the target gc_scope. If gc_scope is NULL,
 * then this value is escaped and it means no gc_scope will hold it
 * The typical usage is for assigning to upvalue */
struct ajj_value
ajj_value_copy( struct ajj* a , struct gc_scope* ,
    const struct ajj_value* src );

/* This allow user to delete a string promptly without waiting for the
 * corresponding gc scope exit */
void
ajj_value_delete_string( struct ajj* a, struct ajj_value* str);

#endif /* _OBJECT_H_ */
