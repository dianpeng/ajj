#ifndef _OBJECT_H_
#define _OBJECT_H_
#include "ajj.h"
#include "util.h"
#include "ajj-priv.h"

struct gc_scope;

struct c_closure {
  void* udata; /* user data */
  ajj_method func; /* function */
};

static inline
void c_closure_init( struct c_closure* cc ) {
  cc->udata = NULL;
  cc->func = NULL;
}

enum {
  C_FUNCTION,
  C_METHOD,
  JJ_BLOCK,
  JJ_MACRO
};

struct function {
  union {
    struct c_closure c_fn; /* C function */
    ajj_method c_mt; /* C method */
    struct program jj_fn; /* JJ_BLOCK/JJ_MACRO */
  } f;
  struct string name;
  int tp;
};

struct func_table {
  struct function func_buf[ AJJ_FUNC_LOCAL_BUF_SIZE ];
  struct function* func_tb; /* function table */
  size_t func_len;
  size_t func_cap;
  ajj_class_dtor dtor;
  ajj_class ctor ctor;
  void* udata;
  struct string name; /* object's name */
};

/* This function will initialize an existed function table */
static inline
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
static inline
void func_table_clear( struct func_table* tb ) {
  if( tb->func_cap > AJJ_FUNC_LOCAL_BUF_SIZE ) {
    free(tb->func_tb); /* on heap */
  }
  string_destroy(&(tb->name));
}

/* Add a new function into the func_table */
static inline
struct function* func_table_add_func( struct func_table* tb ) {
  if( tb->func_len == tb->func_cap ) {
   void* nf = malloc( sizeof(struct function)*(tb->func_cap)*2 );
   memcpy(nf,tb->func_tb,tb->func_len*sizeof(struct function));
   if( tb->func_tb != tb->func_buf ) {
     free(tb->func_tb);
   }
   tb->func_tb = nf;
   tb->func_cap *= 2;
  }
  return tb->func_tb + (tb->func_len++);
}

static inline
void func_table_shrink_to_fit( struct func_table* tb ) {
  if( tb->func_cap > AJJ_FUNC_LOCAL_BUF_SIZE ) {
    if( tb->func_len < tb->func_cap ) {
      struct function* f = malloc(tb->func_len*sizeof(struct function));
      memcpy(f,tb->func_tb,sizeof(struct function)*tb->func_len);
      free(tb->func_tb);
      tb->func_tb = f;
      tb->func_cap = func_len;
    }
  }
}

/* Find a new function in func_table */
static
struct function* func_table_find_func( struct func_table* tb,
    const struct string* name );

static inline
struct c_closure*
func_table_add_c_clsoure( struct func_table* tb , struct string* name , int own ) {
  struct function* f = func_table_find_func(tb,name);
  if( f != NULL )
    return NULL;
  f = func_table_add_func(tb);
  f->name = own ? *name : string_dup(name);
  f->tp = C_FUNCTION;
  c_closure_init(&(f->f,c_fn));
  return &(f->f.c_fn);
}

static inline
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

static inline
struct program*
func_table_add_jj_block( struct func_table* tb, struct string* name , int own ) {
  struct function* f = func_table_find_func(tb,name);
  if( f != NULL )
    return NULL;
  f = func_table_add_func(tb);
  f->name = own ? *name : string_dup(name);
  f->tp = JJ_BLOCK;
  program_init(&(f->f.jj_fn));
  return &(f->f.jj_fn);
}

static inline
struct program*
func_table_add_jj_macro( struct func_table* tb, struct string* name , int own ) {
  struct function* f = func_table_find_func(tb,name);
  if( f != NULL )
    return NULL;
  f = func_table_add_func(tb);
  f->name = own ? *name : string_dup(name);
  f->tp = JJ_MACRO;
  program_init(&(f->f.jj_fn));
  return &(f->f.jj_fn);
}

struct object {
  struct dict prop; /* properties of this objects */
  const struct func_table* fn_tb; /* This function table can be NULL which
                                   * simply represents this object doesn't have
                                   * any defined function related to it */
  void* data; /* object's data */
};

/* Create an object object */
static inline
void object_create( struct object* obj ,
    const struct func_table* func_tb, void* data ) {
  dict_create(&(obj->prop));
  obj->fn_tb = func_tb;
  obj->data = data;
}

