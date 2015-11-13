#ifndef _LEX_H_
#define _LEX_H_
#include "util.h"

struct tokenizer {
  const char* src;
  size_t pos;
  int tk;
  size_t tk_len; /* This represent the length of the this token's corresponding
                  * part inside of the jinja template source code */
  int mode; /* tell me which tokenize mode need to go into */
  struct strubuf lexeme; /* for lexeme */
  double num_lexeme; /* only useful when the token is TK_NUMBER */
};

enum {
  TOKENIZE_JINJA,
  TOKENIZE_SCRIPT
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
  X(TK_WITH,"with") \
  X(TK_ENDWITH,"endwith") \
  X(TK_MOVE,"move") \
  X(TK_BLOCK,"block") \
  X(TK_ENDBLOCK,"endblock") \
  X(TK_EXTENDS,"extends") \
  X(TK_IMPORT,"import") \
  X(TK_INCLUDE,"include") \
  X(TK_ENDINCLUDE,"endinclude") \
  X(TK_FROM,"from") \
  X(TK_IN,"in") \
  X(TK_AS,"as") \
  X(TK_RECURSIVE,"recursive") \
  X(TK_CONTINUE,"continue") \
  X(TK_BREAK,"break") \
  X(TK_UPVALUE,"upvalue") \
  X(TK_ENDUPVALUE,"endupvalue") \
  X(TK_JSON,"json") \
  X(TK_OVERRIDE,"override") \
  X(TK_FIXED,"fix") \
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
  TOKEN_LIST(A),
  SIZE_OF_TOKENS
};
#undef X

const char* tk_get_name( int );

int tk_lex( struct tokenizer* tk );
int tk_move( struct tokenizer* tk );

static inline
int tk_init( struct tokenizer* tk , const char* src ) {
  tk->src = src;
  tk->pos = 0;
  tk->mode = TOKENIZE_JINJA;
  strbuf_init(&(tk->lexeme));
  return tk_lex(tk);
}

static inline
void tk_destroy( struct tokenizer* tk ) {
  strbuf_destroy(&(tk->lexeme));
}

static inline
int tk_expect( struct tokenizer* tk , int t ) {
  if( tk_lex(tk) == t ) {
    tk_move(tk);
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

#endif /* _LEX_H_ */
