#include "vm.h"
#include "ajj-priv.h"
#include "object.h"
#include "gc.h"
#include "util.h"
#include "bc.h"
#include "lex.h"
#include "upvalue.h"
#include "builtin.h"

#include <limits.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* =============================
 * Forward
 * ===========================*/
static
void set_upvalue( struct ajj* a, const struct string* name,
    const struct ajj_value* value ,
    int force ,
    int fixed ,
    int* fail );

static
const struct function*
resolve_test_function( struct ajj* a, const struct string* name );

static
void enter_function( struct ajj* a , const struct function* f,
    int par_cnt , int method ,
    struct ajj_object* obj , int* fail );

static
int exit_function( struct ajj* a , const struct ajj_value* ret );

static
int run_jinja(struct ajj*);

/* ===========================================
 * Macros
 * =========================================*/
#define del_upvalue(A,N) \
  do { \
    upvalue_table_del(A,(A)->rt->global, \
        N,&((A)->env)); \
  } while(0)

/* This FUNC_CALL actually means we want a TAIL call of a
 * new script functions. The return from the LOWEST script function
 * will end up its caller been stk_poped as well. We don't return the
 * continuation from VM to the external C script again if it tries
 * to call a script function.
 * The called script function's return value will be carried over
 * to the caller of the C function that issue the script call.
 * It is used for implementing Super and Caller function, which the
 * C function will just look up the script function entry and then
 * call them, after that the return value from the script function
 * will be used as the return value for the C function builtin */

#define VM_FUNC_CALL 1 /* indicate we want to call a script
                        * function recursively */

#define cur_frame(a) (&(a->rt->call_stk[a->rt->cur_call_stk-1]))
#define cur_function(a) (cur_frame(a)->entry)
#define cur_jinja(a) ((a)->rt->jinja)

/* stack manipulation routine */
#ifndef NDEBUG
static
struct ajj_value*
stack_value( struct ajj* a , int x ) {
  assert( x >= cur_frame(a)->ebp );
  assert( x < cur_frame(a)->esp );
  return x + a->rt->val_stk;
}

#define stk_bot(A,X) stack_value((A),cur_frame((A))->ebp+(X))
#define stk_top(A,X) stack_value((A),cur_frame((A))->esp-(X))
#else
#define stk_bot(A,X) ((A)->rt->val_stk+cur_frame((A))->ebp+(X))
#define stk_top(A,X) ((A)->rt->val_stk+cur_frame((A))->esp-(X))
#endif /* NDEBUG */

int program_add_par( struct program* prg , struct string* name ,
    int own, const struct ajj_value* val ) {
  if( prg->par_size == AJJ_FUNC_ARG_MAX_SIZE )
    return -1;
  else {
    assert(name->len < AJJ_SYMBOL_NAME_MAX_SIZE );
    prg->par_list[prg->par_size].def_val = *val; /* owned the value */
    prg->par_list[prg->par_size].name = own ? *name : string_dup(name);
    ++prg->par_size;
    return 0;
  }
}

int program_const_str( struct program* prg , struct string* str ,
    int own ) {
  if( str->len > SMALL_STRING_THRESHOLD ) {
insert:
    if( prg->str_len == prg->str_cap ) {
      prg->str_tbl = mem_grow(prg->str_tbl,
          sizeof(struct string),
          0,
          &(prg->str_cap));
    }
    if(own) {
      prg->str_tbl[prg->str_len] = *str;
    } else {
      prg->str_tbl[prg->str_len] = string_dup(str);
    }
    return prg->str_len++;
  } else {
    size_t i = 0 ;
    for( ; i < prg->str_len ; ++i ) {
      if( string_eq(prg->str_tbl+i,str) ) {
        if(own) string_destroy(str);
        return i;
      }
    }
    goto insert;
  }
}

int program_const_num( struct program* prg , double num ) {
  size_t i;
  if( prg->num_len== prg->num_cap ) {
    prg->num_tbl = mem_grow(
        prg->num_tbl,sizeof(double),
        0,
        &(prg->num_cap));
  }
  for( i = 0 ; i < prg->num_len ; ++i ) {
    if( num == prg->num_tbl[i] )
      return i;
  }
  prg->num_tbl[prg->num_len] = num;
  return prg->num_len++;
}

void program_init( struct program* prg ) {
  prg->codes = NULL;
  prg->spos = NULL;
  prg->len = 0;

  prg->str_len = 0;
  prg->str_cap = AJJ_LOCAL_CONSTANT_SIZE;
  prg->str_tbl = malloc(sizeof(
        struct string)*AJJ_LOCAL_CONSTANT_SIZE);

  prg->num_len = 0;
  prg->num_cap = AJJ_LOCAL_CONSTANT_SIZE;
  prg->num_tbl = malloc(sizeof(
        double)*AJJ_LOCAL_CONSTANT_SIZE);

  prg->par_size =0;
}

/* This function tries to unwind the current stack and then dump
 * the useful information for user, hopefully it is useful :) */
static
int unwind_stack( struct ajj* a , char* buf ) {
  int i;
  size_t ln;
  size_t pos;
  char* start = buf;
  char* end = a->err + ERROR_BUFFER_SIZE -1 ;
  struct func_frame* fr = a->rt->call_stk;
  const char* obj_name;
  if(buf>=end) return 0;

  /* dump the code in reverse order */
  for( i = a->rt->cur_call_stk-1 ; i >= 0 ; --i ) {
    /* if this frame comes from jinja template, also
     * dump out the code segment in the jinja template */
    char cbuf[64];
    if( IS_JINJA(fr[i].entry) ) {
      const char* src = fr[i].obj->val.obj.src;
      size_t ppc = fr[i].entry->f.jj_fn.spos[fr->ppc];
      assert(src);
      tk_get_code_snippet(
          src,
          ppc,
          cbuf,
          ARRAY_SIZE(cbuf));
      tk_get_coordinate(src,
          ppc,
          &ln,
          &pos);
    } else {
      strcpy(cbuf,"<no-source>");
    }

    if(fr[i].obj) {
      obj_name = GET_OBJECT_TYPE_NAME(fr[i].obj)->str;
    } else {
      obj_name = "free-func";
    }

    if(IS_JINJA(fr[i].entry)) {
      buf += snprintf(buf,end-buf,
          "%d:(... %s ...)@(" SIZEF ":" SIZEF ") <%s>:%s arg-num:%d\n",
          i,
          cbuf,
          SIZEP(ln),
          SIZEP(pos),
          obj_name,
          fr[i].name.str,
          fr[i].par_cnt);
    } else {
      buf += snprintf(buf,end-buf,
          "%d:(... <no-source> ...) <%s>:%s arg-num:%d\n",
          i,
          obj_name,
          fr[i].name.str,
          fr[i].par_cnt);
    }
    if(buf >= end) break;
  }
  return buf - start;
}

static
void vm_rpt_err( struct ajj* a , const char* fmt , ... ) {
  char* b = a->err;
  va_list vl;
  char* end = a->err + ERROR_BUFFER_SIZE;
  va_start(vl,fmt);
  b += vsnprintf(b,end-b,fmt,vl);
  *b = '\n'; ++b;
  unwind_stack(a,b);
}

/* This function is used to rewrite user throwned error
 * into our own error stack plus stack unwind information */
static
void rewrite_error( struct ajj* a ) {
  char msg[ERROR_BUFFER_SIZE];
  strcpy(msg,a->err);
  vm_rpt_err(a,"%s",msg);
}

/* stk_push/stk_pop to manipulate the value stack */
static
void stk_pop(struct ajj* a , int off ) {
  int val = off;
  struct func_frame* fr = cur_frame(a);
  assert(fr->esp >= val);
  fr->esp -= val;
}

#define stk_push(a,v) \
  do { \
    struct func_frame* fr = cur_frame(a); \
    if( fr->esp == a->rt->val_stk_cap ) { \
      a->rt->val_stk = mem_grow(a->rt->val_stk, \
          sizeof(struct ajj_value), \
          0, \
          &(a->rt->val_stk_cap)); \
    } \
    a->rt->val_stk[fr->esp] = v; \
    ++(fr->esp); \
  } while(0)

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
void runtime_init( struct ajj* a , struct runtime* rt,
    struct ajj_object* jinja, struct ajj_io* output , int cnt ) {
  rt->inc_cnt = cnt;
  rt->next = NULL;
  rt->prev = NULL;
  rt->jinja = jinja;
  rt->cur_call_stk = 0;
  rt->cur_gc = rt->root_gc =
    gc_scope_create(a,&(a->gc_root));
  rt->val_stk_cap = AJJ_INIT_VALUE_STACK_SIZE;
  rt->val_stk = malloc(sizeof(struct ajj_value)*
      AJJ_INIT_VALUE_STACK_SIZE);
  rt->output = output;
  rt->global = upvalue_table_create(&(a->env));
}

