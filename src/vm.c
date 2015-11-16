#include "ajj-priv.h"
#include "object.h"
#include <math.h>

#ifndef NDEBUG
#define ALWAYS(X) assert(!(X))
#else
#define ALWAYS(X) X
#endif /* NDEBUG */

/* ============================
 * runtime
 * ==========================*/
static inline
struct runtime* create_runtime( struct ajj_object* tp ) {
  struct runtime* rt = calloc(1,sizeof(*rt));
  rt->cur_tmpl = tp;
  return rt;
}

static inline
void report_error( struct ajj* a , const char* format , ... );

static inline
const char* type_string( int type );

/* =============================
 * Decoding
 * ===========================*/

static inline
int next_b( struct ajj* );

static inline
int next_par( struct ajj* );


/* =============================
 * Type conversions
 * ===========================*/

/* conversion functions */
static inline
double to_number( struct ajj* , const struct ajj_value* , int* fail );

static inline
int to_integer( struct ajj* , const struct ajj_value* , int* fail );

/* =============================
 * Specific instruction handler
 * ============================*/

static inline
struct ajj_value vm_add( struct ajj* a ,
    const struct ajj_value* l,
    const struct ajj_value* r,
    int* fail );

/* inline helpers */
#define enter_function(f,P,M) \
  do { \
    if( rt->cur_call_stk == AJJ_MAX_CALL_STACK ) { \
      report_error(a,"Function recursive call too much,stack overflow!"); \
      return -1; \
    } else { \
      assert(rt->cur_call_stk>0); \
      rt->call_stk[rt->cur_call_stk].entry = (f); \
      rt->call_stk[rt->cur_call_stk].name = (f)->name.str; \
      rt->call_stk[rt->cur_call_stk].esp = rt->call_stk[rt->cur_call_stk-1].esp; \
      rt->call_stk[rt->cur_call_stk].ebp = rt->call_stk[rt->cur_call_stk-1].esp - (P); \
      rt->pc = 0; \
      rt->par_cnt = (P); \
      rt->method = (M); \
    } \
  } while(0)

#define exit_function(ret) \
  do { \
    const struct func_frame* fr; \
    int par_cnt = cur_frame()->par_cnt; \
    assert(rt->cur_call_stk > 0); \
    --rt->cur_call_stk; \
    fr = cur_frame(); \
    fr->esp -= fr->method + par_cnt; \
    assert( fr->esp >= fr->ebp ); \
    PUSH(ret); \
  } while(0)

#define cur_frame() (&(rt->call_stk[rt->cur_call_stk]))
#define cur_function() (cur_frame()->entry)

static inline
const struct string* cur_function_const_str( struct runtime* rt , int idx ) {
  const struct function* c = cur_function();
  assert( IS_JINJA(c) );
  assert( idx < c->f.jj_fn.str_len );
  return &(c->f.jj_fn.str_tbl[idx]);
}

static inline
double cur_function_const_num( struct runtime* rt, int idx ) {
  const struct function* c = cur_function();
  assert( IS_JINJA(c) );
  assert( idx < c->f.jj_fn.num_len );
  return c->f.jj_fn.num_tbl[idx];
}

static inline
int set_upvalue( struct ajj* a, const struct string* name,
    struct ajj_value* value );

static inline
const struct ajj_value*
get_upvalue( struct ajj* a , const struct string* name );

static inline
int del_upvalue( struct ajj* a, const struct string* name );

static inline
const struct function*
get_global_function( struct ajj* a ,const struct string* name ); 

static inline
struct ajj_value*
stack_value( struct runtime* rt , int x ) {
  assert( x >= cur_frame()->ebp && x < cur_frame()->esp );
  return x + rt->val_stk;
}

#ifndef NDEBUG
#define BOT(X) stack_value(rt,cur_frame()->ebp+(X))
#define TOP(X) stack_value(rt,cur_frame()->esp-(X))
#else
#define BOT(X) (rt->val_stk[cur_frame()->ebp+(X)])
#define TOP(X) (rt->val_stk[cur_frame()->esp-(X)])
#endif /* NDEBUG */

#define POP(X) \
  do { \
    struct func_frame* fr = cur_frame(); \
    assert(fr->esp >= (X)); \
    fr->esp -= (X); \
  } while(0)

#define PUSH(X) \
  do { \
    struct func_frame* fr = cur_frame(); \
    if( fr->esp == AJJ_MAX_VALUE_STACK_SIZE ) { \
      report_error(a,"Too much value on value stack!"); \
      return -1; \
    } else { \
      rt->val_stk[fr->esp] = (X); \
      ++fr->esp; \
  } while(0)

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

int vm_run_func( struct ajj* a,
    struct ajj_object* tp,
    const struct string* name,
    struct ajj_value par[AJJ_FUNC_PAR_MAX_SIZE],
    size_t len,
    struct ajj_value* output ) {

  struct runtime* rt;
  struct function* entry;
  struct ajj_value a1,a2,a3,ret;
  int fail;

  assert(a->rt == NULL);
  assert(tp->tp == AJJ_VALUE_OBJECT);

  entry = func_table_find_func(tp->val.obj.fn_tb,name);

  if( entry == NULL ) {
    report_error(a,"No function:%s found in object!",
        name->str);
    goto fail;
  } else if( IS_C(entry) ) {
    report_error(a,"Function:%s is a script function!",
        name->str);
    goto fail;
  }

  /* create all the related resources on stack */
  rt = create_runtime(tp); /* initialize runtime */
  a->rt = rt; /* setup runtime to ajj object */

#define vm_beg(X) case VM_##X:
#define vm_end(X) break;

  do {
    int instr = next_b(a);
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
        int fn_idx= next_par(a);
        int an = next_par(a);
        struct ajj_value argnum = ajj_value_number(an);
        const struct string* fn = cur_function_const_str(rt,fn_idx);
        struct function* f = get_global_function(a,fn);
        if( f == NULL ) {
          report_error(a,"Cannot find function:%s!",fn->str);
          goto fail;
        } else {
          enter_function(f,an,0);
          vm_call(a,CHECK);
        }
      } vm_end(CALL)

      vm_beg(RET) {
        /* check if we have return value here or not !
         * if not, put a dummy NONE on top of the caller stack */
        struct func_frame* fr = cur_frame();
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
        vm_ret(a,CHECK);
        exit_function(ret);
      } vm_end(RET)

      vm_beg(PRINT) {
        const struct string* text = to_string(a,TOP(1),CHECK);
        vm_print(a,text,CHECK);
        POP(1);
      } vm_end(PRINT)

      vm_beg(POP) {
        int arg = next_par(a);
        POP(arg);
      } vm_end(POP)

      vm_beg(TPUSH) {
        int arg = next_par(a);
        struct ajj_value val = *TOP(arg);
        PUSH(val);
      } vm_end(TPUSH)

      vm_beg(BPUSH) {
        int arg = next_par(a);
        struct ajj_value val = *BOT(arg);
        PUSH(val);
      } vm_end(BPUSH)

      vm_beg(BB_MOVE) {
        int l = next_par(a);
        int r = next_par(a);
        struct ajj_val lval = *BOT(l);
        *BOT(l) = *BOT(r);
        *BOT(r) = lval;
      } vm_end(BT_MOVE)
    }
  }while(1);

fail:

done:
}


