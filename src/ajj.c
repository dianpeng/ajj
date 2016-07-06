#include "ajj-priv.h"
#include "util.h"
#include "lex.h"
#include "parse.h"
#include "builtin.h"
#include "vm.h"
#include "opt.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#define PRETTY_PRINT_INDENT "    "

#define MAX_FILE_SIZE (1024*1024*1024)

struct ajj_value AJJ_TRUE = { {1} , AJJ_VALUE_BOOLEAN };
struct ajj_value AJJ_FALSE= { {0} , AJJ_VALUE_BOOLEAN };
struct ajj_value AJJ_NONE = { {0} , AJJ_VALUE_NONE };

struct ajj* ajj_create( struct ajj_vfs* vfs , void* vfs_udata ) {

  struct ajj* r = malloc(sizeof(*r));
  r->err[0] = 0;

  slab_init(&(r->upval_slab),
      UPVALUE_SLAB_SIZE, sizeof(struct upvalue));
  slab_init(&(r->obj_slab),
      OBJECT_SLAB_SIZE, sizeof(struct ajj_object));
  slab_init(&(r->ft_slab),
      FUNCTION_SLAB_TABLE_SIZE,sizeof(struct func_table));
  slab_init(&(r->gc_slab),
      GC_SLAB_SIZE,sizeof(struct gc_scope));
  map_create(&(r->tmpl_tbl),sizeof(struct ajj_object*),32);
  gc_root_init(&(r->gc_root),1);
  r->rt = NULL;
  /* initiliaze the upvalue table */
  upvalue_table_init(&(r->builtins),NULL);
  upvalue_table_init(&(r->env),&(r->builtins));
  r->list = NULL;
  r->dict = NULL;
  r->loop = NULL;
  r->udata = NULL;

  assert(vfs);
  r->vfs = *vfs;
  r->vfs_udata = vfs_udata;

  /* NOTE: */
  /* lastly load the builtins into the ajj things */
  ajj_builtin_load(r);
  return r;
}

void ajj_destroy( struct ajj* r ) {
  /* clear the env and builtin table */
  upvalue_table_clear(r,&(r->env));
  upvalue_table_clear(r,&(r->builtins));
  r->list = NULL;
  r->dict = NULL;
  /* just exit the scope without deleting this scope
   * since it is not a pointer from the gc_slab */
  gc_scope_exit(r,&(r->gc_root));
  /* Now destroy rest of the data structure */
  map_destroy(&(r->tmpl_tbl));
  slab_destroy(&(r->upval_slab));
  slab_destroy(&(r->obj_slab));
  slab_destroy(&(r->ft_slab));
  slab_destroy(&(r->gc_slab));
  free(r);
}

struct gc_scope*
ajj_cur_gc_scope( struct ajj* a ) {
  if(a->rt)
    return a->rt->cur_gc;
  else
    return &(a->gc_root);
}

/* =============================
 * Template
 * ===========================*/
struct jj_file*
ajj_find_template( struct ajj* a , const char* name ) {
  struct jj_file** ret = map_find_c(&(a->tmpl_tbl),name);
  return ret == NULL ? NULL : *ret;
}

struct ajj_object*
ajj_new_template( struct ajj* a ,const char* name ,
    const char* src , int own , time_t ts ) {
  struct jj_file f;
  f.tmpl = ajj_object_create_jinja(a,name,src,own);
  f.ts = ts;
  CHECK(!map_insert_c(&(a->tmpl_tbl),name,&f));
  return f.tmpl;
}

int ajj_delete_template( struct ajj* a, const char* name ) {
  struct ajj_object* obj;
  if( map_remove_c(&(a->tmpl_tbl),name,&obj))
    return -1;
  LREMOVE(obj); /* remove it from gc scope */
  ajj_object_destroy_jinja(a,obj); /* destroy internal gut */
  slab_free(&(a->obj_slab),obj); /* free object back to slab */
  return 0;
}

/* =============================
 * VALUE
 * ===========================*/

const char*
ajj_value_get_type_name( const struct ajj_value* val ) {
  switch(val->type) {
    case AJJ_VALUE_NONE:
      return "none";
    case AJJ_VALUE_BOOLEAN:
      return "boolean";
    case AJJ_VALUE_NUMBER:
      return "number";
    case AJJ_VALUE_STRING:
      return "string";
    case AJJ_VALUE_OBJECT:
      return GET_OBJECT_TYPE_NAME(
          val->value.object)->str;
    default:
      UNREACHABLE();
      return NULL;
  }
}

struct ajj_value ajj_value_number( double val ) {
  struct ajj_value value;
  value.type = AJJ_VALUE_NUMBER;
  value.value.number = val;
  return value;
}

struct ajj_value
ajj_value_boolean( int boolean ) {
  struct ajj_value ret;
  assert(boolean ==0 || boolean ==1);
  ret.type = AJJ_VALUE_BOOLEAN;
  ret.value.boolean = boolean;
  return ret;
}

struct ajj_value
ajj_value_new_string( struct ajj* a,
    const char* str, size_t len ) {
  return ajj_value_assign(
      ajj_object_create_string(a,
        ajj_cur_gc_scope(a),
        str,len,0));
}

