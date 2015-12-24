#include "upvalue.h"
#include "ajj-priv.h"
#include "util.h"

struct upvalue*
upvalue_table_add( struct ajj* a,
    struct upvalue_table* tb,
    const struct string* key,
    int own ) {
  struct upvalue* ret = slab_malloc(&(a->upval_slab));
  struct upvalue** slot;
  /* find out if we have such value in the table, if so
   * we just link a value on top of it */
  if( (slot = map_find(&(tb->d),key)) == NULL ) {
    /* store the pointer */
    CHECK( !map_insert(&(tb->d),key,own,&ret) );
    ret->prev = NULL;
  } else {
    ret->prev = *slot;
    (*slot) = ret;
    if(own) string_destroy((struct string*)key);
  }
  return ret;
}

struct upvalue*
upvalue_table_add_c( struct ajj* a,
    struct upvalue_table* tb,
    const char* key) {
  struct upvalue* ret = slab_malloc(&(a->upval_slab));
  struct upvalue** slot;
  if( (slot = map_find_c(&(tb->d),key)) == NULL ) {
    CHECK( !map_insert_c(&(tb->d),key,&ret) );
    ret->prev = NULL;
  } else {
    ret->prev = *slot;
    (*slot) = ret;
  }
  return ret;
}

struct upvalue*
upvalue_table_overwrite( struct ajj* a ,
    struct upvalue_table* tb,
    const struct string* key ,
    int own ) {
  struct upvalue** slot;
  if( (slot = map_find(&(tb->d),key)) == NULL ) {
    struct upvalue* ret = slab_malloc(&(a->upval_slab));
    CHECK( !map_insert(&(tb->d),key,own,&ret));
    ret->prev = NULL;
    return ret;
  } else {
    assert(*slot);
    return *slot;
  }
}

struct upvalue*
upvalue_table_overwrite_c( struct ajj* a ,
    struct upvalue_table* tb,
    const char* key ) {
  struct upvalue** slot;
  if( (slot = map_find_c(&(tb->d),key)) == NULL ) {
    struct upvalue* ret = slab_malloc(&(a->upval_slab));
    CHECK( !map_insert_c(&(tb->d),key,&ret));
    ret->prev = NULL;
    return ret;
  } else {
    assert(*slot);
    return *slot;
  }
}

int upvalue_table_del( struct ajj* a ,
    struct upvalue_table* tb,
    const struct string* key,
    const struct upvalue_table* util  ) {
  struct upvalue_table* cur_tb = tb;
  do {
    struct upvalue** slot;
    if( (slot = map_find(&(cur_tb->d),key) ) != NULL ) {
      struct upvalue* uv = *slot;
      if( uv->prev == NULL ) {
        /* this is the end of the chain */
        CHECK( !map_remove(&(cur_tb->d),key,NULL) );
      } else {
        *slot = uv->prev;
      }
      slab_free(&(a->upval_slab),uv);
      return 0;
    }
    cur_tb = cur_tb->prev;
  } while(cur_tb != util);
  return -1;
}

int upvalue_table_del_c( struct ajj* a,
    struct upvalue_table* tb,
    const char* key ,
    const struct upvalue_table* util ) {
  struct upvalue_table* cur_tb = tb;
  do {
    struct upvalue** slot;
    if( (slot = map_find_c(&(cur_tb->d),key) ) != NULL ) {
      struct upvalue* uv = *slot;
      if( uv->prev == NULL ) {
        /* this is the end of the chain */
        CHECK( !map_remove_c(&(cur_tb->d),key,NULL) );
      } else {
        *slot = uv->prev;
      }
      slab_free(&(a->upval_slab),uv);
      return 0;
    }
    cur_tb = cur_tb->prev;
  } while(cur_tb != util);
  return -1;
}

struct upvalue*
upvalue_table_find( struct upvalue_table* tb,
    const struct string* key ,
    const struct upvalue_table* util ) {
  struct upvalue_table* cur_tb = tb;
  do {
    struct upvalue** slot;
    if( (slot = map_find(&(cur_tb->d),key)) != NULL ) {
      return *slot;
    }
    cur_tb = cur_tb->prev;
  } while(cur_tb != util);
  return NULL;
}

struct upvalue*
upvalue_table_find_c( struct upvalue_table* tb,
    const char* key ,
    const struct upvalue_table* util ) {
  struct upvalue_table* cur_tb = tb;
  do {
    struct upvalue** slot;
    if( (slot = map_find_c(&(cur_tb->d),key)) != NULL ) {
      return *slot;
    }
    cur_tb = cur_tb->prev;
  } while(cur_tb != util);
  return NULL;
}

void
upvalue_table_clear( struct ajj* a, struct upvalue_table* m ) {
  int itr;
  itr = map_iter_start(&(m->d));
  while( map_iter_has(&(m->d),itr) ) {
    struct map_pair p = map_iter_deref(&(m->d),itr);
    struct upvalue* uv = *(struct upvalue**)p.val;
    while( uv ) {
      struct upvalue* p = uv->prev;
      if( uv->type == UPVALUE_FUNCTION ) {
        if( IS_OBJECTCTOR(&(uv->gut.gfunc)) ) {
          /* We *own* the object ctor memory, so we need to
           * delete it , it is not tracked by any gc chain */
          func_table_destroy(a,
              GET_OBJECTCTOR(&(uv->gut.gfunc)));
        }
      }
      slab_free(&(a->upval_slab),uv);
      uv = p;
    }
    itr = map_iter_move(&(m->d),itr);
  }
  map_destroy(&(m->d));
}

struct upvalue_table*
upvalue_table_destroy_one( struct ajj* a ,
    struct upvalue_table* m ) {
  struct upvalue_table* r = m->prev;
  upvalue_table_clear(a,m);
  free(m);
  return r;
}

void
upvalue_table_destroy( struct ajj* a,
    struct upvalue_table* m ,
    const struct upvalue_table* until ) {
  while(m!=until) {
    m = upvalue_table_destroy_one(a,m);
  }
}
