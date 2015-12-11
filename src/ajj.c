#include "ajj-priv.h"
#include "util.h"
#include "lex.h"
#include "parse.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#define PRETTY_PRINT_INDENT "    "

struct ajj_value AJJ_TRUE = { {1} , AJJ_VALUE_BOOLEAN };
struct ajj_value AJJ_FALSE= { {0} , AJJ_VALUE_BOOLEAN };
struct ajj_value AJJ_NONE = { {0} , AJJ_VALUE_NONE };

struct ajj* ajj_create() {
  struct ajj* r = malloc(sizeof(*r));
  slab_init(&(r->upval_slab),
      UPVALUE_SLAB_SIZE, sizeof(struct upvalue));
  slab_init(&(r->obj_slab),
      OBJECT_SLAB_SIZE, sizeof(struct ajj_object));
  slab_init(&(r->ft_slab),
      FUNCTION_SLAB_TABLE_SIZE,sizeof(struct func_table));
  slab_init(&(r->gc_slab),
      GC_SLAB_SIZE,sizeof(struct gc_scope));
  map_create(&(r->tmpl_tbl),sizeof(struct ajj_object*),32);
  gc_init(&(r->gc_root));
  r->rt = NULL;
  r->upval_tb = upvalue_table_create(NULL);
  return r;
}

void ajj_destroy( struct ajj* r ) {
  /* MUST delete upvalue_table at very first */
  upvalue_table_destroy(r,r->upval_tb);
  map_destroy(&(r->tmpl_tbl));

  slab_destroy(&(r->upval_slab));
  slab_destroy(&(r->obj_slab));
  slab_destroy(&(r->ft_slab));
  slab_destroy(&(r->gc_slab));
}

#define DOINDENT() \
  do { \
    if( opt == AJJ_VALUE_PRETTY ) { \
      int i; \
      for( i = 0 ; i < level ; ++i ) \
        fprintf(output,PRETTY_PRINT_INDENT); \
    } \
  } while(0)

static
void ajj_dict_print( struct map* , FILE* , int , int );
static
void ajj_list_print( struct list*, FILE* , int , int );
static
void ajj_object_print( struct object* , FILE* , int , int );

static
struct string
escape_string( const struct string* val ) {
  struct strbuf buf;
  size_t i;
  strbuf_init(&buf);
  for( i = 0 ; i < val->len ; ++i ) {
    int c = val->str[i];
    int ec = tk_string_reescape_char(c);
    if(ec) {
      strbuf_push(&buf,'\\');
      strbuf_push(&buf,ec);
    } else {
      strbuf_push(&buf,c);
    }
  }
  return strbuf_tostring(&buf);
}

static
void ajj_value_print_priv( const struct ajj_value* val,
    FILE* output , int opt , int level ) {
  struct string str;

  switch(val->type) {
    case AJJ_VALUE_NONE:
      fprintf(output,"%s",NONE_STRING.str);
      break;
    case AJJ_VALUE_BOOLEAN:
      if( val->value.boolean ) {
        fprintf(output,"%s",TRUE_STRING.str);
      } else {
        fprintf(output,"%s",FALSE_STRING.str);
      }
      break;
    case AJJ_VALUE_NUMBER:
      fprintf(output,"%f",val->value.number);
      break;
    case AJJ_VALUE_STRING:
      str = escape_string(ajj_value_to_string(val));
      fprintf(output,"\"%s\"",str.str);
      string_destroy(&str);
      break;
    case AJJ_VALUE_DICT:
      DOINDENT();
      ajj_dict_print(&(val->value.object->val.d),
          output,opt,level);
      break;
    case AJJ_VALUE_LIST:
      DOINDENT();
      ajj_list_print(&(val->value.object->val.l),
          output,opt,level);
      break;
    case AJJ_VALUE_OBJECT:
      DOINDENT();
      ajj_object_print(&(val->value.object->val.obj),
          output,opt,level);
      break;
    default:
      UNREACHABLE();
      break;
  }
}

