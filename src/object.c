#include "ajj-priv.h"

/* ========================
 * List implementation
 * ======================*/

static
void list_reserve( struct list* l ) {
  void* mem;
  assert( l->cap >= LIST_LOCAL_BUF_SIZE );
  if( l->lbuf == l->entry ) {
    mem = malloc(sizeof(struct ajj_value)*2*l->cap);
    memcpy(mem,l->entry,l->len*sizeof(struct ajj_value));
  } else {
    mem = mem_grow(l->entry,sizeof(struct ajj_value),0,&(l->cap));
  }
  l->entry = mem;
}

void list_push( struct list* l , const struct ajj_value* val ) {
  if( l->cap == l->len )
    list_reserve(l);
  l->entry[l->len] = *val;
  ++(l->len);
}

void list_destroy( struct list* l ) {
  if( l->lbuf != l->entry )
    free(l->entry);
  l->cap = LIST_LOCAL_BUF_SIZE;
  l->entry = l->lbuf;
  l->len = 0;
}

/* Function table */
struct function*
func_table_find_func( struct func_table* tb , const struct string* name ) {
  size_t i;
  for( i = 0 ; i < tb->func_len ; ++i ) {
    if( string_eq(name,&(tb->func_tb[i].name)) ) {
      return tb->func_tb + i;
    }
  }
  return NULL;
}

void func_table_destroy( struct ajj* a , struct func_table* tb ) {
  size_t i;
  string_destroy(&(tb->name)); /* delete the name */
  /* travel through each function table to delete program if it
   * is a script program. */
  for( i = 0 ; i < tb->func_len ; ++i ) {
    struct function* f = tb->func_tb + i;
    string_destroy(&(f->name));
    if(IS_JINJA(f)) {
      /* TODO:: delete this program through program_destroy
       * program_destroy(&(f->fn.jj_fn));
       */
    }
  }
  if( tb->func_tb != tb->func_buf )
    free(tb->func_tb); /* free func array */
  slab_free(&(a->ft_slab),tb);
}

/* Object */
struct ajj_object*
ajj_object_create( struct ajj* a , struct gc_scope* scope ) {
  struct ajj_object* ret = slab_malloc(&(a->obj_slab));
  LINSERT(ret,&(scope->gc_tail));
  ret->scp = scope;
  ret->parent_len = 0;
  return ret;
}

struct ajj_object*
ajj_object_jinja( struct ajj* a , struct ajj_object* obj ,
    const char* name , const char* src , int own ) {
  struct func_table* ft = slab_malloc(&(a->ft_slab));
  struct string fn = string_dupc(name);
  func_table_init(ft,
      NULL,NULL, /* null ctor and dtor */
      NULL,NULL, /* slot and udata */
      &fn,
      1);
  dict_create(&(obj->val.obj.prop));
  obj->tp = AJJ_VALUE_JINJA;
  obj->val.obj.data = NULL;
  obj->val.obj.fn_tb = ft;
  obj->val.obj.src = own ? src : strdup(src);
  return obj;
}

struct ajj_object*
ajj_object_create_jinja( struct ajj* a , const char* name ,
    const char* src , int own ) {
  struct ajj_object* obj = ajj_object_create(a,&(a->gc_root));
  return ajj_object_jinja(a,obj,name,src,own);
}

struct ajj_object*
ajj_object_create_list( struct ajj* a , struct gc_scope* scp ) {
  void* udata;
  int tp;
  assert(a->list);

  if(a->list->ctor( a,
      a->list->udata,
      &udata,
      NULL,0,
      &tp) == AJJ_EXEC_FAIL)
    return NULL;
  return ajj_object_create_obj(a,scp,udata,tp);
}

struct ajj_object*
ajj_object_create_dict( struct ajj* a , struct gc_scope* scp ) {
  void* udata;
  int tp;
  assert(a->dict);

  if(a->dict->ctor(a,
        a->dict->udata,
        &udata,
        NULL,0,
        &tp) == AJJ_EXEC_FAIL)
    return NULL;
  return ajj_object_create_obj(a,scp,udata,tp);
}

/* Value */
void
ajj_value_delete_string( struct ajj* a, struct ajj_value* str ) {
  assert(str->type == AJJ_VALUE_STRING);
  if(str->value.object->tp == AJJ_VALUE_STRING) {
    string_destroy(&(str->value.object->val.str));
  }
  /* remove it from the linked list */
  LREMOVE(str->value.object);
  /* delete the slot also */
  slab_free(&(a->obj_slab),str->value.object);
  /* reset value */
  str->type = AJJ_VALUE_NOT_USE;
}

struct ajj_object*
ajj_object_move( struct gc_scope* scp , struct ajj_object* obj ) {
  assert(obj->scp);

  /* only do move when we fonud out that the target scope has smaller
   * scp_id value since this means we have less lifecycle */
  if( obj->scp->scp_id > scp->scp_id ) {
    LREMOVE(obj);
    LINSERT(obj,&(scp->gc_tail));
    obj->scp = scp;
  }
}


struct ajj_value
ajj_value_move( struct gc_scope* scp, struct ajj_value* val ) {
  if(AJJ_IS_PRIMITIVE(val)) {
    return *val;
  } else {
    ajj_object_move(scp,val->value.object);
    return *val;
  }
}
