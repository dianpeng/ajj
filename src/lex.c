#include "ajj-priv.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

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
int tk_lex_str( struct tokenizer* tk ) {
  size_t i = tk->pos+1;
  char c;
  assert(tk->src[tk->pos] == '\'' );
  strbuf_reset( &(tk->lexme) );

  for( ; (c=tk->src[i]) ;++i ){
    if( c == '\\' ) {
      int nc = tk->src[i+1];
      switch(nc){
        case 'n':
        case 't':
        case '\\':
        case '\'':
          ++i;
          break;
        default:
          break;
      }
    } else if( '\'' ) {
      break;
    }
    strbuf_push(&(tk->lexme),c);
  }
  /* include the quotes length for this token length */
  RETURN(TK_STRING,tk->lexme.len+2);
}

static inline
int tk_lex_num( struct tokenizer* tk ) {
  size_t i = tk->pos+1;
  char c;
  char** pend;
  errno = 0;
  tk->num_lexme = strtod(tk->pos+tk->src,&pend);
  if( errno != 0 ) {
    RETURN(TK_UNKNOWN,0);
  } else {
    RETURN(TK_NUMBER,pend-(tk->pos+tk->src));
  }
}

/* keyword: for endfor if elif endif in and or not
 * set endset extends block endblock import include
 * macro endmacro call endcall filter endfilter
 * raw endraw true false none */

static inline
int tk_keyword_check( struct tokenizer* tk , const char* str , int i ) {
  if( str[0] != tk->src[i] )
    return 0;
  if( str[1] && str[1] != tk->src[i+1] )
    return 1;
  if( str[2] && str[2] != tk->src[i+2] )
    return 2;
  if( str[3] && str[3] != tk->src[i+3] )
    return 3;
  if( str[4] && str[4] != tk->src[i+4] )
    return 4;
  if( str[5] && str[5] != tk->src[i+5] )
    return 5;
  if( str[6] && str[6] != tk->src[i+6] )
    return 6;
  if( str[7] && str[7] != tk->src[i+7] )
    return 7;
  if( str[8] && str[8] != tk->src[i+8] )
    return 8;
  assert(0);
  return -1;
}

static
int tk_lex_keyword( struct tokenizer* tk , int offset ) {
  size_t i;
  char c;
  strbuf_reset(&(tk->lexme));

  /* append the existed chunk of string */
  if( offset > 0 )
    strbuf_append(&(tk->lexme),tk->src+tk->pos,offset);

  /* check reset of the string here */
  for( i = offset + tk->pos ; (c=tk->src[i]) ; ++i ) {
    if( tk_is_id_rchar(c) ) {
      strbuf_push(&(tk->lexme),c);
    } else {
      break;
    }
  }

  if( tk->lexme.len == 0 )
    RETURN(TK_UNKNOWN,0);
  else
    RETURN(TK_VARIABLE,i-tk->pos);
}