static
void runtime_destroy( struct ajj* a , struct runtime* rt ) {
  struct gc_scope* c = rt->cur_gc;
  const struct gc_scope* end = &(a->gc_root);
  while(c != end ) {
    struct gc_scope* n = c->parent;
    gc_scope_destroy(a,c);
    c = n;
  }
  /* destroy all the global variable scope */
  upvalue_table_destroy(a,rt->global,&(a->env));
  free(rt->val_stk);
}

static
struct ajj_value create_loop_object( struct ajj* a, size_t len ) {
  struct ajj_value lval;
  struct ajj_object* lobj;

  lobj = ajj_object_create_loop(a,
      a->rt->cur_gc,
      len);
  lval = ajj_value_assign(lobj);
  return lval;
}

/* =============================
 * Program
 * ===========================*/
void program_destroy( struct program* prg ) {
  int i;
  for( i = 0 ; i < prg->str_len ; ++i ) {
    string_destroy(prg->str_tbl+i);
  }

  for( i = 0 ; i < prg->par_size ; ++i ) {
    string_destroy(&(prg->par_list[i].name));
  }

  free(prg->codes);
  free(prg->spos);
  free(prg->str_tbl);
  free(prg->num_tbl);
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

#define instr_1st_arg(c) bc_1st_arg(c)

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
    const struct ajj_value* value ,
    int force ,
    int fixed ,
    int* fail ) {
  struct upvalue* uv = upvalue_table_add(
      a,a->rt->global,name,0,force,fixed);
  if(uv == NULL) {
    *fail = 1;
    vm_rpt_err(a,"Cannot setup upvalue with name:%s,possibly "
        "collide with builtin variable name!",
        name->str);
    return;
  }
  *fail = 0;
  uv->type = UPVALUE_VALUE;
  /* We don't need to move the value from its scope
   * to the root scope of this jinja template. The
   * reason is because this function is always called
   * in a scope based , so this upvalue will always be
   * deleted when it goes out of the scope so the upvalue
   * will never outlive the lifecycle of this *value* */
  uv->gut.val = *value;
}

static
struct ajj_value
get_upvalue( struct ajj* a , const struct string* name ) {
  struct upvalue* uv = upvalue_table_find(
      a->rt->global,name,NULL);
  if( !uv || uv->type != UPVALUE_VALUE )
    return AJJ_NONE;
  else
    return uv->gut.val;
}

/* =============================
 * Type conversions
 * ===========================*/
static
int str_to_number( const char* str , double* val ) {
  char* pend;
  errno = 0;
  *val = strtod(str,&pend);
  if( errno || str == pend ) {
    return -1;
  }
  return 0;
}

int vm_to_number( const struct ajj_value* val, double* d ) {
  assert( val->type != AJJ_VALUE_NOT_USE );
  switch(val->type) {
    case AJJ_VALUE_BOOLEAN:
      *d = ajj_value_to_boolean(val);
      return 0;
    case AJJ_VALUE_NUMBER:
      *d = ajj_value_to_number(val);
      return 0;
    case AJJ_VALUE_STRING:
      return str_to_number(ajj_value_to_cstr(val),d);
    default:
      return -1;
  }
}

static
double to_number( struct ajj* a , const struct ajj_value* val , int* fail ) {
  double ret;
  assert( val->type != AJJ_VALUE_NOT_USE );
  if( vm_to_number(val,&ret) ) {
    vm_rpt_err(a,"Cannot convert to number!");
    *fail = 1;
    return 0;
  } else {
    *fail = 0;
    return ret;
  }
}

int vm_to_string( const struct ajj_value* val,
    struct string* str, int* own ) {
  switch(val->type) {
    case AJJ_VALUE_BOOLEAN:
      *own = 0;
      *str = (ajj_value_to_boolean(val) ?
        TRUE_STRING : FALSE_STRING);
      return 0;
    case AJJ_VALUE_NUMBER:
      *own = 1;
      str->str = dtoc(val->value.number,
          &(str->len));
      return 0;
    case AJJ_VALUE_STRING:
      *own = 0;
      *str = *ajj_value_to_string(val);
      return 0;
    case AJJ_VALUE_NONE:
      *own = 0;
      *str = EMPTY_STRING;
      return 0;
    default:
      *own = 0;
      return -1;
  }
}

static
struct string
to_string( struct ajj* a , const struct ajj_value* val ,
    int* own , int* fail ) {
  struct string s;
  if(vm_to_string(val,&s,own)) {
    *fail = 1;
    vm_rpt_err(a,"Cannot convert to string!");
    return EMPTY_STRING;
  } else {
    *fail = 0;
    return s;
  }
}

int vm_to_integer( const struct ajj_value* val , int* o ) {
  assert( val->type != AJJ_VALUE_NOT_USE );
  switch( val->type ) {
    case AJJ_VALUE_BOOLEAN:
      return ajj_value_to_boolean(val);
    case AJJ_VALUE_NUMBER:
      {
        double d = val->value.number;
        if( d > INT_MAX || d < INT_MIN ) {
          return -1;
        }
        *o = (int)(d);
        return 0;
      }
    default:
      return -1;
  }
}

static
int to_integer( struct ajj* a, const struct ajj_value* val, int* fail ) {
  int o = 0;
  if(vm_to_integer(val,&o)) {
    vm_rpt_err(a,"Cannot convert value to integer!");
    *fail = 1;
    return -1;
  } else {
    *fail = 0;
    return o;
  }
}

int vm_to_boolean( const struct ajj_value* val ) {
  assert( val->type != AJJ_VALUE_NOT_USE );
  switch(val->type) {
    case AJJ_VALUE_NUMBER:
      return ajj_value_to_number(val) != 0;
    case AJJ_VALUE_BOOLEAN:
      return ajj_value_to_boolean(val);
    default:
      return 0;
  }
}

/* =============================
 * Specific instruction handler
 * ============================*/
static
struct ajj_value vm_cat( struct ajj* a,
    const struct ajj_value* l,
    const struct ajj_value* r ) {
  int own_l , own_r;
  struct string ls ;
  struct string rs ;
  struct string str;
  ls.str = ajj_display(a,l,&(ls.len),&own_l);
  rs.str = ajj_display(a,r,&(rs.len),&own_r);
  str = string_concate(&ls,&rs);
  if(own_l) string_destroy(&ls);
  if(own_r) string_destroy(&rs);
  return ajj_value_assign(
      ajj_object_create_string(a,a->rt->cur_gc,
        str.str,str.len,1));
}

static
struct ajj_value vm_add(struct ajj* a,
    const struct ajj_value* l,
    const struct ajj_value* r,
    int* fail ) {
  if( l->type == AJJ_VALUE_STRING ||
      r->type == AJJ_VALUE_STRING ) {
    int own_l , own_r;
    struct string ls ;
    struct string rs ;
    struct string str;
    ls = to_string(a,l,&own_l,fail);
    if( *fail ) return AJJ_NONE;
    rs = to_string(a,r,&own_r,fail);
    if( *fail ) {
      if(own_l)
        string_destroy(&ls);
      return AJJ_NONE;
    }
    str = string_concate(&ls,&rs);
    if(own_l) string_destroy(&ls);
    if(own_r) string_destroy(&rs);
    return ajj_value_assign(
        ajj_object_create_string(a,a->rt->cur_gc,
          str.str,str.len,1));
  } else {
    double ln , rn;
    ln = to_number(a,l,fail);
    if( *fail ) return AJJ_NONE;
    rn = to_number(a,r,fail);
    if( *fail ) return AJJ_NONE;
    return ajj_value_number( ln + rn );
  }
}

#define DEFINE_BIN_HANDLER(T) \
  static \
  struct ajj_value vm_##T( struct ajj* a , \
      const struct ajj_value* l, \
      const struct ajj_value* r, \
      int* fail ) { \
    int cmp; \
    if( ajj_value_##T(a,l,r,&cmp) ) { \
      *fail = 1; rewrite_error(a); return AJJ_NONE; \
    } else { \
      *fail = 0; return ajj_value_boolean(cmp); \
    } \
  }

DEFINE_BIN_HANDLER(eq)
DEFINE_BIN_HANDLER(ne)
DEFINE_BIN_HANDLER(le)
DEFINE_BIN_HANDLER(lt)
DEFINE_BIN_HANDLER(ge)
DEFINE_BIN_HANDLER(gt)

#undef DEFINE_BIN_HANDLER

