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

void
gc_scope_exit( struct ajj* a , struct gc_scope* scp ) {
  struct ajj_object* tail = &(scp->gc_tail);
  struct ajj_object* cur = tail->next;

  while( cur != tail ) {
    struct ajj_object* n;
    assert(cur->scp == scp);
    switch(cur->tp) {
      case AJJ_VALUE_STRING: /* dynamic string */
        string_destroy(&(cur->val.str));
        break;
      case AJJ_VALUE_CONST_STRING:
        break; /* break since we don't delete const string */
      case AJJ_VALUE_OBJECT: /* user defined stuff */

        /* call its destructor to delete this object */
        cur->val.obj.fn_tb->dtor(
            a,
            cur->val.obj.fn_tb->udata, /* user data */
            cur->val.obj.data
            );
        break;
      case AJJ_VALUE_JINJA:
        UNREACHABLE(); /* we should never have jinja template here
                        * since jinja template are not managed by
                        * gc_scope */
        return;
      default:
        UNREACHABLE();
        return;
    }
    /* delete this object slots */
    n = cur->next;
    slab_free(&(a->obj_slab),cur);
    cur = n;
  }
  LINIT(&(scp->gc_tail)); /* reset the gc scope list */
}

void
gc_scope_destroy( struct ajj* a , struct gc_scope* scp ) {
  gc_scope_exit(a,scp);
  slab_free(&(a->gc_slab),scp);
}
