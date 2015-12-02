#include "opt.h"
#include "ajj-priv.h"
#include "util.h"
#include "bc.h"
#include "vm.h"
#include "object.h"
#include <math.h>
#include <errno.h>
#include <limits.h>

/* A peephole optimizer
 * This optimizer basically helps to remove those unused NOP
 * operations and also helps to do constant folding. We cannot
 * track the status of stack ( undecidable problem ). But we
 * can safly resolve/reduce instruction sequence like this:
 *
 * ....
 *
 * LIMM 2
 * LIMM 3
 * MUL
 *
 * Because LIMM 2 and LIMM 3 will always result in the top 2
 * stack elements contains 2 and 3, then we definitly CAN safely
 * perform the resolution here. Similar instruction would be all
 * the LXX instructions. ( All LXX instructions result in constant ).
 *
 * Another thing is by reducing and removing those instructions,
 * we also need to repatch those jump instructions since its
 * target has definitly changed. The only thing we need to note
 * is that, remove or rewrite a instruction ONLY affect the target
 * that is after THIS instructions position. It won't hurt any jump
 * target that is pointed to the position before its position.
 *
 * The current instruction sequence is a one directional forwarded
 * instruction sequence which means user CANNOT decode in backward
 * direction. We gonna build a bi-direcitonal instruction sequence
 * on the fly which support backward decoding as well. By this we
 * support unlimited length peephole which can help us reduce those
 * constant indefinitly long.
 *
 * Another to note, since we only support real number using double
 * precision floating point number and it doesn't support accosiative
 * on EVERY operations. Like expression 2 + a + 3 , we actually
 * cannot constant fold it, since 2+a+3 == (2+a) + 3 != (2+3) + a.
 * Altough it may not be very impactful in language like jinja2 , but
 * it would be useful to just keep these assumptions around here.
 *
 *
 * To enable bi-directional traversal , we pad a 4 bytes header in
 * front of each instruction sections when we decode the instruction
 * in forward direction.
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

#define reserve_buf(O,T,A,E) \
  do { \
    if( (O)->T##_len + (A) >= (O)->T##_cap ) { \
      (O)->T = realloc( (O)->T , sizeof(E)*2*((O)->T##_cap) ); \
      (O)->T##_cap *= 2; \
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
  size_t o_buf_cap;
  size_t o_buf_len;
  int o_buf_pwr;

  int* o_rlink;
  size_t o_rlink_cap;
  size_t o_rlink_len;

  int* o_sref; /* output source code reference */
  size_t o_sref_len;
  size_t o_sref_cap;

  struct program* prg; /* target program */
  size_t pc;
  struct ajj* a; /* ajj */
};

static
void str_destroy( struct str* l ) {
  if(l->own && l->str) free(l->str);
}

static
struct string str_concate( const struct str* l ,
    const struct str* r ) {
  struct strbuf sbuf;
  strbuf_init(&sbuf);
  strbuf_append(&sbuf,l->s,strlen(l->s));
  strbuf_append(&sbuf,r->s,strlen(r->s));
  return strbuf_tostring(&sbuf);
}

static
struct string str_mul( const struct str* l , int times ) {
  struct strbuf sbuf;
  size_t len = strlen(l->s);
  strbuf_init(&sbuf);
  while( --times ) {
    strbuf_append(&sbuf,l->s,len);
  }
  return strbuf_tostring(&sbuf);
}

/* help re-emit those instructions internally */
static
void emit0( struct opt* o , int pos , int bc ) {
  reserve_buf(o,o_rlink,0,int);
  o->o_rlink[o->o_rlink_len++] = o->o_buf_len;
  o->o_buf[o->o_buf_len++] = BC_WRAP_INSTRUCTION0(bc);
  reserve_buf(o,o_sref,0,int);
  o->o_sref[o->o_sref_len++] = pos;
}

static
void emit1( struct opt* o , int pos , int bc , int a1 ) {
  reserve_buf(o,o_rlink,0,int);
  o->o_rlink[o->o_rlink_len++] = o->o_buf_len;
  o->o_buf[o->o_buf_len++] = BC_WRAP_INSTRUCTION1(bc,a1);
  reserve_buf(o,o_sref,0,int);
  o->o_sref[o->o_sref_len++] = pos;
}

static
void report_error( struct opt* o ,const char* fmt , ... );