/* handle multiply */
static
struct ajj_value vm_mul( struct ajj* a , const struct ajj_value* l,
    const struct ajj_value* r , int* fail ) {
  if( (l->type == AJJ_VALUE_STRING && r->type != AJJ_VALUE_STRING) ||
      (r->type == AJJ_VALUE_STRING && l->type != AJJ_VALUE_STRING) ) {
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
  return vm_is_true(val) ? AJJ_FALSE : AJJ_TRUE;
}

static
struct ajj_value vm_len(struct ajj* a,
    const struct ajj_value* val , int* fail ) {
  size_t res;
  if(ajj_value_len(a,val,&res)) {
    *fail = 1;
    rewrite_error(a);
    return AJJ_NONE;
  } else {
    *fail = 0;
    return ajj_value_number(res);
  }
}

static
struct ajj_value
vm_in( struct ajj* a , struct ajj_value* obj ,
    const struct ajj_value* val , int* fail ) {
  int res;
  if( ajj_value_in(a,obj,val,&res) ) {
    *fail = 1;
    rewrite_error(a);
    return AJJ_NONE;
  } else {
    *fail = 0;
    return ajj_value_boolean(res);
  }
}

static
int
is_empty( struct ajj* a , struct ajj_value* val , int* fail ) {
  int res;
  assert( val->type != AJJ_VALUE_NOT_USE );
  if( ajj_value_empty(a,val,&res) ) {
    *fail = 1;
    rewrite_error(a);
    return -1;
  } else {
    *fail = 0;
    return res;
  }
}

/* For C users, we don't allow them to call a script function in their
 * registered C function easily, but we need it for execution in our
 * VM. */

static
int call_ctor( struct ajj* a , struct func_table* ft,
    struct ajj_value* ret ) {
  size_t pc = cur_frame(a)->par_cnt;
  size_t i;
  int tp = -1; /* sanity check */
  void* udata;
  struct ajj_object* obj;
  struct ajj_value par[AJJ_FUNC_ARG_MAX_SIZE];
  assert(pc < AJJ_FUNC_ARG_MAX_SIZE);

  /* stk_push the parametr into the stack */
  for( i = pc ; i >0 ; --i ) {
    par[i-1] = *stk_top(a,pc-i+1);
  }

  if( ft->ctor(
      a,
      ft->udata,
      par,
      pc,
      &udata,
      &tp) ) {
    return AJJ_EXEC_FAIL;
  }

  assert(tp != -1); /* sanity check failed */

  obj = ajj_object_create_obj(
      a,a->rt->cur_gc,ft,udata,tp);
  *ret = ajj_value_assign(obj);

  /* now call move function to ensure the internal
   * referenced value of this object is in the correct
   * gc scope */
  if( ft->slot.move ) {
    ft->slot.move(a,ret);
  }

  return AJJ_EXEC_OK;
}

static
void set_func_builtin_vars( struct ajj* a,
    int argnum, /* argnum */
    const struct string* func, /* function name */
    struct ajj_object* vargs, /* NULL if no */
    const struct string* caller /* NULL if no */ ) {
  struct func_frame* fr = cur_frame(a);

  stk_push(a,ajj_value_number(argnum)); /* argnum */
  stk_push(a,ajj_value_assign(
        ajj_object_create_const_string(a,a->rt->cur_gc,
          func))); /* func */
  stk_push(a,vargs == NULL ? AJJ_NONE :
      ajj_value_assign(vargs)); /* vargs */
  if(caller) {
    stk_push(a,ajj_value_assign(
          ajj_object_create_const_string(a,a->rt->cur_gc,
            caller)));
  } else {
    stk_push(a,AJJ_NONE);
  }
  if( fr->obj == NULL ) {
    stk_push(a,AJJ_NONE);
  } else {
    stk_push(a,ajj_value_assign(fr->obj));
  }
}

/* This function helps to prepare the arguments for a script
 * call and also set up the 4 bulitin varaibles accordingly*/
static
void prepare_script_call( struct ajj* a ) {
  struct func_frame* fr = cur_frame(a);
  const struct program* prg = GET_JINJAFUNC(fr->entry);
  size_t i;
  struct ajj_object* vargs = NULL;
  int real_an = fr->par_cnt;
  assert(fr->par_cnt <= AJJ_FUNC_ARG_MAX_SIZE);

  /* stk_push default parameters if we need to */
  for( i = fr->par_cnt ; i < prg->par_size ; ++i ) {
    stk_push(a,prg->par_list[i].def_val);
  }

  /* check if we have extra args */
  if( (size_t)fr->par_cnt > prg->par_size ) {
    /* the extra arguments passed in serves as the vargs
     * local variables here */
    vargs = ajj_object_create_list(a,a->rt->cur_gc);
    for( i = prg->par_size ; i < fr->par_cnt ; ++i ) {
      size_t idx = (fr->par_cnt-i);
      builtin_list_push(a,vargs,stk_top(a,idx));
    }
    stk_pop(a,fr->par_cnt-prg->par_size);
  }

  /* set up the builtin variables */
  /* currently the stack only covers the parameter that passed
   * in the builtin variable is not allocated here, so we need
   * to allocate the space on stack and set the correct value*/
  set_func_builtin_vars(a,
      real_an,
      &(fr->name),
      vargs,
      a->rt->cur_call_stk > 1 ? &((fr-1)->name) : NULL );
}

/* call */
/* The precondition of vm_call is that the function calling frame is
 * already setup, and the parameters are on the stack. Since we support
 * default parameters, we need to stk_push arguments on stk_top of stack as well.
 * This vm_call needs to handle that as well.
 * Notes: this function must be called right after enter_function since it
 * will setup function builtin vars which is stack related */
static
int
vm_call(struct ajj* a, struct ajj_object* obj ,
    struct ajj_value* ret ) {
  int rval;
  struct func_frame* fr = cur_frame(a);
  struct ajj_value v_obj =
    (obj?ajj_value_assign(obj):AJJ_NONE);

  if( IS_C(fr->entry) ) {
    const struct function* entry = fr->entry;
    struct ajj_value par[AJJ_FUNC_ARG_MAX_SIZE];
    size_t par_sz ;
    assert(fr->par_cnt <= AJJ_FUNC_ARG_MAX_SIZE);

    /* The left most parameter is on lower parts of the stack */
    for( par_sz = fr->par_cnt ; par_sz > 0 ; --par_sz ) {
      par[par_sz-1] = *stk_top(a,fr->par_cnt-par_sz+1);
    }
    par_sz = fr->par_cnt;
    /* c function and test function has same prototype. */
    if( IS_CFUNCTION(entry) || IS_TEST(entry) ) {
      const struct c_closure* cc = &(entry->f.c_fn);
      assert( obj == NULL );
      rval = cc->func(a,cc->udata,par,par_sz,ret);
      if(rval == AJJ_EXEC_FAIL) {
        rewrite_error(a);
      }
      return rval;
    } else {
      assert( obj != NULL );
      assert( IS_CMETHOD(fr->entry) );
      rval = entry->f.c_mt(a,&v_obj,par,par_sz,ret);
      if(rval == AJJ_EXEC_FAIL) {
        rewrite_error(a);
      }
      return rval;
    }
  } else {
    if( IS_JINJA(fr->entry) ) {
      assert( obj != NULL );
      prepare_script_call(a);
      return VM_FUNC_CALL; /* continue call */
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
int vm_attrcall(struct ajj* a, struct ajj_object* obj ,
    struct ajj_value* ret ) {
  assert(obj != NULL);
  return vm_call(a,obj,ret);
}

static
void vm_test( struct ajj* a, int fn_idx, int an , int pos , int* fail ) {
  const struct string* fn;
  const struct function* f;
  fn = const_str(a,fn_idx);
  f = resolve_test_function(a,fn);
  if(f == NULL) {
    vm_rpt_err(a,"Cannot resolve test function:%s!",
        fn->str);
    *fail = 1; return;
  } else {
    struct ajj_value ret;
    int r;
    enter_function(a,f,an,0,NULL,fail);
    if(*fail) return;

    r = vm_call(a,NULL,&ret);
    assert( r == AJJ_EXEC_FAIL || r == AJJ_EXEC_OK );
    if(r) {
      *fail = 1; return;
    } else {
      assert( IS_TEST(f) );
      /* check return value since all the test function
       * is supposed to return a boolean value */
      if( ret.type != AJJ_VALUE_BOOLEAN ) {
        vm_rpt_err(a,"Test function:%s return value is not "
            "boolean value, all the test function should return "
            "boolean value!",fn->str);
        *fail =1; return;
      }
      if(!pos) {
        if(ret.value.boolean)
          ret = AJJ_FALSE;
        else
          ret = AJJ_TRUE;
      }
      /* test function is always a C function, so we need
       * to stk_pop the function frame right after calling the
       * function */
      exit_function(a,&ret);
    }
  }
  *fail = 0;
}

static
struct ajj_value vm_lstr( struct ajj* a, int idx ) {
  struct func_frame* fr = cur_frame(a);
  const struct program* prg = &(fr->entry->f.jj_fn);
  const struct string* cstr = prg->str_tbl + idx;
  struct ajj_object* obj;
  assert(IS_JINJA(fr->entry));
  assert(prg->str_len > (size_t)idx);
  obj = ajj_object_create_const_string(
      a,a->rt->cur_gc,cstr);
  return ajj_value_assign(obj);
}

#define vm_lnum(A,IDX) ajj_value_number(const_num(A,IDX))

#define vm_llist(A) ajj_value_assign( \
    ajj_object_create_list((A),(A)->rt->cur_gc))

#define vm_ldict(A) \
  ajj_value_assign(ajj_object_create_dict((A),(A)->rt->cur_gc))

static
void vm_lift( struct ajj* a , int pos , int level ) {
  struct ajj_value* val = stk_bot(a,pos);
  if( AJJ_IS_PRIMITIVE(val) ) {
    return;
  } else {
    struct gc_scope* scp = a->rt->cur_gc;
    for( ; level > 0 ; --level ) {
      scp = scp->parent;
      assert( scp != NULL );
    }
    ajj_object_move(a,scp,val->value.object);
  }
}

static
void vm_attrset( struct ajj* a , struct ajj_value* obj,
    const struct ajj_value* key , const struct ajj_value* val ,
    int* fail ) {
  if( ajj_value_attr_set(a,obj,key,val) ) {
    rewrite_error(a);
    *fail = 1;
  } else {
    *fail = 0;
  }
}

static
struct ajj_value
vm_attrget( struct ajj* a , struct ajj_value* obj,
    const struct ajj_value* key , int* fail ) {
  struct ajj_value ret;
  if( ajj_value_attr_get(a,obj,key,&ret) ) {
    rewrite_error(a);
    *fail = 1;
    return AJJ_NONE;
  } else {
    *fail = 0;
    return ret;
  }
}

static
void vm_attrstk_push( struct ajj* a , struct ajj_value* obj,
    const struct ajj_value* val , int* fail ) {
  if( ajj_value_attr_push(a,obj,val) ) {
    rewrite_error(a);
    *fail = 1;
  } else {
    *fail = 0;
  }
}

#define vm_print(A,STR) \
  do { \
    ajj_io_write((A)->rt->output,(STR)->str,(STR)->len); \
  } while(0)

#define vm_enter(A) \
  do { \
    (A)->rt->cur_gc = gc_scope_create((A),(A)->rt->cur_gc); \
  } while(0)

static
void vm_exit( struct ajj* a , int loops ) {
  assert( a->rt->cur_gc != &(a->gc_root) );
  while( loops-- ) {
    struct gc_scope* p = a->rt->cur_gc->parent;
    assert(p);
    gc_scope_destroy(a,a->rt->cur_gc);
    a->rt->cur_gc = p;
  }
}

/* Include template.
 * The include of a separate template is done as followed,
 * we will setup a new runtime and then re-enter the VM_MAIN.
 * That new runtime includes all the independent information
 * about the new template. */
static
void setup_env( struct ajj* a , int cnt , struct runtime* nrt ) {
  int i;
  for( i = 0 ; i < cnt ; --i ) {
    struct ajj_value* name = stk_top(a,3*i+1);
    struct ajj_value* value= stk_top(a,3*i+2);
    struct ajj_value* opt = stk_top(a,3*i+3);
    const struct string* symbol;
    int iopt;
    int isys;
    struct upvalue* uv;

    assert(opt->type == AJJ_VALUE_NUMBER);
    assert(name->type == AJJ_VALUE_NUMBER);

    iopt = opt->value.number;
    isys = name->value.number;
    symbol = const_str(a,isys);
    if(iopt == UPVALUE_OPTIONAL) {
      if( upvalue_table_find(nrt->global,
            symbol,&(a->env)) != NULL )
        continue; /* just skip it since we don't care */
    }
    uv = upvalue_table_add(a,
        nrt->global,
        symbol,
        0,
        0,
        0);
    assert(uv);

    /* setup the upvalue as value */
    uv->type = UPVALUE_FUNCTION;
    uv->gut.val = *value;
  }
}

static
int setup_json_env( struct ajj* a , int cnt ,struct runtime* nrt) {
  struct ajj_value* fn = stk_top(a,3*cnt+1);
  struct ajj_object* json;
  struct map* d;
  struct ajj_value json_v;
  int itr;

  if(fn->type != AJJ_VALUE_STRING) {
    vm_rpt_err(a,"Json file name must be a string,but got:%s!",
        ajj_value_get_type_name(fn));
  }

  json = json_parse(a,a->rt->cur_gc,ajj_value_to_cstr(fn),"include");
  if(json == NULL) {
    rewrite_error(a);
    return -1;
  }

  json_v = ajj_value_assign(json);
  if( !object_is_map(&json_v) ) {
    vm_rpt_err(a,"Json file:%s's root element must be an object!",
        ajj_value_to_cstr(fn));
    return -1;
  }

  d = object_cast_to_map(&json_v);
  itr = map_iter_start(d);

  while( map_iter_has(d,itr) ) {
    struct map_pair e = map_iter_deref(d,itr);
    struct upvalue *uv = upvalue_table_add(a,
        nrt->global,
        e.key,
        0,
        0,
        0);
    uv->type = UPVALUE_VALUE;
    uv->gut.val = *(struct ajj_value*)(e.val);
    itr = map_iter_move(d,itr);
  }

  setup_env(a,cnt,nrt);
  return 0;
}

static
void vm_include( struct ajj* a , int type,
    int cnt , int* fail ) {
  struct runtime nrt; /* new runtime */
  struct runtime*ort = a->rt;
  struct ajj_object* jinja; /* jinja template */
  struct ajj_value* jinja_na; /* jinja template name */
  if( a->rt->inc_cnt >= AJJ_MAX_NESTED_INCLUDE_SIZE ) {
    vm_rpt_err(a,"You cannot include more file, you can at most "
        "include %d files!",AJJ_MAX_NESTED_INCLUDE_SIZE);
    *fail = 1; return;
  }

  if(type == INCLUDE_UPVALUE) {
    jinja_na = stk_top(a,3*cnt+1);
  } else {
    assert( type == INCLUDE_JSON );
    jinja_na = stk_top(a,3*cnt+2);
  }
  if( jinja_na->type != AJJ_VALUE_STRING ) {
    vm_rpt_err(a,"The template name for include "
        "directive is not a string type,but get type:%s!",
        ajj_value_get_type_name(jinja_na));
    *fail = 1;
    return;
  }
  /* now we get the jinja template file name,
   * so we just need to load it into the mem */
  jinja = ajj_parse_template(a,ajj_value_to_cstr(jinja_na));
  if(jinja == NULL) {
    rewrite_error(a);
    *fail = 1;
    return;
  }

  /* create new runtime for vm_include */
  runtime_init(a, &nrt,jinja,a->rt->output,ort->inc_cnt+1);

  /* Before we do the rendering , we need to setup the
   * environment accordingly here. All the C side or
   * user defined upvalue are passed into the global
   * table right now */
  if(type == INCLUDE_UPVALUE) {
    setup_env(a,cnt,&nrt);
  } else {
    if(setup_json_env(a,cnt,&nrt)) {
      *fail = 1;
      goto fail;
    }
  }

  a->rt = &nrt; /* new runtime set up */
  *fail = run_jinja(a); /* start run the jinja */
  runtime_destroy(a,&nrt); /* destroy the new runtime */
  a->rt = ort; /* restore the old runtime */
  if(*fail) {
    /* the error report happened when parsing
     * *that* jinja template is dumped by its
     * own runtime, so we need to rewrite it*/
    rewrite_error(a);
  }
  /* take care of the stack status */
  if(type == INCLUDE_UPVALUE) {
    stk_pop(a,3*cnt+1);
  } else {
    stk_pop(a,3*cnt+2);
  }
  return;

fail:
  runtime_destroy(a,&nrt);
  a->rt = ort;
}

static
void vm_import( struct ajj* a, int arg1 , int* fail ) {
  const struct string* symbol = const_str(a,arg1);
  struct ajj_value* fn = stk_top(a,1);
  if( fn->type != AJJ_VALUE_STRING ) {
    vm_rpt_err(a,"The filename for import must be a string,but "
        "get type:%s",ajj_value_get_type_name(fn));
    *fail = 1;
    return;
  } else {
    struct ajj_object* jinja = ajj_parse_template(a,
        ajj_value_to_cstr(fn));
    struct ajj_value val;
    if(jinja == NULL) {
      rewrite_error(a);
      *fail = 1;
      return;
    } else {
      val = ajj_value_assign(jinja);
      set_upvalue(a,symbol,&val,0,0,fail);
      if(*fail) return;
    }
  }
  *fail = 0;
}

/* Extension.
 * Jinja's inheritance chain is really wired, it is possibly easy for
 * jinja to model it inside of python, but for our work it can be
 * way complicated. First, in our code, each block is actually compiled
 * into a function and automatically gets called. Although the behavior
 * for calling each function model's OOP's polymorphism but the big
 * difference is the main function. The main function should be invoked
 * is actually the parent's main function but not children's main function,
 * but we are stuck in the children's main function when we see extends.
 *
 * To solve this problem, we use following method to solve it,
 * 1. Everytime we meet a new extends instruction, we load the template
 * and start to render the main function of this parental template.
 * 2. Meantime, we build a linked list to describe the current inheritance
 * graph for this specific parent template.
 * 3. Every blocks executed inside of the parent function will try to be
 * resolved by look at its children first and then goes back to parent.
 * This ensure the newly extended block gets called correctly.
 *
 * There's another problem is that jinja allows a block to access the variable
 * outside of the scope by tag a keyword. Since we have compiled the block
 * into function, we allow user to use a function call syntax for a block
 * then user is able to access variable outside of the scope, that is just
 * because this function gets called. However some block function will be
 * called after extends instruction happened, at that time this block may
 * not be called at all since probably a children's block function gets picked
 * up and called. But the function parameter is actually on the stack, so
 * the calling parameter may not be matched. But, we are safe since our
 * function call allows arbitary number of arguments. Lastly, the picked
 * up function will always be called in the parent ( inner most ) template's
 * runtime stack, not on its own stack! This ensure we could pass the correct
 * function argument*/

static
void vm_extends( struct ajj* a , int* fail ) {
  struct ajj_value* temp_na = stk_top(a,1);
  struct ajj_object* jinja;
  struct runtime nrt;
  struct runtime* ort = a->rt; /* old runtime */

  if(ort->inc_cnt == AJJ_MAX_NESTED_INCLUDE_SIZE) {
    vm_rpt_err(a,"Cannot extends more file, you can extends/include at "
        "most %d files!",AJJ_MAX_NESTED_INCLUDE_SIZE);
    *fail = 1; return;
  }

  if(temp_na->type != AJJ_VALUE_STRING) {
    vm_rpt_err(a,"The template name for import is not a string,"
        "but get type:%s!",ajj_value_get_type_name(temp_na));
    *fail = 1; return;
  }

  jinja = ajj_parse_template(a,ajj_value_to_cstr(temp_na));
  if(!jinja) {
    *fail = 1; return;
  }

  runtime_init(a,&nrt,jinja,ort->output,ort->inc_cnt+1);
  /* build the correct inheritance chain */
  nrt.next = ort;
  ort->prev = &nrt;

  a->rt = &nrt;
  *fail = run_jinja(a); /* run jinja */
  runtime_destroy(a,&nrt);
  a->rt = ort;
  if(*fail) rewrite_error(a);
  ort->prev = NULL; /* reset to NULL */
}

/* Function resolver.
 * All the function resolver will *NOT* report error if
 * they cannot find any function with given name */
static
const struct function*
resolve_obj_function(struct ajj_object* val,
    const struct string* name ) {
  struct func_table* ft = val->val.obj.fn_tb;
  return func_table_find_func(ft,name);
}

static
const struct function*
resolve_self_block(struct ajj_object* val,
    const struct string* name ) {
  const struct function* f = resolve_obj_function(val,name);
  if( f && IS_JJBLOCK(f) )
    return f;
  else
    return NULL;
}

static
const struct function*
resolve_obj_method(struct ajj_object* val,
    const struct string* name ) {
  const struct function* f = resolve_obj_function(val,name);
  if( f && !IS_JJBLOCK(f) ) {
    assert( !IS_OBJECTCTOR(f) );
    return f;
  } else {
    return NULL;
  }
}

static
const struct function*
resolve_obj_block( struct ajj* a , struct ajj_object** val,
    const struct string* name ) {
  const struct function* f;
  struct runtime* rt = a->rt;

  /* Find the inner most template or the KIDS */
  while( rt->next ) { rt = rt->next; }

  /* Do search from CHILDREN -> PARENT, until we hit one */
  do {
    f = resolve_obj_function(rt->jinja,name);
    if(f && IS_JJBLOCK(f) ) {
      *val = rt->jinja;
      return f;
    }
    rt= rt->prev;
  } while(rt);
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
  *obj = cur_jinja(a);

  /* try to resolve function from object method */
  if((f = resolve_obj_function(*obj,name)))
    return f;

  /* searching through the global function table */
  uv = upvalue_table_find( a->rt->global , name , NULL );
  *obj = NULL;
  if(uv) {
    if(uv->type == UPVALUE_FUNCTION) {
      return &(uv->gut.gfunc);
    }
  }
  return NULL;
}

/* Use this to resolve a test function's name. Test can *only*
 * existed in environment and builtin table. User cannot declare
 * a test function in its jinja template */
static
const struct function*
resolve_test_function( struct ajj* a, const struct string* name ) {
  struct upvalue* uv;
  uv = upvalue_table_find( &(a->env), name , NULL );
  if(!uv||(uv->type != UPVALUE_FUNCTION &&
        !IS_TEST(&(uv->gut.gfunc)))) {
    return NULL;
  } else {
    return &(uv->gut.gfunc);
  }
}

/* method field is used to indicate whether this function call
 * has an object put on the stack. It will ONLY emitted by ATTR_CALL
 * instructions which will have such object on the stack */
static
void enter_function( struct ajj* a , const struct function* f,
    int par_cnt , int method ,
    struct ajj_object* obj , int* fail ) {
  struct runtime* rt = a->rt;
  if( rt->cur_call_stk == AJJ_MAX_CALL_STACK ) {
    vm_rpt_err(a,"Function recursive call too much,"
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
    fr->method = method;
    fr->obj = obj;
    fr->enter_gc = a->rt->cur_gc;
    ++rt->cur_call_stk;
    *fail = 0;
  }
}

static
int exit_function( struct ajj* a , const struct ajj_value* ret ) {
  struct func_frame* fr = cur_frame(a);
  struct runtime* rt = a->rt;
  int stk_sz;
  struct gc_scope* gc = fr->enter_gc;
  assert(rt->cur_call_stk >0);
  /* test whether we need to update our par_cnt
   * because we are a call for a method which means
   * we have an object on stack */
  stk_sz = fr->method ? fr->par_cnt + 1 : fr->par_cnt;
  --rt->cur_call_stk;
  if( rt->cur_call_stk >0  ) {
    fr = cur_frame(a); /* must be assigned AFTER cur_all_stk changed */
    fr->esp -= stk_sz;
    assert( fr->esp >= fr->ebp );
    /* stk_push the return value onto the stack */
    stk_push(a,*ret);

    /* Before clear the GC scope, we need to move the return value
     * from the function's inner scope to the caller's scope */
    if(ret->type == AJJ_VALUE_STRING ||
       ret->type == AJJ_VALUE_OBJECT ) {
      ajj_object_move(a,gc,ret->value.object);
    }
    /* clear gc scope in case the function is returned by a return
     * instruction */
    while( gc != a->rt->cur_gc ) {
      struct gc_scope* p = a->rt->cur_gc->parent;
      gc_scope_destroy(a,a->rt->cur_gc);
      a->rt->cur_gc = p;
    }
  }
  return 0;
}

/* BUILTIN variables */
int vm_get_argnum( struct ajj* a ) {
  return cur_frame(a)->par_cnt;
}

const struct string* vm_get_func( struct ajj* a ) {
  return &(cur_frame(a)->name);
}

const struct ajj_value* vm_get_vargs( struct ajj* a ) {
  struct func_frame* fr = cur_frame(a);
  if( IS_C(fr->entry) || IS_OBJECTCTOR(fr->entry))
    return NULL;
  else {
    const struct program* prg = GET_JINJAFUNC(fr->entry);
    struct ajj_value* vargs;
    assert( IS_JINJA(fr->entry) );

    vargs = a->rt->val_stk + fr->ebp + prg->par_size
      + VARGS_INDEX;

    /* check the stack */
    if( vargs->type == AJJ_VALUE_NONE )
      return NULL;
    else {
      return vargs;
    }
  }
}

const struct string* vm_get_caller( struct ajj* a ) {
  if( a->rt->cur_call_stk == 1 )
    return NULL;
  else {
    return &(a->rt->call_stk[
        a->rt->cur_call_stk-2].name);
  }
}

const struct ajj_object* vm_get_self( struct ajj* a ) {
  return cur_frame(a)->obj;
}

static
int vm_call_script( struct ajj* a , struct ajj_object* tmpl,
    const struct function* f,
    struct ajj_value* par , size_t par_cnt ) {
  size_t i;
  int fail;

  /* stk_push all the function ON TO the stack */
  for( i = 0 ; i < par_cnt ; ++i ) {
    stk_push(a,par[i]);
  }

  /* now enter the function frame , acting as the function
   * arguments is stk_pushed by the caller */
  enter_function(a,f,par_cnt,0,tmpl,&fail);
  if(fail) return AJJ_EXEC_FAIL;

  /* prepare the script call */
  prepare_script_call(a);

  /* notify the VM to execute the current script function */
  return VM_FUNC_CALL;
}

static
int vm_call_script_func( struct ajj* a , struct ajj_object* tmpl,
    const char* name, struct ajj_value* par , size_t par_cnt ) {
  /* resolve the function from the current template object
   * with name "name" */
  struct string n = string_const(name,strlen(name));
  const struct function* f = resolve_obj_method(tmpl,&n);
  assert(f); /* internal usage, should never return NULL */
  assert( IS_JINJA(f) ); /* this function should always be script */
  return vm_call_script(a,tmpl,f,par,par_cnt);
}

/* VM BUILTIN helper functions */

static
struct ajj_object* caller_object( struct ajj* a ) {
  assert(cur_frame(a)->obj == NULL); /* Can only gets called inside of a C function */
  assert(a->rt->cur_call_stk>1);
  assert(a->rt->call_stk[a->rt->cur_call_stk-2].obj->tp == AJJ_VALUE_JINJA);
  return a->rt->call_stk[a->rt->cur_call_stk-2].obj;
}

/* This function is used internally to achieve calling a macro's
 * caller block inside of the macro's body. It is entirely a re-
 * dispatch function that takes the argument in and check the script
 * function name and then invoke it */
int vm_caller( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  struct upvalue* uv;
  UNUSE_ARG(udata);
  assert(a->rt && a->rt->global);

  uv = upvalue_table_find(a->rt->global,
      &CALLER_STUB,NULL);

  if(!uv) {
    ajj_error(a,"The upvalue variable:%s for internal usage is not "
        "set! This is caused by user try to call this macro as a "
        "free function but not invoke it inside of a call block !",
        CALLER_STUB.str);
    return AJJ_EXEC_FAIL;
  }

  if(uv->type != UPVALUE_VALUE ||
     uv->gut.val.type != AJJ_VALUE_STRING) {
    ajj_error(a,"The upvalue variable:%s for internal usage is not "
        "a string value but %s!Possibly collide with user defined "
        "value !Do not use builtin variable ,PLEASE!",
        CALLER_STUB.str,
        uv->type == UPVALUE_VALUE ? ajj_value_get_type_name(
          &(uv->gut.val)) :"global function/test/filter");
    return AJJ_EXEC_FAIL;
  } else {
    const char* name = ajj_value_to_cstr(&(uv->gut.val));
    /* now redispatch this function */
    return vm_call_script_func(a,
        caller_object(a), /* Cannot use cur_jinja since this jinja
                           * object may not be the object which has
                           * the correspoding macros */
        name,
        arg,
        arg_num);
  }

  *ret = AJJ_NONE;
  return AJJ_EXEC_FAIL;
}

/* This function is used to call the same name function but from its
 * parent. */
int vm_super( struct ajj* a,
    void* udata,
    struct ajj_value* arg,
    size_t arg_num,
    struct ajj_value* ret ) {
  struct func_frame* caller_fr;
  struct ajj_object* self;
  struct runtime* rt;
  /* get caller's func_frame and figure out the name of the called
   * function */
  assert(a->rt->cur_call_stk>1);
  caller_fr = a->rt->call_stk + a->rt->cur_call_stk - 2;
  assert(caller_fr->obj);
  assert(caller_fr->obj->tp == AJJ_VALUE_JINJA);

  self = caller_fr->obj;

  /* Locate the self object's struct runtime. This is done by
   * iterating through the inheritance chain */
  rt = a->rt;
  while( rt->jinja != self ) { rt = rt->next; assert(rt); }
  /* Now we locate the correct struct runtime for this self
   * jinja template, we know search upwards to its parent and
   * try to resolve the function name */
  if(rt->prev == NULL) {
    vm_rpt_err(a,"Function::super cannot work since the object doesn't "
        "have a parent!");
    return AJJ_EXEC_FAIL;
  } else {
    rt = rt->prev;
    do {
      const struct function* f = resolve_self_block(rt->jinja,
          &(caller_fr->name));
      if(f) {
        return vm_call_script(a,rt->jinja,f,arg,arg_num);
      }
      rt = rt->prev;
    } while(rt);
  }
  vm_rpt_err(a,"Function::supper cannot locate function:%s in its "
      "inheritance chain!",caller_fr->name.str);
  return AJJ_EXEC_FAIL;
}

#define RCHECK &fail); \
  do { \
    if(fail) goto fail; \
  } while(0

/* The function for invoke a certain code. Before entering into this
 * function user should already prepared well for the stack and the
 * target the function must be already on the stk_top of the stack.
 * Notes: The inline threading is not used but an old and slow while
 * switch cases. The reason is inline threading is *not* portable */
static
int vm_main( struct ajj* a ) {

  int fail;

#define vm_beg(X) case VM_##X:
#define vm_end(X) break;

  do {
    int c = next_instr(a);
    instructions instr = BC_INSTRUCTION(c);

    switch(instr) {
      vm_beg(ADD) {
        struct ajj_value o = vm_add(a,
          stk_top(a,2),stk_top(a,1),RCHECK);
        stk_pop(a,2);
        stk_push(a,o);
      } vm_end(ADD)

      vm_beg(SUB) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,stk_top(a,2),RCHECK);
        r = to_number(a,stk_top(a,1),RCHECK);
        o = ajj_value_number(l-r);
        stk_pop(a,2);
        stk_push(a,o);
      } vm_end(SUB)

      vm_beg(DIV) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,stk_top(a,2),RCHECK);
        r = to_number(a,stk_top(a,1),RCHECK);
        if( r == 0 ) {
          vm_rpt_err(a,"Divid by zero!");
          return -1;
        }
        o = ajj_value_number(l/r);
        stk_pop(a,2);
        stk_push(a,o);
      } vm_end(DIV)

      vm_beg(MUL) {
        struct ajj_value o = vm_mul(a,
            stk_top(a,2),stk_top(a,1),RCHECK);
        stk_pop(a,2);
        stk_push(a,o);
      } vm_end(MUL)

      vm_beg(POW) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,stk_top(a,2),RCHECK);
        r = to_number(a,stk_top(a,1),RCHECK);
        o = ajj_value_number(pow(l,r));
        stk_pop(a,2);
        stk_push(a,o);
      } vm_end(POW)

      vm_beg(MOD) {
        int l , r;
        struct ajj_value o;
        l = to_integer(a,stk_top(a,2),RCHECK);
        r = to_integer(a,stk_top(a,1),RCHECK);
        o = ajj_value_number(l%r);
        stk_pop(a,2);
        stk_push(a,o);
      } vm_end(MOD)

      vm_beg(EQ) {
        struct ajj_value o = vm_eq(a,
            stk_top(a,2),stk_top(a,1),RCHECK);
        stk_pop(a,2);
        stk_push(a,o);
      } vm_end(EQ)

      vm_beg(NE) {
        struct ajj_value o = vm_ne(a,
            stk_top(a,2),stk_top(a,1),RCHECK);
        stk_pop(a,2);
        stk_push(a,o);
      } vm_end(NE)

      vm_beg(LT) {
        struct ajj_value o = vm_lt(a,
            stk_top(a,2),stk_top(a,1),RCHECK);
        stk_pop(a,2);
        stk_push(a,o);
      } vm_end(LT)

      vm_beg(LE) {
        struct ajj_value o = vm_le(a,
            stk_top(a,2),stk_top(a,1),RCHECK);
        stk_pop(a,2);
        stk_push(a,o);
      } vm_end(LE)

      vm_beg(GT) {
        struct ajj_value o = vm_gt(a,
            stk_top(a,2),stk_top(a,1),RCHECK);
        stk_pop(a,2);
        stk_push(a,o);
      } vm_end(GT)

      vm_beg(GE) {
        struct ajj_value o = vm_ge(a,
            stk_top(a,2),stk_top(a,1),RCHECK);
        stk_pop(a,2);
        stk_push(a,o);
      } vm_end(GE)

      vm_beg(NOT) {
        struct ajj_value o = vm_not(a,
            stk_top(a,1),RCHECK);
        stk_pop(a,1);
        stk_push(a,o);
      } vm_end(NOT)

      vm_beg(NEG) {
        double val;
        struct ajj_value o;
        val = to_number(a,stk_top(a,1),RCHECK);
        o = ajj_value_number(-val);
        stk_pop(a,1);
        stk_push(a,o);
      } vm_end(NEG)

      vm_beg(DIVTRUCT) {
        double l,r;
        struct ajj_value o;
        l = to_number(a,stk_top(a,2),RCHECK);
        r = to_number(a,stk_top(a,1),RCHECK);
        if(r == 0.0) {
          vm_rpt_err(a,"Divide by 0!");
          goto fail;
        }
        /* should be painful, but portable */
        o = ajj_value_number( (int64_t)(l/r) );
        stk_pop(a,2);
        stk_push(a,o);
      } vm_end(DIVTRUCT)

      vm_beg(IN) {
        struct ajj_value o = vm_in(a,
            stk_top(a,1),stk_top(a,2),RCHECK);
        stk_pop(a,2);
        stk_push(a,o);
      } vm_end(IN)

      vm_beg(NIN) {
        struct ajj_value o = vm_in(a,
            stk_top(a,1),stk_top(a,2),RCHECK);
        stk_pop(a,2);
        assert(o.type == AJJ_VALUE_BOOLEAN);
        if(o.value.boolean)
          stk_push(a,AJJ_FALSE);
        else
          stk_push(a,AJJ_TRUE);
      } vm_end(NIN)

      vm_beg(LEN) {
        struct ajj_value o = vm_len(a,stk_top(a,1),RCHECK);
        stk_pop(a,1);
        stk_push(a,o);
      } vm_end(LEN)

      vm_beg(CAT) {
        struct ajj_value o = vm_cat(a,stk_top(a,2),stk_top(a,1));
        stk_pop(a,2);
        stk_push(a,o);
      } vm_end(CAT)

      vm_beg(TEST) {
        int fn_idx = instr_1st_arg(c);
        int an = instr_2nd_arg(a);
        vm_test(a,fn_idx,an,1,RCHECK);
      } vm_end(TEST)

      vm_beg(BOOL) {
        if( vm_is_true(stk_top(a,1)) ) {
          stk_pop(a,1);
          stk_push(a,AJJ_TRUE);
        } else {
          stk_pop(a,1);
          stk_push(a,AJJ_FALSE);
        }
      } vm_end(BOOL)

      vm_beg(TESTN) {
        int fn_idx = instr_1st_arg(c);
        int an = instr_2nd_arg(a);
        vm_test(a,fn_idx,an,0,RCHECK);
      } vm_end(TESTN)

      vm_beg(CALL) {
        int fn_idx= instr_1st_arg(c);
        int an = instr_2nd_arg(a);

        const struct string* fn = const_str(a,fn_idx);
        struct ajj_object* obj;
        const struct function* f =
          resolve_free_function(a,fn,&obj);
        if( f == NULL ) {
          vm_rpt_err(a,"Cannot find function:%s!",fn->str);
          goto fail;
        } else {
          struct ajj_value ret = AJJ_NONE;
          int r;
          enter_function(a,f,an,0,obj,RCHECK);
          r = vm_call(a,obj,&ret);
          if( r < 0 )
            goto fail;
          else if( r == 0 ) {
            assert( IS_C(f) || IS_OBJECTCTOR(f) );
            /* We need to handle return value based on the function types,
             * if it is a jinja side function calling, we don't need to do
             * anything, but just return, since the VM_RET will take care
             * of everything. However, if we are calling a C functions, then
             * we need to exit the current function calling frame and also
             * stk_push the return value on to the stack */
            exit_function(a,&ret);
          }
        }
      } vm_end(CALL)

      vm_beg(BCALL) {
        int fn_idx = instr_1st_arg(c);
        int an = instr_2nd_arg(a);
        const struct string* fn = const_str(a,fn_idx);
        struct ajj_object* obj;
        const struct function* f = resolve_obj_block(a,&obj,fn);
        /* After resolving, the object returned from resolve_obj_block
         * can be not the same template currently rendering, because
         * it could be some template that is extends */

        if( f == NULL ) {
          vm_rpt_err(a,"Cannot find block:%s in its inhertiance "
              "chain!",fn->str);
          goto fail;
        } else {
          struct ajj_value ret = AJJ_NONE;
#ifndef NDEBUG
          int r;
#endif
          enter_function(a,f,an,0,obj,RCHECK);
#ifndef NDEBUG
          r =
#endif
            vm_call(a,obj,&ret);
          assert( r == VM_FUNC_CALL );
        }
      } vm_end(BCALL)

      vm_beg(ATTR_CALL) {
        int fn_idx = instr_1st_arg(c);
        int an = instr_2nd_arg(a);
        struct ajj_value obj = *stk_top(a,an+1);
        const struct string* fn;
        struct ajj_object* o;
        const struct function* f;

        if( obj.type != AJJ_VALUE_OBJECT ) {
          vm_rpt_err(a,"Cannot call a member function on type:%s!",
              ajj_value_get_type_name(&obj));
          goto fail;
        }

        fn = const_str(a,fn_idx);
        o = obj.value.object;
        /* Here we need to resolve all the function and *cannot*
         * filter out the block because jinja2 supports invoke a
         * block function using self.some_block_name() syntax.*/
        f = resolve_obj_function(o,fn);

        if( f == NULL ) {
          vm_rpt_err(a,"Cannot find object method or jinja block:%s for object:%s!",
              fn->str,
              o->val.obj.fn_tb->name.str);
          goto fail;
        } else {
          struct ajj_value ret = AJJ_NONE;
          int r;
          /* We have an object on stack, so set the method field
           * of function enter_function to 1 */
          enter_function(a,f,an,1,o,RCHECK);
          r = vm_attrcall(a,o,&ret);
          if( r < 0 ) {
            goto fail;
          } else if( r == 0 ) {
            if( IS_C(f) ) {
              exit_function(a,&ret);
            }
          }
        }
      } vm_end(ATTR_CALL)

      vm_beg(RET) {
        /* check if we have return value here or not !
         * if not, put a dummy NONE on stk_top of the caller stack */
        struct ajj_value ret = *stk_top(a,1); /* TOP of the stack contains our
                                               * return value */
        /* do clean up things */
        exit_function(a,&ret);
        /* check the current function frame to see whether we previously
         * had a c function directly calls into a script function. If so,
         * we just stk_pop that c function again, looks like one return stk_pops
         * 2 function frames */
        if( a->rt->cur_call_stk > 0 ) {
          struct func_frame* fr = cur_frame(a);
          if( IS_C(fr->entry) ) {
            /* do a consecutive stk_pop here */
            stk_pop(a,1); /* stk_pop the original return value on stack */
            exit_function(a,&ret);
          }
        } else {
          /* We unwind the last function frame, so we can return :) */
          goto done;
        }
      } vm_end(RET)

      vm_beg(PRINT) {
        int own;
        size_t l;
        struct string t;
        const char* text = ajj_display(
            a,stk_top(a,1),&l,&own);

        assert(text); /* should never fail */

        t.str = text;
        t.len = l;
        if(!string_empty(&t))
          vm_print(a,&t);
        stk_pop(a,1);
        if(own) free((void*)text);
      } vm_end(PRINT)

      vm_beg(POP) {
        int arg = instr_1st_arg(c);
        stk_pop(a,arg);
      } vm_end(POP)

      vm_beg(TPUSH) {
        int arg = instr_1st_arg(c);
        struct ajj_value val = *stk_top(a,arg);
        stk_push(a,val);
      } vm_end(TPUSH)

      vm_beg(BPUSH) {
        int arg = instr_1st_arg(c);
        struct ajj_value val = *stk_bot(a,arg);
        stk_push(a,val);
      } vm_end(BPUSH)

      vm_beg(MOVE) {
        int l = instr_1st_arg(c);
        int r = instr_2nd_arg(a);
        *stk_bot(a,l) = *stk_bot(a,r);
      } vm_end(MOVE)

      vm_beg(LIFT) {
        int l = instr_1st_arg(c);
        int r = instr_2nd_arg(a);

        vm_lift(a,l,r);
      } vm_end(LIFT)

      vm_beg(STORE) {
        int dst = instr_1st_arg(c);
        struct ajj_value src = *stk_top(a,1);
        *stk_bot(a,dst) = src;
        stk_pop(a,1);
      } vm_end(STORE)

      vm_beg(LSTR) {
        int arg = instr_1st_arg(c);
        struct ajj_value val = vm_lstr(a,arg);
        stk_push(a,val);
      } vm_end(LSTR)

      vm_beg(LTRUE) {
        stk_push(a,AJJ_TRUE);
      } vm_end(LTRUE)

      vm_beg(LFALSE) {
        stk_push(a,AJJ_FALSE);
      } vm_end(LFALSE)

      vm_beg(LZERO) {
        struct ajj_value val = ajj_value_number(0);
        stk_push(a,val);
      } vm_end(LZERO)

      vm_beg(LNONE) {
        stk_push(a,AJJ_NONE);
      } vm_end(LNONE)

      vm_beg(LNUM) {
        int arg = instr_1st_arg(c);
        struct ajj_value val = vm_lnum(a,arg);
        stk_push(a,val);
      } vm_end(LNUM)

      vm_beg(LIMM) {
        int imm = instr_1st_arg(c);
        struct ajj_value val = ajj_value_number(imm);
        stk_push(a,val);
      } vm_end(LIMM)

      vm_beg(LLIST) {
        struct ajj_value val = vm_llist(a);
        stk_push(a,val);
      } vm_end(LLIST)

      vm_beg(LDICT) {
        struct ajj_value val = vm_ldict(a);
        stk_push(a,val);
      } vm_end(LDICT)

      vm_beg(ATTR_SET) {
        struct ajj_value obj = *stk_top(a,3);
        struct ajj_value key = *stk_top(a,2);
        struct ajj_value val = *stk_top(a,1);
        vm_attrset(a,&obj,&key,&val,RCHECK);
        stk_pop(a,2);
      } vm_end(ATTRSET)

      vm_beg(ATTR_GET) {
        struct ajj_value obj = *stk_top(a,2);
        struct ajj_value key = *stk_top(a,1);
        struct ajj_value val =
          vm_attrget( a,&obj,&key,RCHECK);
        stk_pop(a,2);
        stk_push(a,val);
      } vm_end(ATTR_GET)

      vm_beg(ATTR_PUSH) {
        struct ajj_value obj = *stk_top(a,2);
        struct ajj_value val = *stk_top(a,1);
        vm_attrstk_push(a,&obj,&val,RCHECK);
        stk_pop(a,1);
      } vm_end(ATTR_PUSH)

      vm_beg(UPVALUE_SET) {
        int par = instr_1st_arg(c);
        const struct string* upvalue_name =
          const_str(a,par);
        struct ajj_value val = *stk_top(a,1);
        /* deteach the value to be owned by upvalue */
        set_upvalue(a,upvalue_name,&val,0,0,RCHECK);
        stk_pop(a,1);
      } vm_end(UPVALUE_SET)

      vm_beg(UPVALUE_GET) {
        int par = instr_1st_arg(c);
        const struct string* upvalue_name =
          const_str(a,par);
        struct ajj_value val = get_upvalue(a,
            upvalue_name);
        stk_push(a,val);
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
        struct ajj_value cond = *stk_top(a,1);
        if( vm_is_true(&cond) ) {
          cur_frame(a)->pc = pos;
        }
        stk_pop(a,1);
      } vm_end(JT)

      vm_beg(JF) {
        int pos = instr_1st_arg(c);
        struct ajj_value cond = *stk_top(a,1);
        if( vm_is_false(&cond) ) {
          cur_frame(a)->pc = pos;
        }
        stk_pop(a,1);
      } vm_end(JF)

      vm_beg(JLT) {
        int pos = instr_1st_arg(c);
        struct ajj_value cond = *stk_top(a,1);
        if( vm_is_true(&cond) ) {
          cur_frame(a)->pc = pos;
        } else {
          stk_pop(a,1);
        }
      } vm_end(JLT)

      vm_beg(JLF) {
        int pos = instr_1st_arg(c);
        struct ajj_value cond = *stk_top(a,1);
        if( vm_is_false(&cond) ) {
          cur_frame(a)->pc = pos;
        } else {
          stk_pop(a,1);
        }
      } vm_end(JLF)

      vm_beg(JEPT) {
        int pos = instr_1st_arg(c);
        struct ajj_value cond = *stk_top(a,1);
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
        stk_pop(a,1);
      } vm_end(JEPT)

      /* ITERATORS ------------------ */
      vm_beg(ITER_START) {
        int itr;
        size_t len;
        struct ajj_value* obj = stk_top(a,1);
        struct ajj_value loop;

        if(ajj_value_iter_start(a,obj,&itr)) {
          rewrite_error(a);
          goto fail;
        }
        if(ajj_value_len(a,obj,&len)) {
          rewrite_error(a);
          goto fail;
        }

        /* get loop object */
        loop = create_loop_object(a,len);
        assert( stk_top(a,2)->type == AJJ_VALUE_NONE );
        /* since we've already *got* a space to set
         * the loop object which is the one slots that
         * is before the current top of the stack */
        (*stk_top(a,2)) = loop;
        /* do not stk_pop the object out */
        stk_push(a,ajj_value_iter(itr));
      } vm_end(ITER_START)

      vm_beg(ITER_HAS) {
        struct ajj_value* itr = stk_top(a,1);
        struct ajj_value* obj = stk_top(a,2);
        int has;
        assert( itr->type == AJJ_VALUE_ITERATOR );
        if(ajj_value_iter_has(a,obj,ajj_value_to_iter(itr),&has)) {
          rewrite_error(a);
          goto fail;
        }
        /* re-stk_push the iterator onto the stack */
        stk_push(a,ajj_value_boolean(has));
      } vm_end(ITER_HAS)

      vm_beg(ITER_DEREF) {
        int arg = instr_1st_arg(c);
        struct ajj_value* obj = stk_top(a,2);
        struct ajj_value* itr = stk_top(a,1);
        assert( itr->type == AJJ_VALUE_ITERATOR );
        switch(arg) {
          case ITERATOR_VAL:
            {
              struct ajj_value v;
              if(ajj_value_iter_get_val(a,
                    obj,
                    ajj_value_to_iter(itr),
                    &v)) {
                rewrite_error(a);
                goto fail;
              }
              stk_push(a,v);
            }
            break;
          case ITERATOR_KEYVAL:
            {
              struct ajj_value k , v;
              if(ajj_value_iter_get_val(a,
                    obj,ajj_value_to_iter(itr),
                    &v) ||
                  ajj_value_iter_get_key(a,
                    obj,ajj_value_to_iter(itr),
                    &k)) {
                rewrite_error(a);
                goto fail;
              }
              stk_push(a,k);
              stk_push(a,v);
            }
            break;
          case ITERATOR_KEY:
            {
              struct ajj_value k;
              if(ajj_value_iter_get_key(a,
                    obj,
                    ajj_value_to_iter(itr),
                    &k)) {
                rewrite_error(a);
                goto fail;
              }
              stk_push(a,k);
            }
            break;
          default:
            UNREACHABLE();
            break;
        }
      }vm_end(ITER_DEREF)

      vm_beg(ITER_MOVE){
        struct ajj_value* obj = stk_top(a,2);
        struct ajj_value* itr = stk_top(a,1);
        struct ajj_value* loop= stk_top(a,3);
        int i_itr;
        assert( itr->type == AJJ_VALUE_ITERATOR );
        if(ajj_value_iter_move(a,
              obj,
              ajj_value_to_iter(itr),
              &i_itr)) {
          rewrite_error(a);
          goto fail;
        }
        builtin_loop_move(loop); /* move the loop object */
        stk_pop(a,1); /* stk_pop the stk_top iterator */
        stk_push( a , ajj_value_iter(i_itr) );
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
        vm_include(a,a1,a2,RCHECK); /* vm_include takes care of pop */
      } vm_end(INCLUDE)

      vm_beg(IMPORT) {
        vm_import(a,instr_1st_arg(c),RCHECK);
        stk_pop(a,1); /* stk_pop the filename */
      } vm_end(IMPORT)

      vm_beg(EXTENDS) {
        vm_extends(a,RCHECK);
        stk_pop(a,1); /* stk_pop the filename */
      } vm_end(EXTENDS)

      vm_beg(NOP) {
      } vm_end(NOP)

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

#undef vm_beg
#undef vm_end

static
int run_jinja( struct ajj* a ) {
  int fail;
  const struct function* m;
  assert(a->rt && cur_jinja(a));
  m = ajj_object_jinja_main_func(cur_jinja(a));
  assert(m);
  enter_function(a,m,0,0,cur_jinja(a),&fail);
  assert(!fail);
  set_func_builtin_vars(a,
      0,
      &(cur_frame(a)->name),
      NULL,
      NULL);
  fail = vm_main(a);
  return fail;
}

int vm_run_jinja( struct ajj* a , struct ajj_object* jj,
    struct ajj_io* output ) {
  struct runtime rt;
  struct runtime* o_rt = a->rt;
  int fail;
  runtime_init(a,&rt,jj,output,0);
  a->rt = &rt;
  fail = run_jinja(a);
  runtime_destroy(a,&rt);
  a->rt = o_rt; /* resume the old runtime since this
                 * function can be nested */
  return fail;
}
