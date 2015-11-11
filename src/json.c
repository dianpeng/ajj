#include "ajj-priv.h"
#include <errno.h>
#include <stdlib.h>

static void
report_error( struct ajj* a , int pos , const char* src ,
    const char* format , ... );

struct state {
  int st;
  struct ajj_value v;
  struct strbuf k;
};

#define MAX_NESTED_SCOPE 512

#ifndef NDEBUG
static inline
int prev_st( int p , struct state* st ) {
  assert( p > 0 );
  return st[p-1].st;
}
#undef /* NDEBUG */


#define TOP() (stk[p])

#ifndef NDEBUG
#define INARRAY() (prev_st(pos,stk) == A)
#define INOBJECT() (prev_st(pos,stk) == O)
#else
#define INARRAY() (stk[p-1].st == A)
#define INOBJECT() (stk[p-1].st == O)
#endif /* NDEBUG */

#define PUSH(P,X) \
  do { \
    if( p == MAX_NESTED_SCOPE ) { \
      report_error(a,pos,src,"Too many nested object, more than %d!", \
          MAX_NESTED_SCOPE); \
      return -1; \
    } \
    stk[p].st = (P); \
    ++p; \
    stk[p].v.type = AJJ_VALUE_NOT_USE; \
    if( (P) == O ) { \
      strbuf_create(&(stk[p].k)); \
    } \
  } while(0)

#define POP() \
  do { \
    if( p == 0 ) { \
      report_error(a,pos,src,"[] or {} not match!"); \
      return -1; \
    } \
    if(INOBJECT()) { \
      strbuf_destroy(&(stk[p].k)); \
    } \
    if( p == 1 ) goto done; \
    --p; \
  } while(0)

#define SET_VALUE(val,X1,X2) \
  do { \
    if( INARRAY() ) { \
      assert( TOP().v.type == AJJ_VALUE_LIST ); \
      ajj_list_push(&(TOP().v),&val); \
      TOP().st = (X1);  \
    } else { \
      assert( INOBJECT() ); \
      assert( TOP().v.type == AJJ_VALUE_DICT ); \
      ajj_dict_insert(&(TOP().v),TOP().k.str,&val); \
      TOP().st = (X2); \
    } \
  } while(0)

#define FAIL() \
  else { \
    report_error(a,src,pos,"Parsing error,unknown token:%c!",pos[src]); \
    goto fail; \
  }

