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
  r->upval_tb = upvalue_table_create(&(r->builtins));
  r->list = NULL;
  r->dict = NULL;
  /* lastly load the builtins into the ajj things */
  ajj_builtin_load(r);
  return r;
}

void ajj_destroy( struct ajj* r ) {
  /* MUST delete upvalue_table at very first */
  upvalue_table_destroy(r,r->upval_tb,&(r->builtins));
  /* Destroy the builtins */
  upvalue_table_clear(r,&(r->builtins));
  r->list = NULL;
  r->dict = NULL;
  /* just exit the scope without deleting this scope
   * since it is not a pointer from the gc_slab */
  gc_scope_exit(r,&(r->gc_root));
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

static
void* ajj_add_class( struct ajj* a, const struct ajj_class* cls,
    struct upvalue_table* ut ) {
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
void* ajj_add_val( struct ajj* a , struct upvalue_table* ut,
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
    case AJJ_VALUE_CLASS:
      {
        const struct ajj_class* cls;
        cls = va_arg(vl,const struct ajj_class*);
        return ajj_add_class(a,cls,ut);
      }
    default:
      UNREACHABLE();
  }
  return NULL;
}

void* ajj_add_value( struct ajj* a , struct upvalue_table* ut,
    const char* name , int type , ... ) {
  va_list vl;
  va_start(vl,type);
  return ajj_add_val(a,ut,name,type,vl);
}

void ajj_env_add_value( struct ajj* a , const char* name ,
    int type , ... ) {
  va_list vl;
  va_start(vl,type);
  ajj_add_val(a,a->upval_tb,name,type,vl);
}

int ajj_env_del_value( struct ajj* a , const char* name ) {
  return upvalue_table_del_c(a,a->upval_tb,name);
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
  if(io->tp == AJJ_IO_FILE) {
    if(io->out.f != stdout &&
       io->out.f != stdin )
      fclose(io->out.f);
  } else {
    free(io->out.m.mem);
  }
  free(io);
}

static
int io_mem_vprintf( struct ajj_io* io , const char* fmt , va_list vl ) {
  if( io->out.m.cap == io->out.m.len ) {
    /* resize the memory */
    io->out.m.mem = mem_grow(io->out.m.mem,sizeof(char),0,&(io->out.m.len));
  }
  do {
    int ret = vsnprintf(io->out.m.mem+io->out.m.len,
        io->out.m.cap-io->out.m.len,
        fmt,
        vl);
    if( ret == io->out.m.cap-io->out.m.len ) {
      /* resize the memory again */
      io->out.m.mem = mem_grow(io->out.m.mem,sizeof(char),0,&(io->out.m.len));
    } else {
      if(ret >=0) io->out.m.len += ret;
      return ret;
    }
  } while(1);
}

static
void io_mem_write( struct ajj_io* io , const void* mem , size_t len ) {
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
      {
        char buf[256];
        int l;
        *own = 1;
        if( is_int(val->value.number) ) {
          l = sprintf(buf,"%d",(int)val->value.number);
        } else {
          l = sprintf(buf,"%f",val->value.number);
        }
        *len = (size_t)l;
        return strdup(buf);
      }
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
