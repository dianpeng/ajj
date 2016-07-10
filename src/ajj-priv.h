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
#define UPVALUE_SLAB_LIMIT 64

#define FUNCTION_TABLE_SLAB_SIZE 16
#define FUNCTION_TABLE_SLAB_LIMIT 16

#define OBJECT_SLAB_SIZE 64
#define OBJECT_SLAB_LIMIT 64

#define GC_SLAB_SIZE 8
#define GC_SLAB_LIMIT 8

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

  /* Ajj file system layer */
  struct ajj_vfs vfs;
  void* vfs_udata;

  /* User data */
  void* udata;
};

struct jj_file {
  struct ajj_object* tmpl;
  time_t ts;
};

enum {
  AJJ_IO_FILE,
  AJJ_IO_MEM
};

struct ajj_io {
  union {
    FILE* f; /* make c89 initilization happy */
    struct strbuf m;
  } out;
  int tp;
};

/* get the current gc scope */
struct gc_scope*
ajj_cur_gc_scope( struct ajj* a );

struct jj_file*
ajj_find_template( struct ajj* a , const char* name );

struct ajj_object*
ajj_new_template( struct ajj* a ,const char* name ,
    const char* src , int own , time_t ts );

/* THIS FUNCTION IS NOT SAFE!
 * This function is used when we try to recover from the parsing
 * error. It is only used in parser, when the parsing failed, the
 * parser will remove this template and destroy it */
int ajj_delete_template( struct ajj* a, const char* name );

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

const struct function*
ajj_add_function( struct ajj* a , struct upvalue_table* ut,
    const char* name,
    ajj_function entry,
    void* );

/* Just for readability, no difference between a function and a filter */
#define ajj_add_filter ajj_add_function

const struct function*
ajj_add_test( struct ajj* a, struct upvalue_table* ut,
    const char* name,
    ajj_function entry,
    void* );

/* HIGH level API to help to manage the template */
struct ajj_object*
ajj_parse_template( struct ajj* a, const char* filename );

#endif /* _AJJ_PRIV_H_ */
