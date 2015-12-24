#ifndef _AJJ_PRIV_H_
#define _AJJ_PRIV_H_
#include "ajj.h"
#include "util.h"
#include "gc.h"
#include "vm.h"
#include "object.h"
#include "upvalue.h"

#define ERROR_BUFFER_SIZE 1024*4 /* 4kb for error buffer, already very large */
#define UPVALUE_SLAB_SIZE 32
#define FUNCTION_SLAB_TABLE_SIZE 32
#define OBJECT_SLAB_SIZE 128
#define GC_SLAB_SIZE 32

struct runtime;

struct ajj {
  struct slab obj_slab; /* object slab */
  struct slab upval_slab; /* global var slab */
  struct slab ft_slab;  /* function table slab */
  struct slab gc_slab;  /* garbage collector slab */

  struct gc_scope gc_root; /* root of the gc scope. It contains those
                            * value will not be deleted automatically
                            * by the program. Like template's static
                            * memory */
  char err[ERROR_BUFFER_SIZE]; /* error buffer */

  struct map tmpl_tbl; /* template table. Provide key value map to
                        * find a specific template. If we have already
                        * loaded a template, then we can just reference
                        * it here without loading it multiple times */

  /* runtime field that points to places that VM
   * currently working at. It will be set when we
   * start executing the code */
  struct runtime* rt;
  struct upvalue_table env;       /* environment value table */
  struct upvalue_table builtins;  /* builtin table */

  /* INLINE those common builtin types and this makes our builtin type
   * creation easier and faster */
  struct func_table* list;
  struct func_table* dict;
  struct func_table* loop;
};

enum {
  AJJ_IO_FILE,
  AJJ_IO_MEM
};

struct ajj_io {
  union {
    FILE* f; /* make c89 initilization happy */
    struct {
      char* mem;
      size_t len;
      size_t cap;
    } m;
  } out;
  int tp;
};

/* get the current gc scope */
struct gc_scope*
ajj_cur_gc_scope( struct ajj* a );

static
void ajj_io_init_file( struct ajj_io* io , FILE* f ) {
  assert(f);
  io->tp = AJJ_IO_FILE;
  io->out.f = f;
}

static
void ajj_io_init_mem ( struct ajj_io* io , size_t cap ) {
  assert(cap);
  io->tp = AJJ_IO_MEM;
  io->out.m.mem = malloc(cap);
  io->out.m.len = 0;
  io->out.m.cap = cap;
}

static
struct ajj_object*
ajj_find_template( struct ajj* a , const char* name ) {
  struct ajj_object** ret = map_find_c(&(a->tmpl_tbl),name);
  return ret == NULL ? NULL : *ret;
}

static
struct ajj_object*
ajj_new_template( struct ajj* a ,const char* name ,
    const char* src , int own ) {
  struct ajj_object* obj;
  if( ajj_find_template(a,name) != NULL )
    return NULL; /* We already get one */
  obj = ajj_object_create_jinja(a,name,src,own);
  CHECK(!map_insert_c(&(a->tmpl_tbl),name,&obj));
  return obj;
}

/* THIS FUNCTION IS NOT SAFE!
 * This function is used when we try to recover from the parsing
 * error. It is only used in parser, when the parsing failed, the
 * parser will remove this template and destroy it */
static
int ajj_delete_template( struct ajj* a, const char* name ) {
  struct ajj_object* obj;
  if( map_remove_c(&(a->tmpl_tbl),name,&obj))
    return -1;
  LREMOVE(obj); /* remove it from gc scope */
  ajj_object_destroy_jinja(a,obj);
  return 0;
}

/* Wipe out ALL the template is safe operation */
void ajj_clear_template( struct ajj* );

/* Register value/class/function into different uvpalue table */
void
ajj_add_value( struct ajj* a , struct upvalue_table* ut,
    const char* name,
    int type,  ... );

/* Function used to add a class definition in SPECIFIC table
 * and it will return added object's function table */
struct func_table*
ajj_add_class( struct ajj* a, struct upvalue_table* ut,
    const struct ajj_class* cls );

const
struct function*
ajj_add_function( struct ajj* a , struct upvalue_table* ut,
    const char* name,
    ajj_function entry,
    void* );

int ajj_render( struct ajj* a , const char* ,
    const char*  , struct ajj_io* );

#endif /* _AJJ_PRIV_H_ */
