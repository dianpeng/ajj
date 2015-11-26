#ifndef _GC_H_
#define _GC_H_
#include "object.h"

struct ajj;

struct gc_scope {
  struct ajj_object gc_tail; /* tail of the GC objects list */
  struct gc_scope* parent;   /* parent scope */
  unsigned int scp_id;       /* scope id */
};

static
void gc_init( struct gc_scope* scp ) {
  LINIT(&(scp->gc_tail));
  scp->parent = NULL;
  scp->scp_id = 0;
}

struct gc_scope* gc_scope_create( struct ajj* a, struct gc_scope* scp );

/* This function will destroy all the gc scope allocated memory and also
 * the gc_scope object itself */
void gc_scope_destroy( struct ajj* , struct gc_scope* );

#endif /* _GC_H_ */
