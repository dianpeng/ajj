#include <stdio.h>
#include <lex.h>
#include <ajj-priv.h>


static
void test_basic() {
  {
    struct tokenizer tk;
    const char* source= \
      "ThisIsAText \\{{\\{{\\{ AlsoThisIsAText!" \
      "{{ new.object }}\n" \
      "ASD asd sad" \
      "{% for endfor variable else elif endif macro endmacro macroX %}" \
      "{% call endcall filter endfilter do set endset with endwith "\
      "move block endblock extends import endimport include endinclude in as " \
      "continue break upvalue endupvalue json override optional %}";

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
      TK_ENDIMPORT,
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
      TK_OPTIONAL,
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
    struct string lexeme;
    const char* source = \
      "TextAHahaha{{ a.b }}\nYouAreCorrect asd!";
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
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"TextAHahaha")==0);
    tk_move(&tk);
    assert(tk.tk == TK_LEXP);
    tk_move(&tk);
    assert(tk.tk == TK_VARIABLE);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"a")==0);
    tk_move(&tk);
    assert(tk.tk == TK_DOT);
    tk_move(&tk);
    assert(tk.tk == TK_VARIABLE);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"b")==0);
    tk_move(&tk);
    assert(tk.tk == TK_REXP);
    tk_move(&tk);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"\nYouAreCorrect asd!")==0);
    tk_move(&tk);
    assert(tk.tk == TK_EOF);
    tk_destroy(&tk);
  }

  {
    struct tokenizer tk;
    struct string lexeme;
    const char* source = \
      "{{ a }}a b c d{{ a }}";
    token_id token[] = {
      TK_LEXP,
      TK_VARIABLE,
      TK_REXP,
      TK_TEXT,
      TK_LEXP,
      TK_VARIABLE,
      TK_REXP
    };
    tk_init(&tk,source);
    assert(tk.tk == TK_LEXP);
    tk_move(&tk);
    assert(tk.tk == TK_VARIABLE);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"a")==0);
    tk_move(&tk);
    assert(tk.tk == TK_REXP);
    tk_move(&tk);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"a b c d")==0);
    tk_move(&tk);
    assert(tk.tk == TK_LEXP);
    tk_move(&tk);
    assert(tk.tk == TK_VARIABLE);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"a")==0);
    tk_move(&tk);
    assert(tk.tk == TK_REXP);
    tk_move(&tk);
    assert(tk.tk == TK_EOF);
    tk_destroy(&tk);
  }

  {
    struct tokenizer tk;
    struct string lexeme;
    const char* source = \
      "{{ a }}\na b c d{{ a }}";
    token_id token[] = {
      TK_LEXP,
      TK_VARIABLE,
      TK_REXP,
      TK_TEXT,
      TK_LEXP,
      TK_VARIABLE,
      TK_REXP
    };
    tk_init(&tk,source);
    assert(tk.tk == TK_LEXP);
    tk_move(&tk);
    assert(tk.tk == TK_VARIABLE);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"a")==0);
    tk_move(&tk);
    assert(tk.tk == TK_REXP);
    tk_move(&tk);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"\na b c d")==0);
    tk_move(&tk);
    assert(tk.tk == TK_LEXP);
    tk_move(&tk);
    assert(tk.tk == TK_VARIABLE);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"a")==0);
    tk_move(&tk);
    assert(tk.tk == TK_REXP);
    tk_move(&tk);
    assert(tk.tk == TK_EOF);
    tk_destroy(&tk);
  }

  {
    struct tokenizer tk;
    const char* source = \
      "{% raw %} Hi {% HelloWorld %} {% endraw %}" \
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
  {
    struct tokenizer tk;
    struct string lexeme;
    const char* source = \
      "{% 1.2345 123 'Hello\\\'World' True true false False None none %}";

    tk_init(&tk,source);
    assert(tk.tk == TK_LSTMT);
    tk_move(&tk);

    assert(tk.tk == TK_NUMBER);
    assert(tk.num_lexeme == 1.2345);
    tk_move(&tk);

    assert(tk.tk == TK_NUMBER);
    assert(tk.num_lexeme == 123);
    tk_move(&tk);

    assert(tk.tk == TK_STRING);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"Hello'World")==0);
    tk_move(&tk);

    assert(tk.tk == TK_TRUE);
    tk_move(&tk);

    assert(tk.tk == TK_TRUE);
    tk_move(&tk);

    assert(tk.tk == TK_FALSE);
    tk_move(&tk);

    assert(tk.tk == TK_FALSE);
    tk_move(&tk);

    assert(tk.tk == TK_NONE);
    tk_move(&tk);

    assert(tk.tk == TK_NONE);
    tk_move(&tk);

    assert(tk.tk == TK_RSTMT);
    tk_move(&tk);

    assert(tk.tk == TK_EOF);

    tk_destroy(&tk);
  }
  {
    const char* source = \
      "{% 1.2345 123 \'Hello World\' True true FFFFF %}";
    char buf[CODE_SNIPPET_SIZE];
    tk_get_code_snippet(source,4,buf,256);
    printf("%s\n",buf);
    tk_get_code_snippet(source,22,buf,256);
    printf("%s\n",buf);
  }
  {
    struct tokenizer tk;
    struct string lexeme;
    const char* source = \
      "{% raw %} {% Hello World %} {% for } foreachshit {%} %} }%{% {% endraw %}";
    tk_init(&tk,source);
    assert(tk.tk == TK_TEXT);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,
          " {% Hello World %} {% for } foreachshit {%} %} }%{% ")==0);
    tk_destroy(&tk);
  }
}