static inline
int tk_lex_keyword_or_id( struct tokenizer* tk ) {
  size_t i = tk->pos+1;
  int len;
  int c = tk->src[i];
  switch(c) {
    case 'a':
      if( (len=tk_keyword_check(tk,"nd",i+1)) == 2 &&
          tk_not_id_rchar(tk->src[i+3]) )
        RETURN(TK_AND,3);
      else
        return tk_lex_keyword(tk,len+1);
    case 'b':
      if( (len=tk_keyword_check(tk,"lock",i+1)) == 4 &&
          tk_not_id_rchar(tk->src[i+5]))
        RETURN(TK_BLOCK,5);
      else
        return tk_lex_keyword(tk,len+1);
    case 'c':
      if( (len=tk_keyword_check(tk,"all",i+1)) == 3 &&
          tk_not_id_rchar(tk->src[i+4]) )
        RETURN(TK_CALL,4);
      else
        return tk_lex_keyword(tk,len+1);
    case 'd':
      if( (len = tk_keyword_check(tk,"d",i+1)) == 1 &&
          tk_not_id_rchar(tk->src[i+1]))
        RETURN(TK_DO,4);
    case 'e':
      if( (len = tk_keyword_check(tk,"nd",i+1)) == 2 ) {
        /* end starts */
        size_t k = i + 3;
        if( (len=tk_keyword_check(tk,"for")) == 3 &&
            tk_not_id_rchar(tk->src[k+3]) )
          RETURN(TK_ENDFOR,6);
        else if( (len=tk_keyword_check(tk,"block")) == 5 &&
            tk_not_id_rchar(tk->src[k+5]))
          RETURN(TK_ENDBLOCK,8);
        else if( (len=tk_keyword_check(tk,"if")) == 2 &&
            tk_not_id_rchar(tk->src[k+2]) )
          RETURN(TK_ENDIF,5);
        else if( (len=tk_keyword_check(tk,"set")) == 3 &&
            tk_not_id_rchar(tk->src[k+3]) )
          RETURN(TK_ENDSET,6);
        else if( (len=tk_keyword_check(tk,"filter")) == 6 &&
            tk_not_id_rchar(tk->src[k+6]) )
          RETURN(TK_ENDFILTER,9);
        else if( (len=tk_keyword_check(tk,"macro")) == 5 &&
            tk_not_id_rchar(tk->src[k+5]))
          RETURN(TK_ENDMACRO,8);
        else if( (len=tk_keyword_check(tk,"lse")) == 3 &&
            tk_not_id_rchar(tk->src[k+3]))
          RETURN(TK_ELSE,4);
        else
          return tk_lex_keyword(tk,len+3);
      } else if ((len = tk_keyword_check(tk,"lif")) == 3 &&
          tk_not_id_rchar(tk->src[i+4]) ) {
        RETURN(TK_ELIF,4);
      } else {
        return tk_lex_keyword(tk,len+1);
      }
    case 'f':
      if( (len = tk_keyword_check(tk,"ilter")) ==5 &&
          tk_not_id_rchar(tk->src[i+6]) )
        RETURN(TK_FILTER,6);
      else if( (len = tk_keyword_check(tk,"alse")) == 4 &&
          tk_not_id_rchar(tk->src[i+5]) )
        RETURN(TK_FALSE,5);
      else
        return tk_lex_keyword(tk,len+1);
    case 'i':
      if( (len = tk_keyword_check(tk,"n")) == 1 &&
          tk_not_id_rchar(tk->src[i+2]) )
        RETURN(TK_IN,2);
      else if( (len = tk_keyword_check(tk,"f")) == 1 &&
          tk_not_id_rchar(tk->src[i+2]) )
        RETURN(TK_IF,2);
      else if( (len = tk_keyword_check(tk,"nclude")) == 6 &&
          tk_not_id_rchar(tk->src[i+7]))
        RETURN(TK_INCLUDE,7);
      else if( (len = tk_keyword_check(tk,"mport")) == 5 &&
          tk_not_id_rchar(tk->src[i+6]))
        RETURN(TK_IMPORT,6);
      else
        return tk_lex_keyword(tk,len+1);
    case 'm':
      if( (len = tk_keyword_check(tk,"acro")) == 4 &&
          tk_not_id_rchar(tk->src[i+5]))
        RETURN(TK_MACRO,5);
      else
        return tk_lex_keyword(tk,len+1);
    case 'n':
      if( (len = tk_keyword_check(tk,"one")) == 3 &&
          tk_not_id_rchar(tk->src[i+4]))
        RETURN(TK_NONE,4);
      else
        return tk_lex_keyword(tk,len+1);
    case 'o':
      if( (len = tk_keyword_check(tk,"r")) == 1 &&
          tk_not_id_rchar(tk->src[i+2]))
        RETURN(TK_OR,2);
      else
        return tk_lex_keyword(tk,len+1);
    case 's':
      if( (len = tk_keyword_check(tk,"et")) == 2 &&
          tk_not_id_rchar(tk->src[i+3]))
        RETURN(TK_SET,3);
      else
        return tk_lex_keyword(tk,len+1);
    case 't':
      if( (len = tk_keyword_check(tk,"rue")) == 3 &&
          tk_not_id_rchar(tk->src[i+4]))
        RETURN(TK_TRUE,4);
      else
        return tk_lex_keyword(tk,len+1);
    default:
      return tk_lex_keyword(tk,0);
  }
}

