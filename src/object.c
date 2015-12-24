#include "ajj-priv.h"

const char* function_get_type_name( int tp ) {
  switch(tp) {
    case C_FUNCTION: return "c-function";
    case C_METHOD: return "c-method";
    case OBJECT_CTOR: return "object-ctor";
    case JJ_BLOCK: return "jinja-block";
    case JJ_MACRO: return "jinja-macro";
    case JJ_MAIN: return "jinja-main";
    default: UNREACHABLE(); return NULL;
  }
}

/* This function will initialize an existed function table */
void func_table_init( struct func_table* tb ,
    ajj_class_ctor ctor ,
    ajj_class_dtor dtor ,
    const struct ajj_slot* slot,
    void* udata,
    struct string* name , int own ) {
  tb->func_tb = tb->func_buf;
  tb->func_len = 0;
  tb->func_cap = AJJ_FUNC_LOCAL_BUF_SIZE;
  tb->dtor = dtor ;
  tb->ctor = ctor ;
  tb->name = own ? *name : string_dup(name);
  if(slot)
    tb->slot = *slot;
  else
    memset(&(tb->slot),0,sizeof(*slot));
  tb->udata = udata;
}

/* Add a new function into the func_table */
struct function* func_table_add_func( struct func_table* tb ) {
  if( tb->func_len == tb->func_cap ) {
   void* nf;
   if( tb->func_tb == tb->func_buf ) {
     nf = malloc( sizeof(struct function)*(tb->func_cap)*2 );
     memcpy(nf,tb->func_tb,tb->func_len*sizeof(struct function));
   } else {
     nf = mem_grow(tb->func_tb,sizeof(struct function),0,&(tb->func_cap));
   }
   tb->func_tb = nf;
  }
  return tb->func_tb + (tb->func_len++);
}

struct c_closure*
func_table_add_c_clsoure( struct func_table* tb ,
    const struct string* name , int own ) {
  struct function* f = func_table_find_func(tb,name);
  if( f != NULL )
    return NULL;
  f = func_table_add_func(tb);
  f->name = own ? *name : string_dup(name);
  f->tp = C_FUNCTION;
  c_closure_init(&(f->f.c_fn));
  return &(f->f.c_fn);
}

ajj_method*
func_table_add_c_method( struct func_table* tb ,
    const struct string* name , int own ) {
  struct function* f = func_table_find_func(tb,name);
  if( f != NULL )
    return NULL;
  f = func_table_add_func(tb);
  f->name = own ? *name : string_dup(name);
  f->tp = C_METHOD;
  return &(f->f.c_mt);
}

struct function*
func_table_add_jinja_func( struct func_table* tb,
    const struct string* name, int own ) {
  struct function* f = func_table_find_func(tb,name);
  if( f != NULL )
    return NULL;
  f = func_table_add_func(tb);
  f->name = own ? *name : string_dup(name);
  program_init(&(f->f.jj_fn));
  return f;
}

struct program*
func_table_add_jj_block( struct func_table* tb,
    const struct string* name , int own ) {
  struct function* f = func_table_add_jinja_func(tb,name,own);
  if(f) f->tp = JJ_BLOCK;
  else return NULL;
  return &(f->f.jj_fn);
}

struct program*
func_table_add_jj_macro( struct func_table* tb,
    const struct string* name , int own ) {
  struct function* f = func_table_add_jinja_func(tb,name,own);
  if(f) f->tp = JJ_MACRO;
  else return NULL;
  return &(f->f.jj_fn);
}

struct program*
func_table_add_jj_main( struct func_table* tb,
    const struct string* name , int own ) {
  struct function* f = func_table_add_jinja_func(tb,name,own);
  if(f) f->tp = JJ_MAIN;
  else return NULL;
  return &(f->f.jj_fn);
}

struct ajj_object*
ajj_object_string( struct ajj_object* obj,
    const char* str , size_t len , int own ) {
  obj->val.str.str = own ? str : strdup(str);
  obj->val.str.len = len;
  obj->tp = AJJ_VALUE_STRING;
  return obj;
}

struct ajj_object*
ajj_object_const_string( struct ajj_object* obj,
    const struct string* str ) {
  obj->val.str = *str;
  obj->tp = AJJ_VALUE_CONST_STRING;
  return obj;
}


