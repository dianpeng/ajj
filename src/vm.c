#include "ajj-priv.h"
#include "object.h"
#include "ajj-priv.h"

#include <math.h>


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
#define enter_function(f,O,P) \
  do { \
    if( rt->cur_call_stk == AJJ_MAX_CALL_STACK ) { \
      report_error(a,"Function recursive call too much,stack overflow!"); \
      return -1; \
    } else { \
      assert(rt->cur_call_stk>0); \
      rt->call_stk[rt->cur_call_stk].entry = (f); \
      rt->call_stk[rt->cur_call_stk].name = (f)->name.str; \
      rt->call_stk[rt->cur_call_stk].esp = rt->call_stk[rt->cur_call_stk-1].esp; \
      rt->call_stk[rt->cur_call_stk].ebp = rt->call_stk[rt->cur_call_stk-1].esp - (O); \
      rt->pc = 0; \
      rt->par_cnt = (P); \
    } \
  } while(0)

#define exit_function(f) \
  do { \
    assert(rt->cur_call_stk > 0); \
    --rt->cur_call_stk; \
  } while(0)

#define cur_frame() (&(rt->cur_stk[rt->cur_call-stk]))
#define cur_function() (cur_frame()->entry)

static inline
struct ajj_value*
stack_value( struct runtime* rt , int x ) {
  assert( x < rt->cur_val_stk );
  return x + rt->val_stk;
}

#ifndef NDEBUG
#define BOT(X) stack_value(rt,(X))
#define TOP(X) stack_value(rt,rt->cur_val_stk(X))
#else
#define BOT(X) (rt->val_stk[(X)])
#define TOP(X) (rt->val_stk[rt->cur_val_stk-(X)])
#endif /* NDEBUG */

#define POP(X) \
  do { \
    assert( rt->cur_val_stk >= (X) ); \
    rt->cur_val_stk -= (X); \
  } while(0)

#define PUSH(X) \
  do { \
    if( rt->cur_val_stk == AJJ_MAX_VALUE_STACK_SIZE ) { \
      report_error(a,"Expression overflow!"); \
      return -1; \
    } else { \
      rt->val_stk[rt->cur_val_stk] = (X); \
      ++rt->cur_val_stk; \
    } \
  } while(0)

#define MUSTBE(X,T,I) \
  do { \
    if( (T) != (X).type ) { \
      report_error(a,"Type mismatch for operation:%s,expect:%s but get:%s", \
          (I), \
          type_string((T)), \
          type_string((X)->type)); \
      return -1; \
    } \
  } while(0)

#define CHECKN(...) &fail); \
  do { \
    if(fail) { \
      report_error(a,__VA_ARGS__); \
      return -1; \
    } \
  } while(0

#define CHECK &fail); \
  do { \
    if(!fail) return -1; \
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
    return -1;
  } else if( IS_C(entry) ) {
    report_error(a,"Function:%s is a script function!",
        name->str);
    return -1;
  }

  /* create all the related resources on stack */
  rt = create_runtime(tp); /* initialize runtime */
  a->rt = rt; /* setup runtime to ajj object */

#define vm_beg(X) case VM_##X:
#define vm_end(X) break;

  do {
    int instr = next_b(a);
    switch(instr) {

      vm_beg(ADD) {
        struct ajj_value o = vm_add(a,&TOP(2),&TOP(1),CHECK);
        POP(2);
        PUSH(o);
      } vm_end(ADD)

      vm_beg(SUB) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,&TOP(2),CHECK);
        r = to_number(a,&TOP(1),CHECK);
        o = ajj_value_number(l+r);
        POP(2);
        PUSH(o);
      } vm_end(SUB)

      vm_beg(DIV) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,&TOP(2),CHECK);
        r = to_number(a,&TOP(1),CHECK);
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
        l = to_number(a,&TOP(2),CHECK);
        r = to_number(a,&TOP(1),CHECK);
        o = ajj_value_number(l*r);
        POP(2);
        PUSH(o);
      } vm_end(MUL)

      vm_beg(POW) {
        double l , r;
        struct ajj_value o;
        l = to_number(a,&TOP(2),CHECK);
        r = to_number(a,&TOP(1),CHECK);
        o = ajj_value_number(pow(l,r));
        POP(2);
        PUSH(o);
      } vm_end(POW)

      vm_beg(EQ) {
        struct ajj_value o = vm_eq(a,&TOP(2),&TOP(1),CHECK);
        POP(2);
        PUSH(o);
      } vm_end(EQ)

      vm_beg(NE) {
        struct ajj_value o = vm_ne(a,&TOP(2),&TOP(1),CHECK);
        POP(2);
        PUSH(o);
      } vm_end(NE)

      vm_beg(LT) {
        struct ajj_value o = vm_lt(a,&TOP(2),&TOP(1),CHECK);
        POP(2);
        PUSH(o);
      } vm_end(LT)














































}
