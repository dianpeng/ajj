#include "vm.h"
#include "ajj-priv.h"
#include "object.h"
#include "gc.h"
#include "util.h"
#include "bc.h"
#include "upvalue.h"

#include <limits.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>


#define AJJ_EXEC_CALL 1 /* indicate we want to call a script
                         * function recursively */


#define cur_frame(a) (&(a->rt->call_stk[a->rt->cur_call_stk]))
#define cur_function(a) (cur_frame(a)->entry)

/* stack manipulation routine */
#ifndef NDEBUG
static
struct ajj_value*
stack_value( struct ajj* a , int x ) {
  assert( x >= cur_frame(a)->ebp && x < cur_frame(a)->esp );
  return x + a->rt->val_stk;
}

#define BOT(A,X) stack_value((A),cur_frame((A))->ebp+(X))
#define TOP(A,X) stack_value((A),cur_frame((A))->esp-(X))
#else
#define BOT(A,X) ((A)->rt->val_stk[cur_frame((A))->ebp+(X)])
#define TOP(A,X) ((A)->rt->val_stk[cur_frame((A))->esp-(X)])
#endif /* NDEBUG */

static
void report_error( struct ajj* a , const char* , ... );

static
void pop(struct ajj* a , int off ) {
  int val = off;
  struct func_frame* fr = cur_frame(a);
  assert(fr->esp >= val);
  fr->esp -= val;
}

static
int push( struct ajj* a , struct ajj_value v ) {
  struct func_frame* fr = cur_frame(a);
  if( fr->esp == AJJ_MAX_VALUE_STACK_SIZE ) {
    report_error(a,"Too much value on value stack!");
    return -1;
  } else {
    a->rt->val_stk[fr->esp] = v;
    ++fr->esp;
  }
  return 0;
}

static
const struct string* const_str( struct ajj* a , int idx ) {
  const struct function* c = cur_function(a);
  assert( IS_JINJA(c) );
  assert( (size_t)idx < c->f.jj_fn.str_len );
  return &(c->f.jj_fn.str_tbl[idx]);
}

static
double const_num( struct ajj* a, int idx ) {
  const struct function* c = cur_function(a);
  assert( IS_JINJA(c) );
  assert( (size_t)idx < c->f.jj_fn.num_len );
  return c->f.jj_fn.num_tbl[idx];
}

/* ============================
 * runtime
 * ==========================*/
static
struct runtime* runtime_create( struct ajj_object* tp ) {
  struct runtime* rt = calloc(1,sizeof(*rt));
  rt->cur_tmpl = tp;
  return rt;
}

static
void runtime_destroy( struct runtime* rt );

static
void report_error( struct ajj* a , const char* format , ... );

/* =============================
 * Decoding
 * ===========================*/

static
int next_instr( struct ajj* a ) {
  struct func_frame* fr = cur_frame(a);
  const struct program* prg;
  assert(IS_JINJA(fr->entry));
  prg = GET_JINJAFUNC(fr->entry);
  return bc_next(prg,&(fr->pc));
}

static
int instr_1st_arg( int c ) {
  return bc_1st_arg(c);
}

static
int instr_2nd_arg( struct ajj* a ) {
  struct func_frame* fr = cur_frame(a);
  const struct program* prg;
  assert(IS_JINJA(fr->entry));
  prg = GET_JINJAFUNC(fr->entry);
  return bc_2nd_arg(prg,&(fr->pc));
}

/* ============================
 * Upvalue handler
 * ==========================*/

static
void set_upvalue( struct ajj* a, const struct string* name,
    const struct ajj_value* value , int own ) {
  struct upvalue* uv = upvalue_table_add(a,
      a->upval_tb,name,own);
  uv->type = UPVALUE_VALUE;
  uv->gut.val = *value;
}

static
struct ajj_value
get_upvalue( struct ajj* a , const struct string* name ) {
  struct upvalue* uv = upvalue_table_find(a->upval_tb,name);
  if( uv->type != UPVALUE_VALUE )
    return AJJ_NONE;
  else
    return uv->gut.val;
}

static
void del_upvalue( struct ajj* a, const struct string* name ) {
  upvalue_table_del(a,a->upval_tb,name);
}

/* =============================
 * Type conversions
 * ===========================*/

static
double str_to_number( struct ajj* a , const char* str , int* fail ) {
  double val;
  errno = 0;
  val = strtod(str,NULL);
  if( errno ) {
    report_error(a,"Cannot convert str:%s to number because:%s",
        str,
        strerror(errno));
    *fail = 1;
    return 0;
  }
  *fail = 0;
  return val;
}

static
struct string
to_string( struct ajj* a , const struct ajj_value* val ,
    int* own , int* fail ) {
  switch(val->type) {
    case AJJ_VALUE_BOOLEAN:
      *own = 0; *fail = 0;
      return ajj_value_to_boolean(val) ?
        TRUE_STRING : FALSE_STRING;
    case AJJ_VALUE_NUMBER:
      {
        char buf[256];
        sprintf(buf,"%f",ajj_value_to_number(val));
        *own = 1;*fail =0;
        return string_dupc(buf);
      }
    case AJJ_VALUE_STRING:
      *own = 0; *fail = 0;
      return *ajj_value_to_string(val);
    default:
      report_error(a,"Cannot convert type:%s to string!",
          ajj_value_get_type_name(val));
      *fail = 1;
      return NONE_STRING;
  }
}