struct ajj_object*
ajj_object_obj( struct ajj_object* obj ,
    struct func_table* fn_tb, void* data , int tp ) {
  struct object* o = &(obj->val.obj);
  o->fn_tb = fn_tb;
  o->data = data;
  o->src = NULL;
  obj->tp = tp;
  return obj;
}

const struct function*
ajj_object_jinja_main_func( const struct ajj_object* obj ) {
  const struct function* f;
  assert(obj->tp == AJJ_VALUE_JINJA);
  if( (f=func_table_find_func(obj->val.obj.fn_tb,&MAIN)) ) {
    assert(IS_JINJA(f));
    return f;
  }
  return NULL;
}

const struct program*
ajj_object_jinja_main( const struct ajj_object* obj ) {
  const struct function* f = ajj_object_jinja_main_func(obj);
  if(f) return &(f->f.jj_fn);
  return NULL;
}

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

struct ajj_value
ajj_value_iter( int itr ) {
  struct ajj_value ret;
  ret.type = AJJ_VALUE_ITERATOR;
  ret.value.boolean = itr; /* Use boolean to store the iterator */
  return ret;
}

struct ajj_value ajj_value_assign( struct ajj_object* obj ) {
  struct ajj_value val;
  if(obj->tp == AJJ_VALUE_CONST_STRING ||
     obj->tp == AJJ_VALUE_STRING )
    val.type = AJJ_VALUE_STRING;
  else /* else are all categorized as object */
    val.type = AJJ_VALUE_OBJECT;
  val.value.object = obj;
  return val;
}


/* Function table */
struct function*
func_table_find_func( struct func_table* tb , const struct string* name ) {
  size_t i;
  for( i = 0 ; i < tb->func_len ; ++i ) {
    if( string_eq(name,&(tb->func_tb[i].name)) ) {
      return tb->func_tb + i;
    }
  }
  return NULL;
}

void func_table_destroy( struct ajj* a , struct func_table* tb ) {
  size_t i;
  string_destroy(&(tb->name)); /* delete the name */
  /* travel through each function table to delete program if it
   * is a script program. */
  for( i = 0 ; i < tb->func_len ; ++i ) {
    struct function* f = tb->func_tb + i;
    string_destroy(&(f->name));
    if(IS_JINJA(f)) {
      /* destroy scripted function */
      program_destroy(&(f->f.jj_fn));
    }
  }
  if( tb->func_tb != tb->func_buf )
    free(tb->func_tb); /* free func array */
  slab_free(&(a->ft_slab),tb);
}

/* Object */
struct ajj_object*
ajj_object_create( struct ajj* a , struct gc_scope* scope ) {
  struct ajj_object* ret = slab_malloc(&(a->obj_slab));
  LINSERT(ret,&(scope->gc_tail));
  ret->scp = scope;
  ret->parent_len = 0;
  return ret;
}

struct ajj_object*
ajj_object_jinja( struct ajj* a , struct ajj_object* obj ,
    const char* name , const char* src , int own ) {
  struct func_table* ft = slab_malloc(&(a->ft_slab));
  struct string fn = string_dupc(name);
  func_table_init(ft,
      NULL,NULL, /* null ctor and dtor */
      NULL,NULL, /* slot and udata */
      &fn,
      1);
  obj->tp = AJJ_VALUE_JINJA;
  obj->val.obj.data = NULL;
  obj->val.obj.fn_tb = ft;
  obj->val.obj.src = own ? src : strdup(src);
  return obj;
}

struct ajj_object*
ajj_object_create_jinja( struct ajj* a , const char* name ,
    const char* src , int own ) {
  struct ajj_object* obj = ajj_object_create(a,&(a->gc_root));
  return ajj_object_jinja(a,obj,name,src,own);
}

void
ajj_object_destroy_jinja( struct ajj* a , struct ajj_object* obj ) {
  assert( obj->tp == AJJ_VALUE_JINJA );
  func_table_destroy(a,obj->val.obj.fn_tb);
  free((void*)obj->val.obj.src);
  slab_free(&(a->obj_slab),obj);
}

