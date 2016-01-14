#ifndef _LEX_H_
#define _LEX_H_
#include "conf.h"
#include "util.h"
#include "utf.h"
#include <ctype.h>

#define CODE_SNIPPET_SIZE 256

enum {
  TOKENIZE_JINJA,
  TOKENIZE_SCRIPT
};

#define TOKEN_KEYWORD_LIST(X) \
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
  X(TK_ENDIMPORT,"endimport") \
  X(TK_INCLUDE,"include") \
  X(TK_ENDINCLUDE,"endinclude") \
  X(TK_AS,"as") \
  X(TK_CONTINUE,"continue") \
  X(TK_BREAK,"break") \
  X(TK_UPVALUE,"upvalue") \
  X(TK_ENDUPVALUE,"endupvalue") \
  X(TK_JSON,"json") \
  X(TK_OVERRIDE,"override") \
  X(TK_OPTIONAL,"optional")

#define TOKEN_LIST(X) \
  X(TK_TEXT,"<text>") \
  X(TK_COMMENT,"<comment>") \
  X(TK_LSTMT,"{%") \
  X(TK_RSTMT,"%}") \
  X(TK_LEXP,"{{") \
  X(TK_REXP,"}}") \
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
  X(TK_LEN,"#") \
  X(TK_CAT,"~") \
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
  X(TK_PIPE,"|") \
  X(TK_DOT,".") \
  X(TK_COMMA,",") \
  X(TK_SEMICOLON,";") \
  X(TK_COLON,":") \
  X(TK_QUESTION,"?") \
  X(TK_IS,"is") \
  X(TK_ISN,"isnot") \
  X(TK_IN,"in") \
  X(TK_NIN,"notin") \
  X(TK_AND,"and") \
  X(TK_OR,"or") \
  X(TK_NOT,"not") \
  X(TK_RETURN,"return") \
  X(TK_TRUE,"<true>") \
  X(TK_FALSE,"<false>") \
  X(TK_NONE,"<none>") \
  X(TK_STRING,"<string>") \
  X(TK_NUMBER,"<number>") \
  X(TK_VARIABLE,"<variable>") \
  TOKEN_KEYWORD_LIST(X) \
  X(TK_EOF,"<eof>") \
  X(TK_UNKNOWN_NUMBER,"number is too large,overflow!")\
  X(TK_UNKNOWN,"token is not recognized!") \
  X(TK_UNKNOWN_UTF,"utf encoding rune error!")

#define X(A,B) A,
typedef enum {
  TOKEN_LIST(X)
  SIZE_OF_TOKENS
} token_id;
#undef X

struct tokenizer {
  const char* src;
  size_t pos;
  token_id tk;
  size_t tk_len; /* This represent the length of the this token's corresponding
                  * part inside of the jinja template source code */
  int mode; /* tell me which tokenize mode need to go into */
  struct strbuf lexeme; /* for lexeme */
  double num_lexeme; /* only useful when the token is TK_NUMBER */
};

const char* tk_get_name( int );

token_id tk_lex( struct tokenizer* tk );
token_id tk_move( struct tokenizer* tk );

/* Use to rewrite a keyword to a identifier when doing parsing. This is only
 * used when the parsing phase knows he expect a VARIABLE but not a keyword,
 * then user could call this function to rewrite the last keyword , if it has,
 * to a variable and return it back. */
int tk_expect_id( struct tokenizer* tk );

/* Use to get a human readable code snippet for diagnose information.
 * The snippet is the source line that contains the position "pos".
 * But at most 128 characters is fetched as upper bound for information.
 * The return string is owned by the caller, please free it properly.
 * The buffer is always assume has length CODE_SNIPPET_SIZE */
void tk_get_code_snippet( const char* src, size_t pos ,
    char* output , size_t length );
#define tk_get_current_code_snippet(tk,output,l) \
  tk_get_code_snippet((tk)->src,(tk)->pos,output,l)

void tk_get_coordinate( const char* src , size_t until,
    size_t* ln, size_t* pos );

token_id tk_init( struct tokenizer* tk , const char* src );
#define tk_destroy(T) strbuf_destroy(&(T)->lexeme)
int tk_expect( struct tokenizer* tk , token_id t );
#define tk_id_ichar(c) ((c) == '_' || isalpha(c))
#define tk_not_id_char(C) (!tk_id_ichar(C))
#define tk_id_rchar(C) ((C) =='_' || isalpha(C) || isdigit(C))
#define tk_not_id_rchar(C) (!tk_id_rchar(C))
#define tk_body_escape(C) ((C) =='{')

int tk_string_escape_char( Rune c );
int tk_string_reescape_char( Rune c );

#define tk_is_ispace(X) \
  ((X) == ' ' || (X) == '\t' || \
   (X) == '\r'|| (X) == '\b')

#endif /* _LEX_H_ */
