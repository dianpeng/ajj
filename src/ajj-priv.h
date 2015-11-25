#ifndef _AJJ_PRIV_H_
#define _AJJ_PRIV_H_
#include "ajj.h"
#include "util.h"
#include "gc.h"
#include "vm.h"
#include "object.h"

#define ERROR_BUFFER_SIZE 1024

struct runtime;

struct ajj {
  struct slab obj_slab; /* object slab */
  struct slab glb_var_slab; /* global var slab */
  struct slab ft_slab;  /* function table slab */
  struct slab gc_slab;  /* garbage collector slab */

  struct gc_scope gc_root; /* root of the gc scope */
  char err[ERROR_BUFFER_SIZE]; /* error buffer */

  /* runtime field that points to places that VM
   * currently working at. It will be set when we
   * start executing the code */
  struct runtime* rt;
  struct gvar_table* gvar_tb; /* global variable table */
};

/* internal utility functions */
char* ajj_aux_load_file( struct ajj* a, const char* , size_t* );

char* ajj_render( struct ajj* a , const char* , size_t* );

#endif /* _AJJ_PRIV_H_ */
