#include "gc.h"
#include "ajj-priv.h"

#define MERGE_LIST(T,X) \
  do{ \
    if( !LEMPTY(&((X)->gc_tail)) ) { \
      (X)->gc_tail.prev->next = (T)->gc_tail.next; \
      (T)->gc_tail.next->prev = (X)->gc_tail.prev; \
      (T)->gc_tail.next = (X)->gc_tail.next; \
      (X)->gc_tail.next->prev = &((T)->gc_tail); \
    } \
  } while(0)

struct gc_scope*
gc_scope_create( struct ajj* a , struct gc_scope* scp ) {
  struct gc_scope* new_scp = slab_malloc(&(a->gc_slab));
  new_scp->parent = scp;
  new_scp->scp_id = scp->scp_id+1;
  LINIT(&(new_scp->gc_tail));
  return new_scp;
}


void
gc_scope_merge( struct gc_scope* dst , struct gc_scope* src ) {
  /* iterate everything inside of this GC_SCOPE and then happily
   * modify their gc scope pointer */
  struct ajj_object* tail = &(src->gc_tail);
  struct ajj_object* cur = tail->next;
  while( tail != cur ) {
    cur->scp = dst;
    cur = cur->next;
  }
  /* MERGE the pointer */
  MERGE_LIST(dst,src);
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
      case AJJ_VALUE_JINJA:
        ajj_object_destroy_jinja(a,cur);
        break;
      default:
        /* call its destructor to delete this object */
        cur->val.obj.fn_tb->dtor(
            a,
            cur->val.obj.fn_tb->udata, /* user data */
            cur->val.obj.data
            );
        break;
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
