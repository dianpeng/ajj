#ifndef _BC_H_
#define _BC_H_
#include "util.h"
#include "vm.h"
#include <stdio.h>

#ifndef MINIMUM_CODE_PAGE_SIZE
#define MINIMUM_CODE_PAGE_SIZE 256
#endif /* MINIMUM_CODE_PAGE_SIZE */

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

/* iterator */
#define ITERATOR_KEY 0
#define ITERATOR_VAL 1
#define ITERATOR_KEYVAL 2

extern struct string THIS;
extern struct string ARGNUM;
extern struct string MAIN;
extern struct string CALLER;
extern struct string SUPER;
extern struct string SELF;
extern struct string ITER;

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
  X(VM_ADD,0,"add") \
  X(VM_SUB,0,"sub") \
  X(VM_DIV,0,"div") \
  X(VM_MUL,0,"mul") \
  X(VM_MOD,0,"mod") \
  X(VM_POW,0,"pow") \
  X(VM_EQ,0,"eq") \
  X(VM_NE,0,"ne") \
  X(VM_LT,0,"lt") \
  X(VM_LE,0,"le") \
  X(VM_GT,0,"gt") \
  X(VM_GE,0,"ge") \
  X(VM_NOT,0,"not") \
  X(VM_NEG,0,"neg") \
  X(VM_DIVTRUCT,0,"divtruct") \
  X(VM_CALL,2,"call") \
  X(VM_RET,0,"ret") \
  X(VM_PRINT,0,"print") \
  X(VM_POP,1,"pop") \
  X(VM_TPUSH,1,"tpush") \
  X(VM_BPUSH,1,"bpush") \
  X(VM_MOVE,2,"move") \
  X(VM_STORE,1,"store") \
  X(VM_LIFT,2,"lift") \
  X(VM_LSTR,1,"lstr") \
  X(VM_LTRUE,0,"ltrue") \
  X(VM_LFALSE,0,"lfalse") \
  X(VM_LNUM,1,"lnum") \
  X(VM_LZERO,0,"lzero") \
  X(VM_LNONE,0,"lnone") \
  X(VM_LIMM,1,"limm") \
  X(VM_LLIST,0,"llist") \
  X(VM_LDICT,0,"ldict") \
  X(VM_ATTR_SET,0,"attrset") \
  X(VM_ATTR_PUSH,0,"attrpush") \
  X(VM_ATTR_GET,0,"attrget") \
  X(VM_ATTR_CALL,1,"attrcall") \
  X(VM_UPVALUE_SET,1,"upvalueset") \
  X(VM_UPVALUE_GET,1,"upvalueget") \
  X(VM_UPVALUE_DEL,1,"upvaluedel") \
  X(VM_JMP,1,"jmp") \
  X(VM_JT,1,"jt") \
  X(VM_JF,1,"jf") \
  X(VM_JLT,1,"jlt") \
  X(VM_JLF,1,"jlf") \
  X(VM_JMPC,1,"jmpc") \
  X(VM_JEPT,1,"jept") \
  X(VM_ITER_START,0,"iterstart") \
  X(VM_ITER_HAS,0,"iterhas") \
  X(VM_ITER_MOVE,0,"itermove") \
  X(VM_ITER_DEREF,1,"iterderef") \
  X(VM_ENTER,0,"enter") \
  X(VM_EXIT,0,"exit") \
  X(VM_INCLUDE,2,"include") \
  X(VM_IMPORT,2,"import") \
  X(VM_EXTENDS,1,"extends") \
  X(VM_NOP0,0,"nop0") \
  X(VM_NOP1,1,"nop1") \
  X(VM_NOP2,2,"nop2") \
  X(VM_HALT,0,"halt") \
  X(VM_ERROR,0,"error")

typedef enum {
#define X(A,B,C) A,
  VM_INSTRUCTIONS(X)
  SIZE_OF_INSTRUCTIONS
#undef X
} instructions;

const char* bc_get_instruction_name( int );

/* emitter for byte codes */
struct emitter {
  struct program* prg;
  size_t cd_cap; /* capacity for current code */
  size_t spos_cap;/* capacity for source code ref */
};

