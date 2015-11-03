#include "ajj-priv.h"

/* this function JUST clear memory inside of this object */
static inline
void object_clean( struct ajj_object* obj ) {
  if( obj->tp == AJJ_VALUE_STRING ) {
    /* string object , clear the string */
    free(obj->val.str.str);
  } else {
    /* object */
    assert( obj->tp == AJJ_VALUE_OBJECT );
    dict_destroy( &(obj->val.obj.prop) ); /* destroy the properties */
    obj->val.obj.dtor.func(obj->val.obj.dtor.udata, /* udata */
                           obj->val.obj.data );
  }
}

/* delete the whole scope */
static
void ajj_object_destroy( struct ajj* , struct ajj_object* p ) {
  struct ajj_object* end = p;
  struct ajj_object* n = p;
  do {
    n = n->next;
    /* clean this object */
    object_clean(p);
    /* delete this object */
    slab_free( &(ajj->obj_slab), p );
    /* move next pointer into p */
    p = n;
  } while( n != end ); /* only start another loop if we do not meet the
                        * end pointer where we have a cycle */
}

static
struct ajj_object* ajj_object_find( struct ajj_object* scope ,
    const char* key ) {
  /* Lookup a key from the current scope and this function works
   * recursively to its parent */
  struct ajj_object* ret;

  assert( scope->tp == AJJ_VALUE_OBJECT );
  if( (ret = dict_find(&(scope->val.obj.prop),key)) != NULL )
    return (struct ajj_object*)(ret);
  else {
    int i;
    for( i = 0 ; i < scope->parent_len ; ++i ) {
      if( (ret = ajj_object_find(scope->parent[i],key)) != NULL )
        return ret;
    }
    return NULL;
  }
}
