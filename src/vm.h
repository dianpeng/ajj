#ifndef _VM_H_
#define _VM_H_
#include "ajj.h"
#include "util.h"
#include <stdio.h>

struct ajj;
struct ajj_object;
struct func_table;
struct gc_scope;

#define MINIMUM_CODE_PAGE_SIZE 256

/* parametr for looping */
#define LOOP_CONTINUE 0
#define LOOP_BREAK 1

/* parameter for include instruction */
#define INCLUDE_NONE 0
#define INCLUDE_UPVALUE 1
#define INCLUDE_JSON 2

/* upvalue modifier */
#define INCLUDE_UPVALUE_FIX 0
#define INCLUDE_UPVALUE_OVERRIDE 1

/* Instructions documentation =====================================================
 * We will use a stack based VM to do evaluations. It means for our VM, we have a
 * value stack that is used to store temporary evaluating value and also the local
 * variables. For each execution frame (function frame), we have 2 registers, one is
 * EBP points to the start/bottom of the stack ; and the other is the ESP points to
 * the end/top of the stack. Additionally , a frame also has a PC register points to
 * the code points.
 * The execution is based on function/frame, each function has its own code page, it
 * means each function's code doesn't resides on a single memory page.
 * In VM, we have 2 types of addressing mode, and all corresponding instructions will
 * have B or T prefix. B stands from bottom ; and T stands from top.
 *
 * Following will describe each instructions meannig, the format is as follow :
 * instr(par-num) , which stands for , to instruction "instr" , it has "par-num" para
 * -meters per call. Assume EBP points to the end of value stack ; and ESP points to
 *  the start of value stack. B[X] = EBP+X ; T[X] = ESP-X
 *
 * 1. add(0) : add and pop top 2 values, push the result back to stack.
 * 2. sub(0) : sub and pop top 2 values, push the result back to stack.
 * 3. div(0) : div and pop top 2 values, push the result back to stack.
 * 4. mul(0) : mul and pop top 2 values, push the result back to stack.
 * 5. pow(0) : pow and pop top 2 values, push the result back to stack.
 * 6. eq (0) : check equal and pop top 2 values, push result back to stack
 * 7. ne (0) : check not equal and pop top 2 values, push result back to stack
 * 8. lt (0) : check less equal and pop top 2 values, push result back to stack
 * 9. le (0) : check less or equal and pop top 2 values, push result back to stack
 *10. gt (0) : check greater than and pop top 2 values, push result back to stack
 *11. ge (0) : check greater than and pop top 2 values, push result back to stack
 *14. neg(0) : negate and pop top 1 values, push result back to stack.
 *15. divtruct(0): div truncate and pop top 2 values,push result back to stack.
 *17. call(2): @1: function name index ; @2: function parameter count ;
               call a function. Pop all the function calling frame out of stack push result on
 *18. ret (0): return from a function call.
 *19. print(0): print and pop top value.
 *20. pop (1): @1 number of values . Pop top @1 values out.
 *21. tpush(1): @1 address of value on stack (index).
                push the value on T[@1] at the top of stack.
 *23. bpush(1): @1 address of value on stack(index).
                push the value on B[@1]
 *26. move(2): @1 address of left operand ; @2 right operand
                move B[@2] to B[@1].
 *29. lstr(1): @1 index in constant string table.
                load the string onto the stack
 *30. ltrue(0): load trun onto the stack
 *31. lfalse(0):load false onto the stack
 *32. lnum(1) : @1 index in constant number table.
                load the number onto the stack
 *33. lzero(0): load zero onto stack.
 *34. lnone(0): load none onto stack.
 *35. limm(1) : @1 number(integer).
                load this immdieate number onto stack.
 *36. llist(0): load an empty list onto stack.
 *37. ldict(0): load an empty dict onto stack.
 *38. attrset(0): T[3]: objects ; T[2]: key ; T[1]: value.
                push "key" and "value" pair into "object" and pop "key" and "value"
 *39. attrpush(0): T[2]: list ; T[1]: value.
                push "value" into "list" and pop "value".
 *40. attrget(0): T[2]:object ; T[1]:key.
                get value from "object" with "key" onto stack and pop "object" AND "key".
 *41. attrcall(2): @1 function name index ; @2 function parameters count T.
                  T[@2+1]: object
                  call a function with name @1 and has function parameter @2 on "object",
                  and push the result on stack and pop calling frame out.
 *42. upvalueset(1): @1 upvalue name.
                    set upvalue with name @1 with value on T[1] and pop T[1].
 *43. upvalueget(1): @1 upvalue name.
                    get upvalue with name @1 onto stack.
 *44. upvaluedel(1): @1 upvalue name.
                    del upvalue with name @1
 *45. loop(1): @1 name of the loop closure
                basic loop calling , after looping, it will pop the top 1 out of stack.
 *46. looprec(1): @1 name of the loop closure
                recursively loop calling, after looping it will pop the top 1 out of stack.
 *47. jmp(1): @1 PC position.
                jump to @1
 *48. jt (1): @1 PC position.
                jump to @1 only when the top of stack is evaluated to true. Pop T[1].
 *49. jlt(1): @1 PC position @2
                jump to @1 only when the top of stack is evaluated to true. Pop T[1] only when
                jump is not performed
 *50. jlf(1): @1 PC position @2
                jump to @1 only when the top of stack is evaluated to false. Pop T[1] only when
                the jump is not performed
 *51. jept(1): @1 PC position.
                jump to @1 only when T[1] is empty. Pop T[1].
 *52. enter(0): Enter a GC scope.
 *53. exit(0) : Exit a GC scope.
 *54. include(2): @1 include type @2 optional variable.
                @1 could be 0 (NONE) , 1(UPVALUE) and 2(JSON).
                @2 is the function parameter if we have.
                After include/rendering the template on to stack, pop all the related value.
 *55. import(2): ----
 *56. import_symbol:
 *57. extends
 *58. nop0,nop1,nop2
 */