static
double to_number( struct ajj* a , const struct ajj_value* val , int* fail ) {
  assert( val->type != AJJ_VALUE_NOT_USE );

  switch(val->type) {
    case AJJ_VALUE_BOOLEAN:
      *fail = 0;
      return ajj_value_to_boolean(val);
    case AJJ_VALUE_NUMBER:
      *fail = 0;
      return ajj_value_to_number(val);
    case AJJ_VALUE_STRING:
      *fail = 0;
      return str_to_number(a,ajj_value_to_cstr(val),fail);
    default:
      *fail = 1;
      report_error(a,"Cannot convert type from:%s to type:number!",
          ajj_value_get_type_name(val));
      return 0;
  }
}

static
int to_integer( struct ajj* a, const struct ajj_value* val, int* fail ) {
  assert( val->type != AJJ_VALUE_NOT_USE );
  switch( val->type ) {
    case AJJ_VALUE_BOOLEAN:
      *fail = 0;
      return ajj_value_to_boolean(val);
    case AJJ_VALUE_NUMBER:
      {
        double ip;
        double re = modf(val->value.number,&ip);
        if( re > INT_MAX || re < INT_MIN ) {
          report_error(a,"Cannot convert number:%d to integer,overflow!",
              re);
          *fail = 1;
          return -1;
        }
        *fail =0;
        return (int)(ip);
      }
    default:
      report_error(a,"Cannot convert type:%s to integer!",
          ajj_value_get_type_name(val));
      *fail = 1;
      return -1;
  }
}

static
int is_true( const struct ajj_value* val ) {
  assert( val->type != AJJ_VALUE_NOT_USE );
  switch(val->type){
    case AJJ_VALUE_NUMBER:
      return ajj_value_to_number(val) !=0;
    case AJJ_VALUE_BOOLEAN:
      return ajj_value_to_boolean(val);
    case AJJ_VALUE_NONE:
      return 0;
    default:
      return 1;
  }
}

static
int is_false( const struct ajj_value* val ) {
  assert( val->type != AJJ_VALUE_NOT_USE );
  switch(val->type) {
    case AJJ_VALUE_NUMBER:
      return ajj_value_to_number(val) == 0;
    case AJJ_VALUE_BOOLEAN:
      return !ajj_value_to_boolean(val);
    case AJJ_VALUE_NONE:
      return 1;
    default:
      return 0;
  }
}

static
int is_empty( struct ajj* a , const struct ajj_value* val , int* fail ) {
  assert( val->type != AJJ_VALUE_NOT_USE );
  switch(val->type) {
    case AJJ_VALUE_STRING:
      *fail = 0;
      return ajj_value_to_string(val)->len == 0;
    case AJJ_VALUE_LIST:
      *fail = 0;
      return ajj_value_to_list(val)->len == 0;
    case AJJ_VALUE_DICT:
      *fail = 0;
      return ajj_value_to_dict(val)->len == 0;
    default:
      *fail = 1;
      report_error(a,"Type:%s cannot be used to test whether it is empty or not!",
          ajj_value_get_type_name(val));
      return -1;
  }
}

/* =============================
 * Specific instruction handler
 * ============================*/

/* binary operation handler , saves me some typings */
#define DEFINE_BIN_HANDLER(N,ON,OS,TYPE) \
  static   \
  struct ajj_value N(struct ajj* a, \
      const struct ajj_value* l, \
      const struct ajj_value* r, \
      int* fail ) { \
    if( l->type == AJJ_VALUE_STRING || \
        r->type == AJJ_VALUE_STRING ) { \
      int own_l , own_r; \
      struct string ls ; \
      struct string rs ; \
      ls = to_string(a,l,&own_l,fail); \
      if( *fail ) return AJJ_NONE; \
      rs = to_string(a,r,&own_r,fail); \
      if( *fail ) { \
        if(own_l) \
          string_destroy(&ls); \
        return AJJ_NONE; \
      } \
      OS(a,&ls,own_l,&rs,own_r); \
    } else { \
      double ln , rn; \
      ln = to_number(a,l,fail); \
      if( *fail ) return AJJ_NONE; \
      rn = to_number(a,r,fail); \
      if( *fail ) return AJJ_NONE; \
      return ajj_value_##TYPE( ln ON rn ); \
    } \
  }

#define STRING_ADD(a,L,OL,R,OR) \
  do { \
    struct strbuf buf; \
    struct string str; \
    strbuf_init(&buf); \
    strbuf_append(&buf,(L)->str,(L)->len); \
    strbuf_append(&buf,(R)->str,(R)->len); \
    if(OL) string_destroy(L); \
    if(OR) string_destroy(R); \
    str = strbuf_tostring(&buf); \
    return ajj_value_assign( \
        ajj_object_create_string(a,a->rt->cur_gc,\
          str.str,str.len,1)); \
  } while(0)

#define STRING_COMP(a,L,OL,R,OR,CMP) \
  do { \
    int result = strcmp((L)->str,(R)->str) CMP 0; \
    return ajj_value_number(result); \
  } while(0)

DEFINE_BIN_HANDLER(vm_add,+,STRING_ADD,number)

#define STRING_COMP_EQ(A,B,C,D,E) STRING_COMP(A,B,C,D,E,==)
DEFINE_BIN_HANDLER(vm_eq,==,STRING_COMP_EQ,boolean)

#define STRING_COMP_NE(A,B,C,D,E) STRING_COMP(A,B,C,D,E,!=)
DEFINE_BIN_HANDLER(vm_ne,!=,STRING_COMP_NE,boolean)

#define STRING_COMP_LE(A,B,C,D,E) STRING_COMP(A,B,C,D,E,<=)
DEFINE_BIN_HANDLER(vm_le,<=,STRING_COMP_LE,boolean)

#define STRING_COMP_LT(A,B,C,D,E) STRING_COMP(A,B,C,D,E,<)
DEFINE_BIN_HANDLER(vm_lt,<,STRING_COMP_LT,boolean)

