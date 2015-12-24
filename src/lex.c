#include "lex.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#define RETURN(TK,L) \
  do { \
    tk->tk = (TK); \
    tk->tk_len = (L); \
    return (TK); \
  } while(0)

static
int tk_next_skip_cmt( struct tokenizer* tk ) {
  size_t i;
  char c;

  for( i = tk->pos ; (c=tk->src[i]) ; ++i ) {
    if( c == '#' ) {
      if( tk->src[i+1] == '}' ) {
        tk->pos = i+2;
        return 0;
      }
    }
  }
  tk->pos = i;
  tk->tk = TK_UNKNOWN;
  return -1;
}

static
token_id tk_lex_str( struct tokenizer* tk ) {
  size_t i = tk->pos+1;
  char c;
  assert(tk->src[tk->pos] == '\'' );
  strbuf_reset( &(tk->lexeme) );

  for( ; (c=tk->src[i]) ;++i ){
    if( c == '\\' ) {
      int nc = tk->src[i+1];
      if( (nc=tk_string_escape_char(nc)) ) {
        c = nc;
        ++i;
      }
    } else if( c == '\'' ) {
      break;
    }
    strbuf_push(&(tk->lexeme),c);
  }
  /* include the quotes length for this token length */
  RETURN(TK_STRING,tk->lexeme.len+2);
}

static
token_id tk_lex_num( struct tokenizer* tk ) {
  char* pend;
  errno = 0;
  tk->num_lexeme = strtod(tk->pos+tk->src,&pend);
  if( errno != 0 ) {
    RETURN(TK_UNKNOWN_NUMBER,0);
  } else {
    RETURN(TK_NUMBER,pend-(tk->pos+tk->src));
  }
}

/* keyword: for endfor if elif endif in and or not
 * set endset extends block endblock import include
 * macro endmacro call endcall filter endfilter
 * raw endraw true false none */

static
int tk_keyword_check( struct tokenizer* tk , const char* str , int i ) {
  if( str[0] != tk->src[i] )
    return 0;
  if( !str[1] || str[1] != tk->src[i+1] )
    return 1;
  if( !str[2] || str[2] != tk->src[i+2] )
    return 2;
  if( !str[3] ||  str[3] != tk->src[i+3] )
    return 3;
  if( !str[4] || str[4] != tk->src[i+4] )
    return 4;
  if( !str[5] || str[5] != tk->src[i+5] )
    return 5;
  if( !str[6] || str[6] != tk->src[i+6] )
    return 6;
  if( !str[7] || str[7] != tk->src[i+7] )
    return 7;
  if( !str[8] || str[8] != tk->src[i+8] )
    return 8;
  if( !str[9] || str[9] != tk->src[i+9] )
    return 9;
  if( !str[10] || str[10]!= tk->src[i+10])
    return 10;
  assert(0);
  return -1;
}

static
token_id tk_lex_keyword( struct tokenizer* tk , int offset ) {
  size_t i;
  char c;
  strbuf_reset(&(tk->lexeme));

  /* append the existed chunk of string */
  if( offset > 0 )
    strbuf_append(&(tk->lexeme),tk->src+tk->pos,offset);

  /* check reset of the string here */
  for( i = offset + tk->pos ; (c=tk->src[i]) ; ++i ) {
    if( tk_id_rchar(c) ) {
      strbuf_push(&(tk->lexeme),c);
    } else {
      break;
    }
  }

  if( tk->lexeme.len == 0 )
    RETURN(TK_UNKNOWN,0);
  else
    RETURN(TK_VARIABLE,i-tk->pos);
}

static
size_t skip_whitespace( const char* src, size_t pos ) {
  char c;
  for( ; (c =src[pos++]) ; ) {
    if(c==' '||c=='\t'||c=='\b')
      continue;
  }
  return pos;
}

/* Jinja 2 has so many keywords :
 * Sort according to the first character:
 * A: and
 * B: block,break
 * C: call,continue
 * D: do
 * E: elif,else,endfor,endmacro,endcall,
 *    endfilter,endset,endblock,endwith,
 *    endupvalue,endinclude,extends
 * F: filter,false,False,fix
 * G: -
 * H: -
 * I: if,in,is,include,import
 * J: json
 * K: -
 * L: -
 * M: macro,move
 * N: None,none
 * O: or,override
 * P: -
 * Q: -
 * R: -
 * S: set
 * T: -
 * U: upvalue
 * V: -
 * W: with
 * X: -
 * Y: -
 * Z: - */