struct ajj_value
ajj_value_new_const_string( struct ajj* a,
    const char* str, size_t len ) {
  struct string s;
  s.str = str;
  s.len = len;
  return ajj_value_assign(
    ajj_object_create_const_string(a,
        ajj_cur_gc_scope(a),&s));
}

const char*
ajj_value_to_str( const struct ajj_value* val ,
    size_t* len ) {
  assert(val->type == AJJ_VALUE_STRING);
  *len = val->value.object->val.str.len;
  return val->value.object->val.str.str;
}

int ajj_value_call_object_method( struct ajj* a,
    struct ajj_value* obj,
    const char* name,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret ) {
  struct function* f; /* function */
  struct string n;
  ajj_method m;
  n.str = name; n.len = strlen(name);
  f = func_table_find_func(
      obj->value.object->val.obj.fn_tb,
      &n);
  if(f == NULL) {
    /* We don't have such function gets called */
    ajj_error(a,"Cannot find function:%s in object:%s",
        name,
        GET_OBJECT_TYPE_NAME(obj->value.object)->str);
    return AJJ_EXEC_FAIL;
  }
  assert( IS_CMETHOD(f) );
  m = GET_CMETHOD(f);
  return m(a,obj,arg,arg_len,ret);
}

struct ajj_value
ajj_value_new_list( struct ajj* a ) {
  return ajj_value_assign(
      ajj_object_create_list(a,
        ajj_cur_gc_scope(a)));
}

void ajj_value_list_push( struct ajj* a, struct ajj_value* obj,
    struct ajj_value* val ) {
  assert(obj->type == AJJ_VALUE_OBJECT);
  builtin_list_push(a,obj->value.object,val);
}

size_t ajj_value_list_size( struct ajj* a, struct ajj_value* obj ) {
  assert(obj->type == AJJ_VALUE_OBJECT);
  return a->list->slot.len(a,obj);
}

struct ajj_value
ajj_value_list_index( struct ajj* a, struct ajj_value* obj ,
    int index ) {
  assert(obj->type == AJJ_VALUE_OBJECT);
  return builtin_list_index(a,obj->value.object,index);
}

void ajj_value_list_clear( struct ajj* a, struct ajj_value* obj ) {
  assert(obj->type == AJJ_VALUE_OBJECT);
  builtin_list_clear(a,obj->value.object);
}

struct ajj_value
ajj_value_new_dict( struct ajj* a ) {
  return ajj_value_assign(
      ajj_object_create_dict(a,
        ajj_cur_gc_scope(a)));
}

void ajj_value_dict_insert( struct ajj* a,
    struct ajj_value* obj,
    struct ajj_value* key,
    struct ajj_value* val ) {
  assert(obj->type == AJJ_VALUE_OBJECT);
  builtin_dict_insert(a,obj->value.object,key,val);
}

struct ajj_value
ajj_value_dict_find( struct ajj* a,
    struct ajj_value* obj,
    struct ajj_value* key ) {
  assert(obj->type == AJJ_VALUE_OBJECT);
  return builtin_dict_find(a,obj->value.object,key);
}

int ajj_value_dict_remove( struct ajj* a,
    struct ajj_value* obj,
    struct ajj_value* key ) {
  return builtin_dict_remove(a,obj->value.object,key);
}

void ajj_value_dict_clear( struct ajj* a,
    struct ajj_value* obj ) {
  assert(obj->type == AJJ_VALUE_OBJECT);
  builtin_dict_clear(a,obj->value.object);
}

#ifndef NDEBUG
/* Slot function sanity check -----------------------------
 * This check routine only works under DEBUG mode which NDEBUG
 * is not defined. It checks whether certain requirement for
 * consistence of the API is matched or not */
static
void check_slot_sanity( const struct ajj_slot* slot ) {
  if(slot->iter_start) {
    assert(slot->iter_has);
    assert(slot->iter_move);
    assert(slot->iter_get_key);
    assert(slot->iter_get_val);
    assert(slot->len);
    assert(slot->empty);
  }
  if(slot->eq || slot->ne) {
    assert(slot->eq);
    assert(slot->ne);
  }
}

#define CHECK_SLOT_SANITY(SLOT) check_slot_sanity(SLOT)
#else
#define CHECK_SLOT_SANITY(SLOT) (void)(SLOT)
#endif /* NDEBUG */


struct func_table*
ajj_add_class( struct ajj* a,
    struct upvalue_table* ut,
    const struct ajj_class* cls) {
  size_t i;
  struct func_table* tb = slab_malloc(&(a->ft_slab));
  struct upvalue* uv;
  struct string n = string_dupc(cls->name);

  CHECK_SLOT_SANITY(&(cls->slot));

  /* initialize the function table */
  func_table_init(tb,
      cls->ctor,cls->dtor,
      &(cls->slot),cls->udata,
      &n,0);

  uv = upvalue_table_overwrite(a,ut,&n,1,0);
  uv->type = UPVALUE_FUNCTION;
  uv->gut.gfunc.tp = OBJECT_CTOR;
  uv->gut.gfunc.f.obj_ctor = tb; /* owned by this slot, it will be deleted
                                  * by upvalue destroy function */
  uv->gut.gfunc.name = tb->name;

  for( i = 0 ; i < cls->mem_func_len ; ++i ) {
    struct string fn;
    fn.str = cls->mem_func[i].name;
    fn.len = strlen(cls->mem_func[i].name);
    *func_table_add_c_method(tb,&fn,0) =
      cls->mem_func[i].method;
  }
  return tb;
}

