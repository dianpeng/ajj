#include "vm.h"
#include "ajj-priv.h"
#include "object.h"
#include "gc.h"
#include "util.h"
#include "bc.h"
#include "lex.h"
#include "upvalue.h"

#include <limits.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>


/* This FUNC_CALL actually means we want a TAIL call of a
 * new script functions. The return from the LOWEST script function
 * will end up its caller been poped as well. We don't return the
 * continuation from VM to the external C script again if it tries
 * to call a script function.
 * The called script function's return value will be carried over
 * to the caller of the C function that issue the script call.
 * It is used for implementing Super and Caller function, which the
 * C function will just look up the script function entry and then
 * call them, after that the return value from the script function
 * will be used as the return value for the C function builtin */

#define FUNC_CALL 1 /* indicate we want to call a script
                     * function recursively */


#define cur_frame(a) (&(a->rt->call_stk[a->rt->cur_call_stk-1]))
#define cur_function(a) (cur_frame(a)->entry)

/* stack manipulation routine */
#ifndef NDEBUG
static
struct ajj_value*
stack_value( struct ajj* a , int x ) {
  assert( x >= cur_frame(a)->ebp && x < cur_frame(a)->esp );
  return x + a->rt->val_stk;
}

#define bot(A,X) stack_value((A),cur_frame((A))->ebp+(X))
#define top(A,X) stack_value((A),cur_frame((A))->esp-(X))
#else
#define bot(A,X) ((A)->rt->val_stk[cur_frame((A))->ebp+(X)])
#define top(A,X) ((A)->rt->val_stk[cur_frame((A))->esp-(X)])
#endif /* NDEBUG */

/* This function tries to unwind the current stack and then dump
 * the useful information for user, hopefully it is useful :) */
static
int unwind_stack( struct ajj* a , char* buf ) {
  int i;
  char* start = buf;
  char* end = a->err + ERROR_BUFFER_SIZE -1 ;
  struct func_frame* fr = a->rt->call_stk;
  assert(buf < end);

  /* dump the code in reverse order */
  for( i = a->rt->cur_call_stk-1 ; i >= 0 ; --i ) {
    if( fr[i].obj == NULL ) {
      buf += snprintf(buf,end-buf,"%d %s:%s %d\n",i,
          "<free-func>",
          fr[i].name.str,
          fr[i].par_cnt);
      if( buf == end ) break;
    } else {
      const char* obj_name;
      if( fr[i].obj->tp == AJJ_VALUE_DICT )
        obj_name = "dict";
      else if( fr[i].obj->tp == AJJ_VALUE_LIST )
        obj_name = "list";
      else
        obj_name = fr[i].obj->val.obj.fn_tb->name.str;
      buf += snprintf(buf,end-buf,"%d %s:%s %d\n",i,
          obj_name,
          fr[i].name.str,
          fr[i].par_cnt);
      if( buf == end ) break;
    }
  }
  return buf - start;
}

static
void report_error( struct ajj* a , const char* fmt , ... ) {
  char* b = a->err;
  va_list vl;
  char* end = a->err + ERROR_BUFFER_SIZE;
  struct func_frame* fr = cur_frame(a);
  const char* src = a->rt->cur_obj->val.obj.src;
  char cbuf[32];
  assert(a->rt->cur_obj->tp == AJJ_VALUE_JINJA );
  assert(IS_JINJA(fr->entry));
  tk_get_code_snippet(src,
      fr->entry->f.jj_fn.spos[fr->ppc]
      ,cbuf,32);
  b += sprintf(b,"(... %s ...)",cbuf);
  va_start(vl,fmt);
  b += vsnprintf(b,end-b,fmt,vl);
  *b = '\n'; ++b;
  b += unwind_stack(a,b);
}

/* push/pop to manipulate the value stack */
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
struct runtime* runtime_create( struct ajj* a , FILE* output ) {
  struct runtime* rt = malloc(sizeof(*rt));
  rt->cur_obj = NULL;
  rt->cur_call_stk = 0;
  rt->cur_gc = &(a->gc_root);
  rt->output = output;
  return rt;
}

static
void runtime_destroy( struct ajj* a , struct runtime* rt ) {
  struct gc_scope* c = rt->cur_gc;
  while( c != &(a->gc_root) ) {
    struct gc_scope* n = c->parent;
    gc_scope_destroy(a,c);
    c = n;
  }
  free(rt);
}

/* =============================
 * Decoding
 * ===========================*/

static
int next_instr( struct ajj* a ) {
  struct func_frame* fr;
  const struct program* prg;
  assert( a->rt->cur_call_stk > 0 );
  fr = cur_frame(a);
  assert(IS_JINJA(fr->entry));
  prg = GET_JINJAFUNC(fr->entry);
  fr->ppc = fr->pc;
  return bc_next(prg,&(fr->pc));
}