static
int to_number( struct opt* o , const struct ajj_value* v,
    double* val ) {
  switch(v->type) {
    case AJJ_VALUE_BOOLEAN:
      *val = ajj_value_to_boolean(v);
      return 0;
    case AJJ_VALUE_NUMBER:
      *val = ajj_value_to_number(v);
      return 0;
    case AJJ_VALUE_STRING:
      {
        double d;
        errno = 0;
        d = strtod(((struct string*)(v->value.priv))->str,NULL);
        if( errno ) {
          report_error(o,"Cannot convert str:%s to number because:%s",
              ((struct string*)(v->value.priv))->str,
              strerror(errno));
          return -1;
        }
        *val = d;
        return 0;
      }
    default:
      report_error(o,"Cannot convert type:%s to number!",
          ajj_value_get_type_name(v));
      return -1;
  }
}

static
int to_integer( struct opt* o , const struct ajj_value* v,
    int* val ) {
  switch( v->type ) {
    case AJJ_VALUE_BOOLEAN:
      *val = ajj_value_to_boolean(v);
      return 0;
    case AJJ_VALUE_NUMBER:
      {
        double ip;
        double re = modf(v->value.number,&ip);
        if( re > INT_MAX || re < INT_MIN ) {
          report_error(o,"Cannot convert number:%d to integer,overflow!",
              re);
          return -1;
        }
        *val = (int)(ip);
        return 0;
      }
    default:
      report_error(o,"Cannot convert type:%s to integer!",
          ajj_value_get_type_name(v));
      return -1;
  }
}

static
int to_string( struct opt* o , const struct ajj_value* v,
    struct str* val ) {
  switch(v->type) {
    case AJJ_VALUE_BOOLEAN:
      if(v->value.boolean) {
        val->s = TRUE_STRING.str;
        val->own = 0;
      } else {
        val->s = FALSE_STRING.str;
        val->own = 0;
      }
      return 0;
    case AJJ_VALUE_NUMBER:
      {
        char buf[256];
        sprintf(buf,"%f",ajj_value_to_number(v));
        val->s = strdup(buf);
        val->own = 1;
        return 0;
      }
    case AJJ_VALUE_STRING:
      val->s = ((struct string*)(v->value.priv))->str;
      val->own= 0;
      return 0;
    default:
      report_error(o,"Cannot convert type:%s to string!",
          ajj_value_get_type_name(v));
      return -1;
  }
}

static
int fold_bin( struct opt* o , instructions instr );

static
int fold_una( struct opt* o , instructions instr );

static
int copy_ins( struct opt* o , int c );

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
  int a1;
  int a2;
  instructions pinstr = VM_HALT;

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
      default:
copy:
        copy_ins(o,c1);
        break;
    }
    pinstr = instr;
  }

done:

fail:
}

/* Pass2 : patch the jump target */
static
int pass2( struct opt* o );

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
    case VM_LDICT:
      val->type = AJJ_VALUE_DICT;
      val->value.object = NULL;
      return 0;
    case VM_LLIST:
      val->type = AJJ_VALUE_LIST;
      val->value.object = NULL;
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
      assert((size_t)arg < o->prg->num_len );
      val->type = AJJ_VALUE_STRING;
      val->value.priv = arg + o->prg->str_tbl;
      return 0;
    default:
      return -1;
  }
}

#define old_sref(O) ((O)->o_sref[(O)->o_sref_len-2])

/* This function adjust the peephole window. It is called
 * when a new rewrite is able to perform. We have a very
 * fixed rewrite pattern, pop 2 instructions out but write
 * 1 instruction back. We reset peephole pointer back one
 * instruction and pop 2 out then write one back */

static
void rewrite_bin( struct opt* o , int pos , int type , int bc, int a1 ) {
  int srk;
  assert( o->o_rlink_len >= 2 );
  srk = o->o_buf_len - o->p_beg;

  o->o_buf_len = o->p_beg; /* set the current buffer position to
                            * the start of peephole */

  assert( o->o_sref_len >= 2 );
  o->o_sref_len -= 2;

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
        if( to_string(o,&l,&ls) || to_string(o,&l,&rs) ) {
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
    (V) = (lv) + (rv); \
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
  int srk = o->o_buf_len - peephole;

  assert( o->o_rlink_len >= 2 );
  assert( o->o_buf_len - o->p_beg <= 4 );

  /* back one instructions */
  o->o_buf_len = peephole;
  o->o_rlink_len--; 
  o->o_sref_len-- ;

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
        i_val = program_const_num(o->prg,val);
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

