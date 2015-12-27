#include <math.h>
#include "opt.h"
#include "ajj-priv.h"
#include "util.h"
#include "bc.h"
#include "vm.h"
#include "lex.h"
#include "object.h"
#include <errno.h>
#include <limits.h>
#include <stdarg.h>

/* A peephole optimizer
 * ----------------------------------------------------------------
 * 1. NOPs Removal
 * 2. Constant Folding
 * 3. Branch Elimination
 *
 * 1. NOPs removal.
 * Nothing need to say, just remove those nops instructions which is
 * used as a placeholder during the one pass parsing. Removal nops will
 * result the JUMP target changes. We patch JUMP target in the second
 * pass, so we are safe
 *
 * 2. Cosntant Folding
 * Fold those constant that appears at the top of the stack. We don't
 * need CFG based algorithm but just need to open a peephole to inspect
 * the most recent 2-3 instructions then we can do folding, once we finish
 * a folding, we back up the peephole window for one instructions, then
 * we could propogate the constant and do recursive folding.
 *
 * 3. Branch Elimination.
 * Typically the result of the folding constant will result more new opt
 * candidate, The branch elimination will be the simple one to pick up.
 * For conditional branch, typically generated code is as follow :
 * LTRUE/LFALSE
 * JT/JF
 *
 * A constant follows a conditional jump can possibly be eliminated into
 * a direct jump thus removing that previous constant loading.
 *
 * NOTES:
 * Our instruction sequence is a single directional forward code stream,
 * but we need to back the peephole pointer, to achieve the unlimited
 * backwards. We , on the fly , generate a extra reference pointer array
 * to tell the peephole where to backup. This generation is on the fly,
 * so it doesn't occupy any more passes.
 *
 * Last:
 * After all the optimization pass finished, the second/last pass will kick
 * in to finish those patch for all the jump instructions.
 *
 * RISK:
 * We are not allowed to remove some very *TRIVIAL* but dangerouse codes
 * even they are *like* some good fish to catch. Example:
 * POP 1
 * POP 2
 *
 * The POP instruction is typically emitted cross the the sentence boundary,
 * although our byte code doesn't make this explicit.So without CFG analysing
 * we cannot remove those code. Because it is very likely that some other
 * code will JUMP into the second POP then skip the first POP so we cannot
 * fold the above 2 instructions into one single POP safely. Actually in our
 * loop body the code generation has such pattern.
 */

struct offset_buffer {
  size_t pos;
  int shrink;
};

/* A internal string wrapper which contains information
 * whether this string is OWNED by this object or not. */
struct str {
  const char* s;
  int own;
};

#define NULL_STR { NULL , 0 }

#define INIT_BUF_SIZE 128

