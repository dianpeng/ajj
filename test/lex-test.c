#include <stdio.h>
#include "../src/lex.h"


static
void test_basic() {
  {
    struct tokenizer tk;
    const char* source= \
      "ThisIsAText AlsoThisIsAText!" \
      "{{ new.object }} " \
      "{% for endfor variable else elif endif macro endmacro macroX %}" \
      "{% call endcall filter endfilter do set endset with endwith "\
      "move block endblock extends import include endinclude in as " \
      "continue break upvalue endupvalue json override fix %}";

    int i;
    token_id token[] = {
      TK_TEXT,
      TK_LEXP,
      TK_VARIABLE,
      TK_DOT,
      TK_VARIABLE,
      TK_REXP,
      TK_TEXT, /* That whitespace */
      TK_LSTMT,
      TK_FOR,
      TK_ENDFOR,
      TK_VARIABLE,
      TK_ELSE,
      TK_ELIF,
      TK_ENDIF,
      TK_MACRO,
      TK_ENDMACRO,
      TK_VARIABLE,
      TK_RSTMT,
      TK_LSTMT,
      TK_CALL,
      TK_ENDCALL,
      TK_FILTER,
      TK_ENDFILTER,
      TK_DO,
      TK_SET,
      TK_ENDSET,
      TK_WITH,
      TK_ENDWITH,
      TK_MOVE,
      TK_BLOCK,
      TK_ENDBLOCK,
      TK_EXTENDS,
      TK_IMPORT,
      TK_INCLUDE,
      TK_ENDINCLUDE,
      TK_IN,
      TK_AS,
      TK_CONTINUE,
      TK_BREAK,
      TK_UPVALUE,
      TK_ENDUPVALUE,
      TK_JSON,
      TK_OVERRIDE,
      TK_FIX,
      TK_RSTMT
    };

    tk_init(&tk,source);
    i = 0;
    while(tk.tk != TK_EOF) {
      assert(tk.tk == token[i]);
      ++i;
      tk_move(&tk);
    }
    tk_destroy(&tk);
  }
  {
    struct tokenizer tk;
    const char* source = \
      "TextAHahaha{{ a.b }}YouAreCorrect!";
    token_id token[] = {
      TK_TEXT,
      TK_LEXP,
      TK_VARIABLE,
      TK_DOT,
      TK_VARIABLE,
      TK_REXP,
      TK_TEXT
    };
    tk_init(&tk,source);
    assert(tk.tk == TK_TEXT);
    assert(strcmp(tk.lexeme.str,"TextAHahaha")==0);
    tk_move(&tk);
    assert(tk.tk == TK_LEXP);
    tk_move(&tk);
    assert(tk.tk == TK_VARIABLE);
    assert(strcmp(tk.lexeme.str,"a")==0);
    tk_move(&tk);
    assert(tk.tk == TK_DOT);
    tk_move(&tk);
    assert(tk.tk == TK_VARIABLE);
    assert(strcmp(tk.lexeme.str,"b")==0);
    tk_move(&tk);
    assert(tk.tk == TK_REXP);
    tk_move(&tk);
    assert(strcmp(tk.lexeme.str,"YouAreCorrect!")==0);
    tk_move(&tk);
    assert(tk.tk == TK_EOF);
    tk_destroy(&tk);
  }

  {
    struct tokenizer tk;
    const char* source = \
      "{% raw %} ChouBI {% HelloWorld %} {% endraw %}" \
      "{% +-*/ // /// % true false 1234.56 \'helloworld\' %}";
    token_id token[] = {
      TK_TEXT,
      TK_LSTMT,
      TK_ADD,
      TK_SUB,
      TK_MUL,
      TK_DIV,
      TK_DIVTRUCT,
      TK_DIVTRUCT,
      TK_DIV,
      TK_MOD,
      TK_TRUE,
      TK_FALSE,
      TK_NUMBER,
      TK_STRING,
      TK_RSTMT
    };
    int i;

    tk_init(&tk,source);
    i = 0;
    while(tk.tk != TK_EOF) {
      assert(tk.tk == token[i]);
      tk_move(&tk);
      ++i;
    }
    tk_destroy(&tk);
  }
  {
    struct tokenizer tk;
    const char* source = \
      "{% {}()[].**and or not |,:None true True False false none !=" \
      " <= >= <> == % %}";
    token_id token[] = {
      TK_LSTMT,
      TK_LBRA,
      TK_RBRA,
      TK_LPAR,
      TK_RPAR,
      TK_LSQR,
      TK_RSQR,
      TK_DOT,
      TK_POW,
      TK_AND,
      TK_OR,
      TK_NOT,
      TK_PIPE,
      TK_COMMA,
      TK_COLON,
      TK_NONE,
      TK_TRUE,
      TK_TRUE,
      TK_FALSE,
      TK_FALSE,
      TK_NONE,
      TK_NE,
      TK_LE,
      TK_GE,
      TK_LT,
      TK_GT,
      TK_EQ,
      TK_MOD,
      TK_RSTMT
    };
    int i;

    i = 0;
    tk_init(&tk,source);
    while( tk.tk != TK_EOF ) {
      assert(tk.tk == token[i]);
      tk_move(&tk);
      ++i;
    }
    tk_destroy(&tk);
  }
}

int main() {
  test_basic();
}




