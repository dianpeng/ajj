#ifndef _VM_H_
#define _VM_H_
#include "util.h"
#include "ajj.h"

struct ajj;
struct ajj_object;
struct func_table;

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
  X(VM_AND,"and") \
  X(VM_OR,"or") \
  X(VM_NOT,"not") \
  X(VM_NEG,"neg") \
  X(VM_DIVTRUCT,"divtruct") \
  X(VM_TENARY,"tenary") \
  X(VM_CALL,"call") \
  X(VM_RET,"ret") \
  X(VM_PRINT,"print") \
  X(VM_POP,"pop") \
  X(VM_TPUSH,"tpush") \
  X(VM_TSWAP,"tswap") \
  X(VM_TLOAD,"tload") \
  X(VM_BPUSH,"bpush") \
  X(VM_BSWAP,"bswap") \
  X(VM_BLOAD,"bload") \
  X(VM_BT_MOVE,"bt_move") \
  X(VM_BB_MOVE,"bb_move") \
  X(VM_TB_MOVE,"tb_move") \
  X(VM_TT_MOVE,"tt_move") \
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
  X(VM_LOOP,"loop") \
  X(VM_LOOPREC,"looprec") \
  X(VM_JMP,"jmp") \
  X(VM_JT,"jt") \
  X(VM_JEPT,"jept") \
  X(VM_ENTER,"enter") \
  X(VM_EXIT,"exit") \
  X(VM_PIPE,"pipe") \
  X(VM_INCLUDE,"include") \
  X(VM_IMPORT,"import") \
  X(VM_IMPORT_SYMBOL,"importsymbol") \
  X(VM_EXTENDS,"extends")

enum vm_instructions {
#define X(A,B) A,
  VM_INSTRUCTIONS(X),
  SIZE_OF_INSTRUCTIONS
#undef X
};

const char* vm_get_instr_name( int );

extern struct string THIS = { "__this__",8};
extern struct string ARGNUM = {"__argnum__",10};
extern struct string MAIN = { "__main__",8 };
extern struct string CALLER = {"__caller__",10};
extern struct string SUPER  = {"super",5};
extern struct string SELF   = {"self",4};

struct program {
  void* codes;
  size_t len;
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
  } par_list[ AJJ_FUNC_PAR_MAX_SIZE ];
  size_t par_size;
};

/* Execution context */
struct func_frame {
  const struct function* entry; /* Function entry */
  int ebp; /* EBP register */
  int esp; /* ESP register */
  int pc ; /* Program counter register */
  const char* name; /* Pointer to function name */
};

struct calling_stack {
  struct func_frame stk[ AJJ_MAX_CALL_STACK ];
  size_t cur_stk; /* Current stk position */
};


static inline
void program_init( struct program* prg ) {
  prg->len = 0;
  prg->str_len = 0;
  prg->num_len = 0;
  prg->par_size =0;
}

static inline
int program_add_par( struct program* prg ,
    struct string name ,
    int own,
    const struct ajj_value* val ) {
  if( prg->par_size == AJJ_FUNC_PAR_MAX_SIZE )
    return -1;
  else {
    assert(name.len < AJJ_SYMBOL_NAME_MAX_SIZE );
    prg->par_list[prg->par_size].def_val = *val; /* owned the value */
    prg->par_list[prg->par_size].name = own ? name : string_dup(&name);
    ++prg->par_size;
    return 0;
  }
}

/* emitter for byte codes */
struct emitter {
  struct program* prg;
  size_t cd_cap;
};


static inline
void reserve_code_page( struct emitter* em , size_t cap ) {
  void* nc;
  assert( em->cd_cap < cap );
  if( em->prg->len == 0 ) {
    /* for the first time, we allocate a large code page */
    cap = MINIMUM_CODE_PAGE_SIZE;
  }
  nc = malloc(cap);
  if( em->prg->codes ) {
    memcpy(nc,em->prg->codes,em->prg->len);
    free(em->prg->codes);
  }
  em->cd_cap = cap;
}

static inline
void emit0( struct emitter* em , int bc ) {
  if( em->cd_cap <= em->prg->len + 9 ) {
    reserve_code_page(em,2*em->cd_cap);
  }
  *((unsigned char*)(em->prg->codes) + em->prg->len)
    = (unsigned char)(bc);
  ++(em->prg->len);
}

static inline
void emit_int( struct emitter* em , int arg ) {
  int l;
  assert( em->cd_cap > em->prg->len + 4 ); /* ensure we have enough space to run */
  l = (em->prg->len) & 4;
  switch(l) {
    case 0:
      *((int*)(em->prg->codes) + em->prg->len) = arg;
      break;
    case 1:
      {
        int lower = (arg & 0xff000000) >> 24;
        int higher= (arg & 0x00ffffff);
        *((unsigned char*)(em->prg->codes) + em->prg->len)
          = (unsigned char)lower;
        *((int*)((unsigned char*)(em->prg->codes) + em->prg->len+1))
          = higher;
        break;
      }
    case 2:
      {
        int lower = (arg & 0xffff0000) >> 16;
        int higher= (arg & 0x0000ffff);
        *((int*)((unsigned char*)(em->prg->codes) + em->prg->len))
          = lower;
        *((int*)((unsigned char*)(em->prg->codes) + em->prg->len+2))
          = higher;
        break;
      }
    case 3:
      {
        int lower = (arg & 0xffffff00) >> 8;
        int higher= (arg & 0x000000ff);
        *((int*)((unsigned char*)(em->prg->codes) + em->prg->len))
          = lower;
        *((int*)((unsigned char*)(em->prg->codes) + em->prg->len+3))
          = higher;
        break;
      }
    default:
      UNREACHABLE();
      break;
  }
  em->prg->len += 4;
}

static inline
void emit1( struct emitter* em , int bc , int a1 ) {
  emit0(em,bc);
  emit_int(em,a1);
}

static inline
void emit2( struct emitter* em , int bc , int a1 , int a2 ) {
  emit1(em,bc,a1);
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
void emit0_at( struct emitter* em , int pos , int bc ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emit0(em,bc);
  em->prg->len = save;
}

static inline
void emit1_at( struct emitter* em , int pos , int bc , int a1 ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emit1(em,bc,a1);
  em->prg->len = save;
}

static inline
void emit2_at( struct emitter* em , int pos , int bc , int a1 , int a2 ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emit2(em,bc,a1,a2);
  em->prg->len = save;
}

static inline
int emitter_label( struct emitter* em ) {
  return (int)(em->prg->len);
}


int vm_run_func( struct ajj* ,
    struct ajj_object* tp,
    const struct string* name,
    struct ajj_value par[AJJ_FUNC_PAR_MAX_SIZE],
    size_t len,
    ajj_value* output );

int vm_run_jj( struct ajj* a,
    struct ajj_object* tp ,
    struct ajj_value par[AJJ_FUNC_PAR_MAX_SIZE],
    size_t len,
    ajj_value* output );

/* Dump an program to output */
void vm_dump( const struct program* prg, FILE* output );

#endif /* _VM_H_ */