static
int tk_lex_script( struct tokenizer* tk ) {
  int i = tk->pos;
  char nc;
  char c;
  assert(tk->mode = TOKENIZE_SCRIPT);
  strbuf_reset(&(tk->lexme));

  do {
    c = tk->src[i];
    switch(c) {
      case ' ':case '\t':
      case '\b':case '\n':
        /* allowed whitespaces */
        break;
      case '\n':
      case '%':
        nc = tk->src[i+1];
        if( nc == '}' ) {
          RETURN(TK_RSTMT,2);
        } else {
          RETURN(TK_MOD,1);
        }
        break;
      case '}':
        nc = tk->src[i+1];
        if( nc == '}' ) {
          RETURN(TK_REXP,2);
        } else {
          RETURN(TK_UNKNOWN,1);
        }
        break;
      case '+': RETURN(TK_ADD,1);
      case '-': RETURN(TK_SUB,1);
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
      case '.':
        RETURN(TK_DOT,1);
      case ',':
        RETURN(TK_COMMA,1);
      case ':':
        RETURN(TK_COLON,1);
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

/* raw and endraw is treated specially since it actually
 * contains multiple tokens however they will be treated
 * as one token , by this we could switch tokenizer's states
 * without bothering the parser.
 */

static
int tk_check_single_keyword( struct tokenizer* tk , const char* str , size_t len ) {
  size_t i = tk->pos + 2; /* skip the {% */
  do {
    int c = tk->src[i];
    switch(c) {
      case ' ':case '\t':
        continue;
      case str[0]:
        if( tk_lex_keyword( tk , str+1 , i+1 ) == len-1 &&
            tk_not_id_rchar(tk->src[len+i]) ) {
          i += len;
          goto done;
        }
        return 0;
      default:
        return 0;
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
          return (i-tk->pos);
        }
        return -1;
      default:
        return -1;
    }
  }
  return -1;
}


static inline
int tk_check_raw( struct tokenizer* tk ) {
  return tk_check_single_keyword(tk,"raw",3);
}

static inline
int tk_check_endraw( struct tokenizer* tk ) {
  return tk_check_single_keyword(tk,"endraw",6);
}

static
int tk_lex_jinja( struct tokenizer* tk ) {
  int i = tk->pos;

  assert( tk->mode == TOKENIZE_JINJA );

  /* reset lexme buffer */
  strbuf_reset( &(tk->lexme) );

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
            if( tk->lexme.len > 0 ) {
              RETURN(TK_TEXT,(i-tk->pos));
            } else {
              int offset;
              if( (offset = tk_check_raw(tk)) == 0 ) {
                RETURN(TK_LSTMT,2);
              } else {
                if( offset < 0 )
                  RETURN(TK_UNKNOWN,0);
                else
                  RETURN(TK_RAW,2+offset);
              }
            }
          case '{':
            if( tk->lexme.len > 0 ) {
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
    strbuf_push(&(tk->lexme),c);
  } while(1);

done:
  if( tk->lexme.len > 0 ) {
    RETURN(TK_TEXT,(i-tk->pos));
  } else {
    RETURN(TK_EOF,0);
  }
}

static
int tk_lex_raw( struct tokenizer* tk ) {
  size_t i = tk->pos;
  char nc;
  char c;
  assert( tk->mode = TOKENIZE_RAW );

  strbuf_reset(&(tk->lexme));
  for( ; (c = tk->src[i]) ; ++i ) {
    if( c == '{' ) {
      if( tk->src[i+1] == '%' ) {
        /* check whether it is a endraw tag or not */
        int offset;
        if( (offset = tk_check_endraw(tk)) < 0 ) {
          if( tk->lexme.len > 0 ) {
            /* We fail at the second try , since at that time we
             * can have multiple diagnostic information */
            RETURN(TK_TEXT,(i->tk->pos));
          }
          RETURN(TK_UNKNOWN,0);
        } else {
          if( offset > 0 ) {
            if( tk->lexme.len > 0 )
              RETURN(TK_TEXT,(i->tk->pos));
            else
              RETURN(TK_ENDRAW,(2+offset));
          }
        }
      }
    }
    strbuf_push(c);
  }
  /* EOF meet, which is unexpected */
  RETURN(TK_UNKNOWN,tk->lexme.len);
}

/* public interfaces */
int tk_lex( struct tokenizer* tk ) {
  switch(tk->mode){
    case TOKENIZE_JINJA:
      return tk_lex_jinja(tk);
    case TOKENIZE_SCRIPT:
      return tk_lex_script(tk);
    case TOKENIZE_RAW:
      return tk_lex_raw(tk);
    default:
      UNREACHABLE();
      return -1;
  }
}

int tk_consume( struct tokenizer* tk ) {
  assert( tk->tk != TK_UNKNOWN && tk->tk != TK_EOF );
  assert( tk->tk_len > 0 );

  if( tk->tk == TK_LSTMT || tk->tk == TK_LEXP ) {
    tk->mode = TOKENIZE_SCRIPT;
  } else if ( tk->tk == TK_RAW ) {
    tk->mode = TOKENIZE_RAW;
  } else if ( tk->tk == TK_ENDRAW ) {
    tk->mode = TOKENIZE_JINJA;
  } else if( tk->tk == TK_RSTMT || tk->tk == TK_REXP ) {
    tk->mode = TOKENIZE_JINJA;
  }
  tk->pos += tk->tk_len;
  return tk_lex(tk);
}