#define EMIT_RESERVE(em,cap,C,PS,PTR,L) \
  do { \
    void* nc; \
    assert( em->C < (cap) ); \
    nc = malloc((cap)*(PS)); \
    if( em->prg->PTR && em->prg->len != 0 ) { \
      memcpy(nc,em->prg->PTR,em->prg->L); \
      free(em->prg->PTR); \
    } \
    em->prg->PTR = nc; \
    em->C = cap; \
  } while(0)

static  
void emitter_reserve_code_page( struct emitter* em , int cap ) {
  EMIT_RESERVE(em,cap,cd_cap,1,codes,len);
}

static  
void emitter_reserve_spos( struct emitter* em , int cap ) {
  EMIT_RESERVE(em,cap,spos_cap,sizeof(int),spos,spos_len);
}

static  
void emitter_emit0( struct emitter* em , int spos , int bc ) {
  /* reserve size for 2 arrays */
  if( em->cd_cap <= em->prg->len + 9 ) {
    emitter_reserve_code_page(em,2*em->cd_cap+9);
  }
  if( em->spos_cap == em->prg->spos_len ) {
    emitter_reserve_spos(em,em->spos_cap*2);
  }

  /* push source code reference */
  em->prg->spos[em->prg->spos_len] = spos;
  ++em->prg->spos_len;

  assert(bc>=0 && bc< SIZE_OF_INSTRUCTIONS);

  *((unsigned char*)(em->prg->codes) + em->prg->len)
    = (unsigned char)(bc);
  ++(em->prg->len);
}

static  
void emitter_init( struct emitter* em , struct program* prg ) {
  em->prg = prg;
  em->cd_cap = 0;
  em->spos_cap= 0;
  emitter_reserve_spos(em,MINIMUM_CODE_PAGE_SIZE);
  emitter_reserve_code_page(em,MINIMUM_CODE_PAGE_SIZE);
}

static  
void emitter_emit_int( struct emitter* em , int arg ) {
  assert( em->cd_cap >= em->prg->len + 4 ); /* ensure we have enough space to run */
  *((int*)((char*)(em->prg->codes)+em->prg->len)) = arg;
  em->prg->len += 4;
}

static  
void emitter_emit1( struct emitter* em , int spos , int bc , int a1 ) {
  emitter_emit0(em,spos,bc);
  emitter_emit_int(em,a1);
}

static  
void emitter_emit2( struct emitter* em , int spos , int bc , int a1 , int a2 ) {
  emitter_emit1(em,spos,bc,a1);
  emitter_emit_int(em,a2);
}

static  
int emitter_put( struct emitter* em , int arg_sz ) {
  int ret;
  size_t add;
  assert( arg_sz == 0 || arg_sz == 1 || arg_sz == 2 );
  ret = em->prg->len;
  add = arg_sz * 4 + 1;
  if( em->cd_cap <= em->prg->len + add ) {
    emitter_reserve_code_page(em,em->cd_cap * 2 );
  }
  em->prg->len += add;
  return ret;
}

static  
void emitter_emit0_at( struct emitter* em , int pos , int spos , int bc ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emitter_emit0(em,spos,bc);
  em->prg->len = save;
}

static  
void emitter_emit1_at( struct emitter* em , int pos , int spos ,
    int bc , int a1 ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emitter_emit1(em,spos,bc,a1);
  em->prg->len = save;
}

static  
void emitter_emit2_at( struct emitter* em , int pos , int bc ,
    int spos , int a1 , int a2 ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emitter_emit2(em,spos,bc,a1,a2);
  em->prg->len = save;
}

static  
int emitter_label( struct emitter* em ) {
  return (int)(em->prg->len);
}

static  
instructions bc_next( const struct program* prg , size_t* pos ) {
  if( prg->len == *pos ) {
    return VM_HALT;
  } else {
    instructions instr = (instructions)(*((unsigned char*)(prg->codes)+*pos));
    ++*pos;
    return instr;
  }
}

static  
int bc_next_arg( const struct program* prg , size_t* pos ) {
  assert(*pos <= prg->len+4);
  int arg = *((int*)((char*)(prg->codes)+*pos));
  *pos += 4;
  return arg;
}

/* dump program into human readable format */
void dump_program( const struct program* , FILE* output );

#endif /* _BC_H_ */
