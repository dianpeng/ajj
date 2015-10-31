#ifndef _AJJ_PRIV_H_
#define _AJJ_PRIV_H_
#include "ajj.h"

/* data structures */
struct scp;

struct symbol {
  char symbol[AJJ_SYMBOL_NAME_MAX_SIZE];
  struct ajj_value value;
  int used; /* serve as an easy way to delete a symbol */
  struct scope* scp;
};

struct scope {
  struct scope* parent;
  struct symbol g_symbol[ AJJ_GLOBAL_SYMBOL_MAX_SIZE ];
  size_t sym_len;

  struct ajj_object gc_list; /* allocated list */
  struct scope* glb_scp; /* global scope for nesting */
};

static inline
struct symbol*
scope_new_symbol( struct scope* scp ) {
  if( scp->sym_len < AJJ_GLOBAL_SYMBOL_MAX_SIZE ) {
    scp->sym_len++;
    return scp->g_symbol + scp->sym_len-1;
  }
  return NULL;
}

struct symbol*
scope_get_symbol( struct scope* scp , const char* key );

struct ajj {
  struct scope global; /* global scope */
};

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

int tk_lex( struct tokenzier* tk );
void tk_consume( struct tokenizer* tk );

static inline
int tk_expect( struct tokenizer* tk , int tk ) {
  if( tk_lex(tk) == tk ) {
    tk_consume(tk);
    return 0;
  } else {
    return -1;
  }
}

static inline
int tk_id_ichar( char c ) {
  return c == '_' || isalph(c) ;
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

  /* jmp */
  VM_JNE,
  VM_JE,
  VM_JLZ,
  VM_JLEZ,
  VM_JGZ,
  VM_JGEZ,

  VM_ENTER , /* enter into a scope */
  VM_EXIT  , /* exit a scope */

  SIZE_OF_INSTRUCTIONS
};

struct program {
  void* codes;
  size_t len;
  const char* str_tbl[AJJ_LOCAL_CONSTANT_SIZE];
  size_t str_len;
  double num_tbl[AJJ_LOCAL_CONSTANT_SIZE];
  size_t num_len;
};

int vm_run( struct ajj* , /* ajj environment */
            struct scope* , /* scope that is used to run this piece of code */
            const struct program* , /* program to be executed */
            ajj_value* output ); /* output for this program */


/* parser */
int parse( struct ajj* ,
           const char* src,
           ajj_value* output );


struct string {
  const char* str;
  size_t len;
};

struct dict_entry {
  const char* key;
  size_t len;
  struct ajj_object* object;
};

struct dict {
  struct dict_entry* entry;
  size_t len;
  size_t cap;
};

struct array {
  struct ajj_object** val;
  size_t cap;
  size_t len;
};

struct tuple {
  struct ajj_object** val;
  size_t len;
};

struct cfunc {
  ajj_method entry;
  void* udata;
};

struct ajj_object {
  struct ajj_object* prev; /* private */
  struct ajj_object* next; /* private */

  struct ajj_object* extends; /* for extends */

  union {
    struct string str;
    struct dict d;
    struct array arr;
    struct tuple tpl;
    struct cfunc fn;
    struct program s_fn;
  } val;

  int type;
};

#endif /* _AJJ_PRIV_H_ */
