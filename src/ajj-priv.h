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
  struct upvalue_table* upval_tb; /* upvalue table */
  struct upvalue_table builtins;  /* builtin table */
};

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
  assert(obj->tp == AJJ_VALUE_JINJA);
  func_table_destroy(a,obj->val.obj.fn_tb);
  free((void*)obj->val.obj.src); /* always owned by us */
  slab_free(&(a->obj_slab),obj);
  return 0;
}

/* Wipe out ALL the template is safe operation */
void ajj_clear_template( struct ajj* );

/* internal utility functions */
char* ajj_aux_load_file( struct ajj* a, const char* , size_t* );

int ajj_render( struct ajj* a , const char* , const char*  , FILE* );

#endif /* _AJJ_PRIV_H_ */