static
token_id tk_lex_keyword_or_id( struct tokenizer* tk ) {
  size_t i = tk->pos;
  int len;
  int c = tk->src[i];
  switch(c) {
    case 'a':
      if( (len=tk_keyword_check(tk,"nd",i+1)) == 2 &&
          tk_not_id_rchar(tk->src[i+3]) )
        RETURN(TK_AND,3);
      else if( (len=tk_keyword_check(tk,"s",i+1)) == 1 &&
          tk_not_id_rchar(tk->src[i+2]) )
        RETURN(TK_AS,2);
      else
        return tk_lex_keyword(tk,len+1);
    case 'b':
      if( (len=tk_keyword_check(tk,"lock",i+1)) == 4 &&
          tk_not_id_rchar(tk->src[i+5]))
        RETURN(TK_BLOCK,5);
      else if((len=tk_keyword_check(tk,"reak",i+1))==4&&
          tk_not_id_rchar(tk->src[i+5]))
        RETURN(TK_BREAK,5);
      else
        return tk_lex_keyword(tk,len+1);
    case 'c':
      if( (len=tk_keyword_check(tk,"all",i+1)) == 3 &&
          tk_not_id_rchar(tk->src[i+4]) )
        RETURN(TK_CALL,4);
      else if((len=tk_keyword_check(tk,"ontinue",i+1))==7 &&
          tk_not_id_rchar(tk->src[i+8]))
        RETURN(TK_CONTINUE,8);
      else
        return tk_lex_keyword(tk,len+1);
    case 'd':
      if( (len = tk_keyword_check(tk,"o",i+1)) == 1 &&
          tk_not_id_rchar(tk->src[i+2]))
        RETURN(TK_DO,2);
    case 'e':
      if( (len = tk_keyword_check(tk,"nd",i+1)) == 2 ) {
        /* end starts */
        size_t k = i + 3;
        if( (len=tk_keyword_check(tk,"for",k)) == 3 &&
            tk_not_id_rchar(tk->src[k+3]) )
          RETURN(TK_ENDFOR,6);
        else if( (len=tk_keyword_check(tk,"block",k)) == 5 &&
            tk_not_id_rchar(tk->src[k+5]))
          RETURN(TK_ENDBLOCK,8);
        else if( (len=tk_keyword_check(tk,"if",k)) == 2 &&
            tk_not_id_rchar(tk->src[k+2]) )
          RETURN(TK_ENDIF,5);
        else if( (len=tk_keyword_check(tk,"set",k)) == 3 &&
            tk_not_id_rchar(tk->src[k+3]) )
          RETURN(TK_ENDSET,6);
        else if( (len=tk_keyword_check(tk,"filter",k)) == 6 &&
            tk_not_id_rchar(tk->src[k+6]) )
          RETURN(TK_ENDFILTER,9);
        else if( (len=tk_keyword_check(tk,"macro",k)) == 5 &&
            tk_not_id_rchar(tk->src[k+5]))
          RETURN(TK_ENDMACRO,8);
        else if( (len=tk_keyword_check(tk,"upvalue",k)) == 7 &&
            tk_not_id_rchar(tk->src[k+7]))
          RETURN(TK_ENDUPVALUE,10);
        else if( (len=tk_keyword_check(tk,"with",k)) == 4 &&
            tk_not_id_rchar(tk->src[k+4]))
          RETURN(TK_ENDWITH,7);
        else if( (len=tk_keyword_check(tk,"include",k)) == 7 &&
            tk_not_id_rchar(tk->src[k+7]))
          RETURN(TK_ENDINCLUDE,10);
        else if( (len=tk_keyword_check(tk,"call",k))==4 &&
            tk_not_id_rchar(tk->src[k+4]))
          RETURN(TK_ENDCALL,7);
        else if( (len=tk_keyword_check(tk,"import",k))==6 &&
            tk_not_id_rchar(tk->src[k+6]))
          RETURN(TK_ENDIMPORT,9);
        else
          return tk_lex_keyword(tk,len+3);
      } else if ((len = tk_keyword_check(tk,"lif",i+1)) == 3 &&
          tk_not_id_rchar(tk->src[i+4]) ) {
        RETURN(TK_ELIF,4);
      } else if( (len=tk_keyword_check(tk,"lse",i+1)) == 3 &&
            tk_not_id_rchar(tk->src[i+4])) {
          RETURN(TK_ELSE,4);
      } else if( (len=tk_keyword_check(tk,"xtends",i+1))==6 &&
           tk_not_id_rchar(tk->src[i+7])) {
          RETURN(TK_EXTENDS,7);
      } else {
        return tk_lex_keyword(tk,len+1);
      }
    case 'f':
      if( (len = tk_keyword_check(tk,"ilter",i+1)) ==5 &&
          tk_not_id_rchar(tk->src[i+6]))
        RETURN(TK_FILTER,6);
      else if( (len = tk_keyword_check(tk,"alse",i+1)) == 4 &&
          tk_not_id_rchar(tk->src[i+5]))
        RETURN(TK_FALSE,5);
      else if( (len = tk_keyword_check(tk,"ix",i+1)) == 2 &&
          tk_not_id_rchar(tk->src[i+3]))
        RETURN(TK_FIX,3);
      else if( (len = tk_keyword_check(tk,"or",i+1)) == 2 &&
          tk_not_id_rchar(tk->src[i+3]))
        RETURN(TK_FOR,3);
      else
        return tk_lex_keyword(tk,len+1);
    case 'F':
      if( (len = tk_keyword_check(tk,"alse",i+1)) ==4 &&
          tk_not_id_rchar(tk->src[i+5]))
        RETURN(TK_FALSE,5);
    case 'i':
      if( (len = tk_keyword_check(tk,"n",i+1)) == 1 &&
          tk_not_id_rchar(tk->src[i+2]) )
        RETURN(TK_IN,2);
      else if( (len = tk_keyword_check(tk,"f",i+1)) == 1 &&
          tk_not_id_rchar(tk->src[i+2]) )
        RETURN(TK_IF,2);
      else if( (len = tk_keyword_check(tk,"s",i+1)) == 1 &&
          tk_not_id_rchar(tk->src[i+2])) {
        int k = skip_whitespace(tk->src,i+2);
        int c = tk->src[k];
        if(c) {
          if((len = tk_keyword_check(tk,"not",k)) == 3 &&
              tk_not_id_rchar(tk->src[k+3])) {
            RETURN(TK_ISN,(k-tk->pos+3));
          }
          RETURN(TK_IS,2);
        }
      } else if( (len = tk_keyword_check(tk,"nclude",i+1)) == 6 &&
          tk_not_id_rchar(tk->src[i+7]))
        RETURN(TK_INCLUDE,7);
      else if( (len = tk_keyword_check(tk,"mport",i+1)) == 5 &&
          tk_not_id_rchar(tk->src[i+6]))
        RETURN(TK_IMPORT,6);
      else
        return tk_lex_keyword(tk,len+1);
    case 'j':
      if( (len=tk_keyword_check(tk,"son",i+1)) == 3 &&
          tk_not_id_rchar(tk->src[i+4]))
        RETURN(TK_JSON,4);
      else
        return tk_lex_keyword(tk,len+1);
    case 'm':
      if( (len = tk_keyword_check(tk,"acro",i+1)) == 4 &&
          tk_not_id_rchar(tk->src[i+5]))
        RETURN(TK_MACRO,5);
      else if( (len = tk_keyword_check(tk,"ove",i+1)) == 3 &&
          tk_not_id_rchar(tk->src[i+4]))
        RETURN(TK_MOVE,4);
      else
        return tk_lex_keyword(tk,len+1);
    case 'n':
      if( (len = tk_keyword_check(tk,"one",i+1)) == 3 &&
          tk_not_id_rchar(tk->src[i+4]))
        RETURN(TK_NONE,4);
      else if( (len=tk_keyword_check(tk,"ot",i+1))==2 &&
          tk_not_id_rchar(tk->src[i+3])) {
        size_t k = skip_whitespace(tk->src,i+3);
        int c = tk->src[k];
        if(c) {
          /* try to check whether this value is IN or not */
          if((len = tk_keyword_check(tk,"in",k))==2 &&
              tk_not_id_rchar(tk->src[k+2])) {
            RETURN(TK_NIN,(k-tk->pos+2));
          }
        }
        RETURN(TK_NOT,3);
      } else
        return tk_lex_keyword(tk,len+1);
    case 'N':
      if( (len = tk_keyword_check(tk,"one",i+1)) == 3 &&
          tk_not_id_rchar(tk->src[i+4]))
        RETURN(TK_NONE,4);
      else
        tk_lex_keyword(tk,len+1);
    case 'o':
      if( (len = tk_keyword_check(tk,"r",i+1)) == 1 &&
          tk_not_id_rchar(tk->src[i+2]))
        RETURN(TK_OR,2);
      else if( (len = tk_keyword_check(tk,"verride",i+1)) == 7 &&
          tk_not_id_rchar(tk->src[i+8]))
        RETURN(TK_OVERRIDE,8);
      else
        return tk_lex_keyword(tk,len+1);
    case 's':
      if( (len = tk_keyword_check(tk,"et",i+1)) == 2 &&
          tk_not_id_rchar(tk->src[i+3]))
        RETURN(TK_SET,3);
      else
        return tk_lex_keyword(tk,len+1);
    case 't':
      if( (len = tk_keyword_check(tk,"rue",i+1)) == 3 &&
          tk_not_id_rchar(tk->src[i+4]))
        RETURN(TK_TRUE,4);
      else
        return tk_lex_keyword(tk,len+1);
    case 'T':
      if( (len = tk_keyword_check(tk,"rue",i+1)) == 3 &&
          tk_not_id_rchar(tk->src[i+4]))
        RETURN(TK_TRUE,4);
      else
        return tk_lex_keyword(tk,len+1);
    case 'u':
      if( (len = tk_keyword_check(tk,"pvalue",i+1)) == 6 &&
          tk_not_id_rchar(tk->src[i+7]))
        RETURN(TK_UPVALUE,7);
      else
        return tk_lex_keyword(tk,len+1);
    case 'w':
      if( (len = tk_keyword_check(tk,"ith",i+1)) == 3 &&
          tk_not_id_rchar(tk->src[i+4]))
        RETURN(TK_WITH,4);
      else
        return tk_lex_keyword(tk,len+1);
    default:
      if( tk_id_ichar(c) )
        return tk_lex_keyword(tk,1);
      else
        RETURN(TK_UNKNOWN,0);
  }
}

