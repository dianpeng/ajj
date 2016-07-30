#include "lex.h"
#include "utf.h"

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
int tk_lex_rmlead( struct tokenizer* tk , int pos );

token_id tk_init( struct tokenizer* tk , const char* src ) {
  tk->src = src;
  tk->pos = 0;
  tk->mode = TOKENIZE_JINJA;
  strbuf_init(&(tk->lexeme));
  return tk_lex(tk);
}

int tk_expect( struct tokenizer* tk , token_id t ) {
  if( tk_lex(tk) == t ) {
    tk_move(tk);
    return 0;
  } else {
    return -1;
  }
}

int tk_string_escape_char( Rune c ) {
  switch(c) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'b': return '\b';
    case 'r': return '\r';
    case '\'':return '\'';
    case '\\':return '\\';
    default: return 0;
  }
}

int tk_string_reescape_char( Rune c ) {
  switch(c) {
    case '\n': return 'n';
    case '\t': return 't';
    case '\b': return 'b';
    case '\r': return 'r';
    case '\'': return '\'';
    case '\\': return '\\';
    default: return 0;
  }
}

#define GET_C1(C1,I,O) \
  do { \
    O = chartorune(&(C1),tk->src+(I)); \
    if(C1== Runeerror) goto utf_fail; \
  } while(0)

#define GET_C2(C2,I,O1,O2) \
  do { \
    O2 = chartorune(&(C2),tk->src+(I)+(O1)); \
    if(C2 == Runeerror) goto utf_fail; \
  } while(0)

#define GET_C3(C3,I,O1,O2) \
  do { \
    chartorune(&(C3),tk->src+(I)+(O1)+(O2)); \
    if(C3 == Runeerror) goto utf_fail; \
  } while(0)

/* return negative value represent failed; otherwise
 * the length of comments should be skipped will be
 * returned */
static
int tk_skip_cmt( struct tokenizer* tk , size_t pos ) {
  size_t i = pos + 2; /* skip the first {# */
  Rune c1,c2;
  int o1,o2;
  while(1) {
    GET_C1(c1,i,o1); if(!c1) break;
    if( c1 == '#' ) {
      GET_C2(c2,i,o1,o2);
      if( c2 == '}' ) {
        /* return the offset.
         * i+o1+o2+tk_lex_rmlead represnet the position
         * that the new scanning should happen */
        return (i+o1+o2)+
          tk_lex_rmlead(tk,i+o1+o2) - pos;
      }
    }
    i += o1;
  }

utf_fail:
  tk->pos = i;
  tk->tk = TK_UNKNOWN;
  return -1;
}

