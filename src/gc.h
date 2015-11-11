#ifndef _GC_H_
#define _GC_H_
#include "ajj.h"
#include "util.h"
#include "object.h"

struct ajj;

struct gc_scope {
  struct ajj_object gc_tail; /* tail of the GC objects list */
  struct gc_scope* parent;   /* parent scope */
  unsigned int scp_id;       /* scope id */
};

static inline
struct gc_scope* gc_scope_create( struct ajj* a, struct gc_scope* scp ) {
  struct gc_scope* new_scp = slab_malloc(&(a->gc_slb));
  new_scp->parent = scp;
  new_scp->scp_id = scp->scp_id+1;
  LINIT(&(new_scp->gc_tail));
  return new_scope;
}

/* This function will destroy all the gc scope allocated memory and also
 * the gc_scope object itself */
static
void gc_scope_destroy( struct ajj* , struct gc_scope* );

#endif /* _GC_H_ */