/* json parsing */
int json_decode( struct ajj* a, struct gc_scope* scp,
    const char* src , struct ajj_value* output) {
  static const int N = -1;
  static const int A = 0;
  static const int O = 1;
  static const int OA= 2;
  static const int V = 3; /* want a value */
  static const int K = 4; /* want a key   */
  static const int E = 5; /* want a equal */
  static const int C = 6; /* want a comma */
  static const int LV= 7; /* leading value */
  static const int LK= 8; /* leading key */

  size_t pos = 0;
  struct state stk[MAX_NESTED_SCOPE];
  int p = 0;
  int i = 0;

  /* Initialize the states */
  stk[p].v.type = AJJ_VALUE_NOT_USE;
  stk[p].st = OA;

  do {
    int c = src[pos];
    switch(c) {
      case 0: /* eof */
        report_error(a,pos,src,"Unexpected end of the stream!");
        goto fail;
      case ' ':case '\t':case '\n':case '\r':case '\b':
        break;
      case '[':
        if( TOP().st == OA || TOP().st == V ) {
          PUSH(A,LV);
          TOP().v = ajj_value_assign(
              ajj_object_create_list(a,scp));
        } FAIL()
        break;
      case '{':
        if( TOP().st == OA || TOP().st == V ) {
          PUSH(O,LK);
          TOP().v = ajj_value_assign(
              ajj_object_create_dict(a,scp));
        } FAIL()
        break;
      case ']':
        if( TOP().st == C || TOP().st == LV ) {
          struct ajj_value val;
          assert(TOP().v.type == AJJ_VALUE_LIST);
          if( !INARRAY() ) {
            report_error(a,pos,src,"Unknown token here:\"]\"");
            goto fail;
          }
          val = TOP().v;
          POP();
          SET_VALUE(val,C,C);
        } FAIL()
        break;
      case '}':
        if( TOP().st == C ) {
          struct ajj_value val;
          assert(TOP().v.type == AJJ_VALUE_DICT);
          if( !INOBJECT() ) {
            report_error(a,pos,src,"Unknown token here:\"}\"");
            goto fail;
          }
          val = TOP().v;
          POP();
          SET_VALUE(val,C,C);
        } FAIL()
        break;
      case ',':
        if( TOP().st == C ) {
          if( INARRAY() ) {
            TOP().st = V;
          } else {
            assert( INOBJECT() );
            TOP().st = K;
          }
        } FAIL()
        break;
      case '-':
      case '0': case '1':case '2':case '3':case '4':
      case '5': case '6':case '7':case '8':case '9':
        if( TOP().st == V ) {
          char* pend;
          double val;
          struct ajj_value num;
          errno = 0;
          val  = strtod(src+pos,&pend);
          if( errno ) {
            report_error(a,pos,src,"Cannot parse numbers!");
            goto fail;
          }
          num = ajj_value_number(val);
          SET_VALUE(num,C,E);
          pos = pend-src - 1;
        } FAIL()
        break;
      case '\"':
        if( TOP().st == V || TOP().st == K ) {
          int i = pos+1;
          int c;
          strbuf_reset(&(TOP().k));
          for( ; (c=src[i]) ; ++i ) {
            if( c == '\\' ) {
              if( src[i+1] == '\"' ) {
                ++i; c = src[i];
              }
            } else if( c == '\"' ) {
              break;
            }
            strbuf_push(&(TOP().k),c);
          }
          if( !c ) {
            /* We run out the string but we don't see any
             * EOS indicator */
            report_error(a,pos,src,"String literal not closed properly!");
            goto fail;
          }
          if( TOP().st == V ) {
            struct ajj_value str;
            struct string vstr = strbuf_move(&(TOP().k));
            str = ajj_value_assign( ajj_object_create_string(a,scp,
                  vstr.str,vstr.len,1));
            SET_VALUE(str,C,C);
          } else {
            assert( INOBJECT() );
            TOP().st = E;
          }
        } FAIL()
        break;
      case 't':
        if( src[pos+1] == 'r' && src[pos+2] == 'u' &&
            src[pos+3] == 'e' ) {
          if( TOP().st == V ) {
            SET_VALUE(AJJ_TRUE,C,C);
          } FAIL()
        } else {
          report_error(a,pos,src,"Unknown literal here!");
          goto fail;
        }
        pos += 3;
        break;
      case 'f':
        if( src[pos+1] == 'a' && src[pos+2] == 'l' &&
            src[pos+3] == 's' && src[pos+4] == 'e' ) {
          if( TOP().st == V ) {
            SET_VALUE(AJJ_FALSE,C,C);
          } FAIL()
        } else {
          report_error(a,pos,src,"Unknown literal here!");
          goto fail;
        }
        pos += 4;
        break;
      case 'n':
        if( src[pos+1] == 'u' && src[pos+2] == 'l' &&
            src[pos+3] == 'l' ) {
          if( TOP().st == V ) {
            SET_VALUE(AJJ_NONE,C,C);
          } FAIL()
        } else {
          report_error(a,pos,src,"Unknown literal here!");
          goto fail;
        }
        pos += 3;
        break;
      default:
        report_error(a,pos,src,"Unknown literal here!");
        goto fail;
    }
    ++pos;
  } while(1);

done:
  if( stk[1].st == O ) {
    string_destroy(&(stk[1].k));
  }
  *output = stk[1].v;
  return 0;
fail:
  /* Delete all the intermeidate results */
  for( i = 1 ; i < p ; ++i ) {
    if( stk[i].st == O ) {
      string_destroy(&(stk[i].k));
    }
    if( stk[i].v.type != AJJ_VALUE_NOT_USE ) {
      ajj_value_destroy(a,&stk[i].v);
    }
  }
  return -1;
}

