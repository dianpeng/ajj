#include "ajj-priv.h"
#include "object.h"
#include "gc.h"
#include <limits.h>
#include <math.h>

#ifndef NDEBUG
#define ALWAYS(X) assert(!(X))
#else
#define ALWAYS(X) X
#endif /* NDEBUG */

#define cur_frame(a) (&(a->rt->call_stk[a->rt->cur_call_stk]))
#define cur_function(a) (cur_frame(a)->entry)

/* ============================
 * runtime
 * ==========================*/
static
struct runtime* create_runtime( struct ajj_object* tp ) {
  struct runtime* rt = calloc(1,sizeof(*rt));
  rt->cur_tmpl = tp;
  return rt;
}

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
  prg = JINJAFUNC(fr->entry);
  if( prg->len == fr->pc ) {
    return VM_HALT;
  } else {
    int instr = (int)(*((char*)(prg->codes)+fr->pc));
    ++(fr->pc);
    return instr;
  }
}

static
int next_par( struct ajj* a , int* fail ) {
  struct func_frame* fr = cur_frame(a);
  const struct program* prg = JINJAFUNC(fr->entry);
  assert(IS_JINJA(fr->entry));
  if( prg->len < fr->pc+4 ) {
    *fail = 1;
    return -1;
  } else {
    int arg = *((int*)((char*)(prg->codes)+fr->pc));
    fr->pc += 4;
    *fail = 0;
    return arg;
  }
}

/* =============================
 * Type conversions
 * ===========================*/

static
double str_to_number( struct ajj* a , const char* str , int* fail );

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
#define DEFINE_BIN_HANDLER(N,O,TYPE) \
  static   \
  struct ajj_value N(struct ajj* a, \
      const struct ajj_value* l, \
      const struct ajj_value* r, \
      int* fail ) { \
    double ln,rn; \
    ln = to_number(a,l,fail); \
    if(*fail) return AJJ_NONE; \
    rn = to_number(a,r,fail); \
    if(*fail) return AJJ_NONE; \
    *fail = 0; \
    return ajj_value_number( ln O rn ); \
  }

DEFINE_BIN_HANDLER(vm_add,+,number)
DEFINE_BIN_HANDLER(vm_eq,==,boolean)
DEFINE_BIN_HANDLER(vm_ne,!=,boolean)
DEFINE_BIN_HANDLER(vm_le,<=,boolean)
DEFINE_BIN_HANDLER(vm_lt,<,boolean)
DEFINE_BIN_HANDLER(vm_ge,>=,boolean)
DEFINE_BIN_HANDLER(vm_gt,>,boolean)

#undef DEFINE_BIN_HANDLER

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

/* call */
/* The precondition of vm_call is that the function calling frame is
 * already setup, and the parameters are on the stack. Since we support
 * default parameters, we need to push arguments on top of stack as well.
 * This vm_call needs to handle that as well. */
static
struct ajj_value
vm_call(struct ajj* ,int* fail);

static
struct ajj_value
vm_attrcall(struct ajj* , const struct ajj_value* , int* fail);

static
struct ajj_value vm_lstr( struct ajj* , int , int* fail );

static
struct ajj_value vm_llist( struct ajj* , int* fail );
static
struct ajj_value vm_ldict( struct ajj* , int* fail );
static
void vm_attrset( struct ajj* a , const struct ajj_value* obj,
    const struct ajj_value* key , const struct ajj_value* val ,
    int* fail );
static
struct ajj_value
vm_attrget( struct ajj* a , const struct ajj_value* obj,
    const struct ajj_value* key , int* fail );
static
void vm_attrpush( struct ajj* a , const struct ajj_value* obj,
    const struct ajj_value* val , int* fail );

static
void vm_print( struct ajj* , const struct string* , int* );

static
void vm_enter( struct ajj* , int* );

static
void vm_exit( struct ajj* , int* );

/* ============================
 * Upvalue handler
 * ==========================*/

static
void set_upvalue( struct ajj* a, const struct string* name,
    const struct ajj_value* value , int* fail );