static
void ajj_add_vvalue( struct ajj* a , struct upvalue_table* ut,
    struct gc_scope* scp,
    const char* name ,
    int type ,
    va_list vl ) {
  struct string n;

  switch(type) {
    case AJJ_VALUE_NUMBER:
      {
        double val = va_arg(vl,double);
        struct upvalue* uv;
        n = string_dupc(name);
        uv = upvalue_table_overwrite(a,ut,&n,1,1);
        uv->type = UPVALUE_VALUE;
        uv->gut.val = ajj_value_number(val);
        break;
      }
    case AJJ_VALUE_BOOLEAN:
      {
        int val = va_arg(vl,int);
        struct upvalue* uv;
        n = string_dupc(name);
        uv = upvalue_table_overwrite(a,ut,&n,1,1);
        uv->type = UPVALUE_VALUE;
        uv->gut.val = ajj_value_boolean(val);
        break;
      }
    case AJJ_VALUE_NONE:
      {
        struct upvalue* uv;
        n = string_dupc(name);
        uv = upvalue_table_overwrite(a,ut,&n,1,1);
        uv->type = UPVALUE_VALUE;
        uv->gut.val = AJJ_NONE;
        break;
      }
    case AJJ_VALUE_STRING:
      {
        struct upvalue* uv;
        const char* str = va_arg(vl,const char*);
        size_t str_l= va_arg(vl,size_t);
        n = string_dupc(name);
        uv = upvalue_table_overwrite(a,ut,&n,1,1);
        uv->gut.val = ajj_value_assign(
            ajj_object_create_string(a,scp,str,str_l,0));
        break;
      }
    case AJJ_VALUE_OBJECT:
      {
        struct upvalue* uv;
        struct ajj_value* val = va_arg(vl,struct ajj_value*);
        n = string_dupc(name);
        uv = upvalue_table_overwrite(a,ut,&n,1,1);
        uv->gut.val = ajj_value_move_scope(a,scp,val);
        break;
      }
    default:
      UNREACHABLE();
      break;
  }
}

void ajj_add_value( struct ajj* a,
    struct upvalue_table* ut,
    const char* name,
    int type, ... ) {
  va_list vl;
  va_start(vl,type);
  ajj_add_vvalue(a,ut,&(a->gc_root),name,type,vl);
}

const struct function*
ajj_add_function( struct ajj* a, struct upvalue_table* ut,
    const char* name,
    ajj_function entry,
    void* udata ) {
  struct string n = string_dupc(name);
  struct upvalue* uv = upvalue_table_overwrite(a,ut,&n,1,0);
  uv->type = UPVALUE_FUNCTION;
  uv->gut.gfunc.f.c_fn.udata = udata;
  uv->gut.gfunc.f.c_fn.func = entry;
  uv->gut.gfunc.name = n; /* weak */
  uv->gut.gfunc.tp = C_FUNCTION;
  return &(uv->gut.gfunc);
}

const struct function*
ajj_add_test( struct ajj* a, struct upvalue_table* ut,
    const char* name,
    ajj_function entry,
    void* udata ) {
  struct string n = string_dupc(name);
  struct upvalue* uv = upvalue_table_overwrite(a,ut,&n,1,1);
  uv->type = UPVALUE_FUNCTION;
  uv->gut.gfunc.f.c_fn.udata = udata;
  uv->gut.gfunc.f.c_fn.func = entry;
  uv->gut.gfunc.name = n;
  uv->gut.gfunc.tp = C_TEST;
  return &(uv->gut.gfunc);
}

/* ==================================================
 * Registeration
 * ================================================*/
void ajj_env_add_value( struct ajj* a, const char* name,
    int type, ... ) {
  va_list vl;
  va_start(vl,type);
  ajj_add_vvalue(a,&(a->env),&(a->gc_root),name,type,vl);
}

void ajj_env_add_class( struct ajj* a, const struct ajj_class* cls ) {
  ajj_add_class(a,&(a->env),cls);
}

void ajj_env_add_function( struct ajj* a, const char* name,
    ajj_function entry,
    void* udata ) {
  ajj_add_function(a,&(a->env),name,entry,udata);
}

int ajj_env_has( struct ajj* a, const char* name ) {
  return upvalue_table_find_c(&(a->env),name,NULL) != NULL;
}

int ajj_env_del( struct ajj* a, const char* name ) {
  return upvalue_table_del_c(a,&(a->env),name,a->env.prev);
}

/* upvalue */

/* This function uses upvalue_table_add instead of upvalue_table_overwrite
 * because it is supposed to be used by user when they are in the VM
 * execution ! */
