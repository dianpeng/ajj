#ifndef _AJJ_PRIV_H_
#define _AJJ_PRIV_H_
#include "ajj.h"
#include "util.h"
#include "gc.h"
#include "vm.h"

struct ajj {
  struct slab obj_slab; /* object slab */
  struct slab gc_slab;  /* gc_scope slab */
  struct gc_scope gc_root; /* root of the gc scope */
};

static inline
struct gc_scope* gc_root( struct ajj* a ) {
  return &(a->gc_root);
}

#endif /* _AJJ_PRIV_H_ */