static
int tk_lex_script( struct tokenizer* tk ) {
  int i = tk->pos;
  char nc;
  char c;
  assert(tk->mode = TOKENIZE_SCRIPT);
  strbuf_reset(&(tk->lexeme));

  do {
    c = tk->src[i];
    switch(c) {
      case ' ':case '\t':
      case '\b':case '\n':
        /* allowed whitespaces */
        ++tk->pos;
        break;
      case '%':
        nc = tk->src[i+1];
        if( nc == '}' ) {
          RETURN(TK_RSTMT,2);
        } else {
          RETURN(TK_MOD,1);
        }
        break;
      case '+': RETURN(TK_ADD,1);
      case '-': /* -%} will be treated same as %} */
        nc = tk->src[i+1];
        if( nc == '%' ) {
          if( tk->src[i+2] == '}' )
            /* for compatibility, we accept -%} as end of tag,
             * but we it doesn't support any semantic */
            RETURN(TK_RSTMT,3);
        }
        RETURN(TK_SUB,1);
      case '*':
        nc = tk->src[i+1];
        if( nc == '*' )
          RETURN(TK_POW,2);
        else
          RETURN(TK_MUL,1);
      case '/':
        nc = tk->src[i+1];
        if( nc == '/' )
          RETURN(TK_DIVTRUCT,2);
        else
          RETURN(TK_DIV,1);
      case '=':
        nc = tk->src[i+1];
        if( nc == '=' )
          RETURN(TK_EQ,2);
        else
          RETURN(TK_ASSIGN,1);
      case '!':
        nc = tk->src[i+1];
        if( nc == '=' )
          RETURN(TK_NE,2);
        else
          RETURN(TK_UNKNOWN,1);
      case '>':
        nc = tk->src[i+1];
        if( nc == '=' )
          RETURN(TK_GE,2);
        else
          RETURN(TK_GT,1);
      case '<':
        nc = tk->src[i+1];
        if( nc == '=' )
          RETURN(TK_LE,2);
        else
          RETURN(TK_LT,1);
      case '|':
        RETURN(TK_PIPE,1);
      case '(':
        RETURN(TK_LPAR,1);
      case ')':
        RETURN(TK_RPAR,1);
      case '[':
        RETURN(TK_LSQR,1);
      case ']':
        RETURN(TK_RSQR,1);
      case '}':
        nc = tk->src[i+1];
        if( nc == '}' ) {
          RETURN(TK_REXP,2);
        } else {
          RETURN(TK_RBRA,1);
        }
      case '{':
        RETURN(TK_LBRA,1);
      case '.':
        RETURN(TK_DOT,1);
      case ',':
        RETURN(TK_COMMA,1);
      case ':':
        RETURN(TK_COLON,1);
      case ';':
        RETURN(TK_SEMICOLON,1);
      case '?':
        RETURN(TK_QUESTION,1);
      case '#':
        RETURN(TK_LEN,1);
      case '~':
        RETURN(TK_CAT,1);
      case '\'':
        return tk_lex_str(tk);
      case '0':case '1':case '2':case '3':case '4':
      case '5':case '6':case '7':case '8':case '9':
        return tk_lex_num(tk);
      default:
        return tk_lex_keyword_or_id(tk);
    }
    ++i;
  } while(1);
}