void ajj_upvalue_add_value( struct ajj* a,
    const char* name , int type, ... ) {
  va_list vl;
  struct string n;
  assert(a->rt&&a->rt->global);
  va_start(vl,type);
  switch(type) {
    case AJJ_VALUE_NUMBER:
      {
        double val = va_arg(vl,double);
        struct upvalue* uv;
        n = string_dupc(name);
        uv = upvalue_table_add(a,a->rt->global,&n,1,0,0);
        uv->type = UPVALUE_VALUE;
        uv->gut.val = ajj_value_number(val);
        break;
      }
    case AJJ_VALUE_BOOLEAN:
      {
        int val = va_arg(vl,int);
        struct upvalue* uv;
        n = string_dupc(name);
        uv = upvalue_table_add(a,a->rt->global,&n,1,0,0);
        uv->type = UPVALUE_VALUE;
        uv->gut.val = ajj_value_boolean(val);
        break;
      }
    case AJJ_VALUE_NONE:
      {
        struct upvalue* uv;
        n = string_dupc(name);
        uv = upvalue_table_add(a,a->rt->global,&n,1,0,0);
        uv->type = UPVALUE_VALUE;
        uv->gut.val = AJJ_NONE;
        break;
      }
    case AJJ_VALUE_STRING:
      {
        struct upvalue* uv;
        const char* str = va_arg(vl,const char*);
        size_t str_l = va_arg(vl,size_t);
        n = string_dupc(name);
        uv = upvalue_table_add(a,a->rt->global,&n,1,0,0);
        uv->gut.val = ajj_value_assign(
            ajj_object_create_string(a,a->rt->cur_gc,str,str_l,0));
        break;
      }
    case AJJ_VALUE_OBJECT:
      {
        struct upvalue* uv;
        struct ajj_value* val = va_arg(vl,struct ajj_value*);
        n = string_dupc(name);
        uv = upvalue_table_add(a,a->rt->global,&n,1,0,0);
        uv->gut.val = ajj_value_move_scope(a,a->rt->cur_gc,val);
        break;
      }
    default:
      UNREACHABLE();
      break;
  }
}

void ajj_upvalue_add_class( struct ajj* a,
    const struct ajj_class* cls ) {
  assert(a->rt&&a->rt->global);
  ajj_add_class(a,a->rt->global,cls);
}

void ajj_upvalue_add_function( struct ajj* a,
    const char* name ,
    ajj_function entry,
    void* udata ) {
  assert( a->rt && a->rt->global );
  ajj_add_function(a,a->rt->global,name,entry,udata);
}

int ajj_upvalue_has( struct ajj* a, const char* name ) {
  return upvalue_table_find_c(a->rt->global,name,NULL) != 0;
}

void ajj_upvalue_del( struct ajj* a, const char* name ) {
  assert( a->rt && a->rt->global );
  upvalue_table_del_c(a,a->rt->global,name,&(a->env));
}

void ajj_env_clear( struct ajj* a ) {
  upvalue_table_clear(a,&(a->env));
}

/* =======================================================
 * IO
 * =====================================================*/

static
void ajj_io_init_file( struct ajj_io* io , FILE* f ) {
  assert(f);
  io->tp = AJJ_IO_FILE;
  io->out.f = f;
}

struct ajj_io*
ajj_io_create_file( struct ajj* a , FILE* f ) {
  struct ajj_io* r = malloc(sizeof(*r));
  UNUSE_ARG(a);
  ajj_io_init_file(r,f);
  return r;
}

struct ajj_io*
ajj_io_create_mem( struct ajj* a , size_t cap ) {
  struct ajj_io* r = malloc(sizeof(*r));
  UNUSE_ARG(a);
  strbuf_init(&(r->out.m));
  r->tp = AJJ_IO_MEM;
  return r;
}

void ajj_io_destroy( struct ajj* a , struct ajj_io* io ) {
  UNUSE_ARG(a);
  if(io->tp == AJJ_IO_MEM) {
    strbuf_destroy(&(io->out.m));
  }
  free(io);
}

static
int io_file_vprintf( struct ajj_io* io , const char* fmt , va_list vl ) {
  return vfprintf(io->out.f,fmt,vl);
}

static
void io_file_write( struct ajj_io* io , const void* mem , size_t len ) {
  fwrite(mem,sizeof(char),len,io->out.f);
}

int ajj_io_printf( struct ajj_io* io , const char* fmt , ... ) {
  va_list vl;
  va_start(vl,fmt);
  if(io->tp == AJJ_IO_FILE) {
    return io_file_vprintf(io,fmt,vl);
  } else {
    return strbuf_vprintf(&(io->out.m),fmt,vl);
  }
}

int ajj_io_vprintf( struct ajj_io* io , const char* fmt , va_list vl ) {
  if(io->tp == AJJ_IO_FILE) {
    return io_file_vprintf(io,fmt,vl);
  } else {
    return strbuf_vprintf(&(io->out.m),fmt,vl);
  }
}

int ajj_io_write( struct ajj_io* io , const void* mem , size_t len ) {
  if(io->tp == AJJ_IO_FILE) {
    io_file_write(io,mem,len);
  } else {
    strbuf_append(&(io->out.m),(const char*)mem,len);
  }
  return (int)len;
}

