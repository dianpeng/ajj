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
  r->upval_tb = upvalue_table_create(NULL);
  r->list = NULL;
  r->dict = NULL;
  upvalue_table_init(&(r->builtins));
  /* lastly load the builtins into the ajj things */
  ajj_builtin_load(r);
  return r;
}

void ajj_destroy( struct ajj* r ) {
  /* MUST delete upvalue_table at very first */
  upvalue_table_destroy(r,r->upval_tb,&(r->builtins));
  ajj_builtin_destroy(r);
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

static
void ajj_env_add_class( struct ajj* a , const char* name ,
    const struct ajj_class* cls ) {
  size_t i;
  struct func_table* tb = slab_malloc(&(a->ft_slab));
  struct upvalue* uv;
  struct string n = string_dupc(name);

  /* initialize the function table */
  func_table_init(tb,
      cls->ctor,cls->dtor,
      &(cls->slot),cls->udata,
      &n,0);

  uv = upvalue_table_overwrite(a,a->upval_tb,&n,1);
  uv->gut.gfunc.tp = OBJECT_CTOR;
  uv->gut.gfunc.f.obj_ctor = tb; /* owned by this slot */

  for( i = 0 ; i < cls->mem_func_len ; ++i ) {
    struct string fn;
    fn.str = cls->mem_func[i].name;
    fn.len = strlen(cls->mem_func[i].name);
    *func_table_add_c_method(tb,&fn,0) = cls->mem_func[i].method;
  }
}

void ajj_env_add_value( struct ajj* a , const char* name , int type , ... ) {
  va_list vl;
  struct string n;

  va_start(vl,type);

  switch(type) {
    case AJJ_VALUE_NUMBER:
      {
        double val = va_arg(vl,double);
        struct upvalue* uv;
        n = string_dupc(name);
        uv = upvalue_table_overwrite(a,a->upval_tb,&n,1);
        uv->type = UPVALUE_VALUE;
        uv->gut.val = ajj_value_number(val);
        break;
      }
    case AJJ_VALUE_BOOLEAN:
      {
        int val = va_arg(vl,int);
        struct upvalue* uv;
        n = string_dupc(name);
        uv = upvalue_table_overwrite(a,a->upval_tb,&n,1);
        uv->type = UPVALUE_VALUE;
        uv->gut.val = ajj_value_boolean(val);
        break;
      }
    case AJJ_VALUE_NONE:
      {
        struct upvalue* uv;
        n = string_dupc(name);
        uv = upvalue_table_overwrite(a,a->upval_tb,&n,1);
        uv->type = UPVALUE_VALUE;
        uv->gut.val = AJJ_NONE;
        break;
      }
    case AJJ_VALUE_STRING:
      {
        struct upvalue* uv;
        const char* str = va_arg(vl,const char*);
        n = string_dupc(name);
        uv = upvalue_table_overwrite(a,a->upval_tb,&n,1);
        uv->gut.val = ajj_value_assign(
            ajj_object_create_string(a,&(a->gc_root),str,strlen(str),0));
        break;
      }
    case AJJ_VALUE_OBJECT:
      {
        struct upvalue* uv;
        struct ajj_value* val = va_arg(vl,struct ajj_value*);
        n = string_dupc(name);
        uv = upvalue_table_overwrite(a,a->upval_tb,&n,1);
        uv->gut.val = ajj_value_move(&(a->gc_root),val);
        break;
      }
    case AJJ_VALUE_CLASS:
      {
        const struct ajj_class* cls;
        struct upvalue* uv;
        cls = va_arg(vl,const struct ajj_class*);
        ajj_env_add_class(a,name,cls);
        break;
      }
    default:
      UNREACHABLE();
  }
}

int ajj_env_del_value( struct ajj* a , const char* name ) {
  return upvalue_table_del_c(a,a->upval_tb,name);
}

/* =======================================================
 * IO
 * =====================================================*/
static struct ajj_io IO_STDOUT = {
  stdout,
  AJJ_IO_FILE
};
struct ajj_io* AJJ_IO_STDOUT = &IO_STDOUT;

static struct ajj_io IO_STDERR = {
  stderr,
  AJJ_IO_FILE
};
struct ajj_io* AJJ_IO_STDERR = &IO_STDERR;

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
    int ret = vnsprintf(io->out.m.mem+io->out.m.len,
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
void io_mem_write( struct ajj_io* io , void* mem , size_t len ) {
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
  return vprintf(io->out.f,fmt,vl);
}

static
void io_file_write( struct ajj_io* io , void* mem , size_t len ) {
  fwrite(mem,sizeof(char),len,io->out.f);
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

int ajj_io_write( struct ajj_io* io , void* mem , size_t len ) {
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
 * PRINT
 * =====================================*/

#define DOINDENT() \
  do { \
    if( opt == AJJ_VALUE_PRETTY ) { \
      int i; \
      for( i = 0 ; i < level ; ++i ) \
        ajj_io_printf(output,"%s",PRETTY_PRINT_INDENT); \
    } \
  } while(0)

static
struct string
escape_string( const struct string* val ) {
  struct strbuf buf;
  size_t i;
  strbuf_init(&buf);
  for( i = 0 ; i < val->len ; ++i ) {
    int c = val->str[i];
    int ec = tk_string_reescape_char(c);
    if(ec) {
      strbuf_push(&buf,'\\');
      strbuf_push(&buf,ec);
    } else {
      strbuf_push(&buf,c);
    }
  }
  return strbuf_tostring(&buf);
}

static
void ajj_value_print_priv( const struct ajj_value* val,
    struct ajj_io* output , int opt , int level ) {
  struct string str;

  switch(val->type) {
    case AJJ_VALUE_NONE:
      ajj_io_printf(output,"%s",NONE_STRING.str);
      break;
    case AJJ_VALUE_BOOLEAN:
      if( val->value.boolean ) {
        ajj_io_printf(output,"%s",TRUE_STRING.str);
      } else {
        ajj_io_printf(output,"%s",FALSE_STRING.str);
      }
      break;
    case AJJ_VALUE_NUMBER:
      ajj_io_printf(output,"%f",val->value.number);
      break;
    case AJJ_VALUE_STRING:
      str = escape_string(ajj_value_to_string(val));
      ajj_io_printf(output,"\"%s\"",str.str);
      string_destroy(&str);
      break;
    case AJJ_VALUE_OBJECT:
      DOINDENT();
      ajj_object_print(&(val->value.object->val.obj),
          output,opt,level);
      break;
    default:
      UNREACHABLE();
      break;
  }
}

static
void ajj_object_print( struct object* obj , struct ajj_io* output ,
    int opt, int level ) {
  if( opt == AJJ_VALUE_PRETTY ) {
    ajj_io_printf(output,"{\n");
    DOINDENT();
    fprintf(output,"object:%s\n",obj->fn_tb->name.str);
    DOINDENT();
    ajj_io_printf(output,"property \n");
    DOINDENT();
  } else {
    fprintf(output,"{ object:%s property ",obj->fn_tb->name.str);
  }
  ajj_dict_print(&(obj->prop),output,opt,level+1);
  if( opt == AJJ_VALUE_PRETTY ) {
    ajj_io_printf(output,"\n");
    DOINDENT();
    ajj_io_printf(output,"method { \n");
    DOINDENT();
  } else {
    ajj_io_printf(output,"method { ");
  }
  { /* dump the method for this object */
    size_t i;
    for( i = 0 ; i < obj->fn_tb->func_len; ++i ) {
      struct function* f = obj->fn_tb->func_tb+i;
      if( opt == AJJ_VALUE_PRETTY ) {
        DOINDENT();
        fprintf(output,"%s:%s",f->name.str,
            function_get_type_name(f->tp));
        if( i != obj->fn_tb->func_len - 1 ) {
          ajj_io_printf(output,",\n");
        }
      } else {
        fprintf(output,"%s:%s",f->name.str,
            function_get_type_name(f->tp));
        if( i != obj->fn_tb->func_len - 1 ) {
          ajj_io_printf(output,",");
        }
      }
    }
    if( opt == AJJ_VALUE_PRETTY ) {
      ajj_io_printf(output,"}\n");
    } else {
      ajj_io_printf(output,"}");
    }
  }
  if( opt == AJJ_VALUE_PRETTY ) {
    ajj_io_printf(output,"}\n");
  } else {
    ajj_io_printf(output,"}");
  }
}

void ajj_value_print( const struct ajj_value* val ,
    struct ajj_io* output, int opt ) {
  return ajj_value_print_priv(val,output,opt,0);
}

char* ajj_aux_load_file( struct ajj* a , const char* fname ,
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

int ajj_render( struct ajj* a , const char* src ,
    const char* key , FILE* output ) {
  struct ajj_object* obj = parse(a,key,src,0);
  if(!obj) return -1;
#if 0
  return vm_run_jinja(a,obj,output);
#else
  return -1;
#endif
}
