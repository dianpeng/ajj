#include "ajj-priv.h"
#include "util.h"
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
struct ajj_value AJJ_TRUE = { {1} , AJJ_VALUE_BOOLEAN };
struct ajj_value AJJ_FALSE= { {0} , AJJ_VALUE_BOOLEAN };
struct ajj_value AJJ_NONE = { {0} , AJJ_VALUE_NONE };

struct ajj* ajj_create() {
  struct ajj* r = malloc(sizeof(*r));
  slab_init(&(r->glb_var_slab),
      GLOBAL_VARIABLE_SIZE, sizeof(struct global_var));
  slab_init(&(r->obj_slab),
      OBJECT_SIZE, sizeof(struct ajj_object));
  slab_init(&(r->ft_slab),
      FUNCTION_TABLE_SIZE,sizeof(struct func_table));
  slab_init(&(r->gc_slab),
      GC_SIZE,sizeof(struct gc_scope));
  map_create(&(r->tmpl_tbl),sizeof(struct ajj_object*),32);
  gc_init(&(r->gc_root));
  r->rt = NULL;
  r->gvar_tb = gvar_table_create(NULL);
  return r;
}

void ajj_destroy( struct ajj* r ) {
  slab_destroy(&(r->glb_var_slab));
  slab_destroy(&(r->obj_slab));
  slab_destroy(&(r->ft_slab));
  slab_destroy(&(r->gc_slab));
  map_destroy(&(r->tmpl_tbl));

  /* TODO::
  while( r->gvar_tb != NULL ) {
    r->gvar_tb = gvar_table_destroy(r->gvar_tb);
  }
  */
}