void ajj_io_flush( struct ajj_io* io ) {
  if(io->tp == AJJ_IO_FILE)
    fflush(io->out.f);
}

void* ajj_io_get_content( struct ajj_io* io , size_t* size ) {
  if( io->tp == AJJ_IO_FILE ) {
    *size = 0;
    return NULL;
  } else {
    *size = io->out.m.len;
    return io->out.m.str;
  }
}

void* ajj_io_detach( struct ajj_io* io , size_t* size ) {
  if( io->tp == AJJ_IO_FILE ) {
    *size = 0;
    return NULL;
  } else {
    return strbuf_detach(&(io->out.m),size,NULL);
  }
}

/* =======================================
 * Slot
 * =====================================*/
int ajj_value_attr_get( struct ajj* a,
    const struct ajj_value* obj,
    const struct ajj_value* key,
    struct ajj_value* ret ) {
  if( obj->type != AJJ_VALUE_OBJECT &&
      obj->type != AJJ_VALUE_STRING ) {
    ajj_error(a,"Cannot get attributes on type:%s which is not an "
        "object or string!",ajj_value_get_type_name(obj));
    return -1;
  } else {
    if( obj->type == AJJ_VALUE_OBJECT ) {
      struct object* o = &(obj->value.object->val.obj);
      if( o->fn_tb->slot.attr_get == NULL ) {
        ajj_error(a,"Type:%s cannot support attribute get operation!",
            o->fn_tb->name.str);
        return -1;
      } else {
        *ret = o->fn_tb->slot.attr_get(a,obj,key);
        return 0;
      }
    } else {
      int k = 0;
      if( vm_to_integer(key,&k) ) {
        *ret = AJJ_NONE; return 0;
      } else {
        struct string* str = &(obj->value.object->val.str);
        if(str->len <= (size_t)k) {
          *ret = AJJ_NONE;
          return 0;
        } else {
          struct string buf;
          buf.str = const_cstr( str->str[k] );
          buf.len = 1;
          *ret = ajj_value_assign(
              ajj_object_create_const_string(
                a,
                ajj_cur_gc_scope(a),
                &buf)
              );
          return 0;
        }
      }
    }
  }
}

int ajj_value_attr_set( struct ajj* a,
    struct ajj_value* obj,
    const struct ajj_value* key,
    const struct ajj_value* val ) {
  if( obj->type != AJJ_VALUE_OBJECT ) {
    ajj_error(a,"Cannot set attributes on type:%s "
        "which is not an object!",ajj_value_get_type_name(obj));
    return -1;
  } else {
    struct object* o = &(obj->value.object->val.obj);
    if( o->fn_tb->slot.attr_set == NULL ) {
      ajj_error(a,"Type:%s cannot support attribute set operation!",
          o->fn_tb->name.str);
      return -1;
    } else {
      /* invoke the attributes set operation */
      o->fn_tb->slot.attr_set(a,obj,key,val);
      return 0;
    }
  }
}

int ajj_value_attr_push( struct ajj* a,
    struct ajj_value* obj,
    const struct ajj_value* val ) {
  if( obj->type != AJJ_VALUE_OBJECT ) {
    ajj_error(a,"Cannot stk_push attributes on type:%s which is not an "
        "object!",ajj_value_get_type_name(obj));
    return -1;
  } else {
    struct object* o = &(obj->value.object->val.obj);
    if( o->fn_tb->slot.attr_push == NULL ) {
      ajj_error(a,"Type:%s cannot support attribute stk_push operation!",
          o->fn_tb->name.str);
      return -1;
    } else {
      o->fn_tb->slot.attr_push(a,obj,val);
      return 0;
    }
  }
}

struct ajj_value ajj_value_move( struct ajj* a,
    const struct ajj_value* self,
    struct ajj_value* tar ) {
  if( self->type == AJJ_VALUE_OBJECT ||
      self->type == AJJ_VALUE_STRING ){
    ajj_value_move_scope(a,
        self->value.object->scp,
        tar);
  }
  return *tar;
}

int ajj_value_iter_start( struct ajj* a,
    const struct ajj_value* obj,
    int* itr ) {
  /* Here we will implicitly support iterator on top of the
   * string. Because string is not an object, we could not
   * use the default method to handle iterator for string */
  if( obj->type == AJJ_VALUE_STRING ) {
    *itr = 0; return 0;
  } else if( obj->type == AJJ_VALUE_OBJECT ) {
    struct object* o = &(obj->value.object->val.obj);
    if( o->fn_tb->slot.iter_start ) {
      *itr = o->fn_tb->slot.iter_start(a,obj);
      return 0;
    } else {
      ajj_error(a,"Object:%s doesn't support iterator!",
          o->fn_tb->name.str);
      return -1;
    }
  } else {
    ajj_error(a,"Type:%s doesn't support iterator!",
        ajj_value_get_type_name(obj));
    return -1;
  }
}