#define VM_INSTRUCTIONS(X) \
  X(VM_ADD,"add") \
  X(VM_SUB,"sub") \
  X(VM_DIV,"div") \
  X(VM_MUL,"mul") \
  X(VM_MOD,"mod") \
  X(VM_POW,"pow") \
  X(VM_EQ,"eq") \
  X(VM_NE,"ne") \
  X(VM_LT,"lt") \
  X(VM_LE,"le") \
  X(VM_GT,"gt") \
  X(VM_GE,"ge") \
  X(VM_NOT,"not") \
  X(VM_NEG,"neg") \
  X(VM_DIVTRUCT,"divtruct") \
  X(VM_CALL,"call") \
  X(VM_RET,"ret") \
  X(VM_PRINT,"print") \
  X(VM_POP,"pop") \
  X(VM_TPUSH,"tpush") \
  X(VM_BPUSH,"bpush") \
  X(VM_MOVE,"move") \
  X(VM_STORE,"store") \
  X(VM_LIFT,"lift") \
  X(VM_LSTR,"lstr") \
  X(VM_LTRUE,"ltrue") \
  X(VM_LFALSE,"lfalse") \
  X(VM_LNUM,"lnum") \
  X(VM_LZERO,"lzero") \
  X(VM_LNONE,"lnone") \
  X(VM_LIMM,"limm") \
  X(VM_LLIST,"llist") \
  X(VM_LDICT,"ldict") \
  X(VM_ATTR_SET,"attrset") \
  X(VM_ATTR_PUSH,"attrpush") \
  X(VM_ATTR_GET,"attrget") \
  X(VM_ATTR_CALL,"attrcall") \
  X(VM_UPVALUE_SET,"upvalueset") \
  X(VM_UPVALUE_GET,"upvalueget") \
  X(VM_UPVALUE_DEL,"upvaluedel") \
  X(VM_JMP,"jmp") \
  X(VM_JT,"jt") \
  X(VM_JLT,"jlt") \
  X(VM_JLF,"jlf") \
  X(VM_JMPC,"jmpc") \
  X(VM_JEPT,"jept") \
  X(VM_ITER_START,"iterstart") \
  X(VM_ITER_HAS,"iterhas") \
  X(VM_ITER_MOVE,"itermove") \
  X(VM_ITER_DEREF,"iterderef") \
  X(VM_ENTER,"enter") \
  X(VM_EXIT,"exit") \
  X(VM_INCLUDE,"include") \
  X(VM_IMPORT,"import") \
  X(VM_IMPORT_SYMBOL,"importsymbol") \
  X(VM_EXTENDS,"extends") \
  X(VM_NOP0,"nop0") \
  X(VM_NOP1,"nop1") \
  X(VM_NOP2,"nop2") \
  X(VM_HALT,"halt") \
  X(VM_ERROR,"error") \