/* For the raw/endraw block , we handle it entirely in lexing phase.
 * We silently filter out raw/endraw tags and then returns the included
 * text as a text to the upper caller as if no such token there */
static
int tk_check_single_keyword( struct tokenizer* tk , size_t pos ,
    const char* str , size_t len ) {
  size_t i = pos;
  int c;
  do {
    c = tk->src[i];
    if( c == str[0] ) {
      if( tk_keyword_check( tk , str+1 , i+1 ) == len-1 &&
          tk_not_id_rchar(tk->src[len+i]) ) {
        i += len;
        goto done;
      }
      return 0;
    } else {
      if( c != ' ' && c != '\t' ) {
        return 0;
      }
    }
    ++i;
  } while(1);
done:
  /* skip the %} afterwards */
  for( ; (c=tk->src[i]) ; ++i ) {
    switch(c) {
      case ' ':case '\t':
        continue;
      case '%':
        if( tk->src[i+1] == '}' ) {
          return (i+2-pos);
        }
        return -1;
      default:
        return -1;
    }
  }
  return -1;
}


static
int tk_check_raw( struct tokenizer* tk , size_t pos ) {
  return tk_check_single_keyword(tk,pos,"raw",3);
}

static
int tk_check_endraw( struct tokenizer* tk ,size_t pos ) {
  return tk_check_single_keyword(tk,pos,"endraw",6);
}