#define reserve_buf(O,T,A,E) \
  do { \
    if( (O)->T##_len + (A) >= (O)->T##_cap ) { \
      (O)->T = mem_grow( (O)->T, sizeof(E) , A , &((O)->T##_cap) ); \
    } \
  } while(0)

#define push_buf(O,T,V) \
    ((O)->T##_buf[ (O)->T##_len++ ] = (V)); \

struct opt {
  /* peephole */
  size_t p_beg; /* the start of the peephole inside of the
                 * ibuffer . Our peephole is ALWAYS 2 instructions
                 * length */

  /* offset buffer */
  struct offset_buffer* off_buf;
  size_t off_buf_cap;
  size_t off_buf_len;

  /* current pending shrink if we meet a new collapse
   * inside of our instruction stream */
  int cur_shrink;

  /* Output buffer. We pad the 4 bytes reverse link into another
   * array. By this way we don't need another pass to generate
   * the actual output instruction stream */
  int* o_buf;
  int* o_sref; /* output source code reference */
  size_t o_buf_cap;
  size_t o_buf_len;
  int o_buf_pwr;

  int* o_rlink;
  size_t o_rlink_cap;
  size_t o_rlink_len;
  int p_rlink; /* previous rlink */


  int* o_jmp; /* jump instruction position in the output code
               * buffer, later on used for patch those jump */
  size_t o_jmp_len;
  size_t o_jmp_cap;

  struct program* prg; /* target program */
  size_t pc;     /* program counter */
  size_t ppc;    /* previous pc */

  struct ajj_object* jinja; /* jinja */
  struct ajj* a; /* for error buffer */
};

#define reserve_obuf(O) \
  do { \
    reserve_buf(O,o_buf,2,sizeof(int)); \
    (O)->o_sref = realloc((O)->o_sref,sizeof(int)*(O)->o_buf_cap); \
  } while(0)

static
void opt_init( struct opt* o , struct ajj* a , struct ajj_object* jj ) {
  memset(o,0,sizeof(*o));
  o->a = a;
  o->jinja = jj;
}

static
void opt_reset( struct opt* o , struct program* prg ) {
  o->p_beg = 0;
  o->off_buf_len = 0;
  o->cur_shrink =0;
  o->o_buf_len = 0;
  o->o_rlink_len = 0;
  o->p_rlink = 0;
  o->o_jmp_len = 0;
  o->pc = 0;
  o->ppc = 0;
  o->prg = prg;
}

static
void opt_destroy( struct opt* o ) {
  free(o->off_buf);
  free(o->o_rlink);
  free(o->o_jmp);
  free(o->o_sref);
}

static
void report_error( struct opt* o ,const char* fmt , ... ) {
  struct object* obj = &(o->jinja->val.obj);
  const char* src = obj->src;
  va_list vl;
  size_t pos,ln;
  char cs[64];
  int len;

  /* get code snippet and other location information */
  tk_get_coordinate(src,o->prg->spos[o->ppc],&ln,&pos);
  tk_get_code_snippet(src,o->prg->spos[o->ppc],cs,64);

  /* dump the error */
  len = snprintf(o->a->err,1024,
      "[Optimizer:(%s:" SIZEF "," SIZEF ")] at:... %s ...!\nMessage:",
      obj->fn_tb->name.str,
      SIZEP(ln),SIZEP(pos),
      cs);
  assert( len >0 && len < ERROR_BUFFER_SIZE );
  /* output the rest messge even it is truncated */
  va_start(vl,fmt);
  vsnprintf(o->a->err+len,ERROR_BUFFER_SIZE-len, fmt,vl);
}

static
void str_destroy( struct str* l ) {
  if(l->own && l->s) free((void*)l->s);
}

static
struct string str_concate( const struct str* l ,
    const struct str* r ) {
  struct strbuf sbuf;
  const size_t llen = strlen(l->s);
  const size_t rlen = strlen(r->s);
  strbuf_init_cap(&sbuf,llen+rlen+1);
  strbuf_append(&sbuf,l->s,llen);
  strbuf_append(&sbuf,r->s,rlen);
  return strbuf_tostring(&sbuf);
}

static
struct string str_mul( const struct str* l , int times ) {
  struct strbuf sbuf;
  size_t len = strlen(l->s);
  strbuf_init_cap(&sbuf,len);
  while( times-- ) {
    strbuf_append(&sbuf,l->s,len);
  }
  return strbuf_tostring(&sbuf);
}

/* The following function reemit the instructions accordingly
 * and also it build the reverse link on the fly */
static
void emit0( struct opt* o , int pos , int bc ) {
  reserve_buf(o,o_rlink,0,int);
  o->o_rlink[o->o_rlink_len++] = o->p_rlink;

  o->p_rlink = o->o_buf_len;

  reserve_obuf(o);
  o->o_buf[o->o_buf_len] = BC_WRAP_INSTRUCTION0(bc);
  o->o_sref[o->o_buf_len++] = pos;
}

static
void add_jmp( struct opt* o , int bc ) {
  switch(bc) {
    case VM_JMP:
    case VM_JT:
    case VM_JF:
    case VM_JLT:
    case VM_JLF:
    case VM_JMPC:
    case VM_JEPT:
      reserve_buf(o,o_jmp,0,int);
      /* once the jmp position is recorded, it won't change
       * since the constant folding will never go across the
       * boundary of the jmp instructions. You could not fold
       * jump so we are safe */
      o->o_jmp[o->o_jmp_len++] = o->o_buf_len;
      return;
    default:
      return;
  }
}

static
void emit1( struct opt* o , int pos , int bc , int a1 ) {
  reserve_buf(o,o_rlink,0,int);
  o->o_rlink[o->o_rlink_len++] = o->p_rlink;

  o->p_rlink = o->o_buf_len;

  add_jmp(o,bc);

  reserve_obuf(o);
  o->o_buf[o->o_buf_len] = BC_WRAP_INSTRUCTION1(bc,a1);
  o->o_sref[o->o_buf_len++] = pos;
}

static
void emit2( struct opt* o , int pos , int bc , int a1, int a2 ) {
  reserve_buf(o,o_rlink,0,int);
  o->o_rlink[o->o_rlink_len++] = o->p_rlink;

  o->p_rlink = o->o_buf_len;

  add_jmp(o,bc);

  reserve_obuf(o);
  o->o_buf[o->o_buf_len] = BC_WRAP_INSTRUCTION1(bc,a1);
  o->o_sref[o->o_buf_len++] = pos;
  o->o_buf[o->o_buf_len++] = a2;
}

/* ==========================================================
 * Type conversion
 * We jave to use customized version instead of reusing
 * vm_to_XXX function. The reason is because we carry the string
 * constant into the __private__ pointer inside of the struct
 * ajj_value which is opaque to all the vm_to_XXX function
 * ==========================================================*/
static
int to_number( struct opt* o , struct ajj_value* v,
    double* val ) {
  struct ajj_object obj;

  if(v->type == AJJ_VALUE_STRING) {
    v->value.object = &obj;
    v->value.object->tp = AJJ_VALUE_STRING;
    obj.val.str = *((struct string*)v->value.__private__);
  }

  if(vm_to_number(v,val)) {
    if(v->type == AJJ_VALUE_STRING) {
      report_error(o,"Cannot convert string:%s to number!",
          obj.val.str.str);
    } else {
      report_error(o,"Cannot convert type:%s to number!",
          ajj_value_get_type_name(v));
    }
    return -1;
  }
  return 0;
}

static
int to_integer( struct opt* o , struct ajj_value* v,
    int* val ) {
  struct ajj_object obj;
  if(v->type == AJJ_VALUE_STRING) {
    v->value.object = &obj;
    v->value.object->tp = AJJ_VALUE_STRING;
    obj.val.str = *((struct string*)v->value.__private__);
  }
  if(vm_to_integer(v,val)) {
    if(v->type == AJJ_VALUE_STRING) {
      report_error(o,"Cannot convert string:%s to integer!",
          obj.val.str.str);
    } else {
      report_error(o,"Cannot convert type:%s to integer!",
          ajj_value_get_type_name(v));
    }
    return -1;
  }
  return 0;
}

static
int to_string( struct opt* o , struct ajj_value* v,
    struct str* val ) {
  struct ajj_object obj;
  int own;
  struct string s;
  if(v->type == AJJ_VALUE_STRING) {
    v->value.object = &obj;
    v->value.object->tp = AJJ_VALUE_STRING;
    obj.val.str = *((struct string*)v->value.__private__);
  }
  if(vm_to_string(v,&s,&own)) {
    assert(v->type != AJJ_VALUE_STRING);
    report_error(o,"Cannot convert type:%s to string!",
        ajj_value_get_type_name(v));
    return -1;
  }
  val->own = own;
  val->s = s.str;
  return 0;
}

static
int fold_bin( struct opt* o , instructions instr );

static
int fold_una( struct opt* o , instructions instr );

static
int branch_elm( struct opt*  o , instructions instr );

#define DO_0() \
  do { \
    emit0(o,o->prg->spos[o->ppc],instr); \
    if( o->o_rlink_len >= 2 ) { \
      o->p_beg = o->o_rlink[o->o_rlink_len-1]; \
    } \
  } while(0); break

#define DO_1() \
  do { \
    int a1 = BC_1ARG(c1); \
    emit1(o,o->prg->spos[o->ppc],instr,a1); \
    if( o->o_rlink_len >= 2 ) { \
      o->p_beg = o->o_rlink[o->o_rlink_len-1]; \
    } \
  } while(0); break

#define DO_2() \
  do { \
    int a1 = BC_1ARG(c1); \
    int a2 = bc_2nd_arg( o->prg,&(o->pc) ); \
    emit2(o,o->prg->spos[o->ppc],instr,a1,a2); \
    if( o->o_rlink_len >= 2 ) { \
      o->p_beg = o->o_rlink[o->o_rlink_len-1]; \
    } \
  } while(0); break

#define DO(A,B,C) case A: DO_##B();

/* This function directly copy the instruction from the old buffer
 * into the new buffer and move the peephole pointer if we need to */
static
int copy_ins( struct opt* o , int c1 ) {
  instructions instr = BC_INSTRUCTION(c1);
  switch(instr) {
    VM_INSTRUCTIONS(DO)
    default:
      UNREACHABLE();
      return -1;
  }
  return 0;
}
#undef DO
#undef DO_0
#undef DO_1
#undef DO_2

#define CALL_FOLD(X) \
  switch((X)) { \
    case -1: goto fail; \
    case 0: break; \
    case 1: goto copy; \
    default: UNREACHABLE(); \
  } \

/* Pass1 : do constant folding */
static
int pass1( struct opt* o ) {
  while(1) {
    int c1 = bc_next(o->prg,&o->pc);
    instructions instr = bc_instr(c1);
    switch(instr) {
      case VM_HALT:
        goto done;
      case VM_NOP0:
      case VM_NOP1:
        o->cur_shrink+=1; /* one more shrink */
        reserve_buf(o,off_buf,0,struct offset_buffer);
        o->off_buf[o->off_buf_len].pos = o->pc-1;
        o->off_buf[o->off_buf_len].shrink = o->cur_shrink;
        ++o->off_buf_len;
        break;
      case VM_NOP2:
        bc_2nd_arg(o->prg,&(o->pc));
        o->cur_shrink+=2; /* one more shrink */
        reserve_buf(o,off_buf,0,struct offset_buffer);
        o->off_buf[o->off_buf_len].pos = o->pc-2;
        o->off_buf[o->off_buf_len].shrink = o->cur_shrink;
        ++o->off_buf_len;
        break;
      case VM_ADD:
      case VM_MUL:
      case VM_SUB:
      case VM_DIV:
      case VM_MOD:
      case VM_POW:
      case VM_EQ:
      case VM_NE:
      case VM_LT:
      case VM_LE:
      case VM_GE:
      case VM_GT:
      case VM_DIVTRUCT:
        CALL_FOLD(fold_bin(o,instr));
        break;
      case VM_NOT:
      case VM_NEG:
        CALL_FOLD(fold_una(o,instr));
        break;
      /* branch elimination. */
      case VM_JEPT:
      case VM_JT:
      case VM_JF:
      case VM_JLT:
      case VM_JLF:
        CALL_FOLD(branch_elm(o,instr));
      default:
copy:
        copy_ins(o,c1);
        break;
    }
    o->ppc = o->pc;
  }

done:
  return 0;

fail:
  return -1;
}

/* Find the new value for old jump target. During the constant
 * folding and NOP operation remove we build a offset buffer,
 * which contains all the needed information to decide mapping */
static
int find_new_jtar( struct opt* o , int old_jtar ) {
  size_t i;

  for( i = 0 ; i < o->off_buf_len ; ++i ) {
    if( o->off_buf[i].pos >= old_jtar ) {
      if( i > 0 ) {
        assert( o->off_buf[i-1].pos < old_jtar );
        return o->off_buf[i-1].shrink;
      } else {
        return 0;
      }
    }
  }
  return o->off_buf[o->off_buf_len-1].shrink;
}

/* Pass2 : patch the jump target */
static
void pass2( struct opt* o ) {
  size_t i;
  int a1;
  int a2;
  int shrink;
  for( i = 0 ; i < o->o_jmp_len ; ++i ) {
    int c = o->o_buf[o->o_jmp[i]];
    instructions ins = BC_INSTRUCTION(c);
    switch( ins ) {
      case VM_JMP:
      case VM_JT:
      case VM_JF:
      case VM_JLF:
      case VM_JLT:
      case VM_JEPT:
        a1 = BC_1ARG(c);
        shrink = find_new_jtar(o,a1);
        a1 -= shrink;
        /* patch the instruction in place */
        o->o_buf[o->o_jmp[i]] = BC_WRAP_INSTRUCTION1(ins,a1);
        break;
      case VM_JMPC:
        a2 = o->o_buf[o->o_jmp[i]+1];
        shrink = find_new_jtar(o,a2);
        a2 -= shrink;
        o->o_buf[o->o_jmp[i]+1] = a2;
        break;
      default:
        UNREACHABLE();
        return;
    }
  }
}

static
int check_const( struct opt* o , int pos , struct ajj_value* val ) {
  int c = o->o_buf[pos];
  instructions ins = BC_INSTRUCTION(c);
  int arg;
  switch(ins) {
    case VM_LTRUE:
      *val = AJJ_TRUE;
      return 0;
    case VM_LFALSE:
      *val = AJJ_FALSE;
      return 0;
    case VM_LZERO:
      *val = ajj_value_number(0);
      return 0;
    case VM_LNONE:
      *val = AJJ_NONE;
      return 0;
    case VM_LNUM:
      arg = BC_1ARG(c);
      assert((size_t)arg < o->prg->num_len);
      *val = ajj_value_number(o->prg->num_tbl[arg]);
      return 0;
    case VM_LIMM:
      arg = BC_1ARG(c);
      *val = ajj_value_number(arg);
      return 0;
    case VM_LSTR:
      arg = BC_1ARG(c);
      assert((size_t)arg < o->prg->str_len );
      val->type = AJJ_VALUE_STRING;
      val->value.__private__ = arg + o->prg->str_tbl;
      return 0;
    default:
      return -1;
  }
}

/* old instruction's source code reference , used when reemit new
 * instructions at the corresponding position */
#define old_sref(O) ((O)->prg->spos[(O)->ppc])

/* This function adjust the peephole window. It is called
 * when a new rewrite is able to perform. We have a very
 * fixed rewrite pattern, pop 2 instructions out but write
 * 1 instruction back. We reset peephole pointer back one
 * instruction and pop 2 out then write one back */

static
void rewrite_bin( struct opt* o , int pos , int type , int bc, int a1 ) {
  int srk;
  assert( o->o_rlink_len >= 2 );
  srk = o->o_buf_len - o->p_beg + 1; /* the "1" represent the byte code length
                                      * we eat the byte code as well */

  o->o_buf_len = o->p_beg; /* set the current buffer position to
                            * the start of peephole */

  o->p_rlink = o->o_rlink[o->o_rlink_len-2];
  if( o->o_rlink_len > 2 ) {
    /* only back peephole pointer one instructions */
    o->p_beg = o->o_rlink[ o->o_rlink_len-2 ];
  }
  o->o_rlink_len -= 2;

  /* insert the instruction */
  if(type == 0) {
    emit0(o,pos,bc);
    srk--;
  } else {
    emit1(o,pos,bc,a1);
    srk--;
  }

  /* add a new shrink */
  if(srk) {
    o->cur_shrink += srk;
    reserve_buf( o,off_buf,0,struct offset_buffer );
    o->off_buf[o->off_buf_len].pos = o->pc-1;
    o->off_buf[o->off_buf_len].shrink = o->cur_shrink;
    ++(o->off_buf_len);
  }
}

#define bin_emit0(o,pos,bc) rewrite_bin(o,pos,0,bc,0)
#define bin_emit1(o,pos,bc,a1) rewrite_bin(o,pos,1,bc,a1)

static
int fold_bin( struct opt* o , instructions instr ) {
  int p = o->p_beg;
  struct ajj_value l,r;

  assert(o->o_rlink_len >= 2);
  assert( o->o_buf_len - o->p_beg <= 4 );

  if( check_const(o,p++,&l) || check_const(o,p,&r) ) {
    /* cannot fold it , one of the 2 operands is not const */
    return 1;
  }

  switch(instr) {
    /* ALRITHMATIC ----------------------------- */
    case VM_ADD:
      if( l.type != AJJ_VALUE_STRING &&
          r.type != AJJ_VALUE_STRING ) {
        /* convert to number and then do the processing */
        double lv, rv;
        if( to_number(o,&l,&lv) || to_number(o,&r,&rv) )
          return -1; /* failed, and also impossible to execute this
                      * code during the runtime as well */
        else {
          /* able to do the calculation here */
          double val = lv + rv;
          int sref = old_sref(o);
          int i_val = program_const_num(o->prg,val);
          bin_emit1(o,sref,VM_LNUM,i_val);
        }
      } else {
        struct str ls = NULL_STR;
        struct str rs = NULL_STR;
        struct string val;
        int i_val;
        int sref = old_sref(o);
        if( to_string(o,&l,&ls) || to_string(o,&r,&rs) ) {
          str_destroy(&ls);
          str_destroy(&rs);
          return -1;
        }
        val = str_concate(&ls,&rs);
        i_val = program_const_str(o->prg,&val,1);
        bin_emit1(o,sref,VM_LSTR,i_val);
      }
      return 0;
    case VM_MUL:
      if( r.type == AJJ_VALUE_STRING || l.type == AJJ_VALUE_STRING ) {
        if( r.type == AJJ_VALUE_STRING && l.type == AJJ_VALUE_STRING ) {
          report_error(o,"Cannot multiply 2 string!");
          return -1;
        } else {
          int rv;
          struct str lv = NULL_STR;
          struct string val;
          int i_val;
          int sref = old_sref(o);
          struct ajj_value* str_val;
          struct ajj_value* num_val;

          if( r.type == AJJ_VALUE_STRING ) {
            str_val = &r;
            num_val = &l;
          } else {
            str_val = &l;
            num_val = &r;
          }

          if( to_integer(o,num_val,&rv ) )
            return -1;

          if( to_string(o,str_val,&lv) )
            return -1;

          val = str_mul(&lv,rv);
          str_destroy(&lv);

          i_val = program_const_str(o->prg,&val,1);
          bin_emit1(o,sref,VM_LSTR,i_val);
        }
      } else {
        double lv,rv;
        if( to_number(o,&l,&lv) || to_number(o,&r,&rv) ) {
          return -1;
        } else {
          double val = lv * rv;
          int i_val = program_const_num(o->prg,val);
          int sref = old_sref(o);
          bin_emit1(o,sref,VM_LNUM,i_val);
        }
      }
      return 0;

#define DO(C) \
      do { \
        double lv , rv; \
        if( to_number(o,&l,&lv) || to_number(o,&r,&rv) ) { \
          return -1; \
        } else { \
          double val; \
          int sref = old_sref(o); \
          int i_val; \
          C(val); \
          i_val = program_const_num(o->prg,val); \
          bin_emit1(o,sref,VM_LNUM,i_val); \
        } \
        return 0; \
      } while(0)

    /* SUB */
    case VM_SUB:
#define SUB(V) (V) = (lv) + (rv)
      DO(SUB);
#undef SUB

    /* DIV */
    case VM_DIV:
#define DIV(V) do { \
  if( (rv) == 0 ) { \
    report_error(o,"Divide zero!"); \
    return -1; \
  } else { \
    (V) = (lv) / (rv); \
  } \
} while(0)
      DO(DIV);
#undef DIV

    /* MOD */
    case VM_MOD:
#define MOD(V) (V) = (int)(lv) % (int)(rv)
      DO(MOD);
#undef MOD

    /* POW */
    case VM_POW:
#define POW(V) (V) = pow((lv),(rv))
      DO(POW);
#undef POW

    /* DIVTRUCT */
    case VM_DIVTRUCT:
#define DIVTRUCT(V) \
      do { \
        if((rv) == 0 ) { \
          report_error(o,"Divide zero!"); \
          return -1; \
        } else { \
          (V) = (int64_t)((lv) / (rv)); \
        } \
      } while(0)
      DO(DIVTRUCT);
#undef DIVTRUCT
#undef DO

    /* COMPARISON ------------------ */
#define DO(O,T) \
      do { \
        if( l.type == AJJ_VALUE_NONE && \
            r.type == AJJ_VALUE_NONE ) { \
          int sref = old_sref(o); \
          bin_emit0(o,sref,VM_L##T); \
        } else { \
          if( l.type == AJJ_VALUE_STRING || \
              r.type == AJJ_VALUE_STRING ) { \
            struct str lv = NULL_STR; \
            struct str rv = NULL_STR; \
            int sref = old_sref(o); \
            int res; \
            if( to_string(o,&l,&lv) || to_string(o,&r,&rv) ) { \
              str_destroy(&lv); \
              str_destroy(&rv); \
              return -1; \
            } \
            res = strcmp(lv.s,rv.s) O 0; \
            if(res) bin_emit0(o,sref,VM_LTRUE);\
            else bin_emit0(o,sref,VM_LFALSE);\
            str_destroy(&lv); \
            str_destroy(&rv); \
          } else { \
            double lv,rv; \
            int val; \
            int sref = old_sref(o); \
            if( to_number(o,&l,&lv) || to_number(o,&r,&rv) ) { \
              return -1; \
            } \
            val = lv O rv; \
            if(val) bin_emit0(o,sref,VM_LTRUE); \
            else bin_emit0(o,sref,VM_LFALSE); \
          } \
        } \
        return 0; \
      } while(0)

    case VM_NE:
      DO(!=,FALSE);

    case VM_EQ:
      DO(==,TRUE);

#undef DO

#define DO(O) \
      do { \
        if( l.type == AJJ_VALUE_STRING || \
            r.type == AJJ_VALUE_STRING ) { \
          int sref = old_sref(o); \
          struct str lv = NULL_STR; \
          struct str rv = NULL_STR; \
          int res; \
          if( to_string(o,&l,&lv) || to_string(o,&r,&rv) ) { \
            str_destroy(&lv); \
            str_destroy(&rv); \
            return -1; \
          } \
          res = strcmp(lv.s,rv.s) O 0; \
          if(res) bin_emit0(o,sref,VM_LTRUE); \
          else bin_emit0(o,sref,VM_LFALSE); \
          str_destroy(&lv); \
          str_destroy(&rv); \
        } else { \
          double lv,rv; \
          int val; \
          int sref = old_sref(o); \
          if( to_number(o,&l,&lv) || to_number(o,&r,&rv) ) { \
            return -1; \
          } \
          val = lv O rv; \
          if(val) bin_emit0(o,sref,VM_LTRUE); \
          else bin_emit0(o,sref,VM_LFALSE); \
        } \
        return 0; \
      } while(0)

    case VM_LE:
      DO(<=);

    case VM_LT:
      DO(<);

    case VM_GE:
      DO(>=);

    case VM_GT:
      DO(>);
#undef DO
    default:
      UNREACHABLE();
      return -1;
  }
  UNREACHABLE();
  return -1;
}

static
void rewrite_una( struct opt* o , int pp_off ,
    int pos , int type , int bc , int a1) {
  int peephole = o->p_beg + pp_off;
  int srk = o->o_buf_len - peephole + 1; /* 1 represent the byte code for opreation */

  assert( o->o_rlink_len >= 2 );
  assert( o->o_buf_len - o->p_beg <= 4 );

  /* back one instructions */
  o->p_rlink = o->o_rlink[o->o_rlink_len-1];
  o->o_buf_len = peephole;
  o->o_rlink_len--;

  if(type == 0) {
    emit0(o,pos,bc);
    srk--;
  } else {
    emit1(o,pos,bc,a1);
    srk--;
  }

  if(srk) {
    o->cur_shrink += srk;
    reserve_buf( o,off_buf,0,struct offset_buffer );
    o->off_buf[o->off_buf_len].pos = o->pc-1;
    o->off_buf[o->off_buf_len].shrink = o->cur_shrink;
    ++(o->off_buf_len);
  }
}

#define una_emit0(o,pp_off,pos,bc) rewrite_una(o,pp_off,pos,0,bc,0)
#define una_emit1(o,pp_off,pos,bc,a1) rewrite_una(o,pp_off,pos,1,bc,a1)

/* fold the unary operations */
static
int fold_una( struct opt* o , instructions inst ) {
  int c1 = o->o_buf[o->p_beg];
  instructions ins = BC_INSTRUCTION(c1);
  int an = bc_get_argument_num(ins);
  int pos;
  struct ajj_value v;

  an = an <= 1 ? 1 : 2;
  pos = o->p_beg + an;

  if( check_const(o,pos,&v) )
    return 1;

  switch(inst) {
    case VM_NEG:
      {
        double val;
        int sref = old_sref(o);
        int i_val;
        if( to_number(o,&v,&val) )
          return -1;
        i_val = program_const_num(o->prg,-val);
        una_emit1(o,an,sref,VM_LNUM,i_val);
        return 0;
      }
    case VM_NOT:
      {
        int sref = old_sref(o);
        switch(v.type) {
          case AJJ_VALUE_NUMBER:
            if(v.value.number == 0)
              una_emit0(o,an,sref,VM_LTRUE);
            else
              una_emit0(o,an,sref,VM_LFALSE);
            break;
          case AJJ_VALUE_BOOLEAN:
            if(v.value.boolean)
              una_emit0(o,an,sref,VM_LFALSE);
            else
              una_emit0(o,an,sref,VM_LTRUE);
            break;
          case AJJ_VALUE_NONE:
            una_emit0(o,an,sref,VM_LTRUE);
            break;
          default:
            una_emit0(o,an,sref,VM_LFALSE);
            break;
        }
        return 0;
      }
    default:
      UNREACHABLE();
      return -1;
  }
}

/* Although we lose many valuable information after parsing, but we could still
 * eliminate the branch in certain format :
 *
 * LTRUE
 * JT
 *
 * This format can definitly turns into a direct jump instead of a conditional
 * jump and furthur remove the LTRUE. This optimization window can be turned on
 * after constant folding. But our instructions follow a sequencial model, so
 * we could naturally handle the jump elimination directly inside of the first
 * pass */

static
int branch_elm ( struct opt* o , instructions instr ) {
  /* TODO:: NEED IMPLEMENTATION ! */
  return 1;
}

/* optimize a single struct program */
static
int opt_program( struct opt* o , struct program* prg ) {
  opt_reset(o,prg);
  if( pass1(o) )
    return -1;
  else
    pass2(o);

  /* move the o_buffer to the target program */
  free(prg->codes);
  free(prg->spos);

  prg->codes = o->o_buf;
  prg->spos = o->o_sref;
  prg->len = o->o_buf_len;

  o->o_buf = NULL;
  o->o_sref = NULL;
  o->o_buf_len = 0;
  o->o_buf_cap = 0;

  return 0;
}

int optimize( struct ajj* a , struct ajj_object* jinja ) {
  struct opt o; /* optimizer context */
  size_t i;
  struct object* obj = &(jinja->val.obj);

  assert(jinja->tp == AJJ_VALUE_JINJA );

  opt_init(&o,a,jinja);

  for( i = 0 ; i < obj->fn_tb->func_len ; ++i ) {
    struct function* f = obj->fn_tb->func_tb + i;
    assert(IS_JINJA(f));
    if(opt_program(&o,&(f->f.jj_fn)))
      goto fail;
  }

  opt_destroy(&o);
  return 0;

fail:
  opt_destroy(&o);
  return -1;
}