static
void ajj_dict_print( struct map* d, FILE* output ,
    int opt, int level ) {
  int itr;
  size_t i = 0;
  if( opt == AJJ_VALUE_PRETTY )
    fprintf(output,"{\n");
  else
    fprintf(output,"{");

  itr = dict_iter_start(d);
  while( dict_iter_has(d,itr) ) {
    struct map_pair entry = dict_iter_deref(d,itr);
    DOINDENT();
    fprintf(output,"\"%s\":",entry.key->str);
    ajj_value_print_priv((struct ajj_value*)(entry.val),
        output,
        opt,
        level+1);
    if( i != d->len -1 ) {
      if( opt == AJJ_VALUE_PRETTY )
        fprintf(output,",\n");
      else
        fprintf(output,",");
    }
    ++i;
    itr = dict_iter_move(d,itr);
  }
  if( opt == AJJ_VALUE_PRETTY )
    fprintf(output,"\n}");
  else
    fprintf(output,"}");
}

static
void ajj_list_print( struct list* l , FILE* output ,
    int opt, int level ) {
  int itr;
  size_t i = 0;
  if( opt == AJJ_VALUE_PRETTY )
    fprintf(output,"[\n");
  else
    fprintf(output,"[");
  itr = list_iter_start(l);
  while( list_iter_has(l,itr) ) {
    struct ajj_value* obj = list_iter_deref(l,itr);
    DOINDENT();
    ajj_value_print_priv(obj,output,opt,level+1);
    if( i != l->len -1 ) {
      if( opt == AJJ_VALUE_PRETTY )
        fprintf(output,",\n");
      else
        fprintf(output,",");
    }
    ++i;
    itr = list_iter_move(l,itr);
  }
  if( opt == AJJ_VALUE_PRETTY )
    fprintf(output,"\n]");
  else
    fprintf(output,"]");
}

static
void ajj_object_print( struct object* obj , FILE* output ,
    int opt, int level ) {
  if( opt == AJJ_VALUE_PRETTY ) {
    fprintf(output,"{\n");
    DOINDENT();
    fprintf(output,"object:%s\n",obj->fn_tb->name.str);
    DOINDENT();
    fprintf(output,"property \n");
    DOINDENT();
  } else {
    fprintf(output,"{ object:%s property ",obj->fn_tb->name.str);
  }
  ajj_dict_print(&(obj->prop),output,opt,level+1);
  if( opt == AJJ_VALUE_PRETTY ) {
    fprintf(output,"\n");
    DOINDENT();
    fprintf(output,"method { \n");
    DOINDENT();
  } else {
    fprintf(output,"method { ");
  }
  /* dump method */
  {
    size_t i;
    for( i = 0 ; i < obj->fn_tb->func_len; ++i ) {
      struct function* f = obj->fn_tb->func_tb+i;
      if( opt == AJJ_VALUE_PRETTY ) {
        DOINDENT();
        fprintf(output,"%s:%s",f->name.str,
            function_get_type_name(f->tp));
        if( i != obj->fn_tb->func_len - 1 ) {
          fprintf(output,",\n");
        }
      } else {
        fprintf(output,"%s:%s",f->name.str,
            function_get_type_name(f->tp));
        if( i != obj->fn_tb->func_len - 1 ) {
          fprintf(output,",");
        }
      }
    }
    if( opt == AJJ_VALUE_PRETTY ) {
      fprintf(output,"}\n");
    } else {
      fprintf(output,"}");
    }
  }
  if( opt == AJJ_VALUE_PRETTY ) {
    fprintf(output,"}\n");
  } else {
    fprintf(output,"}");
  }
}

void ajj_value_print( const struct ajj_value* val ,
    FILE* output, int opt ) {
  return ajj_value_print_priv(val,output,opt,0);
}

char* ajj_aux_load_file( struct ajj* a , const char* fname ,
    size_t* size) {
  FILE* f = fopen(fname,"r");
  long int start;
  long int end;
  size_t len;
  size_t rsz;
  char* r;

  if(!f) return NULL;
  start = ftell(f);
  fseek(f,0,SEEK_END); /* not portable for SEEK_END can be
                        * meaningless */
  end = ftell(f);
  fseek(f,0,SEEK_SET);
  len = (size_t)(end-start);
  r = malloc(len+1);
  rsz = fread(r,1,len,f);
  assert( rsz <= len );
  r[rsz] = 0;
  *size = rsz;
  fclose(f);
  return r;
}

int ajj_render( struct ajj* a , const char* src ,
    const char* key , FILE* output ) {
  struct ajj_object* obj = parse(a,key,src,0);
  if(!obj) return -1;
#if 0
  return vm_run_jinja(a,obj,output);
#else
  return -1;
#endif
}
