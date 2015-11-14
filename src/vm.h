#ifndef _VM_H_
#define _VM_H_
#include "ajj.h"
#include "util.h"

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
 *12. and(0) : check both true and pop top 2 values, push result back to stack
 *13. or (0) : check one of truen and pop top 2 values, push result back to stack
 *14. neg(0) : negate and pop top 1 values, push result back to stack.
 *15. divtruct(0): div truncate and pop top 2 values,push result back to stack.
 *16. tenary(0): do tenary op and pop top 3 values, push result back to stack.
 *17. call(2): @1: function name index ; @2: function parameter count ;
               call a function. Pop all the function calling frame out of stack push result on
 *18. ret (0): return from a function call.
 *19. print(0): print and pop top value.
 *20. pop (1): @1 number of values . Pop top @1 values out.
 *21. tpush(1): @1 address of value on stack (index).
                push the value on T[@1] at the top of stack.
 *22. tswap(2): @1 address of left operand, @2 address of right operand.
                swap 2 values T[@1],T[@2]
 *23. bpush(1): @1 address of value on stack(index).
                push the value on B[@1]
 *24. bswap(2): @1 address of left operand ; @2 right operand
                swap 2 values B[@1],B[@2]
 *25. bt_move(2): @1 address of left operand ; @2 right operand
                move T[@2] to B[@1].
 *26. bb_move(2): @1 address of left operand ; @2 right operand
                move B[@2] to B[@1].
 *27. tb_move(2): @1 address of left operand ; @2 right operand
                move B[@2] to T[@1].
 *28. tt_move(2): @1 address of left operand ; @2 right operand
                move T[@2] to T[@2].
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
 *49. jept(1): @1 PC position.
                jump to @1 only when T[1] is empty. Pop T[1].
 *50. enter(0): Enter a GC scope.
 *51. exit(0) : Exit a GC scope.
 *52. include(2): @1 include type @2 optional variable.
                @1 could be 0 (NONE) , 1(UPVALUE) and 2(JSON).
                @2 is the function parameter if we have.
                After include/rendering the template on to stack, pop all the related value.
 *53. import(2): ----
 *54. import_symbol:
 *55. extends
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
  X(VM_BPUSH,"bpush") \
  X(VM_BSWAP,"bswap") \
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
  int par_cnt; /* Function parameters count */
  const char* name; /* Pointer to function name */
};

struct runtime {
  struct ajj_object* cur_tmpl;   /* current jinja template */
  struct func_frame call_stk[ AJJ_MAX_CALL_STACK ];
  size_t cur_call_stk; /* Current stk position */
  struct ajj_value val_stk[AJJ_MAX_VALUE_STACK_SIZE]; /* current value stack size */
  size_t cur_val_stk;
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

/* =============================================
 * Interfaces
 * ===========================================*/

#define VM_ERROR_NOT_VALID_ENTRY -1
#define VM_ERROR_NO_SUCH_FUNCTION -2

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
