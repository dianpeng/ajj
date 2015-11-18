#ifndef _OBJECT_H_
#define _OBJECT_H_
#include "ajj.h"
#include "util.h"
#include "vm.h"

struct gc_scope;
struct ajj;

/* Internally we separete a const string or a heap based string by
 * using this tag. Since if it is a AJJ_VALUE_CONST_STRING, then we
 * will never need to release the string memory */
#define AJJ_VALUE_CONST_STRING (AJJ_VALUE_SIZE+1)

#define LIST_LOCAL_BUF_SIZE 4
#define DICT_DEFAULT_BUF_SIZE 4
#define GVAR_DEFAULT_BUF_SIZE 32

struct c_closure {
  void* udata; /* user data */
  ajj_function func; /* function */
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
  JJ_MACRO,
  JJ_MAIN
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

#define IS_CFUNCTION(f) ((f)->tp == C_FUNCTION)
#define IS_CMETHOD(f) ((f)->tp == C_METHOD)
#define IS_JJBLOCK(f) ((f)->tp == JJ_BLOCK)
#define IS_JJMACRO(f) ((f)->tp == JJ_MACRO)
#define IS_JJMAIN(f) ((f)->tp == JJ_MAIN)
#define IS_JINJA(f) ((f)->tp == JJ_BLOCK || (f)->tp == JJ_MACRO || (f)->tp == JJ_MAIN)
#define IS_C(f) (!IS_JINJA(f))
#define CCLOSURE(F) (&((F)->f.c_fn))
#define CMETHOD(F) (&((F)->f.c_mt))
#define JINJAFUNC(F) (&((F)->f.jj_fn))

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

struct object {
  struct map  prop; /* properties of this objects */
  const struct func_table* fn_tb; /* This function table can be NULL which
                                   * simply represents this object doesn't have
                                   * any defined function related to it */
  void* data; /* object's data */
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

static inline
void list_create( struct list* l ) {
  l->entry = l->lbuf;
  l->cap = LIST_LOCAL_BUF_SIZE;
  l->len = 0;
}

void list_destroy(struct list* );
void list_push( struct list* , const struct ajj_value* val );
static inline
size_t list_size( struct list* l ) {
  return l->len;
}
static inline
const struct ajj_value*
list_index( struct list* l , size_t i ) {
  assert(i < l->len);
  return l->entry[i];
}
void list_clear();

/* iterator for list */
static inline
int list_iter_begin( const struct list* ) {
  return 0;
}
static inline
int list_iter_has( const struct list* l , int itr ) {
  return itr < l->len;
}
static inline
int list_iter_move( const struct list* l , int itr ) {
  return itr+1;
}
static inline
void* list_iter_deref( const struct list* l , int itr ) {
  return list_index(l,itr);
}

/* =====================================
 * Dict
 * just wrapper around map structure
 * ===================================*/
static inline
void dict_create( struct map* d ) {
  map_create(d,sizeof(struct ajj_value),DICT_DEFAULT_BUF_SIZE);
}

static inline
void dict_clear( struct map* d ) {
  map_clear(d);
}

static inline
void dict_destroy( struct map* d ) {
  map_destroy(d);
}

static inline
int dict_insert( struct map* d , const struct string* k,
    int own, const struct ajj_value* val ) {
  return map_insert(d,k,own,val);
}

static inline
int dict_insert_c( struct map* d , const char* k,
    const struct ajj_value* val ) {
  return map_insert_c(d,k,val);
}

static inline
int dict_remove( struct map* d , const struct string* k,
    void* val ) {
  return map_remove(d,k,val);
}

static inline
int dict_remove_c( struct map* d , const char* k, void* val ) {
  return map_remove(d,k,val);
}

static inline
const struct ajj_value*
dict_find( struct map* d , const struct string* k ) {
  return map_find(d,k);
}

static inline
const struct ajj_value*
dict_find_c( struct map* d , const char* k ) {
  return map_find(d,k);
}

/* iterator wrapper */
static inline
int dict_iter_start( const struct map* d ) {
  return map_iter_start(d);
}

static inline
int dict_iter_move( const struct map* d , int itr ) {
  return map_itr_move(d,itr);
}

static inline
int dict_iter_has( const struct map* d , int itr ) {
  return itr < d->cap;
}

static inline
struct map_pair
dict_itr_deref( struct map* d , int itr ) {
  return map_itr_deref(d,itr);
}

/* ======================================
 * AJJ_OBJECT
 * ====================================*/
struct ajj_object {
  struct ajj_object* prev;
  struct ajj_object* next;
  struct ajj_object* parent[AJJ_EXTENDS_MAX_SIZE]; /* for extends */
  unsigned short parent_len;
  unsigned short tp;
  union {
    struct string str; /* string */
    struct map  d;     /* dictionary */
    struct list l;     /* list */
    struct object obj; /* object */
  } val;
  struct gc_scope* scp;
};


/* ================================================
 * Global variables
 * only used in environment settings and upvalue
 * ================================================*/

struct global_var {
  struct global_var* prev; /* linked back to its parent */
  struct ajj_value val;
};

struct gvar {
  union {
    struct global_var* value; /* This value is actually pointed to the
                               * end of the upvalue chain. If value == NULL,
                               * just means this value is not set yet */
    struct c_closure func;    /* registered c functions */
    struct func_table* obj;   /* object prototype */
  } gut;
  int type;
};

enum {
  GVAR_VALUE,
  GVAR_FUNCTION,
  GVAR_OBJECT
};

struct gvar_table {
  struct gvar_table_entry* entries;
  size_t cap;
  size_t len;
};

/* Global varialbes table. Wrapper around map */
static inline
void gvar_table_init( struct map* m ) {
  map_init(m,GVAR_DEFUALT_BUF_SIZE,sizeof(struct gvar));
}

static inline
void gvar_table_clear( struct map* m ) {
  map_clear(m);
}

static inline
void gvar_table_destroy( struct map* m ) {
  map_destroy(m);
}

static inline
int gvar_table_insert( struct map* m , const struct string* k,
    int own, const struct gvar* val ) {
  return map_insert(m,k,own,val);
}

static inline
int gvar_table_insert_c( struct map* m , const char* k,
    const struct gvar* val ) {
  return map_insert_c(m,k,val);
}

static inline
int gvar_table_remove( struct map* m , const struct string* k,
    struct gvar* val ) {
  return map_remove(m,k,val);
}

static inline
int gvar_table_remove_c( struct map* m , const char* k,
    struct gvar* val ) {
  return map_remove(m,k,val);
}

static inline
const struct gvar*
gvar_tabel_find( struct map* m , const struct string* k ) {
  return gvar_table_find(m,k);
}

static inline
const struct gvar*
gvar_table_find_c( struct map* m , const char* k ) {
  return gvar_table_find_c(m,k);
}

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

static
void func_table_destroy( struct func_table* tb );

/* Find a new function in func_table */
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

/* Create an object object */
static inline
void object_create( struct object* obj ,
    const struct func_table* func_tb, void* data ) {
  dict_create(&(obj->prop));
  obj->fn_tb = func_tb;
  obj->data = data;
}

/* Create a single ajj_object which is NOT INITIALZIED with any type
 * under the scope object "scope" */
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
struct ajj_object*
ajj_object_string( struct ajj_object* obj,
    const char* str , size_t len , int own ) {
  obj->val.str.str = own ? str : strdup(str);
  obj->val.str.len = len;
  obj->tp = AJJ_VALUE_STRING;
  return obj;
}

static inline
struct ajj_object*
ajj_object_create_string( struct ajj* a, struct gc_scope* scp,
    const char* str, size_t len , int own ) {
  return ajj_object_string( ajj_object_create(a,scp),
      str,len,own);
}

static inline
struct ajj_object*
ajj_object_const_string( struct ajj_object* obj,
    const struct string* str ) {
  obj->val.str = *str;
  obj->tp = AJJ_VALUE_CONST_STRING;
  return obj;
}

static inline
struct ajj_object*
ajj_object_create_const_string( struct ajj*a , struct gc_scope* scp,
    const struct string* str ) {
  return ajj_object_const_string( ajj_object_create(a,scp),
      str);
}

static inline
struct ajj_object*
ajj_object_dict( struct ajj_object* obj ) {
  dict_create(&(obj->val.d));
  obj->tp = AJJ_VALUE_DICT;
  return obj;
}

static inline
struct ajj_object*
ajj_object_create_dict( struct ajj* a, struct gc_scope* scp ) {
  return ajj_object_dict(ajj_object_create(a,scp));
}

static inline
struct ajj_object*
ajj_object_list( struct ajj_object* obj ) {
  list_create(&(obj->val.l));
  obj->tp = AJJ_VALUE_LIST;
  return obj;
}

static inline
struct ajj_object*
ajj_object_create_list( struct ajj* a, struct gc_scope* scp ) {
  return ajj_object_list(ajj_object_create(a,scp));
}

static inline
struct ajj_object*
ajj_object_obj( struct ajj_object* obj ,
    const struct func_table* fn_tb, void* data ) {
  object_create(&(obj_val.o),fn_tb,data);
  obj->tp = AJJ_VALUE_OBJECT;
  return obj;
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
const char* ajj_value_to_cstr( const struct ajj_value* val ) {
  return ajj_value_to_string()->str;
}

static inline
const struct map*
ajj_value_to_dict( const struct ajj_value* val ) {
  assert(val->type == AJJ_VALUE_DICT);
  return &(val->value.object->val.d);
}

static inline
const struct list*
ajj_value_to_list( const struct ajj_value* val ) {
  assert(val->type == AJJ_VALUE_LIST);
  return &(val->value.object->val.l);
}

static inline
const struct object*
ajj_value_to_object( const struct ajj_value* val ) {
  assert(val->type == AJJ_VALUE_OBJECT);
  return &(val->value.object->val.obj);
}

static inline
struct ajj_value
ajj_value_boolean( int boolean ) {
  struct ajj_value ret;
  assert(boolean ==0 || boolean ==1);
  ret.type = AJJ_VALUE_BOOLEAN;
  ret.value.boolean = boolean;
  return ret;
}

static inline
struct ajj_value ajj_value_assign( struct ajj_object* obj ) {
  struct ajj_value val;
  assert(obj->tp != AJJ_VALUE_NOT_USE);
  val.type = obj->tp;
  /* rewrite const string type since it is internally used */
  if( val.type == AJJ_VALUE_CONST_STRING )
    val.type == AJJ_VALUE_STRING;
  val.value.object = obj;
  return val;
}
#endif /* _OBJECT_H_ */