#define STRING_COMP_GE(A,B,C,D,E) STRING_COMP(A,B,C,D,E,>=)
DEFINE_BIN_HANDLER(vm_ge,>=,STRING_COMP_GE,boolean)

#define STRING_COMP_GT(A,B,C,D,E) STRING_COMP(A,B,C,D,E,>)
DEFINE_BIN_HANDLER(vm_gt,>,STRING_COMP_GT,boolean)

#undef DEFINE_BIN_HANDLER
#undef STRING_ADD
#undef STRING_COMP
#undef STRING_COMP_EQ
#undef STRING_COMP_NE
#undef STRING_COMP_LE
#undef STRING_COMP_LT
#undef STRING_COMP_GE
#undef STRING_COMP_GT

/* unary operation handler */
static
struct ajj_value vm_not(struct ajj* a , const struct ajj_value* val ,
    int* fail ) {
  UNUSE_ARG(a);
  *fail = 0;
  return is_true(val) ? AJJ_FALSE : AJJ_TRUE;
}

static
struct ajj_value vm_neg(struct ajj* a, const struct ajj_value* val,
    int* fail ) {
  double v = to_number(a,val,fail);
  if(*fail) return AJJ_NONE;
  return ajj_value_number(-v);
}

/* For C users, we don't allow them to call a script function in their
 * registered C function easily, but we need it for execution in our
 * VM. */
static
int call_script_func( struct ajj* a ,
    const char* name,
    struct ajj_value* par, size_t par_len , int* fail );

/* call */
/* The precondition of vm_call is that the function calling frame is
 * already setup, and the parameters are on the stack. Since we support
 * default parameters, we need to push arguments on top of stack as well.
 * This vm_call needs to handle that as well. */
static
struct ajj_value
vm_call(struct ajj* a, struct ajj_value* obj , int* fail) {
  struct func_frame* fr = cur_frame(a);
  struct ajj_value argnum = ajj_value_number(fr->par_cnt);
  /* setting up the upvalue for argnum ( function parameter count ) */
  set_upvalue(a,&ARGNUM,&argnum,1);

  if( IS_C(fr->entry) ) {
    struct ajj_value ret;
    const struct function* entry = fr->entry;
    struct ajj_value par[AJJ_FUNC_ARG_MAX_SIZE];
    size_t par_sz ;
    /* Populating the function parameter from stack and pass them
     * to the c_routine. Why don't use the LUA style to explicitly
     * pass parameter on a stack ? While, I guess having an array
     * of parameters making user feel less painful. */
    if( fr->par_cnt > AJJ_FUNC_ARG_MAX_SIZE ) {
      report_error(a,"Too much parameters passing into a c function/method.We \
          only allow %d function arguments!",AJJ_FUNC_ARG_MAX_SIZE);
      *fail = 1;
      return AJJ_NONE;
    }

    /* The left most parameter is on lower parts of the stack */
    for( par_sz = fr->par_cnt ; par_sz > 0 ; --par_sz ) {
      par[par_sz-1] = TOP(a,fr->par_cnt-par_sz+1);
    }
    par_sz = fr->par_cnt;
    if( IS_CFUNCTION(entry) ) {
      const struct c_closure* cc = &(entry->f.c_fn);
      assert( obj == NULL );
      *fail = cc->func(a,cc->udata,par,par_sz,&ret);
      if(*fail) return AJJ_NONE;
    } else {
      assert( obj != NULL );
      assert( IS_CMETHOD(fr->entry) );
      *fail = entry->f.c_mt(a,obj,par,par_sz);
      if(*fail) return AJJ_NONE;
    }

    *fail = 0;
    return ret;
  } else {
    struct program* prg = &(fr->entry->f.jj_fn);
    int i;

    assert( obj != NULL );
    if( fr->par_cnt > AJJ_FUNC_ARG_MAX_SIZE ||
        fr->par_cnt > prg->par_size ) {
      report_error(a,"Too much parameters passing into a jinja function "\
          "expect :%zu but get:%zu",prg->par_size,fr->par_cnt);
      *fail = 1;
      return AJJ_NONE;
    }
    for( i = fr->par_cnt ; i < prg->par_size ; ++i ) {
      push(a,prg->par_list[i].def_val);
    }
    *fail = 0;
    return AJJ_NONE;
  }
}

static
struct ajj_value
vm_attrcall(struct ajj* a, struct ajj_value* obj , int* fail) {
  assert(obj != NULL);
  return vm_call(a,obj,fail);
}

static
struct ajj_value vm_lstr( struct ajj* a, int idx ) {
  struct func_frame* fr = cur_frame(a);
  struct program* prg = &(fr->entry->f.jj_fn);
  const struct string* cstr = prg->str_tbl + idx;
  struct ajj_object* obj;
  assert(IS_JINJA(fr->entry));
  assert(prg->str_len > idx);
  obj = ajj_object_create_const_string(a,a->rt->cur_gc,cstr);
  return ajj_value_assign(obj);
}

static
struct ajj_value vm_lnum( struct ajj* a , int idx ) {
  struct func_frame* fr = cur_frame(a);
  struct program* prg = &(fr->entry->f.jj_fn);
  assert(IS_JINJA(fr->entry));
  assert(prg->num_len > idx);
  return ajj_value_number( prg->num_tbl[idx] );
}

static
struct ajj_value vm_llist( struct ajj* a ) {
  struct func_frame* fr = cur_frame(a);
  assert(IS_JINJA(fr->entry));
  return ajj_value_assign(
      ajj_object_create_list(a,a->rt->cur_gc));
}

