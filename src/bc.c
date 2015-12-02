#include "bc.h"
#include "lex.h"

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
  fprintf(output,"Constant String Table======================\n\n");
  for( i = 0 ; i < prg->str_len ; ++i ) {
    fprintf(output,"%zu: %s\n",i,prg->str_tbl[i].str);
  }
  fprintf(output,"\nConstant Number Table======================\n\n");
  for( i = 0 ; i < prg->num_len ; ++i ) {
    fprintf(output,"%zu: %f\n",i,prg->num_tbl[i]);
  }
  fprintf(output,"\nFunction Argument Table====================\n\n");
  for( i = 0 ; i < prg->par_size ; ++i ) {
    fprintf(output,"%zu:%s:",i,prg->par_list[i].name.str);
    ajj_value_print(&(prg->par_list[i].def_val),
        stdout,AJJ_VALUE_COMPACT);
    fprintf(output,"\n");
  }
  fprintf(output,"\n");
}

#define DO0(N) \
  do { \
    char buf[64]; \
    tk_get_code_snippet(src,prg->spos[cnt],buf,64); \
    fprintf(output,"%d %zu:%d(... %s ...) %s\n",cnt+1,i-1,prg->spos[cnt],buf,N); \
  } while(0); break

#define DO1(N) \
  do { \
    char buf[64]; \
    tk_get_code_snippet(src,prg->spos[cnt],buf,64); \
    a1 = bc_1st_arg(c1);\
    fprintf(output,"%d %zu:%d(... %s ...) %s %d\n",cnt+1,i-5,prg->spos[cnt],buf,N,a1); \
  } while(0); break

#define DO2(N) \
 do { \
   char buf[64]; \
   tk_get_code_snippet(src,prg->spos[cnt],buf,64); \
   a1 = bc_1st_arg(c1);\
   a2 = bc_2nd_arg(prg,&i); \
   fprintf(output,"%d %zu:%d(... %s ...) %s %d %d\n",cnt+1,i-9,prg->spos[cnt],buf,N,a1,a2); \
 } while(0); break

#define DO(A,B,C) case A: DO##B(C);

void dump_program( const char* src , const struct program* prg , FILE* output ) {
  size_t i = 0;
  int a1,a2;
  int cnt = 0;
  dump_program_ctable(prg,output);
  fprintf(output,"Code=======================================\n\n");
  while(1) {
    int c1 = bc_next(prg,&i);
    instructions instr = bc_instr(c1);
    if( instr == VM_HALT ) break;
    switch(instr) {
      VM_INSTRUCTIONS(DO)
      default:
        UNREACHABLE();
        return;
    }
    ++cnt;
  }
}
