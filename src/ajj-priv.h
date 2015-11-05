#ifndef _AJJ_PRIV_H_
#define _AJJ_PRIV_H_
#include "ajj.h"
#include "util.h"
#include <ctype.h>
#include <assert.h>

/* =================================
 * Forward
 * ================================*/
struct gc_scope;

/* ==================================
 * Tokenizer
 * ================================*/
struct tokenizer {
  const char* src;
  size_t pos;
  int tk;
  size_t tk_len;
  int mode; /* tell me which tokenize mode need to go into */
  struct strubuf lexme; /* for lexme */
  double num_lexme; /* only useful when the token is TK_NUMBER */
};

enum {
  TOKENIZE_JINJA,
  TOKENIZE_SCRIPT,
  TOKENIZE_RAW
};

#define TOKEN_LIST(X) \
  X(TK_TEXT,"<text>") \
  X(TK_COMMENT,"<comment>") \
  X(TK_LSTMT,"{%") \
  X(TK_RSTMT,"%}") \
  X(TK_LEXP,"{{") \
  X(TK_REXP,"}}") \
  X(TK_IF,"if") \
  X(TK_ELIF,"elif") \
  X(TK_ELSE,"else") \
  X(TK_ENDIF,"endif") \
  X(TK_FOR,"for") \
  X(TK_ENDFOR,"endfor") \
  X(TK_MACRO,"macro") \
  X(TK_ENDMACRO,"endmacro") \
  X(TK_CALL,"call") \
  X(TK_ENDCALL,"endcall") \
  X(TK_FILTER,"filter") \
  X(TK_ENDFILTER,"endfilter") \
  X(TK_DO ,"do") \
  X(TK_SET,"set") \
  X(TK_ENDSET,"endset") \
  X(TK_MOVE,"move") \
  X(TK_BLOCK,"block") \
  X(TK_ENDBLOCK,"endblock") \
  X(TK_RAW,"raw") \
  X(TK_ENDRAW,"endraw") \
  X(TK_EXTENDS,"extends") \
  X(TK_IMPORT,"import") \
  X(TK_INCLUDE,"include") \
  X(TK_FROM,"from") \
  X(TK_IN,"in") \
  X(TK_AS,"as") \
  X(TK_RECURSIVE,"recursive") \
  X(TK_LPAR,"(") \
  X(TK_RPAR,")") \
  X(TK_LSQR,"[") \
  X(TK_RSQR,"]") \
  X(TK_LBRA,"{") \
  X(TK_RBRA,"}") \
  X(TK_ADD,"+") \
  X(TK_SUB,"-") \
  X(TK_MUL,"*") \
  X(TK_DIV,"/") \
  X(TK_DIVTRUCT,"//") \
  X(TK_MOD,"%") \
  X(TK_POW,"**") \
  X(TK_ASSIGN,"=") \
  X(TK_EQ,"==") \
  X(TK_NE,"!=") \
  X(TK_LT,"<") \
  X(TK_LE,"<=") \
  X(TK_GT,">") \
  X(TK_GE,">=") \
  X(TK_AND,"and") \
  X(TK_OR,"or") \
  X(TK_NOT,"not") \
  X(TK_PIPE,"|") \
  X(TK_DOT,".") \
  X(TK_COMMA,",") \
  X(TK_COLON,":") \
  X(TK_STRING,"<string>") \
  X(TK_NUMBER,"<number>") \
  X(TK_VARIABLE,"<variable>") \
  X(TK_TRUE,"<true>") \
  X(TK_FALSE,"<false>") \
  X(TK_NONE,"<none>") \
  X(TK_EOF,"<eof>") \
  X(TK_UNKNOWN,"<unknown>")

#define X(A) A ,
enum {
  TOKEN_LIST(A)
  SIZE_OF_TOKENS
};
#undef X

const char* tk_get_name( int );

int tk_lex( struct tokenizer* tk );
int tk_consume( struct tokenizer* tk );