static
struct ajj_value vm_ldict( struct ajj* a ) {
  struct func_frame* fr = cur_frame(a);
  assert( IS_JINJA(fr->entry) );
  return ajj_value_assign(
      ajj_object_create_dict(a,a->rt->cur_gc));
}

static
void vm_lift( struct ajj* a , int pos , int level ) {
  struct ajj_value* val = BOT(a,pos);
  if( AJJ_IS_PRIMITIVE(val) ) {
    return;
  } else {
    struct gc_scope* scp = val->value.object->scp;
    for( ; level > 0 ; --level ) {
      scp = scp->parent;
      assert( scp != NULL );
    }
    ajj_object_move(scp,val->value.object);
  }
}

static
void vm_attrset( struct ajj* a , struct ajj_value* obj,
    const struct ajj_value* key , const struct ajj_value* val ,
    int* fail ) {
  if( obj->type != AJJ_VALUE_DICT ) {
    *fail = 1;
    report_error(a,"Cannot push key value pair into a object has type:%s",
        ajj_value_get_type_name(obj));
  } else {
    struct map* d = &(obj->value.object->val.d);
    int own;
    struct string k = to_string(a,key,&own,fail);
    if(*fail) return;
    *fail = dict_insert(d,&k,own,val);
    if(*fail) {
      report_error(a,"Cannot add key value pair to dictionary," \
          "too many entries inside of the dictionary already!");
      return;
    }
    *fail = 0;
  }
}

static
struct ajj_value
vm_attrget( struct ajj* a , const struct ajj_value* obj,
    const struct ajj_value* key , int* fail ) {
  if( obj->type != AJJ_VALUE_DICT ||
      obj->type != AJJ_VALUE_LIST ) {
    *fail = 1;
    report_error(a,"Cannot lookup component on an object has type:%s",
        ajj_value_get_type_name(obj));
  } else {
    if( obj->type == AJJ_VALUE_LIST ) {
      struct list* l = &(obj->value.object->val.l);
      int idx = to_integer(a,key,fail);
      if(*fail) return AJJ_NONE;
      if( idx >= l->len ) {
        *fail = 1;
        report_error(a,"The index has size:%zu,but index is :%d." \
            "Out of boundary of list!",l->len,idx);
        return AJJ_NONE;
      }
      return *list_index(l,idx);
    } else {
      struct map* d = &(obj->value.object->val.d);
      struct ajj_value ret;
      const struct ajj_value* dict_ret;
      int own;
      struct string k = to_string(a,key,&own,fail);
      if(*fail) return AJJ_NONE;
      dict_ret = dict_find(d,&k);
      if( dict_ret == NULL ) {
        ret = AJJ_NONE;
      } else {
        ret = *dict_ret;
      }
      *fail = 0;
      if( own )
        string_destroy(&k);
      return ret;
    }
  }
}

static
void vm_attrpush( struct ajj* a , struct ajj_value* obj,
    const struct ajj_value* val , int* fail ) {
  if( obj->type != AJJ_VALUE_LIST ) {
    *fail = 1;
    report_error(a,"Cannot push a value into a none-list/tuple object." \
        "The object has type:%s!",ajj_value_get_type_name(val));
    return;
  } else {
    struct list* l = &(obj->value.object->val.l);
    list_push(l,val);
    *fail = 0;
    return;
  }
}

static
void vm_print( struct ajj* a , const struct string* str ) {
  FILE* f = a->rt->output;
  assert(f);
  fwrite(str->str,1,str->len+1,f); /* faster than fprintf */
}

static
void vm_enter( struct ajj* a) {
  a->rt->cur_gc = gc_scope_create(a,a->rt->cur_gc);
}

static
void vm_exit( struct ajj* a , int loops ) {
  for( ; loops > 0 ; --loops ) {
    struct gc_scope* p = a->rt->cur_gc->parent;
    assert(p != NULL);
    gc_scope_destroy(a,a->rt->cur_gc);
    a->rt->cur_gc = p;
  }
}

static
void setup_env( struct ajj* a , int cnt , int* fail ) {
  struct gvar_table* tb = gvar_table_create(a->gvar_tb);
  struct gvar_table* ptb = a->gvar_tb; /* parent table */
  a->gvar_tb = tb; /* change the global table */
  if( cnt == 0 ) { *fail = 1; return; }
  for( ; cnt > 0 ; --cnt ) {
    const int idx = 3*cnt;
    struct ajj_value* sym = TOP(a,idx);
    struct ajj_value* val = TOP(a,idx-1);
    struct ajj_value* opt = TOP(a,idx-2);
    int iopt = (int)(opt->value.number);
    int isym = (int)(sym->value.number);
    assert( sym->type == AJJ_VALUE_NUMBER );
    assert( opt->type == AJJ_VALUE_NUMBER );
    /* check the options */
    if( iopt == INCLUDE_UPVALUE_FIX ) {
      if(gvar_table_find(ptb,const_str(a,isym))) {
        continue;
      }
    }
    /* set up the value thing */
    set_upvalue(a,const_str(a,isym),val,1,fail);
    if(*fail) { *fail = 1; return; }
  }
  *fail = 0;
}

static
void setup_json_env( struct ajj* a , int cnt , int* fail );

static
void remove_env( struct ajj* a ) {
  struct gvar_table* tb;
  int itr;
  assert( a->gvar_tb->prev != NULL );
  tb = a->gvar_tb;
  a->gvar_tb = tb->prev;
  itr = map_iter_start(&(tb->d));
  while( map_iter_has(&(tb->d),itr) ) {
    struct map_pair entry = map_iter_deref(&(tb->d),itr);
    struct gvar* v = (struct gvar*)(entry.val);
    struct global_var* glb_var;
    assert(v->type == GVAR_VALUE);
    while(v->gut.value) {
      glb_var = v->gut.value->prev;
      ajj_value_destroy(a,&(v->gut.value->val));
      slab_free(&(a->glb_var_slab),v->gut.value);
      v->gut.value = glb_var;
    }
    itr = map_iter_next(&(tb->d),itr);
  }
  gvar_table_destroy(tb);
}