static
struct ajj_value
get_upvalue( struct ajj* a , const struct string* name , int* fail );

static
void del_upvalue( struct ajj* a, const struct string* name , int* fail );

/* Use to resolve function when you call it globally, VM_CALL */
static
const struct function*
resolve_free_function( struct ajj* a, const struct string* name , int* fail );

/* Use to resolve function when you call it under an object, VM_ATTRCALL */
static
const struct function*
resolve_method( struct ajj* a , const struct ajj_value* val ,
    const struct string* name , int* fail );

#ifndef NDEBUG
static
struct ajj_value*
stack_value( struct ajj* a , int x ) {
  assert( x >= cur_frame(a)->ebp && x < cur_frame(a)->esp );
  return x + a->rt->val_stk;
}

#define BOT(X) stack_value(a,cur_frame(a)->ebp+(X))
#define TOP(X) stack_value(a,cur_frame(a)->esp-(X))
#else
#define BOT(X) (a->rt->val_stk[cur_frame(a)->ebp+(X)])
#define TOP(X) (a->rt->val_stk[cur_frame(a)->esp-(X)])
#endif /* NDEBUG */

#define POP(X) \
  do { \
    struct func_frame* fr = cur_frame(a); \
    assert(fr->esp >= (X)); \
    fr->esp -= (X); \
  } while(0)

#define PUSH(X) \
  do { \
    struct func_frame* fr = cur_frame(a); \
    if( fr->esp == AJJ_MAX_VALUE_STACK_SIZE ) { \
      report_error(a,"Too much value on value stack!"); \
      return -1; \
    } else { \
      rt->val_stk[fr->esp] = (X); \
      ++fr->esp; \
    } \
  } while(0)

