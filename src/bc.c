#include "bc.h"

struct string THIS = CONST_STRING("__this__");
struct string ARGNUM = CONST_STRING("__argnum__");
struct string MAIN = CONST_STRING("__main__");
struct string CALLER = CONST_STRING("__caller__");
struct string SUPER  = CONST_STRING("super");
struct string SELF   = CONST_STRING("self");
struct string ITER   = CONST_STRING("__iterator__");


const char*
bc_get_instruction_name( int bc ) {
#define X(A,B,C) case A: return C;
  switch(bc) {
    VM_INSTRUCTIONS(X)
    default:
      return NULL;
  }
#undef X
}

/* dump the constant table of a program */
static
void dump_program_ctable( const struct program* prg , FILE* output ) {
  size_t i;
  fprintf(output,"-----Constant String Table--------------\n");
  for( i = 0 ; i < prg->str_len ; ++i ) {
    fprintf(output,"%zu: %s\n",i,prg->str_tbl[i].str);
  }
  fprintf(output,"-----Constant Number Table--------------\n");
  for( i = 0 ; i < prg->num_len ; ++i ) {
    fprintf(output,"%zu: %f\n",i,prg->num_tbl[i]);
  }
}

#define DO0(N) \
  do { \
    fprintf(output,"%zu:%d %s\n",i,prg->spos[i],N); \
  } while(0); break

#define DO1(N) \
  do { \
    a1 = instru_next_arg(prg,&i); \
    fprintf(output,"%zu:%d %s %d\n",i,prg->spos[i],N,a1); \
  } while(0); break

#define DO2(N) \
 do { \
   a1 = instru_next_arg(prg,&i); \
   a2 = instru_next_arg(prg,&i); \
   fprintf(output,"%zu:%d %s %d %d\n",i,prg->spos[i],N,a1,a2); \
 } while(0); break

#define DO(A,B,C) case A: DO##B(C);

void dump_program( const struct program* prg , FILE* output ) {
  size_t i = 0;
  int a1,a2;
  dump_program_ctable(prg,output);
  fprintf(output,"-----Code------------------------\n");
  while(1) {
    instructions instr = instru_next(prg,&i);
    if( instr == VM_HALT ) break;
    switch(instr) {
      VM_INSTRUCTIONS(DO)
      default:
        printf("%d\n",instr);
        UNREACHABLE();
        return;
    }
  }
}