struct ajj_object*
ajj_object_create_list( struct ajj* a , struct gc_scope* scp ) {
  void* udata;
  int tp;
  assert(a->list);
  CHECK(a->list->ctor( a,
      a->list->udata,
      NULL,0,
      &udata,
      &tp) == AJJ_EXEC_OK);
  return ajj_object_create_obj(a,scp,
      a->list,udata,tp);
}

void
list_push( struct ajj* a, struct ajj_object* obj,
    const struct ajj_value* val ) {
  struct object* o;
  struct ajj_value v_obj = ajj_value_assign(obj);
  o = &(obj->val.obj);
  assert( o->fn_tb->slot.attr_push );
  o->fn_tb->slot.attr_push(a,&v_obj,val);
}

struct ajj_value
list_index( struct ajj* a, struct ajj_object* obj,
    int index ) {
  struct object* o;
  struct ajj_value i;
  struct ajj_value v_obj = ajj_value_assign(obj);
  o = &(obj->val.obj);
  assert( o->fn_tb->slot.attr_get );
  i = ajj_value_number(index);
  return o->fn_tb->slot.attr_get(a,&v_obj,&i);
}

struct ajj_object*
ajj_object_create_loop( struct ajj* a, struct gc_scope* scp ,
    size_t len) {
  void* udata;
  int tp;
  struct ajj_value arg;
  assert(a->loop);

  arg = ajj_value_number(len);

  CHECK(a->loop->ctor(a,
        a->loop->udata,
        &arg,
        1,
        &udata,
        &tp) == AJJ_EXEC_OK);
  return ajj_object_create_obj(a,scp,a->loop,udata,tp);
}

struct ajj_object*
ajj_object_create_dict( struct ajj* a , struct gc_scope* scp ) {
  void* udata;
  int tp;
  assert(a->dict);
  CHECK(a->dict->ctor(a,
        a->dict->udata,
        NULL,0,
        &udata,
        &tp) == AJJ_EXEC_OK);
  return ajj_object_create_obj(a,scp,a->dict,udata,tp);
}

void dict_insert( struct ajj* a , struct ajj_object* obj,
    const struct ajj_value* key, const struct ajj_value* val ) {
  struct object* o = &(obj->val.obj);
  struct ajj_value v_obj = ajj_value_assign(obj);
  assert( o->fn_tb->slot.attr_set );
  o->fn_tb->slot.attr_set(a,&v_obj,key,val);
}

struct ajj_value
dict_find( struct ajj* a, struct ajj_object* obj,
    const struct ajj_value* key ) {
  struct object* o = &(obj->val.obj);
  struct ajj_value v_obj = ajj_value_assign(obj);
  assert( o->fn_tb->slot.attr_get );
  return o->fn_tb->slot.attr_get(a,&v_obj,key);
}

/* Value */
void
ajj_value_delete_string( struct ajj* a, struct ajj_value* str ) {
  assert(str->type == AJJ_VALUE_STRING);
  if(str->value.object->tp == AJJ_VALUE_STRING) {
    string_destroy(&(str->value.object->val.str));
  }
  /* remove it from the linked list */
  LREMOVE(str->value.object);
  /* delete the slot also */
  slab_free(&(a->obj_slab),str->value.object);
  /* reset value */
  str->type = AJJ_VALUE_NOT_USE;
}

struct ajj_object*
ajj_object_move( struct ajj* a,
    struct gc_scope* scp , struct ajj_object* obj ) {
  assert(obj->scp);
  /* only do move when we fonud out that the target scope has smaller
   * scp_id value since this means we have less lifecycle */
  if( obj->scp->scp_id > scp->scp_id ) {
    LREMOVE(obj);
    LINSERT(obj,&(scp->gc_tail));
    obj->scp = scp;
    /* Now propogate the move operation into the object's internal
     * states */
    if(obj->tp != AJJ_VALUE_STRING) {
      if(obj->val.obj.fn_tb->slot.move) {
        struct ajj_value objv = ajj_value_assign(obj);
        /* Notify the object to move all its children object */
        obj->val.obj.fn_tb->slot.move(a,&objv);
      }
    }
  }
  return obj;
}

struct ajj_value
ajj_value_move( struct ajj* a,
    struct gc_scope* scp, const struct ajj_value* val ) {
  if(AJJ_IS_PRIMITIVE(val)) {
    return *val;
  } else {
    ajj_object_move(a,scp,val->value.object);
    return *val;
  }
}