/* Test the whitespace control for AJJ :
 *
 * The script instruction has 3 type , ie :
 * 1) {% %} statement
 * 2) {{ }} expression
 * 3) {# #} comment
 *
 * These 3 types are not the normal type of text. In AJJ,
 * for simplicity, the whitespace control are applied automatically.
 *
 * These 3 types of instruction are have *SAME* whitespace remove
 * rules
 *
 * ===========================================================
 *
 * The rules are as follow:
 * 1. Any leading whitespace that resides at the same line of the
 * statement will be removed , if no text resides at the same line.
 *
 * 2. Any trailing whitespace that resides at the same line of the
 * statment will be removed , if no text resides at the same line.
 * Additionally , if the trailing whitespace line follows by another
 * statement, then the last line break will be removed. Eg:
 * This is a text \n
 *     {% statement %}
 *
 * Since the line includes statement's leading whitespace is effectively
 * the trailing whitespace for the text. So the last line break which
 * is the line break that after "text" will be removed. NOTES: only this
 * line break will be removed, the extra space before this line break
 * will not be removed.
 *
 * Some example:
 *
 * 1) Text resides at the same line of statement
 *
 * Text {% some_statement %}  ( Suppose statement will not output
 * any text ) . For this case, since text resides at the same line,
 * no whitespace will be removed, therefore it outputs Text , pay
 * attention to that extra space following "Text".
 * Same as {% some_statement %} Text , which the trailing text resides
 * at the same line. So the extra space before "Text" won't be removed.
 *
 * 2) Text not resides at the same line of statement.
 *
 * Text\n
 *  {% some_statement %}
 *
 * In this case, since Text is not resides at the same line of the
 * statement, so the leading whitespace of {% some_statement %} will
 * be removed.
 *
 * Same here : {% some_statement %}   \n
 *             Text
 *
 * In this case, the trailing whitespace of some_statement will be
 * removed.
 *
 * ============================================================
 */


static void test_ws_case( const char* L , const char* R ,
    const char* Text , const char* Exp ,
    token_id LT , token_id RT ) {
  struct tokenizer tk;
  struct string lexeme;
  char source[1024];
  sprintf(source,"%s %s%s%s %s",L,R,Text,L,R);
  tk_init(&tk,source);
  assert(tk.tk == LT);
  tk_move(&tk);
  assert(tk.tk == RT);
  tk_move(&tk);
  assert(tk.tk == TK_TEXT);
  lexeme = strbuf_tostring(&(tk.lexeme));
  assert(string_cmpc(&lexeme,Exp)==0);
  tk_move(&tk);
  assert(tk.tk == LT);
  tk_move(&tk);
  assert(tk.tk == RT);
  tk_move(&tk);
  assert(tk.tk == TK_EOF);
  tk_destroy(&tk);
}


static void test_ws() {
  test_ws_case("{%","%}"," A Test "," A Test ",
      TK_LSTMT,TK_RSTMT);
  test_ws_case("{%","%}","  \n A Test \n "," A Test ",
      TK_LSTMT,TK_RSTMT);
  test_ws_case("{%","%}"," \n A Test \n Here "," A Test \n Here ",
      TK_LSTMT,TK_RSTMT);
  test_ws_case("{%","%}"," Here \n A Test \n "," Here \n A Test ",
      TK_LSTMT,TK_RSTMT);


  test_ws_case("{{","}}"," A Test "," A Test ",
      TK_LEXP,TK_REXP);
  test_ws_case("{{","}}"," \n A Test \n "," A Test ",
      TK_LEXP,TK_REXP);
  test_ws_case("{{","}}"," \n A Test \n Here "," A Test \n Here ",
      TK_LEXP,TK_REXP);
  test_ws_case("{{","}}"," Here \n A Test \n "," Here \n A Test ",
      TK_LEXP,TK_REXP);


  { /* text on different line */
    struct tokenizer tk;
    struct string lexeme;
    const char* source =  "This Text {# comment #} Another Text";
    tk_init(&tk,source);
    assert(tk.tk == TK_TEXT);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"This Text  Another Text")==0);
    tk_move(&tk);
    assert(tk.tk == TK_EOF);
  }

  { /* text on different line */
    struct tokenizer tk;
    struct string lexeme;
    const char* source =  "This Text \n  {# comment #}   \n Another Text";
    tk_init(&tk,source);
    assert(tk.tk == TK_TEXT);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"This Text  Another Text")==0);
    tk_move(&tk);
    assert(tk.tk == TK_EOF);
  }

  { /* text on different line */
    struct tokenizer tk;
    struct string lexeme;
    const char* source =  "This Text {# comment #}   \n Another Text";
    tk_init(&tk,source);
    assert(tk.tk == TK_TEXT);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"This Text  Another Text")==0);
    tk_move(&tk);
    assert(tk.tk == TK_EOF);
  }

  { /* text on different line */
    struct tokenizer tk;
    struct string lexeme;
    const char* source =  "This Text \n   {# comment #} Another Text";
    tk_init(&tk,source);
    assert(tk.tk == TK_TEXT);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"This Text  Another Text")==0);
    tk_move(&tk);
    assert(tk.tk == TK_EOF);
  }
}

#ifndef DO_COVERAGE
int main() {
#else
int lex_test_main() {
#endif
  test_ws();
}