/*   helpers */
static
void enter_function( struct ajj* a , const struct function* f,
    size_t par_cnt , int method , int* fail ) {
  struct runtime* rt = a->rt;
  if( rt->cur_call_stk == AJJ_MAX_CALL_STACK ) {
    report_error(a,"Function recursive call too much,frame stack overflow!");
    *fail = 1;
  } else {
    struct func_frame* fr = rt->call_stk+rt->cur_call_stk;
    int esp = rt->cur_call_stk == 0 ? 0 :
      rt->call_stk[rt->cur_call_stk-1].esp;

    int ebp = rt->cur_call_stk == 0 ? 0 :
      rt->call_stk[rt->cur_call_stk-1].ebp;

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

  /* ugly hack to use PUSH macro since it can exit, so we
   * set up fail to 1 in case it exit the function */
  *fail = 1;
  PUSH(*ret);

  *fail = 0;
  return 0;
}

static
const struct string* cur_function_const_str( struct ajj* a , int idx ) {
  const struct function* c = cur_function(a);
  assert( IS_JINJA(c) );
  assert( idx < c->f.jj_fn.str_len );
  return &(c->f.jj_fn.str_tbl[idx]);
}

static
double cur_function_const_num( struct ajj* a, int idx ) {
  const struct function* c = cur_function(a);
  assert( IS_JINJA(c) );
  assert( idx < c->f.jj_fn.num_len );
  return c->f.jj_fn.num_tbl[idx];
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

static
int vm_run_func( struct ajj* a, struct ajj_value* output ) {

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
        struct ajj_value o = vm_add(a,TOP(2),TOP(1),CHECK);
        POP(2);
        PUSH(o);
      } vm_end(ADD)

      vm_beg(SUB) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,TOP(2),CHECK);
        r = to_number(a,TOP(1),CHECK);
        o = ajj_value_number(l+r);
        POP(2);
        PUSH(o);
      } vm_end(SUB)

      vm_beg(DIV) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,TOP(2),CHECK);
        r = to_number(a,TOP(1),CHECK);
        if( r == 0 ) {
          report_error(a,"Divid by zero!");
          return -1;
        }
        o = ajj_value_number(l/r);
        POP(2);
        PUSH(o);
      } vm_end(DIV)

      vm_beg(MUL) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,TOP(2),CHECK);
        r = to_number(a,TOP(1),CHECK);
        o = ajj_value_number(l*r);
        POP(2);
        PUSH(o);
      } vm_end(MUL)

      vm_beg(POW) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,TOP(2),CHECK);
        r = to_number(a,TOP(1),CHECK);
        o = ajj_value_number(pow(l,r));
        POP(2);
        PUSH(o);
      } vm_end(POW)

      vm_beg(EQ) {
        struct ajj_value o = vm_eq(a,TOP(2),TOP(1),CHECK);
        POP(2);
        PUSH(o);
      } vm_end(EQ)

      vm_beg(NE) {
        struct ajj_value o = vm_ne(a,TOP(2),TOP(1),CHECK);
        POP(2);
        PUSH(o);
      } vm_end(NE)

      vm_beg(LT) {
        struct ajj_value o = vm_lt(a,TOP(2),TOP(1),CHECK);
        POP(2);
        PUSH(o);
      } vm_end(LT)

      vm_beg(LE) {
        struct ajj_value o = vm_le(a,TOP(2),TOP(1),CHECK);
        POP(2);
        PUSH(o);
      } vm_end(LE)

      vm_beg(GT) {
        struct ajj_value o = vm_gt(a,TOP(2),TOP(1),CHECK);
        POP(2);
        PUSH(o);
      } vm_end(GT)

      vm_beg(GE) {
        struct ajj_value o = vm_ge(a,TOP(2),TOP(1),CHECK);
        POP(2);
        PUSH(o);
      } vm_end(GE)

      vm_beg(NOT) {
        struct ajj_value o = vm_not(a,TOP(1),CHECK);
        POP(1);
        PUSH(o);
      } vm_end(NOT)

      vm_beg(NEG) {
        double val = to_number(a,TOP(1),CHECK);
        struct ajj_value o = ajj_value_number(-val);
        POP(2);
        PUSH(o);
      } vm_end(NEG)

      vm_beg(DIVTRUCT) {
        double l,r;
        struct ajj_value o;
        l = to_number(a,TOP(2),CHECK);
        r = to_number(a,TOP(1),CHECK);
        if(r == 0.0) {
          report_error(a,"Divide by 0!");
          goto fail;
        }
        /* should be painful, but portable */
        o = ajj_value_number( (int64_t)(l/r) );
      } vm_end(DIVTRUCT)

      vm_beg(CALL) {
        int fn_idx= next_par(a,CHECK);
        int an = next_par(a,CHECK);
        struct ajj_value argnum = ajj_value_number(an);
        const struct string* fn = cur_function_const_str(a,fn_idx);
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
        int fn_idx = next_par(a,CHECK);
        int an = next_par(a,CHECK);
        struct ajj_value obj = TOP(an+1);
        struct ajj_value argnum = ajj_value_number(an);
        const struct string* fn = cur_function_const_str(a,fn_idx);
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
          ret = TOP(1); /* on top of the stack should be the
                         * return value in this situations */
        }
        /* do clean up things */
        exit_function(a,&ret,CHECK);
      } vm_end(RET)

      vm_beg(PRINT) {
        const struct string* text = to_string(a,TOP(1),CHECK);
        vm_print(a,text,CHECK);
        POP(1);
      } vm_end(PRINT)

      vm_beg(POP) {
        int arg = next_par(a,CHECK);
        POP(arg);
      } vm_end(POP)

      vm_beg(TPUSH) {
        int arg = next_par(a,CHECK);
        struct ajj_value val = *TOP(arg);
        PUSH(val);
      } vm_end(TPUSH)

      vm_beg(BPUSH) {
        int arg = next_par(a,CHECK);
        struct ajj_value val = *BOT(arg);
        PUSH(val);
      } vm_end(BPUSH)

      vm_beg(BB_MOVE) {
        int l = next_par(a,CHECK);
        int r = next_par(a,CHECK);
        struct ajj_value lval = *BOT(l);
        *BOT(l) = *BOT(r);
        *BOT(r) = lval;
      } vm_end(BT_MOVE)

      vm_beg(LSTR) {
        int arg = next_par(a,CHECK);
        struct ajj_value val = vm_lstr(a,arg,CHECK);
        PUSH(val);
      } vm_end(LSTR)

      vm_beg(LTRUE) {
        PUSH(AJJ_TRUE);
      } vm_end(LTRUE)

      vm_beg(LFALSE) {
        PUSH(AJJ_FALSE);
      } vm_end(LFALSE)

      vm_beg(LZERO) {
        struct ajj_value val = ajj_value_number(0);
        PUSH(val);
      } vm_end(LZERO)

      vm_beg(LNONE) {
        PUSH(AJJ_NONE);
      } vm_end(LNONE)

      vm_beg(LIMM) {
        int imm = next_par(a,CHECK);
        struct ajj_value val = ajj_value_number(imm);
        PUSH(val);
      } vm_end(LIMM)

      vm_beg(LLIST) {
        struct ajj_value val = vm_llist(a,CHECK);
        PUSH(val);
      } vm_end(LLIST)

      vm_beg(LDICT) {
        struct ajj_value val = vm_ldict(a,CHECK);
        PUSH(val);
      } vm_end(LDICT)

      vm_beg(ATTR_SET) {
        struct ajj_value obj = TOP(3);
        struct ajj_value key = TOP(2);
        struct ajj_value val = TOP(1);
        vm_attrset(a,&obj,&key,&val,CHECK);
        POP(3);
      } vm_end(ATTRSET)

      vm_beg(ATTR_GET) {
        struct ajj_value obj = TOP(2);
        struct ajj_value key = TOP(1);
        struct ajj_value val =
          vm_attrget( a,&obj,&key,CHECK);
        POP(2);
        PUSH(val);
      } vm_end(ATTR_GET)

      vm_beg(ATTR_PUSH) {
        struct ajj_value obj = TOP(2);
        struct ajj_value val = TOP(1);
        vm_attrpush(a,&obj,&val,CHECK);
        POP(2);
      } vm_end(ATTR_PUSH)

      vm_beg(UPVALUE_SET) {
        int par = next_par(a,CHECK);
        const struct string* upvalue_name =
          cur_function_const_str(a,par);
        struct ajj_value val = TOP(1);
        set_upvalue(a,upvalue_name,&val,CHECK);
        POP(1);
      } vm_end(UPVALUE_SET)

      vm_beg(UPVALUE_GET) {
        int par = next_par(a,CHECK);
        const struct string* upvalue_name =
          cur_function_const_str(a,par);
        struct ajj_value val = get_upvalue(a,
            upvalue_name,CHECK);
        PUSH(val);
      } vm_end(UPVALUE_GET)

      vm_beg(UPVALUE_DEL) {
        int par = next_par(a,CHECK);
        const struct string* upvalue_name =
          cur_function_const_str(a,par);
        del_upvalue(a,upvalue_name,CHECK);
      } vm_end(UPVALUE_DEL)

      vm_beg(JMP) {
        int pos = next_par(a,CHECK);
        cur_frame(a)->pc = pos;
      } vm_end(JMP)

      vm_beg(JT) {
        int pos = next_par(a,CHECK);
        struct ajj_value cond = TOP(1);
        if( is_true(&cond) ) {
          cur_frame(a)->pos = pos;
        }
        POP(1);
      } vm_end(JT)

      vm_beg(JLT) {
        int pos = next_par(a,CHECK);
        struct ajj_value cond = TOP(1);
        if( is_true(&cond) ) {
          cur_frame(a)->pos = pos;
        } else {
          POP(1);
        }
      } vm_end(JLT)

      vm_beg(JLF) {
        int pos = next_par(a,CHECK);
        struct ajj_value cond = TOP(1);
        if( is_false(&cond) ) {
          cur_frame(a)->pos = pos;
        } else {
          POP(1);
        }
      } vm_end(JLF)

      vm_beg(JEPT) {
        int pos = next_par(a,CHECK);
        struct ajj_value cond = TOP(1);
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
        POP(1);
      } vm_end(JEPT)
    }
  }while(1);

fail:

done:
}