/* This function will get all the text data inside of the raw scope
 * and then finish its raw tag. */
static
token_id tk_lex_raw( struct tokenizer* tk ) {
  size_t i = tk->pos;
  size_t s = tk->pos;
  char nc;
  char c;

  strbuf_reset(&(tk->lexeme));
  for( ; (c = tk->src[i]) ; ++i ) {
    if( c == '{' ) {
      if( tk->src[i+1] == '%' ) {
        /* check whether it is a endraw tag or not */
        int offset;
        if( (offset=tk_check_endraw(tk,i+2)) >0 ) {
          /* checking if we have some data in the buffer or not */
          if( tk->lexeme.len == 0 ) {
            /* we don't have any data, it is an empty raw/endraw.
             * so we just move the parser forward */
            tk->pos = i+2+offset;
            return tk_lex(tk);
          } else {
            RETURN(TK_TEXT,offset+i+2-s);
          }
        }
      }
    }
    strbuf_push(&(tk->lexeme),c);
  }
  /* EOF meet, which is unexpected */
  RETURN(TK_UNKNOWN,tk->lexeme.len);
}

static
token_id tk_lex_jinja( struct tokenizer* tk ) {
  int i = tk->pos;

  assert( tk->mode == TOKENIZE_JINJA );

  /* reset lexeme buffer */
  strbuf_reset( &(tk->lexeme) );

  do {
    char c = tk->src[i];
    switch(c) {
      case '{':
        switch(tk->src[i+1]) {
          case '#':
            tk->pos += 2;
            if(tk_next_skip_cmt(tk))
              return tk->tk;
            continue;
          case '%':
            if( tk->lexeme.len > 0 ) {
              RETURN(TK_TEXT,(i-tk->pos));
            } else {
              int offset;
              if( (offset = tk_check_raw(tk,i+2)) == 0 ) {
                RETURN(TK_LSTMT,2);
              } else {
                if( offset < 0 )
                  RETURN(TK_UNKNOWN,0);
                else {
                  tk->pos += offset+2;
                  return tk_lex_raw(tk);
                }
              }
            }
          case '{':
            if( tk->lexeme.len > 0 ) {
              RETURN(TK_TEXT,(i-tk->pos));
            } else {
              RETURN(TK_LEXP,2);
            }
          default:
            break;
        }
        break;
      case '\\':
        if( tk_body_escape(tk->src[i+1]) ) {
          ++i; c = tk->src[i];
        }
        break;
      case 0:
        goto done;
      default:
        break;
    }
    ++i;
    strbuf_push(&(tk->lexeme),c);
  } while(1);

done:
  if( tk->lexeme.len > 0 ) {
    RETURN(TK_TEXT,(i-tk->pos));
  } else {
    RETURN(TK_EOF,0);
  }
}