static
int instr_1st_arg( int c ) {
  return bc_1st_arg(c);
}

static
int instr_2nd_arg( struct ajj* a ) {
  struct func_frame* fr;
  const struct program* prg;
  assert( a->rt->cur_call_stk > 0 );
  fr = cur_frame(a);
  assert(IS_JINJA(fr->entry));
  prg = GET_JINJAFUNC(fr->entry);
  return bc_2nd_arg(prg,&(fr->pc));
}

/* ============================
 * Upvalue handler
 * ==========================*/

static
void set_upvalue( struct ajj* a, const struct string* name,
    const struct ajj_value* value ) {
  struct upvalue* uv = upvalue_table_add(a,
      a->upval_tb,name,0);
  uv->type = UPVALUE_VALUE;
  uv->gut.val = *value;
}

static
void overwrite_upvalue( struct ajj* a , const struct string* name,
    const struct ajj_value* value ) {
  struct upvalue* uv = upvalue_table_overwrite(a,
      a->upval_tb,name,0);
  uv->type = UPVALUE_VALUE;
  uv->gut.val = *value;
}

static
struct ajj_value
get_upvalue( struct ajj* a , const struct string* name ) {
  struct upvalue* uv = upvalue_table_find(a->upval_tb,name);
  if( !uv || uv->type != UPVALUE_VALUE )
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
int is_int( double val ) {
  double i;
  modf(val,&i);
  if( i < INT_MAX && i > INT_MIN )
    return i == val;
  else
    return 0;
}

static
double str_to_number( struct ajj* a , const char* str , int* fail ) {
  double val;
  char* pend;
  errno = 0;
  val = strtod(str,&pend);
  if( errno || str == pend ) {
    if(errno)
      report_error(a,"Cannot convert str:%s to number because:%s",
          str,
          strerror(errno));
    else
      report_error(a,"Cannot convert str:%s to number!",str);
    *fail = 1;
    return 0;
  }
  *fail = 0;
  return val;
}

static
struct string
to_display_string( struct ajj* a , const struct ajj_value* val ,
    int* own ) {
  switch(val->type) {
    case AJJ_VALUE_BOOLEAN:
      *own = 0;
      return ajj_value_to_boolean(val) ?
        TRUE_STRING : FALSE_STRING;
    case AJJ_VALUE_NUMBER:
      {
        char buf[256];
        if(is_int(ajj_value_to_number(val))) {
          sprintf(buf,"%d",(int)ajj_value_to_number(val));
        } else {
          sprintf(buf,"%f",ajj_value_to_number(val));
        }
        *own = 1;
        return string_dupc(buf);
      }
    case AJJ_VALUE_STRING:
      *own = 0;
      return *ajj_value_to_string(val);
    default:
      *own = 0;
      return EMPTY_STRING;
  }
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
        double d = val->value.number;
        if( d > INT_MAX || d < INT_MIN ) {
          report_error(a,"Cannot convert value:%f to integer, overflow!",
              d);
          *fail = 1;
          return -1;
        }
        *fail =0;
        return (int)(d);
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
    struct string str = string_concate(L,R); \
    if(OL) string_destroy(L); \
    if(OR) string_destroy(R); \
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

static
struct ajj_value vm_mul( struct ajj* a , const struct ajj_value* l,
    const struct ajj_value* r , int* fail ) {
  if( l->type == AJJ_VALUE_STRING ||
      r->type == AJJ_VALUE_STRING ) {
    int own;
    struct string s;
    struct string str;
    int i;
    const struct ajj_value* str_ajj;
    const struct ajj_value* num_ajj;

    if( l->type == AJJ_VALUE_STRING ) {
      str_ajj = l ; num_ajj = r;
    } else {
      str_ajj = r ; num_ajj = l;
    }

    s = to_string(a,str_ajj,&own,fail);
    if(*fail) return AJJ_NONE;
    i = to_number(a,num_ajj,fail);
    if(*fail) return AJJ_NONE;

    str = string_multiply(&s,i);
    if(own) string_destroy(&s);

    return ajj_value_assign(
        ajj_object_create_string(a,a->rt->cur_gc,
          str.str,str.len,1));
  } else {
    double lv , rv;
    lv = to_number(a,l,fail);
    if(*fail) return AJJ_NONE;
    rv = to_number(a,r,fail);
    if(*fail) return AJJ_NONE;
    return ajj_value_number(lv*rv);
  }
}

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

static
int call_ctor( struct ajj* a , struct func_table* ft,
    struct ajj_value* ret ) {
  size_t pc = cur_frame(a)->par_cnt;
  size_t i;
  void* udata;
  struct ajj_object* obj;
  struct ajj_value par[AJJ_FUNC_ARG_MAX_SIZE];
  if( pc >= AJJ_FUNC_ARG_MAX_SIZE ) {
    report_error(a,"Too much prameters passing into a object CTOR. We \
        only allow at most :%d function arguments!",AJJ_FUNC_ARG_MAX_SIZE);
    return -1;
  }

  /* push the parametr into the stack */
  for( i = pc ; i >0 ; --i ) {
    par[i-1] = *top(a,pc-i+1);
  }

  if( ft->ctor(
      a,
      ft->udata,
      par,
      pc,
      &udata) ) {
    return -1;
  }

  obj = ajj_object_create_obj(a,a->rt->cur_gc,ft,udata);
  *ret = ajj_value_assign(obj);
  return 0; /* finish the call */
}

static
void set_func_upvalue( struct ajj* a ) {
  struct func_frame* fr = cur_frame(a);
  struct ajj_value argnum = ajj_value_number(fr->par_cnt);
  struct ajj_value fname = ajj_value_assign(
      ajj_object_create_const_string(a,
        a->rt->cur_gc,&(fr->name)));
  overwrite_upvalue(a,&ARGNUM,&argnum);
  overwrite_upvalue(a,&FUNC,&fname);
}

/* call */
/* The precondition of vm_call is that the function calling frame is
 * already setup, and the parameters are on the stack. Since we support
 * default parameters, we need to push arguments on top of stack as well.
 * This vm_call needs to handle that as well. */
static
int
vm_call(struct ajj* a, struct ajj_object* obj ,
    struct ajj_value* ret ) {
  int rval;
  struct func_frame* fr = cur_frame(a);

  /* set up the upvalue */
  set_func_upvalue(a);

  if( IS_C(fr->entry) ) {
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
      return -1;
    }

    /* The left most parameter is on lower parts of the stack */
    for( par_sz = fr->par_cnt ; par_sz > 0 ; --par_sz ) {
      par[par_sz-1] = *top(a,fr->par_cnt-par_sz+1);
    }
    par_sz = fr->par_cnt;
    if( IS_CFUNCTION(entry) ) {
      const struct c_closure* cc = &(entry->f.c_fn);
      assert( obj == NULL );
      rval = cc->func(a,cc->udata,par,par_sz,ret);
      assert( rval >= -1 && rval <= 1);
      return rval;
    } else {
      assert( obj != NULL );
      assert( IS_CMETHOD(fr->entry) );
      rval = entry->f.c_mt(a,obj,par,par_sz,ret);
      assert( rval >= -1 && rval <= 1);
      return rval;
    }
  } else {
    if( IS_JINJA(fr->entry) ) {
      const struct program* prg = &(fr->entry->f.jj_fn);
      int i;

      assert( obj != NULL );
      if( fr->par_cnt > AJJ_FUNC_ARG_MAX_SIZE ||
          fr->par_cnt > prg->par_size ) {
        report_error(a,"Too much parameters passing into a jinja function "\
            "expect :%zu but get:%zu",prg->par_size,fr->par_cnt);
        return -1;
      }
      for( i = fr->par_cnt ; i < prg->par_size ; ++i ) {
        push(a,prg->par_list[i].def_val);
      }
      return FUNC_CALL; /* continue call */
    } else {
      struct func_table* ft;
      assert( IS_OBJECTCTOR(fr->entry) );
      assert( obj == NULL );
      ft = fr->entry->f.obj_ctor;
      return call_ctor(a,ft,ret);
    }
  }
}

static
int
vm_attrcall(struct ajj* a, struct ajj_object* obj ,
    struct ajj_value* ret ) {
  assert(obj != NULL);
  return vm_call(a,obj,ret);
}

static
struct ajj_value vm_lstr( struct ajj* a, int idx ) {
  struct func_frame* fr = cur_frame(a);
  const struct program* prg = &(fr->entry->f.jj_fn);
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
  const struct program* prg = &(fr->entry->f.jj_fn);
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
  struct ajj_value* val = bot(a,pos);
  if( AJJ_IS_PRIMITIVE(val) ) {
    return;
  } else {
    struct gc_scope* scp = a->rt->cur_gc;
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
void setup_env( struct ajj* a , int cnt ) {
  assert(cnt>0);
  a->upval_tb = upvalue_table_create( a->upval_tb );
  for( ; cnt > 0 ; --cnt ) {
    const int idx = 3*cnt;
    struct ajj_value* sym = top(a,idx);
    struct ajj_value* val = top(a,idx-1);
    struct ajj_value* opt = top(a,idx-2);
    int iopt = (int)(opt->value.number);
    int isym = (int)(sym->value.number);
    const struct string* k = const_str(a,isym);
    assert( sym->type == AJJ_VALUE_NUMBER );
    assert( opt->type == AJJ_VALUE_NUMBER );
    /* check the options */
    if( iopt == UPVALUE_FIX ) {
      if( upvalue_table_find(a->upval_tb,k) != NULL )
        continue;
    }
    /* set up the value thing */
    set_upvalue(a,k,val);
  }
}

static
void setup_json_env( struct ajj* a , int cnt , int* fail ) {
  assert(0);
}

static
void remove_env( struct ajj* a ) {
  a->upval_tb = upvalue_table_destroy_one(a,a->upval_tb);
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
  struct runtime* rt;
  switch(type) {
    case INCLUDE_UPVALUE:
      fn = top(a,3*cnt+1); /* filename */
      break;
    case INCLUDE_JSON:
      fn = top(a,3*cnt+2); /* filename */
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
    if(cnt>0)
      setup_env(a,cnt);
  } else if( type == INCLUDE_JSON ) {
    setup_json_env(a,cnt,fail);
  }
  rt = a->rt;
  if( ajj_render(a,fc,ajj_value_to_cstr(fn),a->rt->output) )
    goto fail;
  a->rt = rt;
  free(fc);
  /* remove the environment if it uses */
  switch(type) {
    case INCLUDE_UPVALUE:
      pop(a,3*cnt+1);
      if(cnt >0 ) remove_env(a);
      break;
    case INCLUDE_JSON:
      pop(a,3*cnt+2);
      remove_env(a);
      break;
    default:
      UNREACHABLE();
      break;
  }
  *fail = 0; return;
fail:
  free(fc);
  if( (type == INCLUDE_UPVALUE && cnt >  0) ||
      type == INCLUDE_JSON )
    remove_env(a);
  *fail = 1; return;
}

static
const struct function*
resolve_obj_method( struct ajj* a , struct ajj_object* val,
    const struct string* name ) {
  size_t i;
  struct func_table* ft = val->val.obj.fn_tb;
  for( i = 0 ; i < ft->func_len ; ++i ) {
    if( string_eq(&(ft->func_tb[i].name),name) ) {
      return ft->func_tb + i;
    }
  }
  return NULL;
}

/* Use to resolve function when you call it under an object, VM_ATTRCALL */
static
const struct function*
resolve_method( struct ajj* a , struct ajj_object** val ,
    const struct string* name ) {
  struct ajj_object* tmpl = *val;
  int p = 0;
  const struct function* f;
  do {
    /* search the function inside of the template/object */
    if( (f = resolve_obj_method(a,tmpl,name)) ) {
      *val = tmpl;
      return f;
    }
    if( p < (*val)->parent_len )
      tmpl = (*val)->parent[ p++ ];
    else
      break;
  } while(1);
  return NULL;
}

/* Use to resolve function when you call it globally, VM_CALL.
 * This name is not really correct since for this function we
 * can still get a object call or method call. It is just a
 * correpsonding function search to VM_CALL instruction not the
 * VM_ATTR_CALL. The search chain is as follow:
 * 1. Search the current template objects
 * 2. Search its parent objects from left to right
 * 3. Search global functions */

static
const struct function*
resolve_free_function( struct ajj* a, const struct string* name ,
    struct ajj_object** obj ) {
  const struct function* f;
  struct upvalue* uv;
  *obj = a->rt->cur_obj;
  assert( a->rt->cur_obj );

  if((f = resolve_method(a,obj,name)))
    return f;

  /* searching through the global function table */
  uv = upvalue_table_find( a->upval_tb, name );
  *obj = NULL;
  if(uv) {
    if(uv->type == UPVALUE_FUNCTION) {
      return &(uv->gut.gfunc);
    }
  }
  return NULL;
}

/* helpers */
static
void enter_function( struct ajj* a , const struct function* f,
    int par_cnt , struct ajj_object* obj , int* fail ) {
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

    fr->entry = f;
    fr->name = f->name;
    fr->esp = esp;
    fr->ebp = ebp;
    fr->pc = 0;
    fr->par_cnt = par_cnt;
    fr->obj = obj;
    /* only update the rt->cur_obj when we enter into a function
     * call that really has a object, otherwise just don't update*/
    if(obj) a->rt->cur_obj = obj;
    ++rt->cur_call_stk;
    *fail = 0;
  }
}

static
int exit_function( struct ajj* a , const struct ajj_value* ret ,
    int* fail ) {
  struct func_frame* fr = cur_frame(a);
  struct runtime* rt = a->rt;
  int par_cnt = fr->par_cnt;
  assert(rt->cur_call_stk >0);
  --rt->cur_call_stk;
  if(fr->obj && rt->cur_call_stk >0)
    fr->esp -= 1 + par_cnt;
  else
    fr->esp -= par_cnt;
  assert( fr->esp >= fr->ebp );
  /* ugly hack to use push macro since it can exit, so we
   * set up fail to 1 in case it exit the function */
  *fail = 1;
  push(a,*ret);
  *fail = 0;

  /* udpate the rt->cur_obj */
  a->rt->cur_obj = cur_frame(a)->obj;
  return 0;
}

static
int call_script_func( struct ajj* a , const char* name,
    struct ajj_value* par , size_t par_cnt , int* fail ) {
  /* resolve the function from the current template object
   * with name "name" */
  struct ajj_object* root_obj = a->rt->cur_obj;
  struct string n = string_const(name,strlen(name));
  const struct function* f = resolve_obj_method(a,root_obj,&n);
  const struct program* prg = &(f->f.jj_fn);
  size_t i;

  assert(f); /* internal usage, should never return NULL */

  assert( IS_JINJA(f) );
  set_func_upvalue(a);

  /* push the object value onto the stack */
  push(a,*par);

  /* push all the function ON TO the stack */
  for( i = 0 ; i < par_cnt ; ++i ) {
    push(a,par[i]);
  }

  enter_function(a,f,par_cnt,root_obj,fail);
  if(!*fail) return -1;

  /* notify the VM to execute the current script function */
  return FUNC_CALL;
}

#define RCHECK &fail); \
  do { \
    if(fail) goto fail; \
  } while(0

/* The function for invoke a certain code. Before entering into this
 * function user should already prepared well for the stack and the
 * target the function must be already on the top of the stack */
static
int vm_main( struct ajj* a ) {

  struct runtime* rt = a->rt;
  int fail;

#define vm_beg(X) case VM_##X:
#define vm_end(X) break;

  do {
    int c = next_instr(a);
    instructions instr = BC_INSTRUCTION(c);

    switch(instr) {
      vm_beg(ADD) {
        struct ajj_value o = vm_add(a,
          top(a,2),top(a,1),RCHECK);
        pop(a,2);
        push(a,o);
      } vm_end(ADD)

      vm_beg(SUB) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,top(a,2),RCHECK);
        r = to_number(a,top(a,1),RCHECK);
        o = ajj_value_number(l+r);
        pop(a,2);
        push(a,o);
      } vm_end(SUB)

      vm_beg(DIV) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,top(a,2),RCHECK);
        r = to_number(a,top(a,1),RCHECK);
        if( r == 0 ) {
          report_error(a,"Divid by zero!");
          return -1;
        }
        o = ajj_value_number(l/r);
        pop(a,2);
        push(a,o);
      } vm_end(DIV)

      vm_beg(MUL) {
        struct ajj_value o = vm_mul(a,top(a,2),top(a,1),RCHECK);
        pop(a,2);
        push(a,o);
      } vm_end(MUL)

      vm_beg(POW) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,top(a,2),RCHECK);
        r = to_number(a,top(a,1),RCHECK);
        o = ajj_value_number(pow(l,r));
        pop(a,2);
        push(a,o);
      } vm_end(POW)

      vm_beg(MOD) {
        int l , r;
        struct ajj_value o;
        l = to_integer(a,top(a,2),RCHECK);
        r = to_integer(a,top(a,1),RCHECK);
        o = ajj_value_number(l%r);
        pop(a,2);
        push(a,o);
      } vm_end(MOD)

      vm_beg(EQ) {
        struct ajj_value o = vm_eq(a,top(a,2),top(a,1),RCHECK);
        pop(a,2);
        push(a,o);
      } vm_end(EQ)

      vm_beg(NE) {
        struct ajj_value o = vm_ne(a,top(a,2),top(a,1),RCHECK);
        pop(a,2);
        push(a,o);
      } vm_end(NE)

      vm_beg(LT) {
        struct ajj_value o = vm_lt(a,top(a,2),top(a,1),RCHECK);
        pop(a,2);
        push(a,o);
      } vm_end(LT)

      vm_beg(LE) {
        struct ajj_value o = vm_le(a,top(a,2),top(a,1),RCHECK);
        pop(a,2);
        push(a,o);
      } vm_end(LE)

      vm_beg(GT) {
        struct ajj_value o = vm_gt(a,top(a,2),top(a,1),RCHECK);
        pop(a,2);
        push(a,o);
      } vm_end(GT)

      vm_beg(GE) {
        struct ajj_value o = vm_ge(a,top(a,2),top(a,1),RCHECK);
        pop(a,2);
        push(a,o);
      } vm_end(GE)

      vm_beg(NOT) {
        struct ajj_value o = vm_not(a,top(a,1),RCHECK);
        pop(a,1);
        push(a,o);
      } vm_end(NOT)

      vm_beg(NEG) {
        double val = to_number(a,top(a,1),RCHECK);
        struct ajj_value o = ajj_value_number(-val);
        pop(a,1);
        push(a,o);
      } vm_end(NEG)

      vm_beg(DIVTRUCT) {
        double l,r;
        struct ajj_value o;
        l = to_number(a,top(a,2),RCHECK);
        r = to_number(a,top(a,1),RCHECK);
        if(r == 0.0) {
          report_error(a,"Divide by 0!");
          goto fail;
        }
        /* should be painful, but portable */
        o = ajj_value_number( (int64_t)(l/r) );
        pop(a,2);
        push(a,o);
      } vm_end(DIVTRUCT)

      vm_beg(CALL) {
        int fn_idx= instr_1st_arg(c);
        int an = instr_2nd_arg(a);

        struct ajj_value argnum = ajj_value_number(an);
        const struct string* fn = const_str(a,fn_idx);
        struct ajj_object* obj;
        const struct function* f =
          resolve_free_function(a,fn,&obj);
        if( f == NULL ) {
          report_error(a,"Cannot find function:%s!",fn->str);
          goto fail;
        } else {
          struct ajj_value ret;
          int r;
          enter_function(a,f,an,0,RCHECK);
          r = vm_call(a,obj,&ret);
          if( r < 0 )
            goto fail;
          else if( r == 0 ) {
            assert( IS_C(f) );
            /* We need to handle return value based on the function types,
             * if it is a jinja side function calling, we don't need to do
             * anything, but just return, since the VM_RET will take care
             * of everything. However, if we are calling a C functions, then
             * we need to exit the current function calling frame and also
             * push the return value on to the stack */
            exit_function(a,&ret,RCHECK);
          }
        }
      } vm_end(CALL)

      vm_beg(ATTR_CALL) {
        int fn_idx = instr_1st_arg(c);
        int an = instr_2nd_arg(a);

        struct ajj_value obj = *top(a,an+1);
        struct ajj_value argnum = ajj_value_number(an);
        const struct string* fn = const_str(a,fn_idx);
        struct ajj_object* o = obj.value.object;

        const struct function* f = resolve_obj_method(a,o,fn);
        if( f == NULL ) {
          report_error(a,"Cannot find object method:%s!",fn->str);
          goto fail;
        } else {
          struct ajj_value ret;
          int r;
          enter_function(a,f,an,o,RCHECK);
          r = vm_attrcall(a,o,&ret);
          if( r < 0 ) {
            goto fail;
          } else if( r == 0 ) {
            if( IS_C(f) ) {
              exit_function(a,&ret,RCHECK);
            }
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
          ret = *top(a,1); /* on top of the stack should be the
                            * return value in this situations */
        }
        /* do clean up things */
        exit_function(a,&ret,RCHECK);
        /* check the current function frame to see whether we previously
         * had a c function directly calls into a script function. If so,
         * we just pop that c function again, looks like one return pops
         * 2 function frames */
        if( a->rt->cur_call_stk > 0 ) {
          fr = cur_frame(a);
          if( IS_C(fr->entry) ) {
            /* do a consecutive pop here */
            pop(a,1); /* pop the original return value on stack */
            exit_function(a,&ret,RCHECK); /* exit the current function again */
          }
        } else {
          /* We unwind the last function frame, so we can return :) */
          goto done;
        }
      } vm_end(RET)

      vm_beg(PRINT) {
        int own;
        struct string text = to_display_string(a,top(a,1),&own);
        vm_print(a,&text);
        pop(a,1);
        if(own) string_destroy(&text);
      } vm_end(PRINT)

      vm_beg(POP) {
        int arg = instr_1st_arg(c);
        pop(a,arg);
      } vm_end(POP)

      vm_beg(TPUSH) {
        int arg = instr_1st_arg(c);
        struct ajj_value val = *top(a,arg);
        push(a,val);
      } vm_end(TPUSH)

      vm_beg(BPUSH) {
        int arg = instr_1st_arg(c);
        struct ajj_value val = *bot(a,arg);
        push(a,val);
      } vm_end(BPUSH)

      vm_beg(MOVE) {
        int l = instr_1st_arg(c);
        int r = instr_2nd_arg(a);

        struct ajj_value temp = *bot(a,l);
        *bot(a,l) = *bot(a,r);
        *bot(a,r) = temp;
      } vm_end(MOVE)

      vm_beg(LIFT) {
        int l = instr_1st_arg(c);
        int r = instr_2nd_arg(a);

        vm_lift(a,l,r);
      } vm_end(LIFT)

      vm_beg(STORE) {
        int dst = instr_1st_arg(c);
        struct ajj_value src = *top(a,1);
        *bot(a,dst) = src;
        pop(a,1);
      } vm_end(STORE)

      vm_beg(LSTR) {
        int arg = instr_1st_arg(c);
        struct ajj_value val = vm_lstr(a,arg);
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
        int arg = instr_1st_arg(c);
        struct ajj_value val = vm_lnum(a,arg);
        push(a,val);
      } vm_end(LNUM)

      vm_beg(LIMM) {
        int imm = instr_1st_arg(c);
        struct ajj_value val = ajj_value_number(imm);
        push(a,val);
      } vm_end(LIMM)

      vm_beg(LLIST) {
        struct ajj_value val = vm_llist(a);
        push(a,val);
      } vm_end(LLIST)

      vm_beg(LDICT) {
        struct ajj_value val = vm_ldict(a);
        push(a,val);
      } vm_end(LDICT)

      vm_beg(ATTR_SET) {
        struct ajj_value obj = *top(a,3);
        struct ajj_value key = *top(a,2);
        struct ajj_value val = *top(a,1);
        vm_attrset(a,&obj,&key,&val,RCHECK);
        pop(a,3);
      } vm_end(ATTRSET)

      vm_beg(ATTR_GET) {
        struct ajj_value obj = *top(a,2);
        struct ajj_value key = *top(a,1);
        struct ajj_value val =
          vm_attrget( a,&obj,&key,RCHECK);
        pop(a,2);
        push(a,val);
      } vm_end(ATTR_GET)

      vm_beg(ATTR_PUSH) {
        struct ajj_value obj = *top(a,2);
        struct ajj_value val = *top(a,1);
        vm_attrpush(a,&obj,&val,RCHECK);
        pop(a,2);
      } vm_end(ATTR_PUSH)

      vm_beg(UPVALUE_SET) {
        int par = instr_1st_arg(c);
        const struct string* upvalue_name =
          const_str(a,par);
        struct ajj_value val = *top(a,1);
        /* deteach the value to be owned by upvalue */
        set_upvalue(a,upvalue_name,&val);
        pop(a,1);
      } vm_end(UPVALUE_SET)

      vm_beg(UPVALUE_GET) {
        int par = instr_1st_arg(c);
        const struct string* upvalue_name =
          const_str(a,par);
        struct ajj_value val = get_upvalue(a,
            upvalue_name);
        push(a,val);
      } vm_end(UPVALUE_GET)

      vm_beg(UPVALUE_DEL) {
        int par = instr_1st_arg(c);
        const struct string* upvalue_name =
          const_str(a,par);
        del_upvalue(a,upvalue_name);
      } vm_end(UPVALUE_DEL)

      vm_beg(JMP) {
        int pos =instr_1st_arg(c);
        cur_frame(a)->pc = pos;
      } vm_end(JMP)

      vm_beg(JMPC) {
        int loops = instr_1st_arg(c);
        int pos = instr_2nd_arg(a);
        assert(loops>0);
        vm_exit(a,loops);
        cur_frame(a)->pc = pos;
      } vm_end(JMPC)

      vm_beg(JT) {
        int pos = instr_1st_arg(c);
        struct ajj_value cond = *top(a,1);
        if( is_true(&cond) ) {
          cur_frame(a)->pc = pos;
        }
        pop(a,1);
      } vm_end(JT)

      vm_beg(JF) {
        int pos = instr_1st_arg(c);
        struct ajj_value cond = *top(a,1);
        if( is_false(&cond) ) {
          cur_frame(a)->pc = pos;
        }
        pop(a,1);
      } vm_end(JF)

      vm_beg(JLT) {
        int pos = instr_1st_arg(c);
        struct ajj_value cond = *top(a,1);
        if( is_true(&cond) ) {
          cur_frame(a)->pc = pos;
        } else {
          pop(a,1);
        }
      } vm_end(JLT)

      vm_beg(JLF) {
        int pos = instr_1st_arg(c);
        struct ajj_value cond = *top(a,1);
        if( is_false(&cond) ) {
          cur_frame(a)->pc = pos;
        } else {
          pop(a,1);
        }
      } vm_end(JLF)

      vm_beg(JEPT) {
        int pos = instr_1st_arg(c);
        struct ajj_value cond = *top(a,1);
        int res = is_empty(a,&cond,&fail);
        /* We should always have an non-failed result since our object
         * should never be an incompatible object. The code generate this
         * instruction should always inside of a loop, but the loop call
         * stub should already be able to handle this situations since it
         * must check the object type before executing the codes */
        assert(!fail);
        if( res ) {
          cur_frame(a)->pc = pos;
        }
        pop(a,1);
      } vm_end(JEPT)

      /* ITERATORS ------------------ */
      vm_beg(ITER_START) {
        struct ajj_value* obj = top(a,1);
        if( obj->type == AJJ_VALUE_LIST ) {
          struct list* l = &(obj->value.object->val.l);
          struct ajj_value itr = ajj_value_iter(
              list_iter_start(l));
          push(a,itr);
        } else if( obj->type == AJJ_VALUE_DICT ) {
          struct map* d = &(obj->value.object->val.d);
          struct ajj_value itr = ajj_value_iter(
              dict_iter_start(d));
          push(a,itr);
        } else {
          report_error(a,"Cannot iterate on object has type:%s!",
              ajj_value_get_type_name(obj));
          return -1;
        }
      } vm_end(ITER_START)

      vm_beg(ITER_HAS) {
        struct ajj_value* itr = top(a,1);
        struct ajj_value* obj = top(a,2);
        assert( itr->type == AJJ_VALUE_ITERATOR );
        assert( obj->type == AJJ_VALUE_LIST ||
                obj->type == AJJ_VALUE_DICT );
        if( obj->type == AJJ_VALUE_LIST ) {
          if( list_iter_has(&(obj->value.object->val.l),
                ajj_value_to_iter(itr)) ) {
            push(a,AJJ_TRUE);
          } else {
            push(a,AJJ_FALSE);
          }
        } else {
          if( dict_iter_has(&(obj->value.object->val.d),
                ajj_value_to_iter(itr)) ) {
            push(a,AJJ_TRUE);
          } else {
            push(a,AJJ_FALSE);
          }
        }
      } vm_end(ITER_HAS)

      vm_beg(ITER_DEREF) {
        int arg = instr_1st_arg(c);
        struct ajj_value* obj = top(a,2);
        struct ajj_value* itr = top(a,1);
        assert( obj->type == AJJ_VALUE_LIST ||
                obj->type == AJJ_VALUE_DICT );
        assert( itr->type == AJJ_VALUE_ITERATOR );
        if( obj->type == AJJ_VALUE_LIST ) {
          struct list* l = &(obj->value.object->val.l);
          struct ajj_value val =
            *list_iter_deref(l,ajj_value_to_iter(itr));
          if( arg == 1 ) {
            push(a,val);
          } else {
            assert( arg == 2 );
            struct ajj_value idx =
              ajj_value_number(ajj_value_to_iter(itr));
            push(a,idx);
            push(a,val);
          }
        } else {
          struct map* d = &(obj->value.object->val.d);
          struct map_pair entry =
            dict_iter_deref(d,ajj_value_to_iter(itr));
          if( arg == 1 ) {
            struct ajj_value val = *(struct ajj_value*)entry.val;
            push(a,val);
          } else {
            struct ajj_value val = *(struct ajj_value*)entry.val;
            struct ajj_value key = ajj_value_assign(
                ajj_object_create_const_string(
                    a,a->rt->cur_gc,entry.val));
            push(a,key);
            push(a,val);
          }
        }
      }vm_end(ITER_DEREF)

      vm_beg(ITER_MOVE){
        struct ajj_value* obj = top(a,2);
        struct ajj_value* itr = top(a,1);
        assert( obj->type == AJJ_VALUE_LIST ||
                obj->type == AJJ_VALUE_DICT );
        assert( itr->type == AJJ_VALUE_ITERATOR );
        if( obj->type == AJJ_VALUE_LIST ) {
          struct list* l = &(obj->value.object->val.l);
          struct ajj_value nitr =
            ajj_value_iter(list_iter_move(l,ajj_value_to_iter(itr)));
          pop(a,1);
          push(a,nitr);
        } else {
          struct map* d = &(obj->value.object->val.d);
          struct ajj_value nitr =
            ajj_value_iter(dict_iter_move(d,ajj_value_to_iter(itr)));
          pop(a,1);
          push(a,nitr);
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
        int a1 = instr_1st_arg(c);
        int a2 = instr_2nd_arg(a);
        vm_include(a,a1,a2,RCHECK);
      } vm_end(INCLUDE)


      /* NOPS, should not exist after optimization */
      vm_beg(NOP0) {
      } vm_end(NOP0)

      vm_beg(NOP1) {
      } vm_end(NOP1)

      vm_beg(NOP2) {
        (void)instr_2nd_arg(a);
      } vm_end(NOP2)

      default:
        UNREACHABLE();
        break;
    }
  }while(1);

fail:
  return -1;
done:
  assert( a->rt->cur_call_stk == 0 );
  return 0;
}

int vm_run_jinja( struct ajj* a , struct ajj_object* jj,
    FILE* output ) {
  struct runtime* rt = runtime_create(a,output);
  const struct function* main;
  int fail;
  a->rt = rt;
  /* let's enter the main function now */
  main = ajj_object_jinja_main_func(jj);
  enter_function(a,main,0,jj,&fail);
  assert(!fail);
  set_func_upvalue(a);
  fail = vm_main(a);
  runtime_destroy(a,rt);
  a->rt = NULL;
  return fail;
}
