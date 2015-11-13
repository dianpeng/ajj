#ifndef _AJJ_PRIV_H_
#define _AJJ_PRIV_H_
#include "ajj.h"
#include "util.h"
#include "gc.h"
#include "vm.h"
#include "object.h"

#define ERROR_BUFFER_SIZE 1024

struct runtime;

/* Any value that resides inside of AJJ environments
 * are called global_var. Each global_var could be
 * any types of value as following:
 * 1. A ajj_value.
 * 2. A function , remember in ajj_value you are not
 * allowed to have a function.
 * 3. A object constructor. */

struct global_var {
  union {
    struct ajj_value value;
    struct c_closure func;
    struct obj_ctor ctor;
  } value;
  int type;
};

struct ajj {
  struct slab obj_slab; /* object slab */
  struct slab gc_slab;  /* gc_scope slab */
  struct gc_scope gc_root; /* root of the gc scope */
  char err[ERROR_BUFFER_SIZE]; /* error buffer */

  /* runtime field that points to places that VM
   * currently working at. It will be set when we
   * start executing the code */
  struct runtime* rt;
};

#endif /* _AJJ_PRIV_H_ */