int ajj_value_iter_has( struct ajj* a,
    const struct ajj_value* obj,
    int itr , int* result ) {
  if( obj->type == AJJ_VALUE_STRING ) {
    *result = itr < (int)(obj->value.object->val.str.len);
    return 0;
  } else if( obj->type == AJJ_VALUE_OBJECT ) {
    struct object* o = &(obj->value.object->val.obj);
    assert( o->fn_tb->slot.iter_has );
    *result = o->fn_tb->slot.iter_has(a,obj,itr);
    return 0;
  } else {
    ajj_error(a,"Type:%s doesn't support iterator!",
        ajj_value_get_type_name(obj));
    return -1;
  }
}

int ajj_value_iter_move( struct ajj* a,
    const struct ajj_value* obj,
    int itr , int* result ) {
  if( obj->type == AJJ_VALUE_STRING ) {
    *result = itr+1;
    return 0;
  } else if( obj->type == AJJ_VALUE_OBJECT ) {
    struct object* o = &(obj->value.object->val.obj);
    assert( o->fn_tb->slot.iter_move );
    *result = o->fn_tb->slot.iter_move(a,obj,itr);
    return 0;
  } else {
    ajj_error(a,"Type:%s doesn't support iterator!",
        ajj_value_get_type_name(obj));
    return -1;
  }
}

int ajj_value_iter_get_key( struct ajj* a,
    const struct ajj_value* obj,
    int itr,
    struct ajj_value* key ) {
  if( obj->type == AJJ_VALUE_STRING ) {
    *key = ajj_value_number(itr);
    return 0;
  } else if( obj->type == AJJ_VALUE_OBJECT ) {
    struct object* o = &(obj->value.object->val.obj);
    assert( o->fn_tb->slot.iter_get_key );
    *key = o->fn_tb->slot.iter_get_key(a,obj,itr);
    return 0;
  } else {
    ajj_error(a,"Type:%s doesn't support iterator!",
        ajj_value_get_type_name(obj));
    return -1;
  }
}

int ajj_value_iter_get_val( struct ajj* a,
    const struct ajj_value* obj,
    int itr,
    struct ajj_value* val ) {
  if( obj->type == AJJ_VALUE_STRING ) {
    struct string buf;
    struct string* str = ajj_value_to_string(obj);
    assert( (size_t)itr < str->len );
    buf.str = const_cstr( str->str[itr] );
    buf.len = 1;
    *val = ajj_value_assign(
        ajj_object_create_const_string(
          a,
          ajj_cur_gc_scope(a),
          &buf)
        );
    return 0;
  } else if( obj->type == AJJ_VALUE_OBJECT ) {
    struct object* o = &(obj->value.object->val.obj);
    assert( o->fn_tb->slot.iter_get_val );
    *val = o->fn_tb->slot.iter_get_val(a,
        obj,itr);
    return 0;
  } else {
    ajj_error(a,"Type:%s doesn't support iterator!",
        ajj_value_get_type_name(obj));
    return -1;
  }
}