static inline
int tk_init( struct tokenizer* tk , const char* src ) {
  tk->src = src;
  tk->pos = 0;
  tk->mode = TOKENIZE_JINJA;
  strbuf_init(&(tk->lexme));
  return tk_lex(tk);
}

static inline
void tk_destroy( struct tokenizer* tk ) {
  strbuf_destroy(&(tk->lexme));
}

static inline
int tk_expect( struct tokenizer* tk , int t ) {
  if( tk_lex(tk) == t ) {
    tk_consume(tk);
    return 0;
  } else {
    return -1;
  }
}

static inline
int tk_id_ichar( char c ) {
  return c == '_' || isalpha(c) ;
}

static inline
int tk_not_id_ichar( char c ) {
  return !tk_id_ichar(c);
}

static inline
int tk_id_rchar( char c ) {
  return c == '_' || isalpha(c) || isdigit(c);
}

static inline
int tk_not_id_rchar( char c ) {
  return !tk_id_rchar(c);
}

static inline
int tk_body_escape( char c ) {
  return c == '{';
}


/* =============================
 * Virtual Machine
 * ============================*/
enum {
  /* arithmatic */
  VM_ADD , VM_SUB , VM_DIV , VM_MUL , VM_MOD , VM_POW ,
  VM_EQ  , VM_NE  , VM_LT  , VM_LE  , VM_GT  , VM_GE  ,
  VM_AND , VM_OR  , VM_NOT , VM_NEG , VM_DIVTRUCT , VM_TENARY ,

  /* function invoking */
  VM_CALL,VM_RET,

  /* output value */
  VM_PRINT,

  /* misc */
  VM_POP,
  VM_TPUSH  , /* push another piece of data to top of stack */
  VM_TSWAP  , /* swap 2 places on stack */
  VM_TLOAD  , /* load the top stack element into a position */

  VM_BPUSH  ,
  BM_BSWAP  ,
  VM_BLOAD  ,

  /* Move support 2*2 types of addressing mode */
  VM_BT_MOVE , /* Move (Bottom,Top) */
  VM_BB_MOVE , /* Move (Bottom,Bottom) */
  VM_TB_MOVE , /* Move (Top,Bottom) */
  VM_TT_MOVE , /* Move (Top,Top) */

  VM_LSTR  , /* load str into the stack by lookup */
  VM_LTRUE , /* load true into stack */
  VM_LFALSE, /* load false into stack */
  VM_LNUM  , /* load number into stack */
  VM_LZERO , /* load zero into stack */
  VM_LNONE , /* load none into stack */
  VM_LIMM  , /* load an immdiet number into stack */

  /* ATTR */
  VM_ATTR_SET,
  VM_ATTR_GET,

  /* UpValue */
  VM_UPVAL_SET,
  VM_UPVAL_GET,
  VM_UPVAL_DEL,

  /* loop things */
  VM_LOOP,   /* loop instructions */
  VM_LOOPREC,/* recursive loop instructions */

  /* jmp , ALL THE JMP ARE ABSOLUTE VALUE , so no jmp parameter is
   * negative */
  VM_JMP,
  VM_JT, /* jmp when true */
  VM_JEPT, /* jmp when this object is empty */

  /* scope */
  VM_ENTER , /* enter into a scope */
  VM_EXIT  , /* exit a scope */

  /* NOP */
  VM_NOP0,
  VM_NOP1,
  VM_NOP2,

  /* special instructions for PIPE */
  VM_PIPE  , /* Basically it will make stake(0) = stake(-1) and
              * stake(-1) = current-scope */

  SIZE_OF_INSTRUCTIONS
};

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
    struct ajj_value def_val; /* This value is default value for this function.
                               * The memory it contains are OWNED by the program.
                               * Please make sure to delete those memory when
                               * delete the program objects */
  } par_list[ AJJ_FUNC_PAR_MAX_SIZE ];
  size_t par_size;
};

