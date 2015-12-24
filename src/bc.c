#include "bc.h"
#include "lex.h"

struct string THIS = CONST_STRING("__this__");
struct string ARGNUM = CONST_STRING("__argnum__");
struct string MAIN = CONST_STRING("__main__");
struct string FUNC = CONST_STRING("__func__");
struct string CALLER = CONST_STRING("__caller__");
struct string SUPER  = CONST_STRING("super");
struct string SELF   = CONST_STRING("self");
struct string ITER   = CONST_STRING("__iterator__");
struct string LOOP   = CONST_STRING("loop");

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

void emitter_init( struct emitter* em , struct program* prg ) {
  em->prg = prg;
  em->cd_cap = 0;
  emitter_ensure(em);
}

void emitter_emit0( struct emitter* em , int spos , int bc ) {
  emitter_ensure(em);
  em->prg->spos[em->prg->len] = spos;
  assert(bc>=0 && bc< SIZE_OF_INSTRUCTIONS);
  em->prg->codes[em->prg->len++] = BC_WRAP_INSTRUCTION0(bc);
}

void emitter_emit1( struct emitter* em , int spos , int bc , int a1 ) {
  emitter_ensure(em);
  em->prg->spos[em->prg->len] = spos;
  assert(bc>=0 && bc< SIZE_OF_INSTRUCTIONS);
  assert( (a1&BC_1ST_MASK) == a1 );
  em->prg->codes[em->prg->len++] = BC_WRAP_INSTRUCTION1(bc,a1);
}

void emitter_emit2( struct emitter* em , int spos , int bc , int a1 , int a2 ) {
  emitter_emit1(em,spos,bc,a1);
  em->prg->codes[em->prg->len++] = a2;
}

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

void emitter_emit0_at( struct emitter* em , int pos , int spos , int bc ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emitter_emit0(em,spos,bc);
  em->prg->len = save;
}

void emitter_emit1_at( struct emitter* em , int pos ,
    int spos , int bc , int a1 ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emitter_emit1(em,spos,bc,a1);
  em->prg->len = save;
}

void emitter_emit2_at( struct emitter* em , int pos ,
    int spos , int bc , int a1 , int a2 ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emitter_emit2(em,spos,bc,a1,a2);
  em->prg->len = save;
}

int bc_next( const struct program* prg , size_t* pos ) {
  if( prg->len == *pos ) {
    return BC_WRAP_INSTRUCTION0(VM_HALT);
  } else {
    return prg->codes[(*pos)++];
  }
}

int bc_2nd_arg( const struct program* prg , size_t* pos ) {
  assert( prg->len > *pos );
  return prg->codes[(*pos)++];
}

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

int bc_get_argument_num( instructions instr ) {
#define X(A,B,C) case A: return B;
  switch(instr) {
    VM_INSTRUCTIONS(X)
    default:
      UNREACHABLE();
      return -1;
  }
#undef X
}

/* dump the constant table of a program */
static
void dump_program_ctable( struct ajj* a,
    const struct program* prg ,
    struct ajj_io* output ) {
  size_t i;
  ajj_io_printf(output,"Constant String Table======================\n\n");
  for( i = 0 ; i < prg->str_len ; ++i ) {
    ajj_io_printf(output,"%zu: %s\n",i,prg->str_tbl[i].str);
  }
  ajj_io_printf(output,"\nConstant Number Table======================\n\n");
  for( i = 0 ; i < prg->num_len ; ++i ) {
    ajj_io_printf(output,"%zu: %f\n",i,prg->num_tbl[i]);
  }
  ajj_io_printf(output,"\nFunction Argument Table====================\n\n");
  for( i = 0 ; i < prg->par_size ; ++i ) {
    const char* c;
    size_t l;
    int own;
    ajj_io_printf(output,"%zu:%s:",i,prg->par_list[i].name.str);
    c = ajj_display(a,
        &(prg->par_list[i].def_val),
        &l,&own);
    ajj_io_printf(output,"%s\n",c);
    if(own) free((void*)c);
  }
  ajj_io_printf(output,"\n");
}

#define DO0(N) \
  do { \
    char buf[64]; \
    tk_get_code_snippet(src,sref,buf,64); \
    ajj_io_printf(output,"%d %zu:%d(... %s ...) %s\n",cnt+1,i-1,sref,buf,N); \
  } while(0); break

#define DO1(N) \
  do { \
    char buf[64]; \
    tk_get_code_snippet(src,sref,buf,64); \
    a1 = bc_1st_arg(c1);\
    ajj_io_printf(output,"%d %zu:%d(... %s ...) %s %d\n",cnt+1,i-1,sref,buf,N,a1); \
  } while(0); break

#define DO2(N) \
 do { \
   char buf[64]; \
   tk_get_code_snippet(src,sref,buf,64); \
   a1 = bc_1st_arg(c1);\
   a2 = bc_2nd_arg(prg,&i); \
   ajj_io_printf(output,"%d %zu:%d(... %s ...) %s %d %d\n",cnt+1,i-2,sref,buf,N,a1,a2); \
 } while(0); break

#define DO(A,B,C) case A: DO##B(C);

void dump_program( struct ajj* a ,
    const char* src ,
    const struct program* prg ,
    struct ajj_io* output ) {
  size_t i = 0;
  int a1,a2;
  int cnt = 0;
  dump_program_ctable(a,prg,output);
  ajj_io_printf(output,"Code=======================================\n\n");
  while(1) {
    int sref = prg->spos[i];
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