enum vm_instructions {
#define X(A,B) A,
  VM_INSTRUCTIONS(X)
  SIZE_OF_INSTRUCTIONS
#undef X
};

const char* vm_get_instr_name( int );

extern struct string THIS = CONST_STRUBUF("__this__");
extern struct string ARGNUM = CONST_STRBUF("__argnum__");
extern struct string MAIN = CONST_STRBUF("__main__");
extern struct string CALLER = CONST_STRBUF("__caller__");
extern struct string SUPER  = CONST_STRBUF("super");
extern struct string SELF   = CONST_STRBUF("self");
extern struct string ITER   = CONST_STRBUF("__iterator__");

struct program {
  void* codes;
  size_t len;
  int* spos;
  int spos_len;
  struct string str_tbl[ AJJ_LOCAL_CONSTANT_SIZE ];
  size_t str_len;
  double num_tbl[AJJ_LOCAL_CONSTANT_SIZE];
  size_t num_len;
  /* parameter prototypes. Program is actually a script based
   * function routine. Each program will have a prototypes */
  struct {
    struct string name;
    struct ajj_value def_val; /* Default value for function parameters.
                               * These values are owned by the scope that owns
                               * this template object. It is typically global
                               * scope */
  } par_list[ AJJ_FUNC_ARG_MAX_SIZE ];
  size_t par_size;
};

/* Execution context */
struct func_frame {
  const struct function* entry; /* Function entry */
  int ebp; /* EBP register */
  int esp; /* ESP register */
  int pc ; /* Program counter register */
  struct string name; /* function name */
  int par_cnt : 16 ;
  int method  : 1; /* whether this call is a method call */
};

struct runtime {
  struct ajj_object* cur_tmpl;   /* current jinja template */
  struct func_frame call_stk[ AJJ_MAX_CALL_STACK ];
  size_t cur_call_stk; /* Current stk position */
  struct ajj_value val_stk[AJJ_MAX_VALUE_STACK_SIZE]; /* current value stack size */
  struct gc_scope* cur_gc; /* current gc scope */
  FILE* output;
};

/* emitter for byte codes */
struct emitter {
  struct program* prg;
  size_t cd_cap; /* capacity for current code */
  size_t spos_cap;/* capacity for source code ref */
};

static inline
void program_init( struct program* prg ) {
  prg->codes = NULL;
  prg->len = 0;
  prg->str_len = 0;
  prg->num_len = 0;
  prg->par_size =0;
  prg->spos = NULL;
  prg->spos_len = 0;
}

static inline
int program_add_par( struct program* prg , struct string* name ,
    int own, const struct ajj_value* val ) {
  if( prg->par_size == AJJ_FUNC_ARG_MAX_SIZE )
    return -1;
  else {
    assert(name.len < AJJ_SYMBOL_NAME_MAX_SIZE );
    prg->par_list[prg->par_size].def_val = *val; /* owned the value */
    prg->par_list[prg->par_size].name = own ? *name : string_dup(name);
    ++prg->par_size;
    return 0;
  }
}