/* comparison */
#define DEFINE_CMP_BODY(L,R,RES,OP,T) \
  do { \
    if((L)->type == AJJ_VALUE_STRING || \
       (R)->type == AJJ_VALUE_STRING ) { \
      const struct ajj_value* str_val; \
      const struct ajj_value* nstr_val; \
      int own; \
      struct string str; \
      if((L)->type == AJJ_VALUE_STRING) { \
        str_val = (L); nstr_val = (R); \
      } else { \
        str_val = (R); nstr_val = (L); \
      } \
      if( vm_to_string(nstr_val,&str,&own) ) { \
        ajj_error(a,"Cannot convert type:%s to string!", \
            ajj_value_get_type_name(nstr_val)); \
        return -1; \
      } \
      *(RES) = string_cmp(ajj_value_to_string(str_val), \
          &str) OP 0; \
      if(own) string_destroy(&str); \
      return 0; \
    } else { \
      if((L)->type == AJJ_VALUE_OBJECT && \
         (R)->type == AJJ_VALUE_OBJECT ) { \
        const struct object* lo = ajj_value_to_obj(L); \
        if( lo->fn_tb->slot.T ) { \
          if(lo->fn_tb->slot.T(a,L,R,RES)) { \
            return -1; \
          } else { \
            return 0; \
          } \
        } else { \
          ajj_error(a,"The object:%s doesn't support operator:%s!", \
              ajj_value_get_type_name(L),#T); \
          return -1; \
        } \
      } else { \
        double lval,rval; \
        if(vm_to_number(L,&lval)) { \
          ajj_error(a,"Cannot convert type:%s to number!", \
              ajj_value_get_type_name(L)); \
          return -1; \
        } \
        if(vm_to_number(R,&rval)) { \
          ajj_error(a,"Cannot convert type:%s to number!", \
              ajj_value_get_type_name(R)); \
          return -1; \
        } \
        *result = lval OP rval; \
        return 0; \
      } \
    } \
  } while(0)

#define DEFINE_NONE_CMP(L,R,RVAL,OP) \
  do { \
    if( (L)->type == AJJ_VALUE_NONE ) { \
      if( (R)->type == AJJ_VALUE_NONE ) { \
        *(RVAL) = OP ? 1 : 0; return 0; \
      } else { \
        *(RVAL) = OP ? 0 : 1; return 0; \
      } \
    } else if( (R)->type == AJJ_VALUE_NONE ) { \
      if( (L)->type == AJJ_VALUE_NONE ) { \
        *(RVAL) = OP ? 1 : 0; return 0; \
      } else { \
        *(RVAL) = OP ? 0 : 1; return 0; \
      } \
    } \
  } while(0)

int ajj_value_eq( struct ajj* a,
    const struct ajj_value* l,
    const struct ajj_value* r,
    int* result ) {
  DEFINE_NONE_CMP(l,r,result,1);
  DEFINE_CMP_BODY(l,r,result,==,eq);
}

/* DO NOT implement ne as !eq , if we do this all the
 * ne operator will never be executed . We don' try
 * to interpret the ne as a less than in mathmatic
 * world but just a hint for calling a speicific function.
 */
int ajj_value_ne( struct ajj* a,
    const struct ajj_value* l,
    const struct ajj_value* r,
    int* result ) {
  DEFINE_NONE_CMP(l,r,result,0);
  DEFINE_CMP_BODY(l,r,result,!=,ne);
}

int ajj_value_lt( struct ajj* a,
    const struct ajj_value* l,
    const struct ajj_value* r,
    int* result ) {
  if( l->type == AJJ_VALUE_NONE ||
      r->type == AJJ_VALUE_NONE ) {
    ajj_error(a,"Cannot use lt(<) between None type!");
    return -1;
  } else {
    DEFINE_CMP_BODY(l,r,result,<,lt);
  }
}

int ajj_value_le( struct ajj* a,
    const struct ajj_value* l,
    const struct ajj_value* r,
    int* result ) {
  if( l->type == AJJ_VALUE_NONE ||
      r->type == AJJ_VALUE_NONE ) {
    ajj_error(a,"Cannot use le(<=) between None type!");
    return -1;
  } else {
    DEFINE_CMP_BODY(l,r,result,<=,le);
  }
}

int ajj_value_gt( struct ajj* a,
    const struct ajj_value* l,
    const struct ajj_value* r,
    int* result ) {
  if( l->type == AJJ_VALUE_NONE ||
      r->type == AJJ_VALUE_NONE ) {
    ajj_error(a,"Cannot use gt(>) between None type!");
    return -1;
  } else {
    DEFINE_CMP_BODY(l,r,result,>,le);
  }
}

int ajj_value_ge( struct ajj* a,
    const struct ajj_value* l,
    const struct ajj_value* r,
    int* result ) {
  if( l->type == AJJ_VALUE_NONE ||
      r->type == AJJ_VALUE_NONE ) {
    ajj_error(a,"Cannot use ge(>=) between None type!");
    return -1;
  } else {
    DEFINE_CMP_BODY(l,r,result,>=,ge);
  }
}

#undef DEFINE_NONE_CMP
#undef DEFINE_CMP_BODY

int ajj_value_in( struct ajj* a,
    const struct ajj_value* l, /* object */
    const struct ajj_value* r, /* target */
    int* result ) {
  switch(l->type) {
    case AJJ_VALUE_NONE:
    case AJJ_VALUE_BOOLEAN:
    case AJJ_VALUE_NUMBER:
      ajj_error(a,"None/Boolean/Number doesn't support in operator!");
      return -1;
    case AJJ_VALUE_STRING:
      if(r->type != AJJ_VALUE_STRING) {
        ajj_error(a,"String can only test in operator with string!");
        return -1;
      } else {
        *result = string_str(ajj_value_to_string(l),
              ajj_value_to_string(r)) != NULL;
        return 0;
      }
    case AJJ_VALUE_OBJECT:
      {
        const struct object* lr = ajj_value_to_obj(l);
        if(lr->fn_tb->slot.in) {
          return lr->fn_tb->slot.in(a,l,r,result);
        } else {
          ajj_error(a,"Type:%s doesn't support in operator!",
              ajj_value_get_type_name(l));
          return -1;
        }
      }
    default:
      UNREACHABLE();
      return -1;
  }
}

int ajj_value_len( struct ajj* a,
    const struct ajj_value* val ,
    size_t* result ) {

  switch(val->type) {
    case AJJ_VALUE_NONE:
      *result = 0;
      return 0;
    case AJJ_VALUE_NUMBER:
    case AJJ_VALUE_BOOLEAN:
      *result = 1;
      return 0;
    case AJJ_VALUE_STRING:
      *result = ajj_value_to_string(val)->len;
      return 0;
    case AJJ_VALUE_OBJECT:
      {
        const struct object* o = ajj_value_to_obj(val);
        if(o->fn_tb->slot.len) {
          *result = o->fn_tb->slot.len(a,val);
          return 0;
        } else {
          ajj_error(a,"Type:%s doesn't support len operator!",
            ajj_value_get_type_name(val));
          return -1;
        }
      }
    default:
      UNREACHABLE();
      return -1;
  }
}

int ajj_value_empty( struct ajj* a,
    const struct ajj_value* val,
    int* result ) {
  switch(val->type) {
    case AJJ_VALUE_NONE:
      *result = 1;
      return 0;
    case AJJ_VALUE_NUMBER:
    case AJJ_VALUE_BOOLEAN:
      *result = 0;
      return 0;
    case AJJ_VALUE_STRING:
      *result = ajj_value_to_string(val)->len == 0 ;
      return 0;
    case AJJ_VALUE_OBJECT:
      {
        const struct object* o = ajj_value_to_obj(val);
        if(o->fn_tb->slot.empty) {
          *result = o->fn_tb->slot.empty(a,val);
          return 0;
        } else {
          ajj_error(a,"Type:%s doesn't support empty operator!",
            ajj_value_get_type_name(val));
          return -1;
        }
      }
    default:
      UNREACHABLE();
      return -1;
  }
}

const char*
ajj_display( struct ajj* a, const struct ajj_value* val,
    size_t* len , int* own ) {
  struct ajj_object* obj;
  switch(val->type) {
    case AJJ_VALUE_STRING:
      obj = val->value.object;
      *own = 0;
      *len = obj->val.str.len;
      return obj->val.str.str;
    case AJJ_VALUE_NONE:
      *own = 0;
      *len = 0;
      return ""; /* empty string, NONE MUST never display
                  * anything correctly or out. This makes
                  * {{ some_function() }} with function returns
                  * None no harm to the output */
    case AJJ_VALUE_BOOLEAN:
      *own = 0;
      if(val->value.boolean) {
        *len = TRUE_STRING.len;
        return TRUE_STRING.str;
      } else {
        *len = FALSE_STRING.len;
        return FALSE_STRING.str;
      }
    case AJJ_VALUE_NUMBER:
      *own = 1;
      return dtoc(val->value.number,len);
    case AJJ_VALUE_OBJECT:
      {
        struct object* o = &(val->value.object->val.obj);
        if(o->fn_tb->slot.display) {
          *own = 1;
          return o->fn_tb->slot.display(a,val,len);
        } else {
          *own = 0;
          *len = EMPTY_STRING.len;
          return EMPTY_STRING.str;
        }
      }
    default:
      UNREACHABLE();
      return NULL;
  }
}

void ajj_error( struct ajj* a , const char* format , ... ) {
  va_list vl;
  va_start(vl,format);
  vsnprintf(a->err,ERROR_BUFFER_SIZE,format,vl);
}

const char* ajj_last_error( struct ajj* a ) {
  return a->err;
}

void ajj_set_udata( struct ajj* a , void* udata ) {
  a->udata = udata;
}

void* ajj_get_udata( struct ajj* a ) {
  return a->udata;
}

void* ajj_runtime_get_udata( struct ajj* a ) {
  if(a->rt) return a->rt->udata;
  return NULL;
}

struct ajj_object*
ajj_parse_template( struct ajj* a , const char* filename ) {
  size_t len;
  const char* src;
  time_t ts;
  struct jj_file* f;
  /* try to load the template directly from existed one */
  f = ajj_find_template(a,filename);
  if(f) {
    int ret = a->vfs.vfs_timestamp_is_current(
        a,filename,f->ts,a->vfs_udata);
    if(ret <0) {
      return NULL; /* failed */
    } else if(ret) {
      /* Hit the cache, so just return this template */
      return f->tmpl;
    } else {
      ts = f->ts;
    }
  }

  /* Either we don't have such file parsed or the timestamp
   * is outdated, so we need to load the whole file into the
   * memory */
  src = a->vfs.vfs_load(a,filename,&len, f ? &ts : NULL,a->vfs_udata);

  if(!f) {
    /* In this case , since we don't know the ts, we do
     * an extra call to retreieve the timestamp */
    int ret = a->vfs.vfs_timestamp(a,filename,&ts,a->vfs_udata);
    if(ret) {
      /* Failed here */
      free((void*)src);
      return NULL;
    }
  }

  if(!src) {
    ajj_error(a,"Cannot load file with name:%s!",filename);
    return NULL;
  } else {

#ifdef DISABLE_OPTIMIZATION
    /* During debugging phase we may not want to switch optimization
     * on for debugging purpose */
    return parse(a,filename,src,1,ts);
#else
    struct ajj_object* ret;
    ret = parse(a,filename,src,1,ts);
    if(!ret) return NULL;
    if(optimize(a,ret)) return NULL;
    return ret;
#endif
  }
}

/* Currently this function is not *safe* in terms of memory since
 * we put all jinja template related memory inside of our global
 * gc scope. If a template fails for rendering , it will be delayed
 * until user try to destroy ajj engine pointer. Suppose we have many
 * jinja template that is malformed or runtime error, then we will
 * end up with no memory ... To solve this problem, we need a major
 * change to our jinja template which I would like a  per template
 * per gc-scope style */

int ajj_render_file( struct ajj* a,
    struct ajj_io* output,
    const char* file ,
    void* udata) {
  struct ajj_object* jinja = ajj_parse_template(a,file);
  if(!jinja) return -1;
  return vm_run_jinja(a,jinja,output,udata);
}

int ajj_render_data( struct ajj* a,
    struct ajj_io* output,
    const char* src,
    const char* key,
    void* udata ) {
  struct ajj_object* jinja = parse(a,key,src,0,0);
  if(!jinja) return -1;
  return vm_run_jinja(a,jinja,output,udata);
}