static
char* load_template( struct ajj* a , const struct ajj_value* fn ) {
  char* fc; /* file content */
  if( fn->type != AJJ_VALUE_STRING ) {
    report_error(a,"Include file name is not a string!");
    return NULL;
  }
  if((fc = ajj_aux_load_file(a,ajj_value_to_cstr(fn),NULL))==NULL){
    return NULL;
  }
  return fc;
}

static
void vm_include( struct ajj* a , int type, int cnt , int* fail ) {
  struct ajj_value* fn;
  char* fc = NULL;
  char* ro = NULL;
  size_t ro_len;
  struct runtime* rt;
  switch(type) {
    case INCLUDE_NONE:
      fn = TOP(a,1); /* filename */
      break;
    case INCLUDE_UPVALUE:
      fn = TOP(a,3*cnt+1); /* filename */
      break;
    case INCLUDE_JSON:
      fn = TOP(a,3*cnt+2); /* filename */
      break;
    default:
      UNREACHABLE();
      break;
  }
  if((fc=load_template(a,fn))==NULL) {
    *fail = 1;
    return;
  }
  if( type == INCLUDE_UPVALUE ) {
    setup_env(a,cnt,fail);
    if(*fail) goto fail;
  } else if( type == INCLUDE_JSON ) {
    setup_json_env(a,cnt,fail);
    if(*fail) goto fail;
  }

  rt = a->rt;
  if((ro=ajj_render(a,fc,&ro_len))==NULL)
    goto fail;
  a->rt = rt;
  fwrite(ro,1,ro_len,rt->output);
  free(fc);free(ro);
  /* remove the environment if it uses */
  switch(type) {
    case INCLUDE_NONE:
      pop(a,1);break;
    case INCLUDE_UPVALUE:
      pop(a,3*cnt+1);remove_env(a);break;
    case INCLUDE_JSON:
      pop(a,3*cnt+2);remove_env(a);break;
    default:
      UNREACHABLE();
      break;
  }
  *fail = 0; return;
fail:
  free(fc);free(ro);
  if( type != INCLUDE_NONE)
    remove_env(a);
  *fail = 1; return;
}

/* Use to resolve function when you call it globally, VM_CALL */
static
const struct function*
resolve_free_function( struct ajj* a, const struct string* name ,
    struct ajj_value** obj , int* fail );

/* Use to resolve function when you call it under an object, VM_ATTRCALL */
static
const struct function*
resolve_method( struct ajj* a , const struct ajj_value* val ,
    const struct string* name , int* fail );

/* helpers */
static
void enter_function( struct ajj* a , const struct function* f,
    size_t par_cnt , int method , int* fail ) {
  struct runtime* rt = a->rt;
  if( rt->cur_call_stk == AJJ_MAX_CALL_STACK ) {
    report_error(a,"Function recursive call too much," \
        "frame stack overflow!");
    *fail = 1;
  } else {
    struct func_frame* fr = rt->call_stk+rt->cur_call_stk;
    int prev_esp = rt->cur_call_stk == 0 ? 0 :
      rt->call_stk[rt->cur_call_stk-1].esp;

    int esp = prev_esp;

    int ebp = prev_esp > 0 ? esp - par_cnt : 0;

    assert( rt->cur_call_stk >0 );
    fr->entry = f;
    fr->name = f->name;
    fr->esp = esp;
    fr->ebp = ebp;
    fr->pc = 0;
    fr->par_cnt = par_cnt;
    fr->method = method;
    ++rt->call_stk;
    *fail = 0;
  }
}

static
int exit_function( struct ajj* a , const struct ajj_value* ret ,
    int* fail ) {
  struct func_frame* fr = cur_frame(a);
  struct runtime* rt = a->rt;
  int par_cnt = fr->par_cnt;
  assert( rt->cur_call_stk > 0 );
  --rt->cur_call_stk;
  fr->esp -= fr->method + par_cnt;
  assert( fr->esp >= fr->ebp );

  /* ugly hack to use push macro since it can exit, so we
   * set up fail to 1 in case it exit the function */
  *fail = 1;
  push(a,*ret);
  *fail = 0;
  return 0;
}

static
int exit_loop( struct ajj* a ) {
  struct func_frame* fr = cur_frame(a);
  struct runtime* rt = a->rt;
  int par_cnt = fr->par_cnt;
  assert( rt->cur_call_stk > 0 );
  --rt->cur_call_stk;
  fr->esp -= fr->method + par_cnt;
  assert( fr->esp >= fr->ebp );
}

static
int call_script_func( struct ajj* a , const char* name,
    struct ajj_value* par , size_t par_cnt , int* fail ) {
  /* resolve the function from the current template object
   * with name "name" */
  struct function* f = resolve_method(a,a->tmpl_tbl,name);
  struct program* prg = &(f->f.jj_fn);
  struct ajj_value argnum = ajj_value_to_number(par_cnt);
  size_t i;

  assert( IS_JINJA(f) );
  set_upvalue(a,&ARGNUM,&argnum,1);
  
  /* push the object value onto the stack */
  push(a,*par);

  /* push all the function ON TO the stack */
  for( i = 0 ; i < par_cnt ; ++i ) {
    push(a,par[i]);
  }

  enter_function(a,f,par_cnt,1,fail);
  if(!*fail) return -1;

  /* notify the VM to execute the current script function */
  return AJJ_EXEC_CONT;
}