struct ajj_object {
  struct ajj_object* prev;
  struct ajj_object* next;
  struct ajj_object* parent[AJJ_EXTENDS_MAX_SIZE]; /* for extends */
  unsigned short parent_len;
  unsigned short tp;
  union {
    struct string str; /* string */
    struct dict d;     /* dictionary */
    struct list l;     /* list */
    struct object obj; /* object */
  } val;
  struct gc_scope* scp;
};

/* Internally we separete a const string or a heap based string by
 * using this tag. Since if it is a AJJ_VALUE_CONST_STRING, then we
 * will never need to release the string memory */
#define AJJ_VALUE_CONST_STRING (AJJ_VALUE_SIZE+1)

/* Create a single ajj_object which is NOT INITIALZIED with any type
 * under the scope object "scope" */
static
struct ajj_object*
ajj_object_create ( struct ajj* , struct gc_scope* scope );

static inline
struct ajj_object*
ajj_object_create_child( struct ajj* , struct ajj_object* obj ) {
  return ajj_object_create(ajj,obj->scp);
}

/* Delete routine. Delete a single object from its GC. This deletion
 * function will only delete the corresponding object , not its children,
 * it is not safe to delete children, since the parent object doesn't
 * really have the ownership */
static inline
void ajj_object_destroy( struct ajj* , struct ajj_object* obj );

/* Initialize an created ajj_object to a certain type */
static inline
void ajj_object_string( struct ajj_object* obj,
    const char* str , size_t len , int own ) {
  obj->val.str.str = own ? str : strdup(str);
  obj->val.str.len = len;
  obj->tp = AJJ_VALUE_STRING;
}

static inline
struct ajj_object*
ajj_object_create_string( struct ajj* a, struct gc_scope* scp,
    const char* str, size_t len , int own ) {
  return ajj_object_string( ajj_object_create(a,scp),
      str,len,own);
}

static inline
void ajj_object_const_string( struct ajj_object* obj,
    const char* str, size_t len ) {
  obj->val.str.str = str;
  obj->val.str.len = len;
  obj->tp = AJJ_VALUE_CONST_STRING;
}

static inline
void ajj_object_create_const_string( struct ajj_object* obj,
    const char* str, size_t len ) {
  return ajj_object_const_string( ajj_object_create(a,scp),
      str,len);
}

static inline
void ajj_object_dict( struct ajj_object* obj ) {
  dict_create(&(obj->val.d));
  obj->tp = AJJ_VALUE_DICT;
}

static inline
struct ajj_object*
ajj_object_create_dict( struct ajj* a, struct gc_scope* scp ) {
  return ajj_object_dict(ajj_object_create(a,scp));
}

static inline
void ajj_object_list( struct ajj_object* obj ) {
  list_create(&(obj->val.l));
  obj->tp = AJJ_VALUE_LIST;
}

static inline
struct ajj_object*
ajj_object_create_list( struct ajj* a, struct gc_scope* scp ) {
  return ajj_object_list(ajj_object_create(a,scp));
}

static inline
void ajj_object_obj( struct ajj_object* obj ,
    const struct func_table* fn_tb, void* data ) {
  object_create(&(obj_val.o),fn_tb,data);
  obj->tp = AJJ_VALUE_OBJECT;
}

static inline
struct ajj_object*
ajj_object_create_obj( struct ajj* a, struct gc_scope* scp ) {
  return ajj_object_obj(ajj_object_create(a,scp));
}

/* ===================================================
 * Value wrapper for internal use
 * =================================================*/

static inline
void ajj_value_destroy( struct ajj* a , struct ajj_value* val ) {
  if( val->type != AJJ_VALUE_NOT_USE &&
      val->type != AJJ_VALUE_BOOLEAN &&
      val->type != AJJ_VALUE_NONE &&
      val->type != AJJ_VALUE_NUMBER )
    ajj_object_destroy(a,val->value.object);
}

static inline
const struct string* ajj_value_to_string( const struct ajj_value* val ) {
  assert(val->type == AJJ_VALUE_STRING);
  return &(val->value.object->val.str);
}


static inline
struct ajj_value ajj_value_assign( struct ajj_object* obj ) {
  struct ajj_value val;
  assert(obj->tp != AJJ_VALUE_NOT_USE);
  val.type = obj->tp;
  val.value.object = obj;
  return val;
}


#endif /* _OBJECT_H_ */