static
token_id tk_lex_str( struct tokenizer* tk ) {
  size_t i = tk->pos+1;
  Rune c1,c2;
  int o1,o2;

  strbuf_reset( &(tk->lexeme) );

  while(1) {
    GET_C1(c1,i,o1);
    if( c1 == '\\' ) {
      GET_C2(c2,i,o1,o2);
      if( (c2=tk_string_escape_char(c2)) ) {
        c1 = c2;
        i += o2;
      }
    } else if( c1 == '\'' ) {
      break;
    } else if( c1 == 0 ) {
      /* find an EOF character which is not needed */
      RETURN(TK_STRING_ERROR,i-tk->pos);
    }
    strbuf_push_rune(&(tk->lexeme),c1);
    i += o1;
  }
  /* include the quotes length for this token length */
  RETURN(TK_STRING,i+1-tk->pos);

utf_fail:
  RETURN(TK_UNKNOWN_UTF,i-tk->pos);
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

static
int tk_lex_rmtrail( struct tokenizer* tk , size_t len ) {
  /* Trailing algorithm is a bit of complicated because
   * UTF8 cannot decode in backward direction. So we cannot
   * just traverl backwards. We just record the last seen
   * line break then we know which characters we can safely
   * drop */

  size_t i = 0;
  int last_ws = -1; /* last whitespace */
  int last_ln = -1; /* last linebreak. set to -1 enable us to
                     * delete the empty line when the buffer
                     * only contains a line of empty spaces */
  int can_del = 1;  /* can delete it or not */

  assert( tk->lexeme.len );

  while(i<tk->lexeme.len) {
    Rune c;
    int o = chartorune(&c,tk->lexeme.str+i);
    assert( o != Runeerror );
    assert( o );
    if(!tk_is_ispace(c)) {
      if(c == '\n') {
        last_ln = i;
        last_ws = -1;
        can_del = 1;
      } else {
        can_del = 0;
      }
    } else {
      /* only when last is negative one we set to
       * the correct index. But initially this value
       * is -2, which can only be set to -1 when a
       * new line is met. So we will start to remove
       * whitespace when we at least see one line break */
      if(last_ws <0) last_ws = i;
    }
    i += o;
  }

  if(!can_del) {
    /* we cannot remove anything */
    return 0;
  } else {
    if(last_ws == 0) {
      strbuf_reset(&(tk->lexeme));
      tk->pos += len;
      return -1;
    } else {
      if(last_ws >0) {
        assert(last_ln + 1 == last_ws);
        strbuf_resize(&(tk->lexeme),last_ln);
      }
      /* If last_ws is negative value which means
       * that the last line is an empty line, so
       * we really need not to delete it since you
       * cannot have a empty line without space. If
       * user have it, then it means user has an
       * extra empty line which indeed should not be
       * deleted */
      return 0;
    }
  }
}

static
int tk_lex_rmlead( struct tokenizer* tk , int pos ) {
  size_t i = pos;
  while(1) {
    Rune c;
    int o = chartorune(&c,tk->src+i);
    assert( c != Runeerror );
    if( !tk_is_ispace(c) ) {
      if(c == '\n') {
        i += o;
      } else {
        i = pos; /* We haven't seen line break, but see
                  * a non whitespace character, so we don't
                  * remove any leading whitespaces */
      }
      break;
    }
    i += o;
  }
  return i - pos;
}

/* keyword: for endfor if elif endif in and or not
 * set endset extends block endblock import include
 * macro endmacro call endcall filter endfilter
 * raw endraw true false none */

static
int tk_keyword_check( struct tokenizer* tk , const char* str , int i ) {
  int k;
  for( k = 0 ; k < 10 ; ++k ) {
    const unsigned char t = str[k];
    Rune c; int o;
    if(!t) return k;
    o = chartorune(&c,tk->src+i);
    if( c != t ) return k;
    i += o;
  }
  assert(0);
  return -1;
}

static
token_id tk_lex_keyword( struct tokenizer* tk , int offset ) {
  size_t i;
  Rune c;
  int o;
  strbuf_reset(&(tk->lexeme));

  /* append the existed chunk of string */
  if( offset > 0 )
    strbuf_append(&(tk->lexeme),tk->src+tk->pos,offset);

  /* check reset of the string here */
  i = offset + tk->pos;
  while(1) {
    o = chartorune(&c,tk->src+i);
    if( tk_id_rchar(c) ) {
      strbuf_push_rune(&(tk->lexeme),c);
    } else {
      /* runeerror and 0 is not valid rchar */
      break;
    }
    i += o;
  }

  if( tk->lexeme.len == 0 )
    RETURN(TK_UNKNOWN,0);
  else
    RETURN(TK_VARIABLE,i-tk->pos);
}

static
size_t skip_whitespace( const char* src, size_t pos ) {
  Rune c;
  int o;
  while(1) {
    o = chartorune(&c,src+pos);
    if(!o) break;
    if(o == Runeerror) break; /* treat invalid rune char
                               * as a terminator */
    if(!tk_is_ispace(c))
      break;
    pos += o;
  }
  return pos;
}

static
int check_not_id_rchar( const char* src , size_t offset ) {
  Rune c;
  chartorune(&c,src+offset);
  return tk_not_id_rchar(c);
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
 * O: or,override, optional
 * P: -
 * Q: -
 * R: return
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
  Rune c;

#ifndef NDEBUG
  int o =
#endif
    chartorune(&c,tk->src+i);

  switch(c) {
    case 'a': assert(o == 1);
      if( (len=tk_keyword_check(tk,"nd",i+1)) == 2 &&
          check_not_id_rchar(tk->src,i+3) )
        RETURN(TK_AND,3);
      else if( (len=tk_keyword_check(tk,"s",i+1)) == 1 &&
          check_not_id_rchar(tk->src,i+2) )
        RETURN(TK_AS,2);
      else
        return tk_lex_keyword(tk,len+1);
    case 'b': assert(o == 1);
      if( (len=tk_keyword_check(tk,"lock",i+1)) == 4 &&
          check_not_id_rchar(tk->src,i+5))
        RETURN(TK_BLOCK,5);
      else if((len=tk_keyword_check(tk,"reak",i+1))==4&&
          check_not_id_rchar(tk->src,i+5))
        RETURN(TK_BREAK,5);
      else
        return tk_lex_keyword(tk,len+1);
    case 'c': assert(o == 1);
      if( (len=tk_keyword_check(tk,"all",i+1)) == 3 &&
          check_not_id_rchar(tk->src,i+4) )
        RETURN(TK_CALL,4);
      else if((len=tk_keyword_check(tk,"ontinue",i+1))==7 &&
          check_not_id_rchar(tk->src,i+8))
        RETURN(TK_CONTINUE,8);
      else
        return tk_lex_keyword(tk,len+1);
    case 'd': assert(o == 1);
      if( (len = tk_keyword_check(tk,"o",i+1)) == 1 &&
          check_not_id_rchar(tk->src,i+2))
        RETURN(TK_DO,2);
    case 'e': assert(o == 1);
      if( (len = tk_keyword_check(tk,"nd",i+1)) == 2 ) {
        /* end starts */
        size_t k = i + 3;
        if( (len=tk_keyword_check(tk,"for",k)) == 3 &&
            check_not_id_rchar(tk->src,k+3) )
          RETURN(TK_ENDFOR,6);
        else if( (len=tk_keyword_check(tk,"block",k)) == 5 &&
            check_not_id_rchar(tk->src,k+5))
          RETURN(TK_ENDBLOCK,8);
        else if( (len=tk_keyword_check(tk,"if",k)) == 2 &&
            check_not_id_rchar(tk->src,k+2) )
          RETURN(TK_ENDIF,5);
        else if( (len=tk_keyword_check(tk,"set",k)) == 3 &&
            check_not_id_rchar(tk->src,k+3) )
          RETURN(TK_ENDSET,6);
        else if( (len=tk_keyword_check(tk,"filter",k)) == 6 &&
            check_not_id_rchar(tk->src,k+6) )
          RETURN(TK_ENDFILTER,9);
        else if( (len=tk_keyword_check(tk,"macro",k)) == 5 &&
            check_not_id_rchar(tk->src,k+5))
          RETURN(TK_ENDMACRO,8);
        else if( (len=tk_keyword_check(tk,"upvalue",k)) == 7 &&
            check_not_id_rchar(tk->src,k+7))
          RETURN(TK_ENDUPVALUE,10);
        else if( (len=tk_keyword_check(tk,"with",k)) == 4 &&
            check_not_id_rchar(tk->src,k+4))
          RETURN(TK_ENDWITH,7);
        else if( (len=tk_keyword_check(tk,"include",k)) == 7 &&
            check_not_id_rchar(tk->src,k+7))
          RETURN(TK_ENDINCLUDE,10);
        else if( (len=tk_keyword_check(tk,"call",k))==4 &&
            check_not_id_rchar(tk->src,k+4))
          RETURN(TK_ENDCALL,7);
        else if( (len=tk_keyword_check(tk,"import",k))==6 &&
            check_not_id_rchar(tk->src,k+6))
          RETURN(TK_ENDIMPORT,9);
        else
          return tk_lex_keyword(tk,len+3);
      } else if ((len = tk_keyword_check(tk,"lif",i+1)) == 3 &&
          check_not_id_rchar(tk->src,i+4) ) {
        RETURN(TK_ELIF,4);
      } else if( (len=tk_keyword_check(tk,"lse",i+1)) == 3 &&
            check_not_id_rchar(tk->src,i+4)) {
          RETURN(TK_ELSE,4);
      } else if( (len=tk_keyword_check(tk,"xtends",i+1))==6 &&
           check_not_id_rchar(tk->src,i+7)) {
          RETURN(TK_EXTENDS,7);
      } else {
        return tk_lex_keyword(tk,len+1);
      }
    case 'f': assert( o == 1 );
      if( (len = tk_keyword_check(tk,"ilter",i+1)) ==5 &&
          check_not_id_rchar(tk->src,i+6))
        RETURN(TK_FILTER,6);
      else if( (len = tk_keyword_check(tk,"alse",i+1)) == 4 &&
          check_not_id_rchar(tk->src,i+5))
        RETURN(TK_FALSE,5);
      else if( (len = tk_keyword_check(tk,"or",i+1)) == 2 &&
          check_not_id_rchar(tk->src,i+3))
        RETURN(TK_FOR,3);
      else
        return tk_lex_keyword(tk,len+1);
    case 'F': assert( o == 1 );
      if( (len = tk_keyword_check(tk,"alse",i+1)) ==4 &&
          check_not_id_rchar(tk->src,i+5))
        RETURN(TK_FALSE,5);
    case 'i': assert( o == 1 );
      if( (len = tk_keyword_check(tk,"n",i+1)) == 1 &&
          check_not_id_rchar(tk->src,i+2) )
        RETURN(TK_IN,2);
      else if( (len = tk_keyword_check(tk,"f",i+1)) == 1 &&
          check_not_id_rchar(tk->src,i+2) )
        RETURN(TK_IF,2);
      else if( (len = tk_keyword_check(tk,"s",i+1)) == 1 &&
          check_not_id_rchar(tk->src,i+2)) {
        int k = skip_whitespace(tk->src,i+2);
        int c = tk->src[k];
        if(c) {
          if((len = tk_keyword_check(tk,"not",k)) == 3 &&
              check_not_id_rchar(tk->src,k+3)) {
            RETURN(TK_ISN,(k-tk->pos+3));
          }
          RETURN(TK_IS,2);
        }
      } else if( (len = tk_keyword_check(tk,"nclude",i+1)) == 6 &&
          check_not_id_rchar(tk->src,i+7))
        RETURN(TK_INCLUDE,7);
      else if( (len = tk_keyword_check(tk,"mport",i+1)) == 5 &&
          check_not_id_rchar(tk->src,i+6))
        RETURN(TK_IMPORT,6);
      else
        return tk_lex_keyword(tk,len+1);
    case 'j': assert( o == 1 );
      if( (len=tk_keyword_check(tk,"son",i+1)) == 3 &&
          check_not_id_rchar(tk->src,i+4))
        RETURN(TK_JSON,4);
      else
        return tk_lex_keyword(tk,len+1);
    case 'm': assert( o == 1 );
      if( (len = tk_keyword_check(tk,"acro",i+1)) == 4 &&
          check_not_id_rchar(tk->src,i+5))
        RETURN(TK_MACRO,5);
      else if( (len = tk_keyword_check(tk,"ove",i+1)) == 3 &&
          check_not_id_rchar(tk->src,i+4))
        RETURN(TK_MOVE,4);
      else
        return tk_lex_keyword(tk,len+1);
    case 'n': assert( o == 1 );
      if( (len = tk_keyword_check(tk,"one",i+1)) == 3 &&
          check_not_id_rchar(tk->src,i+4))
        RETURN(TK_NONE,4);
      else if( (len=tk_keyword_check(tk,"ot",i+1))==2 &&
          check_not_id_rchar(tk->src,i+3)) {
        size_t k = skip_whitespace(tk->src,i+3);
        int c = tk->src[k];
        if(c) {
          /* try to check whether this value is IN or not */
          if((len = tk_keyword_check(tk,"in",k))==2 &&
              check_not_id_rchar(tk->src,k+2)) {
            RETURN(TK_NIN,(k-tk->pos+2));
          }
        }
        RETURN(TK_NOT,3);
      } else
        return tk_lex_keyword(tk,len+1);
    case 'N': assert( o == 1 );
      if( (len = tk_keyword_check(tk,"one",i+1)) == 3 &&
          check_not_id_rchar(tk->src,i+4))
        RETURN(TK_NONE,4);
      else
        return tk_lex_keyword(tk,len+1);
    case 'o': assert( o == 1 );
      if( (len = tk_keyword_check(tk,"r",i+1)) == 1 &&
          check_not_id_rchar(tk->src,i+2))
        RETURN(TK_OR,2);
      else if( (len = tk_keyword_check(tk,"verride",i+1)) == 7 &&
          check_not_id_rchar(tk->src,i+8))
        RETURN(TK_OVERRIDE,8);
      else if( (len = tk_keyword_check(tk,"ptional",i+1)) == 7 &&
          check_not_id_rchar(tk->src,i+8))
        RETURN(TK_OPTIONAL,8);
      else
        return tk_lex_keyword(tk,len+1);
    case 'r': assert( o == 1 );
      if( (len = tk_keyword_check(tk,"eturn",i+1)) == 5 &&
          check_not_id_rchar(tk->src,i+6))
        RETURN(TK_RETURN,6);
      else
        return tk_lex_keyword(tk,len+1);
    case 's': assert( o == 1 );
      if( (len = tk_keyword_check(tk,"et",i+1)) == 2 &&
          check_not_id_rchar(tk->src,i+3))
        RETURN(TK_SET,3);
      else
        return tk_lex_keyword(tk,len+1);
    case 't': assert( o == 1 );
      if( (len = tk_keyword_check(tk,"rue",i+1)) == 3 &&
          check_not_id_rchar(tk->src,i+4))
        RETURN(TK_TRUE,4);
      else
        return tk_lex_keyword(tk,len+1);
    case 'T': assert( o == 1 );
      if( (len = tk_keyword_check(tk,"rue",i+1)) == 3 &&
          check_not_id_rchar(tk->src,i+4))
        RETURN(TK_TRUE,4);
      else
        return tk_lex_keyword(tk,len+1);
    case 'u': assert( o == 1 );
      if( (len = tk_keyword_check(tk,"pvalue",i+1)) == 6 &&
          check_not_id_rchar(tk->src,i+7))
        RETURN(TK_UPVALUE,7);
      else
        return tk_lex_keyword(tk,len+1);
    case 'w': assert( o == 1 );
      if( (len = tk_keyword_check(tk,"ith",i+1)) == 3 &&
          check_not_id_rchar(tk->src,i+4))
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
  Rune c1,c2,c3;
  int o1,o2;

  assert(tk->mode = TOKENIZE_SCRIPT);
  strbuf_reset(&(tk->lexeme));

  do {
    GET_C1(c1,i,o1);
    switch(c1) {
      case ' ':case '\t':
      case '\b':case '\n':
        ++(tk->pos);
        break;
      case '%':
        GET_C2(c2,i,o1,o2);
        if( c2 == '}' ) {
          RETURN(TK_RSTMT,2+tk_lex_rmlead(tk,i+2));
        } else {
          RETURN(TK_MOD,1);
        }
        break;
      case '+': RETURN(TK_ADD,1);
      case '-': /* -%} will be treated same as %} */
        GET_C2(c2,i,o1,o2);
        if( c2 == '%' ) {
          GET_C3(c3,i,o1,o2);
          if( c3 == '}' )
            /* for compatibility, we accept -%} as end of tag,
             * but we it doesn't support any semantic */
            RETURN(TK_RSTMT,3+tk_lex_rmlead(tk,i+3));
        }
        RETURN(TK_SUB,1);
      case '*':
        GET_C2(c2,i,o1,o2);
        if( c2 == '*' )
          RETURN(TK_POW,2);
        else
          RETURN(TK_MUL,1);
      case '/':
        GET_C2(c2,i,o1,o2);
        if( c2 == '/' )
          RETURN(TK_DIVTRUCT,2);
        else
          RETURN(TK_DIV,1);
      case '=':
        GET_C2(c2,i,o1,o2);
        if( c2 == '=' )
          RETURN(TK_EQ,2);
        else
          RETURN(TK_ASSIGN,1);
      case '!':
        GET_C2(c2,i,o1,o2);
        if( c2 == '=' )
          RETURN(TK_NE,2);
        else
          RETURN(TK_UNKNOWN,1);
      case '>':
        GET_C2(c2,i,o1,o2);
        if( c2 == '=' )
          RETURN(TK_GE,2);
        else
          RETURN(TK_GT,1);
      case '<':
        GET_C2(c2,i,o1,o2);
        if( c2 == '=' )
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
        GET_C2(c2,i,o1,o2);
        if( c2 == '}' ) {
          RETURN(TK_REXP,2+tk_lex_rmlead(tk,i+2));
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
    i+=o1;
  } while(1);

utf_fail:
  RETURN(TK_UNKNOWN_UTF,0);
}

/* For the raw/endraw block , we handle it entirely in lexing phase.
 * We silently filter out raw/endraw tags and then returns the included
 * text as a text to the upper caller as if no such token there */
static
int tk_check_single_keyword( struct tokenizer* tk , size_t pos ,
    const char* str , size_t len ) {
  size_t i = pos;
  do {
    Rune c;
#ifndef NDEBUG
    int o =
#endif
      chartorune(&c,tk->src+i);
    if( c == str[0] ) {
      assert( o == 1 );
      if( (size_t)(tk_keyword_check( tk,str+1,i+1)) == len-1 &&
          check_not_id_rchar(tk->src,len+i) ) {
        i += len;
        goto done;
      }
      return 0;
    } else {
      if( !tk_is_ispace(c) ) {
        return 0;
      }
    }
    assert( o == 1 ); ++i; /* only spaces will be here */
  } while(1);
done:
  /* skip the %} afterwards */
  while(1) {
    Rune c1,c2;
    int o1,o2;
    GET_C1(c1,i,o1);
    switch(c1) {
      case ' ':case '\t':
      case '\b':case '\r':
        break;
      case '%':
        GET_C2(c2,i,o1,o2); (void)o2;
        if( c2 == '}' ) {
          return (i+2-pos);
        }
        return -1;
      default:
        return -1;
    }
    assert(o1 == 1); ++i;
  }
utf_fail:
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
  Rune c1,c2;
  size_t o1,o2;

  strbuf_reset(&(tk->lexeme));
  while(1) {
    GET_C1(c1,i,o1); if(!c1) break; /* rule out the null terminator */
    if( c1 == '{' ) {
      GET_C2(c2,i,o1,o2);
      if( c2 ) {
        /* check whether it is a endraw tag or not */
        int offset;
        if( (offset=tk_check_endraw(tk,i+o1+o2)) >0 ) {
          /* checking if we have some data in the buffer or not */
          if( tk->lexeme.len == 0 ) {
            /* we don't have any data, it is an empty raw/endraw.
             * so we just move the parser forward */
            tk->pos = i+o1+o2+offset;
            return tk_lex(tk);
          } else {
            RETURN(TK_TEXT,offset+i+o1+o2-s);
          }
        }
      }
    }
    strbuf_push_rune(&(tk->lexeme),c1);
    i += o1; /* advance the i */
  }

  /* EOF meet, which is unexpected */
  RETURN(TK_UNKNOWN,tk->lexeme.len);

utf_fail:
  RETURN(TK_UNKNOWN_UTF,0);
}

static
token_id tk_lex_jinja( struct tokenizer* tk ) {
  int ret;
  int i = tk->pos;

  Rune c1,c2; /* c1 is the current character,
               * c2 is the look ahead one */
  size_t o1,o2; /* offset1 is the current character's offset
                 * offset2 is the look ahead one's offset */

  assert( tk->mode == TOKENIZE_JINJA );

  /* reset lexeme buffer */
  strbuf_reset( &(tk->lexeme) );

  do {
    GET_C1(c1,i,o1);

    switch(c1) {
      case '{':
        GET_C2(c2,i,o1,o2);
        switch(c2) {
          case '#':
            if(tk->lexeme.len >0) {
              tk_lex_rmtrail(tk,i-tk->pos);
            }
            if((ret=tk_skip_cmt(tk,i))<0)
              return tk->tk; /* failed */
            i += ret;
            continue;
          case '%':
            if( tk->lexeme.len > 0 && !tk_lex_rmtrail(tk,i-tk->pos) ) {
              RETURN(TK_TEXT,i-tk->pos);
            } else {
              int offset;
              if( (offset = tk_check_raw(tk,i+o1+o2)) == 0 ) {
                RETURN(TK_LSTMT,i+o1+o2-tk->pos);
              } else {
                if( offset < 0 )
                  RETURN(TK_UNKNOWN,0);
                else {
                  tk->pos += offset+o1+o2;
                  return tk_lex_raw(tk);
                }
              }
            }
          case '{':
            if( tk->lexeme.len > 0 && !tk_lex_rmtrail(tk,i-tk->pos) ) {
              RETURN(TK_TEXT,(i-tk->pos));
            } else {
              RETURN(TK_LEXP,i+o1+o2-tk->pos);
            }
          default:
            break;
        }
        break;
      case '\\':
        GET_C2(c2,i,o1,o2);
        if( tk_body_escape(c2) ) {
          i+=o2; c1 = c2;
        }
        break;
      case 0:
        goto done;
      default:
        break;
    }
    i+=o1;
    strbuf_push_rune(&(tk->lexeme),c1);
  } while(1);

done:
  if( tk->lexeme.len > 0 )
    RETURN(TK_TEXT,i-tk->pos);
  else
    RETURN(TK_EOF,0);

utf_fail:
  /* handle utf failaure */
  RETURN(TK_UNKNOWN_UTF,0);
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
  token_id ret;
  assert( tk->tk != TK_UNKNOWN && tk->tk != TK_EOF );

  if( tk->tk == TK_LSTMT || tk->tk == TK_LEXP ) {
    tk->mode = TOKENIZE_SCRIPT;
  } else if( tk->tk == TK_RSTMT || tk->tk == TK_REXP ) {
    tk->mode = TOKENIZE_JINJA;
  }
  tk->pos += tk->tk_len;
  ret = tk_lex(tk);
  return ret;
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
  assert( tk->mode = TOKENIZE_SCRIPT );
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

/* This function is *not* safe right now. Safe one may require us to
 * do a fully decoding of the input UTF8 sequence and then do the location */
void tk_get_code_snippet( const char* src , size_t pos ,
    char* output , size_t length ) {
  /* Fitting a CODE_SNIPPET_SIZE chunk of source code characters into
   * the buffer to make caller happy. This function is mostly used for
   * providing diagnostic information. */
  size_t start ; /* Start of the code snippet, we cannot decide the end here,
                  * so we just loop forward */
  size_t i;
  const char* end;

  if( length == 0 )
    length = CODE_SNIPPET_SIZE;
  else
    length = CODE_SNIPPET_SIZE >= length ? length : CODE_SNIPPET_SIZE;
  start = pos > length/2 ? pos-length/2 : 0;
  end = output + length;

  /* Here we have do a fully decode of the source file since the source
   * is UTF encoded scripts */
  for( i = 0 ; i < pos + length/2-1 && src[i] ; ) {
    Rune r;
    int len = chartorune(&r,i+src);
    assert(r != Runeerror); /* We should never see a runeerror */
    if( i >= start ) {
      if( r  == '\n' ) r = ' ';
      if( r != '\r' ) {
        int left = (end-output);
        if(left < len) break; /* Break promptly */
        else {
          output += runetochar(output,&r);
        }
      }
    }
    i += len;
  }
  *output = 0;
}

void tk_get_coordinate( const char* src , size_t until,
    size_t* ln, size_t* pos ) {
  size_t i;
  size_t l = 1;
  size_t p = 1;
  for( i = 0 ; i < until ; ) {
    Rune c;
    int o = chartorune(&c,src+i);
    if( c == '\n' ) {
      ++l; p = 1;
    } else {
      ++p;
    }
    i += o;
  }
  *ln = l;
  *pos = p;
}

#undef GET_C1
#undef GET_C2
#undef GET_C3
#undef RETURN