#define CHECKN(...) &fail); \
  do { \
    if(fail) { \
      report_error(a,__VA_ARGS__); \
      goto fail; \
    } \
  } while(0

#define CHECK &fail); \
  do { \
    if(!fail) goto fail; \
  } while(0

/* We will not do any inline threading or direct threading for GCC.
 * Because currently I really don't care too much about the performance.
 * Since if we need to optimize it, lost of other places needs to be
 * optimized , not only the VM implementation. For simplicity and mostly
 * portability, I will use very old and traditional dispatching loop */
static
int vm_main( struct ajj* a ) {

  struct runtime* rt = a->rt;
  int fail;

#define vm_beg(X) case VM_##X:
#define vm_end(X) break;

  do {
    int instr = next_instr(a);
    switch(instr) {

      vm_beg(HALT) {
        goto done;
      } vm_end(HALT)

      vm_beg(ADD) {
        struct ajj_value o = vm_add(a,
          TOP(a,2),TOP(a,1),CHECK);
        pop(a,1);
        push(a,o);
      } vm_end(ADD)

      vm_beg(SUB) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,TOP(a,2),CHECK);
        r = to_number(a,TOP(a,1),CHECK);
        o = ajj_value_number(l+r);
        pop(a,2);
        push(a,o);
      } vm_end(SUB)

      vm_beg(DIV) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,TOP(a,2),CHECK);
        r = to_number(a,TOP(a,1),CHECK);
        if( r == 0 ) {
          report_error(a,"Divid by zero!");
          return -1;
        }
        o = ajj_value_number(l/r);
        pop(a,2);
        push(a,o);
      } vm_end(DIV)

      vm_beg(MUL) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,TOP(a,2),CHECK);
        r = to_number(a,TOP(a,1),CHECK);
        o = ajj_value_number(l*r);
        pop(a,2);
        push(a,o);
      } vm_end(MUL)

      vm_beg(POW) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,TOP(a,2),CHECK);
        r = to_number(a,TOP(a,1),CHECK);
        o = ajj_value_number(pow(l,r));
        pop(a,2);
        push(a,o);
      } vm_end(POW)

      vm_beg(EQ) {
        struct ajj_value o = vm_eq(a,TOP(a,2),TOP(a,1),CHECK);
        pop(a,2);
        push(a,o);
      } vm_end(EQ)

      vm_beg(NE) {
        struct ajj_value o = vm_ne(a,TOP(a,2),TOP(a,1),CHECK);
        pop(a,2);
        push(a,o);
      } vm_end(NE)

      vm_beg(LT) {
        struct ajj_value o = vm_lt(a,TOP(a,2),TOP(a,1),CHECK);
        pop(a,2);
        push(a,o);
      } vm_end(LT)

      vm_beg(LE) {
        struct ajj_value o = vm_le(a,TOP(a,2),TOP(a,1),CHECK);
        pop(a,2);
        push(a,o);
      } vm_end(LE)

      vm_beg(GT) {
        struct ajj_value o = vm_gt(a,TOP(a,2),TOP(a,1),CHECK);
        pop(a,2);
        push(a,o);
      } vm_end(GT)

      vm_beg(GE) {
        struct ajj_value o = vm_ge(a,TOP(a,2),TOP(a,1),CHECK);
        pop(a,2);
        push(a,o);
      } vm_end(GE)

      vm_beg(NOT) {
        struct ajj_value o = vm_not(a,TOP(a,1),CHECK);
        pop(a,1);
        push(a,o);
      } vm_end(NOT)

      vm_beg(NEG) {
        double val = to_number(a,TOP(a,1),CHECK);
        struct ajj_value o = ajj_value_number(-val);
        pop(a,2);
        push(a,o);
      } vm_end(NEG)

      vm_beg(DIVTRUCT) {
        double l,r;
        struct ajj_value o;
        l = to_number(a,TOP(a,2),CHECK);
        r = to_number(a,TOP(a,1),CHECK);
        if(r == 0.0) {
          report_error(a,"Divide by 0!");
          goto fail;
        }
        /* should be painful, but portable */
        o = ajj_value_number( (int64_t)(l/r) );
      } vm_end(DIVTRUCT)

      vm_beg(CALL) {
        int fn_idx= next_arg(a,CHECK);
        int an = next_arg(a,CHECK);
        struct ajj_value argnum = ajj_value_number(an);
        const struct string* fn = const_str(a,fn_idx);
        const struct function* f = resolve_free_function(a,fn,CHECK);
        if( f == NULL ) {
          report_error(a,"Cannot find function:%s!",fn->str);
          goto fail;
        } else {
          struct ajj_value ret;
          enter_function(a,f,an,0,CHECK);
          ret = vm_call(a,CHECK);

          /* We need to handle return value based on the function types,
           * if it is a jinja side function calling, we don't need to do
           * anything, but just return, since the VM_RET will take care
           * of everything. However, if we are calling a C functions, then
           * we need to exit the current function calling frame and also
           * push the return value on to the stack */
          if( IS_C(f) ) {
            exit_function(a,&ret,CHECK);
          }
        }
      } vm_end(CALL)

      vm_beg(ATTR_CALL) {
        int fn_idx = next_arg(a,CHECK);
        int an = next_arg(a,CHECK);
        struct ajj_value obj = TOP(a,an+1);
        struct ajj_value argnum = ajj_value_number(an);
        const struct string* fn = const_str(a,fn_idx);
        const struct function* f = resolve_method(a,&obj,fn,CHECK);
        if( f == NULL ) {
          report_error(a,"Cannot find object method:%s!",fn->str);
          goto fail;
        } else {
          struct ajj_value ret;
          enter_function(a,f,an,1,CHECK);
          ret = vm_attrcall(a,&obj,CHECK);
          if( IS_C(f) ) {
            exit_function(a,&ret,CHECK);
          }
        }
      } vm_end(ATTR_CALL)

      vm_beg(RET) {
        /* check if we have return value here or not !
         * if not, put a dummy NONE on top of the caller stack */
        struct func_frame* fr = cur_frame(a);
        struct ajj_value ret;
        if( fr->ebp + fr->par_cnt == fr->esp ) {
          /* nothing return */
          ret = AJJ_NONE;
        } else {
          assert( fr->ebp + fr->par_cnt == fr->esp -1 );
          ret = TOP(a,1); /* on top of the stack should be the
                         * return value in this situations */
        }
        /* do clean up things */
        exit_function(a,&ret,CHECK);
      } vm_end(RET)

      vm_beg(PRINT) {
        const struct string* text = to_string(a,TOP(a,1),CHECK);
        vm_print(a,text);
        pop(a,1);
      } vm_end(PRINT)

      vm_beg(pop) {
        int arg = next_arg(a,CHECK);
        pop(a,arg);
      } vm_end(pop)

      vm_beg(Tpush) {
        int arg = next_arg(a,CHECK);
        struct ajj_value val = *TOP(a,arg);
        push(a,val);
      } vm_end(Tpush)

      vm_beg(Bpush) {
        int arg = next_arg(a,CHECK);
        struct ajj_value val = *BOT(a,arg);
        push(a,val);
      } vm_end(Bpush)

      vm_beg(MOVE) {
        int l = next_arg(a,CHECK);
        int r = next_arg(a,CHECK);
        struct ajj_value temp = *BOT(a,l);
        *BOT(a,l) = *BOT(a,r);
        *BOT(a,r) = temp;
      } vm_end(MOVE)

      vm_beg(LIFT) {
        int l = next_arg(a,CHECK);
        int r = next_arg(a,CHECK);
        vm_lift(a,l,r);
      } vm_end(LIFT)

      vm_beg(STORE) {
        int dst = next_arg(a,CHECK);
        struct ajj_value src = *TOP(a,1);
        *BOT(a,dst) = src;
        pop(a,1);
      } vm_end(STORE)

      vm_beg(LSTR) {
        int arg = next_arg(a,CHECK);
        struct ajj_value val = vm_lstr(a,arg,CHECK);
        push(a,val);
      } vm_end(LSTR)

      vm_beg(LTRUE) {
        push(a,AJJ_TRUE);
      } vm_end(LTRUE)

      vm_beg(LFALSE) {
        push(a,AJJ_FALSE);
      } vm_end(LFALSE)

      vm_beg(LZERO) {
        struct ajj_value val = ajj_value_number(0);
        push(a,val);
      } vm_end(LZERO)

      vm_beg(LNONE) {
        push(a,AJJ_NONE);
      } vm_end(LNONE)

      vm_beg(LNUM) {
        int arg = next_arg(a);
        struct ajj_value val = vm_lnum(a,arg);
        push(a,val);
      } vm_end(LNUM)

      vm_beg(LIMM) {
        int imm = next_arg(a,CHECK);
        struct ajj_value val = ajj_value_number(imm);
        push(a,val);
      } vm_end(LIMM)

      vm_beg(LLIST) {
        struct ajj_value val = vm_llist(a,CHECK);
        push(a,val);
      } vm_end(LLIST)

      vm_beg(LDICT) {
        struct ajj_value val = vm_ldict(a,CHECK);
        push(a,val);
      } vm_end(LDICT)

      vm_beg(ATTR_SET) {
        struct ajj_value obj = TOP(a,3);
        struct ajj_value key = TOP(a,2);
        struct ajj_value val = TOP(a,1);
        vm_attrset(a,&obj,&key,&val,CHECK);
        pop(a,3);
      } vm_end(ATTRSET)

      vm_beg(ATTR_GET) {
        struct ajj_value obj = TOP(a,2);
        struct ajj_value key = TOP(a,1);
        struct ajj_value val =
          vm_attrget( a,&obj,&key,CHECK);
        pop(a,2);
        push(a,val);
      } vm_end(ATTR_GET)

      vm_beg(ATTR_push) {
        struct ajj_value obj = TOP(a,2);
        struct ajj_value val = TOP(a,1);
        vm_attrpush(a,&obj,&val,CHECK);
        pop(a,2);
      } vm_end(ATTR_push)

      vm_beg(UPVALUE_SET) {
        int par = next_arg(a,CHECK);
        const struct string* upvalue_name =
          const_str(a,par);
        struct ajj_value val = TOP(a,1);
        /* deteach the value to be owned by upvalue */
        set_upvalue(a,upvalue_name,&val,1,CHECK);
        pop(a,1);
      } vm_end(UPVALUE_SET)

      vm_beg(UPVALUE_GET) {
        int par = next_arg(a,CHECK);
        const struct string* upvalue_name =
          const_str(a,par);
        struct ajj_value val = get_upvalue(a,
            upvalue_name,CHECK);
        push(a,val);
      } vm_end(UPVALUE_GET)

      vm_beg(UPVALUE_DEL) {
        int par = next_arg(a,CHECK);
        const struct string* upvalue_name =
          const_str(a,par);
        del_upvalue(a,upvalue_name,CHECK);
      } vm_end(UPVALUE_DEL)

      vm_beg(JMP) {
        int pos = next_arg(a,CHECK);
        cur_frame(a)->pc = pos;
      } vm_end(JMP)

      vm_beg(JMPC) {
        int loops = next_arg(a,CHECK);
        int pos = next_arg(a,CHECK);
        assert(loops>0);
        vm_exit(a,loops);
        cur_frame(a)->pc = pos;
      } vm_end(JMPC)

      vm_beg(JT) {
        int pos = next_arg(a,CHECK);
        struct ajj_value cond = TOP(a,1);
        if( is_true(&cond) ) {
          cur_frame(a)->pos = pos;
        }
        pop(a,1);
      } vm_end(JT)

      vm_beg(JLT) {
        int pos = next_arg(a,CHECK);
        struct ajj_value cond = TOP(a,1);
        if( is_true(&cond) ) {
          cur_frame(a)->pos = pos;
        } else {
          pop(a,1);
        }
      } vm_end(JLT)

      vm_beg(JLF) {
        int pos = next_arg(a,CHECK);
        struct ajj_value cond = TOP(a,1);
        if( is_false(&cond) ) {
          cur_frame(a)->pos = pos;
        } else {
          pop(a,1);
        }
      } vm_end(JLF)

      vm_beg(JEPT) {
        int pos = next_arg(a,CHECK);
        struct ajj_value cond = TOP(a,1);
        int res = is_empty(a,&cond,&fail);
        /* We should always have an non-failed result since our object
         * should never be an incompatible object. The code generate this
         * instruction should always inside of a loop, but the loop call
         * stub should already be able to handle this situations since it
         * must check the object type before executing the codes */
        assert(fail);
        if( res ) {
          cur_frame(a)->pos = pos;
        }
        pop(a,1);
      } vm_end(JEPT)

      /* ITERATORS ------------------ */
      vm_beg(ITER_START) {
        struct ajj_value* obj = TOP(1);
        if( obj.type == AJJ_VALUE_LIST ) {
          struct list* l = &(obj->value.object->val.l);
          struct ajj_value itr = ajj_value_iter(
              list_iter_start(l));
          push(itr);
        } else if( obj.type == AJJ_VALUE_DICT ) {
          struct map* d = &(obj->value.object->val.d);
          struct ajj_value itr = ajj_value_iter(
              dict_iter_start(d));
          push(itr);
        } else {
          report_error(a,"Cannot iterate on object has type:%s!",
              ajj_value_get_type_name(obj));
          return -1;
        }
      } vm_end(ITER_START)

      vm_beg(ITER_HAS) {
        struct ajj_value* itr = TOP(1);
        struct ajj_value* obj = TOP(2);
        assert( itr->type == AJJ_VALUE_ITERATOR );
        assert( obj->type == AJJ_VALUE_LIST ||
                obj->type == AJJ_VALUE_DICT );
        if( obj->type == AJJ_VALUE_LIST ) {
          if( list_iter_has(&(obj->value.object->val.l),
                ajj_value_to_iter(itr)) ) {
            push(AJJ_TRUE);
          } else {
            push(AJJ_FALSE);
          }
        } else {
          if( dict_iter_has(&(obj->value.object->val.d),
                ajj_value_to_iter(itr)) ) {
            push(AJJ_TRUE);
          } else {
            push(AJJ_FALSE);
          }
        }
      } vm_end(ITER_HAS)

      vm_beg(ITER_DEREF) {
        int arg = next_arg(a,CHECK);
        struct ajj_value* obj = TOP(2);
        struct ajj_value* itr = TOP(1);
        assert( obj->type == AJJ_VALUE_LIST ||
                obj->type == AJJ_VALUE_DICT );
        assert( itr->type == AJJ_VALUE_ITERATOR );
        if( obj->type == AJJ_VALUE_LIST ) {
          struct list* l = &(obj->value.object->val.l);
          struct ajj_value val =
            *list_iter_deref(l,ajj_value_to_iter(itr));
          if( arg == 1 ) {
            push(val);
          } else {
            assert( arg == 2 );
            struct ajj_value idx =
              ajj_value_number(ajj_value_to_iter(itr));
            push(idx);
            push(val);
          }
        } else {
          struct map* d = &(obj->value.object->val.d);
          struct map_pair entry =
            dict_iter_deref(d,ajj_value_to_iter(itr));
          if( arg == 1 ) {
            struct ajj_value val = *entry.val;
            push(val);
          } else {
            struct ajj_value val = *entry.val;
            struct ajj_value key = ajj_value_assign(
                ajj_object_create_const_string(
                    a,a->rt->cur_gc,entry.val));
            push(key);
            push(val);
          }
        }
      }vm_end(ITER_DEREF)

      vm_beg(ITER_MOVE){
        struct ajj_value* obj = TOP(2);
        struct ajj_value* itr = TOP(1);
        assert( obj->type == AJJ_VALUE_LIST ||
                obj->type == AJJ_VALUE_DICT );
        assert( itr->type == AJJ_VALUE_ITERATOR );
        if( obj->type == AJJ_VALUE_LIST ) {
          struct list* l = &(obj->value.object->val.l);
          struct ajj_value nitr =
            ajj_value_iter(list_iter_move(l,ajj_value_to_iter(itr)));
          pop(1);
          push(nitr);
        } else {
          struct map* d = &(obj->value.object->val.d);
          struct ajj_value nitr =
            ajj_value_iter(dict_iter_move(d,ajj_value_to_iter(itr)));
          pop(1);
          push(nitr);
        }
      } vm_end(ITER_MOVE)

      /* MISC -------------------------------------- */
      vm_beg(ENTER) {
        vm_enter(a);
      } vm_end(ENTER)

      vm_beg(EXIT) {
        vm_exit(a,1);
      } vm_end(EXIT)

      vm_beg(INCLUDE) {
        int a1 = next_arg(a,CHECK);
        int a2 = next_arg(a,CHECK);
        vm_include(a,a1,a2,CHECK);
      } vm_end(INCLUDE)


  }while(1);

fail:

done:
}
