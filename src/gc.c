#include "gc.h"
#include "ajj-priv.h"

struct gc_scope*
gc_scope_create( struct ajj* a , struct gc_scope* scp ) {
  struct gc_scope* new_scp = slab_malloc(&(a->gc_slab));
  new_scp->parent = scp;
  new_scp->scp_id = scp->scp_id+1;
  LINIT(&(new_scp->gc_tail));
  return new_scp;
}
