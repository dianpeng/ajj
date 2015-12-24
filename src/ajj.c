#include "ajj-priv.h"
#include "util.h"
#include "lex.h"
#include "parse.h"
#include "builtin.h"
#include "vm.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#define PRETTY_PRINT_INDENT "    "

struct ajj_value AJJ_TRUE = { {1} , AJJ_VALUE_BOOLEAN };
struct ajj_value AJJ_FALSE= { {0} , AJJ_VALUE_BOOLEAN };
struct ajj_value AJJ_NONE = { {0} , AJJ_VALUE_NONE };

struct ajj* ajj_create() {
  struct ajj* r = malloc(sizeof(*r));
  slab_init(&(r->upval_slab),
      UPVALUE_SLAB_SIZE, sizeof(struct upvalue));
  slab_init(&(r->obj_slab),
      OBJECT_SLAB_SIZE, sizeof(struct ajj_object));
  slab_init(&(r->ft_slab),
      FUNCTION_SLAB_TABLE_SIZE,sizeof(struct func_table));
  slab_init(&(r->gc_slab),
      GC_SLAB_SIZE,sizeof(struct gc_scope));
  map_create(&(r->tmpl_tbl),sizeof(struct ajj_object*),32);
  gc_init(&(r->gc_root));
  r->rt = NULL;
  /* initiliaze the upvalue table */
  upvalue_table_init(&(r->builtins),NULL);
  upvalue_table_init(&(r->env),&(r->builtins));
  r->list = NULL;
  r->dict = NULL;
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

struct func_table*
ajj_add_class( struct ajj* a,
    struct upvalue_table* ut,
    const struct ajj_class* cls ) {
  size_t i;
  struct func_table* tb = slab_malloc(&(a->ft_slab));
  struct upvalue* uv;
  struct string n = string_dupc(cls->name);

  /* initialize the function table */
  func_table_init(tb,
      cls->ctor,cls->dtor,
      &(cls->slot),cls->udata,
      &n,0);

  uv = upvalue_table_overwrite(a,ut,&n,1);
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
    const char* name , int type , va_list vl ) {
  struct string n;

  switch(type) {
    case AJJ_VALUE_NUMBER:
      {
        double val = va_arg(vl,double);
        struct upvalue* uv;
        n = string_dupc(name);
        uv = upvalue_table_overwrite(a,ut,&n,1);
        uv->type = UPVALUE_VALUE;
        uv->gut.val = ajj_value_number(val);
        break;
      }
    case AJJ_VALUE_BOOLEAN:
      {
        int val = va_arg(vl,int);
        struct upvalue* uv;
        n = string_dupc(name);
        uv = upvalue_table_overwrite(a,ut,&n,1);
        uv->type = UPVALUE_VALUE;
        uv->gut.val = ajj_value_boolean(val);
        break;
      }
    case AJJ_VALUE_NONE:
      {
        struct upvalue* uv;
        n = string_dupc(name);
        uv = upvalue_table_overwrite(a,ut,&n,1);
        uv->type = UPVALUE_VALUE;
        uv->gut.val = AJJ_NONE;
        break;
      }
    case AJJ_VALUE_STRING:
      {
        struct upvalue* uv;
        const char* str = va_arg(vl,const char*);
        n = string_dupc(name);
        uv = upvalue_table_overwrite(a,ut,&n,1);
        uv->gut.val = ajj_value_assign(
            ajj_object_create_string(a,
              &(a->gc_root),str,strlen(str),0));
        break;
      }
    case AJJ_VALUE_OBJECT:
      {
        struct upvalue* uv;
        struct ajj_value* val = va_arg(vl,struct ajj_value*);
        n = string_dupc(name);
        uv = upvalue_table_overwrite(a,ut,&n,1);
        uv->gut.val = ajj_value_move(&(a->gc_root),val);
        break;
      }
    default:
      UNREACHABLE();
  }
}

void ajj_add_value( struct ajj* a, struct upvalue_table* ut,
    const char* name, int type, ... ) {
  va_list vl;
  va_start(vl,type);
  ajj_add_vvalue(a,ut,name,type,vl);
}

const
struct function*
ajj_add_function( struct ajj* a, struct upvalue_table* ut,
    const char* name,
    ajj_function entry,
    void* udata ) {
  struct string n = string_dupc(name);
  struct upvalue* uv = upvalue_table_add(a,ut,&n,1);
  uv->type = UPVALUE_FUNCTION;
  uv->gut.gfunc.f.c_fn.udata = udata;
  uv->gut.gfunc.f.c_fn.func = entry;
  uv->gut.gfunc.name = n; /* weak */
  return &(uv->gut.gfunc);
}

/* ==================================================
 * Registeration
 * ================================================*/
void ajj_env_add_value( struct ajj* a, const char* name,
    int type, ... ) {
  va_list vl;
  va_start(vl,type);
  ajj_add_vvalue(a,&(a->env),name,type,vl);
}

void ajj_env_add_class( struct ajj* a, const struct ajj_class* cls ) {
  ajj_add_class(a,&(a->env),cls);
}

void ajj_env_add_function( struct ajj* a, const char* name,
    ajj_function entry,
    void* udata ) {
  ajj_add_function(a,&(a->env),name,entry,udata);
}

int ajj_env_del( struct ajj* a, const char* name ) {
  return upvalue_table_del_c(a,&(a->env),name,a->env.prev);
}

void ajj_upvalue_add_value( struct ajj* a,
    const char* name , int type, ... ) {
  va_list vl;
  assert(a->rt&&a->rt->global);
  va_start(vl,type);
  ajj_add_vvalue(a,a->rt->global,name,type,vl);
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

  ajj_io_init_mem(r,cap);
  return r;
}

void ajj_io_destroy( struct ajj_io* io ) {
  if(io->tp == AJJ_IO_MEM) {
    free(io->out.m.mem);
  }
  free(io);
}

static
int io_mem_vprintf( struct ajj_io* io , const char* fmt , va_list vl ) {
  if( io->out.m.cap == io->out.m.len ) {
    /* resize the memory */
    io->out.m.mem = mem_grow(io->out.m.mem,
        sizeof(char),0,&(io->out.m.len));
  }
  do {
    int ret = vsnprintf(
        io->out.m.mem+io->out.m.len,
        io->out.m.cap-io->out.m.len,
        fmt,
        vl);
    if( ret == io->out.m.cap-io->out.m.len ) {
      /* resize the memory again */
      io->out.m.mem = mem_grow(io->out.m.mem,
          sizeof(char),0,&(io->out.m.len));
    } else {
      if(ret >=0) io->out.m.len += ret;
      return ret;
    }
  } while(1);
}

static
void io_mem_write( struct ajj_io* io ,const void* mem , size_t len ) {
  if( io->out.m.cap < io->out.m.len + len ) {
    io->out.m.mem = mem_grow(io->out.m.mem,
        sizeof(char),
        len,
        &(io->out.m.cap));
  }
  memcpy(io->out.m.mem+io->out.m.len,mem,len);
  io->out.m.len += len;
}

static
int io_file_vprintf( struct ajj_io* io , const char* fmt , va_list vl ) {
  return vfprintf(io->out.f,fmt,vl);
}

static
void io_file_write( struct ajj_io* io , const void* mem , size_t len ) {
  int ret = fwrite(mem,sizeof(char),len,io->out.f);
}

int ajj_io_printf( struct ajj_io* io , const char* fmt , ... ) {
  va_list vl;
  va_start(vl,fmt);
  if(io->tp == AJJ_IO_FILE) {
    return io_file_vprintf(io,fmt,vl);
  } else {
    return io_mem_vprintf(io,fmt,vl);
  }
}

int ajj_io_vprintf( struct ajj_io* io , const char* fmt , va_list vl ) {
  if(io->tp == AJJ_IO_FILE) {
    return io_file_vprintf(io,fmt,vl);
  } else {
    return io_mem_vprintf(io,fmt,vl);
  }
}

int ajj_io_write( struct ajj_io* io , const void* mem , size_t len ) {
  if(io->tp == AJJ_IO_FILE) {
    io_file_write(io,mem,len);
  } else {
    io_mem_write(io,mem,len);
  }
  return (int)len;
}

void ajj_io_flush( struct ajj_io* io ) {
  if(io->tp == AJJ_IO_FILE)
    fflush(io->out.f);
}

/* =======================================
 * MISC
 * =====================================*/
const char*
ajj_value_get_type_name( const struct ajj_value* val ) {
  switch(val->type) {
    case AJJ_VALUE_NONE:
      return "<None>";
    case AJJ_VALUE_BOOLEAN:
      return "<Boolean>";
    case AJJ_VALUE_NUMBER:
      return "<Number>";
    case AJJ_VALUE_STRING:
      return "<String>";
    case AJJ_VALUE_OBJECT:
      return GET_OBJECT_TYPE_NAME(
          val->value.object)->str;
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

int ajj_value_eq( struct ajj* a,
    const struct ajj_value* l,
    const struct ajj_value* r,
    int* result ) {
  if(l->type == AJJ_VALUE_NONE) {
    if(r->type == AJJ_VALUE_NONE) {
      *result = 1; return 0;
    } else {
      *result = 0; return 0;
    }
  } else {
    DEFINE_CMP_BODY(l,r,result,==,eq);
  }
}

/* DO NOT implement ne as !eq , if we do this all the
 * __ne__ operator will never be executed . We don' try
 * to interpret the __ne__ as a less than in mathmatic
 * world but just a hint for calling a speicific function.
 */
int ajj_value_ne( struct ajj* a,
    const struct ajj_value* l,
    const struct ajj_value* r,
    int* result ) {
  if(l->type == AJJ_VALUE_NONE) {
    if(r->type == AJJ_VALUE_NONE) {
      *result = 1; return 0;
    } else {
      *result = 0; return 0;
    }
  } else {
    DEFINE_CMP_BODY(l,r,result,!=,ne);
  }
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
        *result = strstr(ajj_value_to_cstr(l),
              ajj_value_to_cstr(r)) != NULL;
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
    const struct ajj_value* val ,int* result ) {
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
      *len = NONE_STRING.len;
      return NONE_STRING.str;
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

void* ajj_load_file( struct ajj* a , const char* fname ,
    size_t* size) {
  FILE* f = fopen(fname,"r");
  long int start;
  long int end;
  size_t len;
  size_t rsz;
  char* r;

  if(!f) return NULL;
  start = ftell(f);
  fseek(f,0,SEEK_END); /* not portable for SEEK_END can be
                        * meaningless */
  end = ftell(f);
  fseek(f,0,SEEK_SET);
  len = (size_t)(end-start);
  r = malloc(len+1);
  rsz = fread(r,1,len,f);
  assert( rsz <= len );
  r[rsz] = 0;
  *size = rsz;
  fclose(f);
  return r;
}

void ajj_error( struct ajj* a , const char* format , ... ) {
  va_list vl;
  va_start(vl,format);
  vsnprintf(a->err,ERROR_BUFFER_SIZE,format,vl);
}

int ajj_render( struct ajj* a , const char* src ,
    const char* key , struct ajj_io* output ) {
  struct ajj_object* obj = parse(a,key,src,0);
  if(!obj) return -1;
#if 0
  return vm_run_jinja(a,obj,output);
#else
  return -1;
#endif
}