static inline
void program_init( struct program* prg ) {
  prg->len = 0;
  prg->str_len = 0;
  prg->num_len = 0;
  prg->par_size =0;
}

static
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

#define MINIMUM_CODE_PAGE_SIZE 256

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

int vm_run( struct ajj* , /* ajj environment */
            struct ajj_object* , /* scope */
            const struct program* , /* program to be executed */
            ajj_value* output ); /* output for this program */


/* ===============================
 * Parser
 * ==============================*/
int parse( struct ajj* ,
           const char* src,
           const char* key, /* key for this file */
           ajj_value* output );


/* ===============================
 * Object representation
 * =============================*/

struct c_closure {
  void* udata; /* user data */
  ajj_method func; /* function */
};

static inline
void c_closure_init( struct c_closure* cc ) {
  cc->udata = NULL;
  cc->func = NULL;
}

enum {
  C_FUNCTION,
  C_METHOD,
  JJ_BLOCK,
  JJ_MACRO
};

struct function {
  union {
    struct c_closure c_fn; /* C function */
    ajj_method c_mt; /* C method */
    struct program jj_fn; /* JJ_BLOCK/JJ_MACRO */
  } f;
  struct string name;
  int tp;
};

struct func_table {
  struct function func_buf[ AJJ_FUNC_LOCAL_BUF_SIZE ];
  struct function* func_tb; /* function table */
  size_t func_len;
  size_t func_cap;

  struct c_closure dtor; /* delete function */
  struct string name;
};

/* This function will initialize an existed function table */
static inline
void func_table_init( struct func_table* tb ,
    const struct ajj_dtor* dtor ,
    struct string name , int own ) {
  tb->func_tb = tb->func_buf;
  tb->func_len = 0;
  tb->func_cap = AJJ_FUNC_LOCAL_BUF_SIZE;
  tb->dtor = dtor ;
  tb->name = own ? name : string_dup(name);
}

/* Clear the GUT of func_table object */
static inline
void func_table_clear( struct func_table* tb ) {
  if( tb->func_cap > AJJ_FUNC_LOCAL_BUF_SIZE ) {
    free(tb->func_tb); /* on heap */
  }
  free(tb->name);
}

/* Add a new function into the func_table */
static inline
struct function* func_table_add_func( struct func_table* tb ) {
  if( tb->func_len == tb->func_cap ) {
   void* nf = malloc( sizeof(struct function)*(tb->func_cap)*2 );
   memcpy(nf,tb->func_tb,tb->func_len*sizeof(struct function));
   if( tb->func_tb != tb->func_buf ) {
     free(tb->func_tb);
   }
   tb->func_tb = nf;
   tb->func_cap *= 2;
  }
  return tb->func_tb + (tb->func_len++);
}

static inline
void func_table_shrink_to_fit( struct func_table* tb ) {
  if( tb->func_cap > AJJ_FUNC_LOCAL_BUF_SIZE ) {
    if( tb->func_len < tb->func_cap ) {
      struct function* f = malloc(tb->func_len*sizeof(struct function));
      memcpy(f,tb->func_tb,sizeof(struct function)*tb->func_len);
      free(tb->func_tb);
      tb->func_tb = f;
      tb->func_cap = func_len;
    }
  }
}

static inline
struct c_closure*
func_table_add_c_clsoure( struct func_table* tb , struct string name , int own ) {
  struct function* f = func_table_add_func(tb);
  f->name = own ? name : string_dup(name);
  f->tp = C_FUNCTION;
  c_closure_init(&(f->f,c_fn));
  return &(f->f.c_fn);
}

static inline
ajj_method*
func_table_add_c_method( struct func_table* tb , struct string name , int own ) {
  struct function* f = func_table_add_func(tb);
  f->name = own ? name : string_dup(name);
  f->tp = C_METHOD;
  return &(f->f.c_mt);
}