/* public interfaces */
token_id tk_lex( struct tokenizer* tk ) {
  switch(tk->mode){
    case TOKENIZE_JINJA:
      return tk_lex_jinja(tk);
    case TOKENIZE_SCRIPT:
      return tk_lex_script(tk);
    default:
      UNREACHABLE();
      return -1;
  }
}

token_id tk_move( struct tokenizer* tk ) {
  assert( tk->tk != TK_UNKNOWN && tk->tk != TK_EOF );
  assert( tk->tk_len > 0 );

  if( tk->tk == TK_LSTMT || tk->tk == TK_LEXP ) {
    tk->mode = TOKENIZE_SCRIPT;
  } else if( tk->tk == TK_RSTMT || tk->tk == TK_REXP ) {
    tk->mode = TOKENIZE_JINJA;
  }
  tk->pos += tk->tk_len;
  return tk_lex(tk);
}

#define HANDLE_KEYWORD(A,B) case A: \
  do { \
    strbuf_reset(&(tk->lexeme)); \
    strbuf_append(&(tk->lexeme),B,ARRAY_SIZE(B)-1); \
    tk->tk_len = ARRAY_SIZE(B)-1; \
    tk->tk = TK_VARIABLE; \
    return 0; \
  } while(0);

int tk_expect_id( struct tokenizer* tk ) {
  token_id t = tk->tk; /* we don't do tk_move */
  switch(t) {
    case TK_VARIABLE:
      return 0;
    TOKEN_KEYWORD_LIST(HANDLE_KEYWORD)
    default:
      return -1;
  }
}

#undef HANDLE_KEYWORD

const char* tk_get_name( int tk ) {
  switch(tk) {
#define X(A,B) case A: return B;
    TOKEN_LIST(X)
#undef X
    default:
      UNREACHABLE();
      return NULL;
  }
}

void tk_get_code_snippet( const char* src , size_t pos ,
    char* output , size_t length ) {
  /* Fitting a CODE_SNIPPET_SIZE chunk of source code characters into
   * the buffer to make caller happy. This function is mostly used for
   * providing diagnostic information. */
  int start ; /* Start of the code snippet, we cannot decide the end here,
               * so we just loop forward */
  size_t i;
  int c;
  if( length == 0 )
    length = CODE_SNIPPET_SIZE;
  else
    length = CODE_SNIPPET_SIZE >= length ? length : CODE_SNIPPET_SIZE;
  start = pos > length/2 ? pos-length/2: 0;
  for( i = start ; i < pos + length/2 -1 && (c=src[i]) ; ++i ) {
    if( c == '\n' ) c = ' '; /* rewrite the line break to space */
    if( c == '\r' ) continue;
    *output++ = c;
  }
  *output = 0;
}

void tk_get_coordinate( const char* src , size_t until,
    size_t* ln, size_t* pos ) {
  size_t i;
  size_t l = 1;
  size_t p = 1;
  for( i = 0 ; i < until ; ++i ) {
    assert(src[i]);
    if( src[i] == '\n' ) {
      ++l; p = 1;
    } else {
      ++p;
    }
  }
  *ln = l;
  *pos = p;
}