#define EMIT_RESERVE(em,cap,C,PS,PTR,L) \
  do { \
    void* nc; \
    assert( em->C < (cap) ); \
    nc = malloc((C)*(PS)); \
    if( em->prg->PTR ) { \
      memcpy(nc,em->prg->PTR,em->prg->L); \
      free(em->prg->PTR); \
    } \
    em->prg->PTR = nc; \
    em->C = cap; \
  } while(0)

static inline
void reserve_code_page( struct emitter* em , int cap ) {
  EMIT_RESERVE(em,cap,cd_cap,1,codes,len);
}

static inline
void reserve_spos( struct emmitter* em , int cap ) {
  EMIT_RESERVE(em,cap,spos_cap,sizeof(int),spos,spos_len);
}

static inline
void emit0( struct emitter* em , int spos , int bc ) {
  /* reserve size for 2 arrays */
  if( em->cd_cap <= em->prg->len + 9 ) {
    reserve_code_page(em,2*em->cd_cap);
  }
  if( em->spos_cap == em->prg->spos_len ) {
    reserve_spos(em->spos_cap*2);
  }

  /* push source code reference */
  em->prg->spos[em->prg->spos_len] = spos;
  ++em->prg->spos_len;

  *((unsigned char*)(em->prg->codes) + em->prg->len)
    = (unsigned char)(bc);
  ++(em->prg->len);
}

static inline
void emitter_init( struct emitter* em , struct program* prg ) {
  em->prg = prg;
  em->cd_cap = MINIMUM_CODE_PAGE_SIZE;
  em->spos_cap= MINIMUM_CODE_PAGE_SIZE;
  reserve_spos(em,MINIMUM_CODE_PAGE_SIZE/2);
  reserve_code_page(em,MINIMUM_CODE_PAGE_SIZE/2);
}

static inline
void emit_int( struct emitter* em , int arg ) {
  int l;
  assert( em->cd_cap > em->prg->len + 4 ); /* ensure we have enough space to run */
  *((int*)((char*)(em->prg->codes)+em->prg->len)) = arg;
  em->prg->len += 4;
}

static inline
void emitter_emit1( struct emitter* em , int spos , int bc , int a1 ) {
  emit0(em,spos,bc);
  emit_int(em,a1);
}

static inline
void emitter_emit2( struct emitter* em , int spos , int bc , int a1 , int a2 ) {
  emit1(em,spos,bc,a1);
  emit_int(em,a2);
}

static inline
int emitter_put( struct emitter* em , int arg_sz ) {
  int ret;
  size_t add;
  assert( arg_sz == 0 || arg_sz == 1 || arg_sz == 2 );
  ret = em->prg->len;
  add = arg_sz * 4 + 1;
  if( em->cd_cap <= em->prg->len + add ) {
    reserve_code_page(em,em->cd_cap * 2 );
  }
  em->prg->len += add;
  return ret;
}

static inline
void emitter_emit0_at( struct emitter* em , int pos , int spos , int bc ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emit0(em,spos,bc);
  em->prg->len = save;
}

static inline
void emitter_emit1_at( struct emitter* em , int pos , int spos ,
    int bc , int a1 ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emit1(em,spos,bc,a1);
  em->prg->len = save;
}

static inline
void emitter_emit2_at( struct emitter* em , int pos , int bc ,
    int spos , int a1 , int a2 ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emit2(em,spos,bc,a1,a2);
  em->prg->len = save;
}

static inline
int emitter_label( struct emitter* em ) {
  return (int)(em->prg->len);
}

/* =============================================
 * Interfaces
 * ===========================================*/

int vm_run_func( struct ajj* ,
    struct ajj_object* tp,
    const struct string* name,
    struct ajj_value par[AJJ_FUNC_ARG_MAX_SIZE],
    size_t len,
    ajj_value* output );

int vm_run_jj( struct ajj* a,
    struct ajj_object* tp ,
    struct ajj_value par[AJJ_FUNC_ARG_MAX_SIZE],
    size_t len,
    ajj_value* output );

/* Dump an program to output */
void vm_dump( const struct program* prg, FILE* output );

#endif /* _VM_H_ */