static inline
struct program*
func_table_add_jj_block( struct func_table* tb, struct string name , int own ) {
  struct function* f = func_table_add_func(tb);
  f->name = own ? name : string_dup(name);
  f->tp = JJ_BLOCK;
  program_init(&(f->f.jj_fn));
  return &(f->f.jj_fn);
}

static inline
struct program*
func_table_add_jj_macro( struct func_table* tb, struct string name , int own ) {
  struct function* f = func_table_add_func(tb);
  f->name = own ? name : string_dup(name);
  f->tp = JJ_MACRO;
  program_init(&(f->f.jj_fn));
  return &(f->f.jj_fn);
}

struct object {
  struct dict prop; /* properties of this objects */
  const struct func_table* fn_tb; /* This function table can be NULL which
                                   * simply represents this object doesn't have
                                   * any defined function related to it */
  void* data; /* object's data */
};

/* Create an object object */
static inline
void object_create( struct object* obj ,
    const struct func_table* func_tb, void* data ) {
  dict_create(&(obj->prop));
  obj->fn_tb = func_tb;
  obj->data = data;
}

struct ajj_object {
  struct ajj_object* prev;
  struct ajj_object* next;
  struct ajj_object* parent[AJJ_EXTENDS_MAX_SIZE]; /* for extends */
  unsigned short parent_len;
  unsigned short tp;
  union {
    struct string str; /* string */
    struct dict d;     /* dictionary */
    struct list l;     /* list */
    struct object obj; /* object */
  } val;
  struct gc_scope* scp;
};

/* Create a single ajj_object which is NOT INITIALZIED with any type
 * under the scope object "scope" */
static inline
struct ajj_object*
ajj_object_create ( struct ajj* , struct ajj_object* scope );

/* Delete a single ajj_object. This deletion will not destroy
 * any other objects inside of the linked chain */
static inline
struct void
ajj_object_destroy( struct ajj* , struct ajj_object* obj );

/* Clear an object's internal GUTS , not clear this object from
 * free list */
static
void ajj_object_clear( struct ajj* , struct ajj_object* obj );


/* Initialize an created ajj_object to a certain type */
static inline
void ajj_object_string( struct ajj_object* obj,
    const char* str , size_t len , int own ) {
  obj->val.str.str = own ? str : strdup(str);
  obj->val.str.len = len;
  obj->tp = AJJ_VALUE_STRING;
}

static inline
void ajj_object_dict( struct ajj_object* obj ) {
  dict_create(&(obj->val.d));
  obj->tp = AJJ_VALUE_DICT;
}

static inline
void ajj_object_list( struct ajj_object* obj ) {
  list_create(&(obj->val.l));
  obj->tp = AJJ_VALUE_LIST;
}

static
inline void ajj_object_obj( struct ajj_object* obj ,
    const struct func_table* fn_tb, void* data ) {
  object_create(&(obj_val.o),fn_tb,data);
  obj->tp = AJJ_VALUE_OBJECT;
}

/* Create an ajj_object serves as the scope object and inherited from
 * parent object "parent" */
static inline
struct ajj_object*
ajj_object_enter  ( struct ajj* , struct ajj_object* parent );

/* Exit an ajj_object represented scope. This will delete the scope object
 * and any objects that is OWNED by this scope object */
static inline
struct void
ajj_object_exit   ( struct ajj* , struct ajj_object* scope );

/* Move an object from its CURRENT scope to target scope "scp" */
static inline
struct ajj_object*
ajj_object_move( struct ajj_object* obj , struct ajj_object* scp );

/* ================================
 * GC Scope
 * ===============================*/
struct gc_scope {
  struct ajj_object gc_tail; /* tail of the GC objects list */
  struct gc_scope* parent;   /* parent scope */
  unsigned int scp_id;       /* scope id */
};

/* =================================
 * AJJ
 * ===============================*/
struct ajj {
  struct all_object global; /* global scope */
  struct slab obj_slab; /* object slab */
};

#endif /* _AJJ_PRIV_H_ */
