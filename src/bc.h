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
#define INCLUDE_UPVALUE 0
#define INCLUDE_JSON 1

/* upvalue modifier */
#define UPVALUE_FIX 0
#define UPVALUE_OVERRIDE 1

/* iterator */
#define ITERATOR_KEY 0
#define ITERATOR_VAL 1
#define ITERATOR_KEYVAL 2

extern struct string THIS;
extern struct string ARGNUM;
extern struct string MAIN;
extern struct string CALLER;
extern struct string FUNC;
extern struct string SUPER;
extern struct string SELF;
extern struct string ITER;

/* Instructions
 * All the instruction itself only occupies one byte, but for alignment, each
 * instruction will at least occupy 4 bytes, or 8 bytes. Each instruction is
 * able to carry 0 , 1 or 2 parameters , depend on the instruction itself. The
 * general format is as follow:
 * --------------
 * |OP|---------|    Instruction carries no parameters
 * --------------
 *
 * --------------
 * |OP| par     |    Instruction carries 1 parameters
 * --------------
 *
 * --------------------------
 * |OP| par 1   |   par 2   |  Instruction carries 2 parameters
 * --------------------------
 *
 * NOTES:
 * This design is a *horrible* design, but I have made it here, so I don't want
 * to refactory it until a major rewrite is needed. The lesson I learned here
 * is to AVOID variant length instruction as much as possible. It is horrible
 * since it is not self indexed and because the decode order is always forward,
 * you are not even sure that where to find an arbitary entry for an instruction.
 * Which is extreamly painful to do patch ! A fixed length instruction will save
 * us much efforts when we want to locate a random instruction or go forward
 * orbackward
 */

#define VM_INSTRUCTIONS(X) \
  X(VM_ADD,0,"add") \
  X(VM_SUB,0,"sub") \
  X(VM_DIV,0,"div") \
  X(VM_MUL,0,"mul") \
  X(VM_MOD,0,"mod") \
  X(VM_POW,0,"pow") \
  X(VM_IN,0,"in") \
  X(VM_NIN,0,"notin") \
  X(VM_LEN,0,"len") \
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
  X(VM_ATTR_CALL,2,"attrcall") \
  X(VM_UPVALUE_SET,1,"upvalueset") \
  X(VM_UPVALUE_GET,1,"upvalueget") \
  X(VM_UPVALUE_DEL,1,"upvaluedel") \
  X(VM_JMP,1,"jmp") \
  X(VM_JT,1,"jt") \
  X(VM_JF,1,"jf") \
  X(VM_JLT,1,"jlt") \
  X(VM_JLF,1,"jlf") \
  X(VM_JMPC,2,"jmpc") \
  X(VM_JEPT,1,"jept") \
  X(VM_ITER_START,0,"iterstart") \
  X(VM_ITER_HAS,0,"iterhas") \
  X(VM_ITER_MOVE,0,"itermove") \
  X(VM_ITER_DEREF,1,"iterderef") \
  X(VM_ENTER,0,"enter") \
  X(VM_EXIT,0,"exit") \
  X(VM_INCLUDE,2,"include") \
  X(VM_IMPORT,2,"import") \
  X(VM_EXTENDS,0,"extends") \
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

#define BC_1ST_MASK (0x00ffffff)

#define BC_INSTRUCTION(C) ((instructions)((C)>>24))
#define BC_1ARG(C) ((C)&BC_1ST_MASK)
#define BC_WRAP_INSTRUCTION0(C) ((C)<<24)
#define BC_WRAP_INSTRUCTION1(C,A) (((C)<<24) | (A&BC_1ST_MASK))

const char* bc_get_instruction_name( int );

int bc_get_argument_num( instructions );

/* emitter for byte codes */
struct emitter {
  struct program* prg;
  size_t cd_cap; /* capacity for current code */
};

static
void emitter_ensure( struct emitter* em ) {
  /* reserve size for 2 arrays */
  if( em->cd_cap < em->prg->len + 2 ) {
    size_t c = em->cd_cap;
    em->prg->codes = mem_grow( em->prg->codes ,
        sizeof(int),2,&c);
    em->prg->spos = mem_grow(em->prg->spos,
        sizeof(int),2,&(em->cd_cap));
  }
}

static
void emitter_init( struct emitter* em , struct program* prg ) {
  em->prg = prg;
  em->cd_cap = 0;
  emitter_ensure(em);
}

static
void emitter_emit0( struct emitter* em , int spos , int bc ) {
  emitter_ensure(em);
  em->prg->spos[em->prg->len] = spos;
  assert(bc>=0 && bc< SIZE_OF_INSTRUCTIONS);
  em->prg->codes[em->prg->len++] = BC_WRAP_INSTRUCTION0(bc);
}

static
void emitter_emit1( struct emitter* em , int spos , int bc , int a1 ) {
  emitter_ensure(em);
  em->prg->spos[em->prg->len] = spos;
  assert(bc>=0 && bc< SIZE_OF_INSTRUCTIONS);
  assert( (a1&BC_1ST_MASK) == a1 );
  em->prg->codes[em->prg->len++] = BC_WRAP_INSTRUCTION1(bc,a1);
}

static
void emitter_emit2( struct emitter* em , int spos , int bc , int a1 , int a2 ) {
  emitter_emit1(em,spos,bc,a1);
  em->prg->codes[em->prg->len++] = a2;
}

static
int emitter_put( struct emitter* em , int arg_sz ) {
  int ret;
  int add;
  assert( arg_sz == 0 || arg_sz == 1 || arg_sz == 2 );
  emitter_ensure(em);
  add = (arg_sz == 0 || arg_sz == 1) ? 1 : 2;
  ret = em->prg->len;
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
void emitter_emit1_at( struct emitter* em , int pos ,
    int spos , int bc , int a1 ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emitter_emit1(em,spos,bc,a1);
  em->prg->len = save;
}

static
void emitter_emit2_at( struct emitter* em , int pos ,
    int spos , int bc , int a1 , int a2 ) {
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
int bc_next( const struct program* prg , size_t* pos ) {
  if( prg->len == *pos ) {
    return BC_WRAP_INSTRUCTION0(VM_HALT);
  } else {
    return prg->codes[(*pos)++];
  }
}

static
instructions bc_instr( int c ) {
  return (instructions)(c>>24);
}

static
int bc_1st_arg( int c ) {
  return BC_1ARG(c);
}

static
int bc_2nd_arg( const struct program* prg , size_t* pos ) {
  assert( prg->len > *pos );
  return prg->codes[(*pos)++];
}

/* dump program into human readable format */
void dump_program( struct ajj* a,
    const char* src ,
    const struct program* ,
    struct ajj_io* output );

#endif /* _BC_H_ */
