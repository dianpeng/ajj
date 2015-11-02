#ifndef _AJJ_PRIV_H_
#define _AJJ_PRIV_H_
#include "ajj.h"
#include <ctype.h>

/* tokenizer */
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

enum {
  TK_TEXT,TK_COMMENT,
  TK_LSTMT,TK_RSTMT,TK_LEXP,TK_REXP,
  TK_IF,TK_ELIF,TK_ENDIF,TK_FOR,TK_ENDFOR,TK_MACRO,TK_ENDMACRO,
  TK_CALL,TK_ENDCALL,TK_FILTER,TK_ENDFILTER,
  TK_SET,TK_ENDSET,TK_MOVE,TK_BLOCK,TK_ENDBLOCK,TK_RAW,TK_ENDRAW,
  TK_EXTENDS,TK_IMPORT,TK_INCLUDE,
  TK_FROM,TK_IN,TK_AS,TK_LPAR,TK_RPAR,TK_LSQR,TK_RSQR,TK_LBRA,TK_RBRA,
  TK_ADD,TK_SUB,TK_MUL,TK_DIV,TK_DIVTRUCT,TK_MOD,TK_POW,
  TK_ASSIGN,TK_EQ,TK_NE,TK_LT,TK_LE,TK_GT,TK_GE,
  TK_AND,TK_OR,TK_NOT,TK_PIPE,TK_DOT,TK_COMMA,
  TK_STRING,TK_NUMBER,TK_VARIABLE,TK_TRUE,TK_FALSE,TK_NONE,
  TK_EOF,TK_UNKNOWN,

  SIZE_OF_TOKENS
};

int tk_lex( struct tokenizer* tk );
void tk_consume( struct tokenizer* tk );

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

/* A pretty small stack based VM. Every constants are loaded into stack
 * for manipulation. Each operator is almost a 1:1 mapping. Pretty simple.
 */

enum {
  /* arithmatic */
  VM_ADD , VM_SUB , VM_DIV , VM_MUL , VM_MOD , VM_POW ,
  VM_EQ  , VM_NEQ , VM_LT  , VM_LE  , VM_GT  , VM_GE  ,
  VM_AND , VM_OR  , VM_NOT , VM_NEG ,

  /* function invoking */
  VM_CALL,VM_RET,

  /* output value */
  VM_PRINT,

  /* memory operation */
  VM_SETVAR,VM_GETVAR,VM_ATTR,

  /* misc */
  VM_POP,
  VM_LVAR  , /* load var into the stack by lookup */
  VM_LSTR  , /* load str into the stack by lookup */
  VM_LTRUE , /* load true into stack */
  VM_LFALSE, /* load false into stack */
  VM_LNUM  , /* load number into stack */
  VM_LZERO , /* load zero into stack */
  VM_LNONE , /* load none into stack */
  VM_LIMM  , /* load an immdiet number into stack */
  VM_SVAR  , /* setup a variable in scope */
  VM_NVAR  , /* new an empty var in scope */

  /* dict/object */
  VM_LDICT, /* load a dict into stack */
  VM_DICT_ADD, /* add the key value into the dict on the stack */
  
  /* list */
  VM_LLIST, /* load a list into the stack */
  VM_LIST_ADD, /* load a value into the list */

  /* tuple */
  VM_LTUPLE, /* load a tuple into the stack */
  VM_LTUPLE_ADD, /* load a tuple into the stack */

  /* iterator */
  VM_ITR_BEG, /* setup the iterator frame */
  VM_ITR_TEST,/* test iterator is correct or not */
  VM_ITR_DEREF,/* deref the iterator */
  VM_ITR_MOV, /* move the iterator */

  /* jmp */
  VM_JNE,
  VM_JE,
  VM_JLT,
  VM_JLE,
  VM_JGE,
  VM_JGT,

  /* scope */
  VM_ENTER , /* enter into a scope */
  VM_EXIT  , /* exit a scope */
  VM_RESET , /* reset a scope */

  SIZE_OF_INSTRUCTIONS
};

struct program {
  int* codes;
  size_t len;
  const char* str_tbl[AJJ_LOCAL_CONSTANT_SIZE];
  size_t str_len;
  double num_tbl[AJJ_LOCAL_CONSTANT_SIZE];
  size_t num_len;
};

int vm_run( struct ajj* , /* ajj environment */
            struct ajj_object* , /* scope */
            const struct program* , /* program to be executed */
            ajj_value* output ); /* output for this program */


/* parser */
int parse( struct ajj* ,
           const char* src,
           ajj_value* output );


/* string */
struct string {
  const char* str;
  size_t len;
};

/* dictionary */
struct dict_entry {
  char key[AJJ_SYMBOL_NAME_MAX_SIZE];
  size_t len;
  struct ajj_object* object;
};

struct dict {
  struct dict_entry* entry;
  size_t len;
  size_t cap;
};

/* list/tuple */
struct list {
  struct ajj_object** val;
  size_t cap;
  size_t len;
};

/* object */
struct c_func {
  void* udata; /* user data */
  ajj_method func; /* function */
  char name[ AJJ_SYMBOL_NAME_MAX_SIZE ]; /* name */
};

struct jj_func {
  struct program code;
  char name [ AJJ_SYMBOL_NAME_MAX_SIZE ]; /* name */
};

enum {
  JINJA_BLOCK,
  JINJA_MACRO
};

struct function {
  union {
    struct c_func c_fn; /* C function */
    struct jj_func jj_fn; /* Jinja function */
  } f;
  int tp : 1; /* type of function */
  int jj_tp: 31; /* if function is jinja function, then the type */
};

struct object_vtable {
  struct function* func_tb; /* function table */
  size_t tb_len;

  /* object name could be larger than AJJ_SYMBOL_MAX_SIZE , since they
   * can be path of object while the execution */
  const char* name;
};

struct object {
  struct dict prop; /* properties of this objects */
  const struct object_vtable* vtbl; /* function table, not owned by it */
};

struct ajj_object {
  struct ajj_object* prev; /* private */
  struct ajj_object* next; /* private */

  struct ajj_object* parent[AJJ_EXTENDS_MAX_SIZE]; /* for extends */
  size_t parent_len;

  union {
    struct string str;
    struct dict d;
    struct list li;
    struct list tpl;
    struct object obj;
  } val;
  unsigned short type;
  unsigned short scp_id;
};


#endif /* _AJJ_PRIV_H_ */
