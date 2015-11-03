#ifndef _AJJ_PRIV_H_
#define _AJJ_PRIV_H_
#include "ajj.h"
#include "util.h"
#include <ctype.h>
#include <assert.h>

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
  TK_IF,TK_ELIF,TK_ELSE,TK_ENDIF,TK_FOR,TK_ENDFOR,TK_MACRO,TK_ENDMACRO,
  TK_CALL,TK_ENDCALL,TK_FILTER,TK_ENDFILTER, TK_DO ,
  TK_SET,TK_ENDSET,TK_MOVE,TK_BLOCK,TK_ENDBLOCK,TK_RAW,TK_ENDRAW,
  TK_EXTENDS,TK_IMPORT,TK_INCLUDE,
  TK_FROM,TK_IN,TK_AS,TK_LPAR,TK_RPAR,TK_LSQR,TK_RSQR,TK_LBRA,TK_RBRA,
  TK_ADD,TK_SUB,TK_MUL,TK_DIV,TK_DIVTRUCT,TK_MOD,TK_POW,
  TK_ASSIGN,TK_EQ,TK_NE,TK_LT,TK_LE,TK_GT,TK_GE,
  TK_AND,TK_OR,TK_NOT,TK_PIPE,TK_DOT,TK_COMMA,TK_COLON,
  TK_STRING,TK_NUMBER,TK_VARIABLE,TK_TRUE,TK_FALSE,TK_NONE,
  TK_EOF,TK_UNKNOWN,

  SIZE_OF_TOKENS
};

const char* tk_get_name( int );

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
  VM_EQ  , VM_NE  , VM_LT  , VM_LE  , VM_GT  , VM_GE  ,
  VM_AND , VM_OR  , VM_NOT , VM_NEG , VM_DIVTRUCT , VM_TENARY ,

  /* function invoking */
  VM_CALL,VM_RET,

  /* output value */
  VM_PRINT,

  /* memory operation */
  VM_SETVAR,VM_GETVAR,VM_ATTR,

  /* misc */
  VM_POP,
  VM_PUSH  , /* push another piece of data to top of stack */
  VM_MOVE  , /* move a data from the stack to another position */
  VM_LSTR  , /* load str into the stack by lookup */
  VM_LTRUE , /* load true into stack */
  VM_LFALSE, /* load false into stack */
  VM_LNUM  , /* load number into stack */
  VM_LZERO , /* load zero into stack */
  VM_LNONE , /* load none into stack */
  VM_LIMM  , /* load an immdiet number into stack */

  VM_VAR_LOAD, /* load a var from scope into stack */
  VM_VAR_SET  , /* setup a variable in scope */
  VM_VAR_NEW  , /* new an empty var in scope */

  VM_ATTR_LOAD ,/* load the attributes from current object on stack */

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
  VM_ITR_DELETE, /* delete the iterator */

  /* jmp */
  VM_JMP,
  VM_JT, /* jmp when true */

  /* scope */
  VM_SCOPE , /* load a scope */
  VM_ENTER , /* enter into a scope */
  VM_EXIT  , /* exit a scope */

  /* special instructions for PIPE */
  VM_PIPE  , /* Basically it will make stake(0) = stake(-1) and
              * stake(-1) = current-scope */

  SIZE_OF_INSTRUCTIONS
};

struct program {
  void* codes;
  size_t len;
  struct string *str_tbl[ AJJ_LOCAL_CONSTANT_SIZE ];
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



/* object */
struct c_func {
  void* udata; /* user data */
  ajj_method func; /* function */
  char name[ AJJ_SYMBOL_NAME_MAX_SIZE ]; /* name */
};

struct c_method {
  ajj_method func;
  char name [ AJJ_SYMBOL_NAME_MAX_SIZE ];
};

struct jj_func {
  struct program code;
  char name [ AJJ_SYMBOL_NAME_MAX_SIZE ]; /* name */
};

enum {
  C_FUNCTION,
  C_METHOD,
  JJ_BLOCK,
  JJ_MACRO
};

struct function {
  union {
    struct c_func c_fn; /* C function */
    struct c_method c_mt; /* C method */
    struct jj_func jj_fn; /* JJ_BLOCK/JJ_MACRO */
  } f;
  int tp;
};

struct object_vtable {
  struct function* func_tb; /* function table */
  size_t len;

  struct c_func dtor; /* delete function */


  /* object name could be larger than AJJ_SYMBOL_MAX_SIZE , since they
   * can be path of object while the execution */
  const char* name;
};

struct object {
  struct dict prop; /* properties of this objects */
  const struct object_vtable* vtbl; /* function table, not owned by it */
  void* data; /* object's data */
};

struct ajj_object {
  struct ajj_object* prev;
  struct ajj_object* next;

  struct ajj_object* parent[AJJ_EXTENDS_MAX_SIZE]; /* for extends */
  unsigned short parent_len;

  unsigned short tp;

  union {
    struct string str;
    struct object obj;
  } val;

  unsigned int scp_id;
};

void ajj_object_destroy( struct ajj* , struct ajj_object* p );

static inline
struct ajj_object*
ajj_object_create ( struct ajj* , struct ajj_object* scope );

static inline
struct ajj_object*
ajj_object_enter  ( struct ajj* , struct ajj_object* parent );

/* move an object from its own scope to another scope scp */
static inline
struct ajj_object*
ajj_object_move( struct ajj_object* obj , struct ajj_object* scp ) {
  assert( obj->scp_id < scp->scp_id );
  assert( obj->next != obj->prev ); /* cannot be a singleton scope object */

  /* remove object from its existed chain */
  obj->prev->next = obj->next;
  obj->next->prev = obj->prev;

  /* insert this object into the scope chain , insert _BEFORE_ this scope */
  scp->prev->next = obj;
  obj->prev = scp->prev;
  scp->prev = obj;
  obj->next = scp->prev;

  /* change scope id */
  obj->scp_id = scp->scp_id;

  return obj;
}

/* lookup */
static
struct ajj_object* ajj_object_find( struct ajj_object* scope ,
    const char* key );

/* ajj */
struct ajj {
  struct all_object global; /* global scope */
  struct slab obj_slab; /* object slab */
};


#endif /* _AJJ_PRIV_H_ */
