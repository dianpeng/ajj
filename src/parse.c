#include "ajj-priv.h"
#include "builtin.h"
#include "util.h"
#include "parse.h"
#include "lex.h"
#include "bc.h"

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* used to do tokenizer forwarding for all the none TK_VARIABLE */
#define EXPECT(TK) \
  do { \
    if( p->tk.tk != (TK) ) { \
      parser_rpt_err(p,"Unexpected token :%s expect :%s", \
          tk_get_name(p->tk.tk), \
          tk_get_name((TK))); \
      return -1; \
    } \
  } while(0)

/* used to do tokenizer forwarding for all TK_VARIABLE */
#define EXPECT_VARIABLE() \
  do { \
    if( tk_expect_id(&(p->tk)) ) { \
      parser_rpt_err(p,"Unexpected token :%s expect :%s", \
          tk_get_name(p->tk.tk), \
          tk_get_name(TK_VARIABLE)); \
      return -1; \
    } \
  } while(0)

#define CONSUME(TK) \
  do { \
    EXPECT(TK); \
    tk_move(&(p->tk)); \
  } while(0)

#define CALLE(P) \
  do { \
    if( (P) ) { \
      return -1; \
    } \
  } while(0)

#define ENTER_SCOPE() \
  do { \
    if( lex_scope_top(p)->in_loop ) { \
      lex_scope_top(p)->lctrl->cur_enter++; \
    } \
    EMIT0(em,VM_ENTER); \
  } while(0)


#define EMIT0(em,BC) emitter_emit0(em,p->tk.pos,BC)
#define EMIT1(em,BC,A1) emitter_emit1(em,p->tk.pos,BC,A1)
#define EMIT2(em,BC,A1,A2) emitter_emit2(em,p->tk.pos,BC,A1,A2)
#define EMIT0_AT(em,P,BC) emitter_emit0_at(em,P,p->tk.pos,BC)
#define EMIT1_AT(em,P,BC,A1) emitter_emit1_at(em,P,p->tk.pos,BC,A1)
#define EMIT2_AT(em,P,BC,A1,A2) emitter_emit2_at(em,P,p->tk.pos,BC,A1,A2)
#define EMIT_PUT(em,T) emitter_put(em,T)

struct loop_ctrl {
  int cur_enter; /* Count for current accumulated enter
                  * instructions. If means , if we have to shfit
                  * control flow across the scope, then we need
                  * to account for this count as well */
  int stk_pos;   /* Used to record the exact stack position when
                  * the loop is entered. It is used to generate
                  * POP code when break and continue happens */
  struct {
    int code_pos;
    int enter_cnt;
  } brks[ MAX_LOOP_CTRL_SIZE ];
  size_t brks_len;

  struct {
    int code_pos;
    int enter_cnt;
  } conts[ MAX_LOOP_CTRL_SIZE ];
  size_t conts_len;
};

struct lex_scope {
  struct lex_scope* parent; /* parent scope */
  struct {
    struct {
      char str[ AJJ_SYMBOL_NAME_MAX_SIZE ];
      size_t len;
    } name;
    int idx;
  } lsys[ AJJ_LOCAL_CONSTANT_SIZE ];
  size_t len;
  int end   ; /* Maximum stack offset taken by this scope.
               * This value must be derived from its nested
               * scope serves as the base index to store local
               * variables */

  unsigned short in_loop;  /* If it is in a loop */
  unsigned short is_loop;  /* If this scope is a loop */
  struct loop_ctrl* lctrl; /* Loop control if this scope is a loop
                            * scope */
};

#define MAX_NESTED_FUNCTION_DEFINE 128

struct parser {
  struct tokenizer tk;
  const char* src_key; /* source file key */
  struct ajj* a;
  struct ajj_object* tpl;
  unsigned short unname_cnt;

  /* lexical scope stack. Since our parse is able to parse
   * multiple functions at same time and save its parsing
   * context. We maintain a stack of lex_scope objects during
   * parsing phase.
   * Also the first element in this array is a dummy element
   * and serves as a placeholder. */
  struct lex_scope* cur_scp[ MAX_NESTED_FUNCTION_DEFINE ];
  size_t scp_tp;
  int extends; /* Do we have seen extends instructions ?
                * If extends is seen, then from current parsing,
                * the parser will enter into a extension mode,
                * which means everything, except BLOCK is not
                * allowed and also BLOCK is not automatically
                * called */
  struct gc_scope* root_gc; /* root gc */
};

static
void parser_init( struct parser* p , const char* src_key,
    const char* src, struct ajj* a,
    struct ajj_object* tp , struct gc_scope* scp ) {
  tk_init(&(p->tk),src);
  p->a = a;
  p->src_key = src_key;
  p->tpl = tp;
  p->unname_cnt = 0;
  memset(p->cur_scp,0,sizeof(p->cur_scp));
  p->scp_tp = 0;
  p->extends= 0;
  p->root_gc = scp;
  assert(tp->scp);
}

static
void parser_destroy( struct parser* p ) {
  size_t i;
  for( i = p->scp_tp ; i >0 ; --i ) {
    struct lex_scope* scp = p->cur_scp[i];
    struct lex_scope* n;
    while(scp) {
      n = scp->parent;
      free(scp);
      scp = n;
    }
  }
  tk_destroy(&(p->tk));
}

#define lex_scope_top(P) ((P)->cur_scp[(P)->scp_tp])
#define is_in_main(P) ((P)->scp_tp == 1)

static
void parser_rpt_err( struct parser* p , const char* format, ... ) {
  va_list vl;
  int len;
  size_t ln,pos;
  char cs[32];
  struct tokenizer* tk = &(p->tk);

  /* get the position and line number of tokenizer */
  tk_get_coordinate(tk->src,tk->pos,&ln,&pos);

  tk_get_current_code_snippet(tk,cs,32);

  /* output the prefix message */
  len = snprintf(p->a->err,ERROR_BUFFER_SIZE,
      "[Parser:(%s:" SIZEF "," SIZEF ")] at:(... %s ...)\nMessage:",
      p->src_key,SIZEP(ln),SIZEP(pos),cs);

  assert( len >0 && len < ERROR_BUFFER_SIZE );

  /* output the rest messge even it is truncated */
  va_start(vl,format);
  vsnprintf(p->a->err+len,ERROR_BUFFER_SIZE-len, format,vl);
}

/* Lexical scope operation for parsing */
static
struct lex_scope* lex_scope_enter( struct parser* p , int is_loop ) {
  struct lex_scope* scp;
  scp = malloc(sizeof(*scp));
  scp->parent = lex_scope_top(p);
  scp->end = lex_scope_top(p)->end;
  scp->len = 0;
  if( is_loop ) {
    /* This scope IS a loop scope */
    scp->in_loop = 1;
    scp->lctrl = malloc(sizeof(struct loop_ctrl));
    scp->lctrl->brks_len = 0;
    scp->lctrl->conts_len= 0;
    scp->lctrl->cur_enter = 0;
    scp->lctrl->stk_pos = -1;
    scp->is_loop = 1;
  } else {
    scp->in_loop = lex_scope_top(p)->in_loop;
    scp->lctrl = lex_scope_top(p)->lctrl;
    scp->is_loop = 0;
  }
  return (lex_scope_top(p) = scp);
}

static
struct lex_scope* lex_scope_jump( struct parser* p ) {
  if( p->scp_tp == MAX_NESTED_FUNCTION_DEFINE ) {
    parser_rpt_err(p,"Too much nested functoin definition!");
    return NULL;
  } else {
    struct lex_scope* scp;
    scp = malloc(sizeof(*scp));
    ++p->scp_tp;
    lex_scope_top(p) = scp;
    scp->parent = NULL;
    scp->len = 0;
    scp->end = 0;
    scp->in_loop = 0;
    scp->is_loop = 0;
    scp->lctrl = NULL;
    return lex_scope_top(p);
  }
}

static
struct lex_scope* lex_scope_exit( struct parser*  p ) {
  struct lex_scope* scp;
  assert( lex_scope_top(p) != NULL );
  /* move the current ptop to its parent */
  scp = lex_scope_top(p)->parent;
  if( lex_scope_top(p)->is_loop ) {
    free(lex_scope_top(p)->lctrl);
  } else if( lex_scope_top(p)->in_loop ) {
    --lex_scope_top(p)->lctrl->cur_enter;
  }
  free( lex_scope_top(p) );
  if( scp == NULL ) {
    lex_scope_top(p) = NULL; /* don't forget to set it to NULL */
    /* exit the function scope */
    assert( p->scp_tp != 0 );
    --p->scp_tp;
  } else {
    lex_scope_top(p) = scp;
  }
  return lex_scope_top(p);
}
/* This function is not an actual set but a set if not existed.
 * Because most of the local symbol definition has such semantic.
 * This function returns -2 represent error,
 * returns -1 represent a new symbol is set up
 * and non-negative number represents the name is found */
static
int lex_scope_set( struct parser* p , const struct string* name ) {
  size_t i;
  struct lex_scope* scp = lex_scope_top(p);

  /* Try to find symbol:name on local scope */
  for( i = 0 ; i < scp->len ; ++i ) {
    if( string_cmpcl(name,scp->lsys[i].name.str,
          scp->lsys[i].name.len ) == 0 ) {
      /* Find one, why we need to define it */
      return scp->lsys[i].idx;
    }
  }

  if( scp->len == AJJ_LOCAL_CONSTANT_SIZE ) {
    parser_rpt_err(p,"Too much local constant in a scope!More than :%d",
        AJJ_LOCAL_CONSTANT_SIZE);
    return -2; /* We cannot set any more constant */
  }
  assert( name->len < AJJ_SYMBOL_NAME_MAX_SIZE );
  string_cpy(scp->lsys[scp->len].name.str,name);
  scp->lsys[scp->len].name.len = name->len;
  scp->lsys[scp->len].idx = scp->end++;
  ++scp->len;
  return -1; /* Indicate we have a new one */
}

static
int lex_scope_get_from_scope( struct lex_scope* cur,
    const struct string* name , int* lvl ) {
  int l = 0;
  assert( cur ) ;
  assert( name->len < AJJ_LOCAL_CONSTANT_SIZE );
  do {
    size_t i;
    for( i = 0 ; i < cur->len ; ++i ) {
      if( string_cmpcl(name,cur->lsys[i].name.str,
            cur->lsys[i].name.len) == 0 ) {
        if( lvl ) *lvl = l;
        return cur->lsys[i].idx;
      }
    }
    cur = cur->parent;
    ++l;
  } while(cur);
  return -1;
}

#define lex_scope_get(P,N,LVL) \
  lex_scope_get_from_scope(lex_scope_top(P),N,LVL)

static
struct string
random_name( struct parser* p , char l ) {
  char name[1024];
#ifndef NDEBUG
  int result;
#endif
  if(p->unname_cnt == USHRT_MAX) {
    parser_rpt_err(p,"Too much scope/blocks,more than:%d!",USHRT_MAX);
    return NULL_STRING;
  }
#ifndef NDEBUG
  result =
#endif
    sprintf(name,"@%c%d",l,p->unname_cnt);
  assert(result < AJJ_SYMBOL_NAME_MAX_SIZE );
  ++p->unname_cnt;
  return string_dupc(name);
}
/* Parser helpers */
static
int finish_scope_tag( struct parser* p, int token  ) {
  CONSUME(TK_LSTMT);
  CONSUME(token);
  CONSUME(TK_RSTMT);
  return 0;
}

static
int symbol( struct parser* p , struct string* output ) {
  struct tokenizer* tk = &(p->tk);
  assert(tk->tk == TK_VARIABLE);
  if( tk->lexeme.len >= AJJ_SYMBOL_NAME_MAX_SIZE ) {
    parser_rpt_err(p,"Local symbol is too long ,longer than:%d!",
        AJJ_SYMBOL_NAME_MAX_SIZE);
    return -1;
  } else {
    strbuf_move(&(tk->lexeme),output);
    return 0;
  }
}

/* Expr ============================== */
static
int parse_expr( struct parser* , struct emitter* );

/* List literal.
 * List literal are like this [ expr , expr , expr ].
 * The problem is how to generate code that create the list for you, to do this
 * we will use instruction VM_ATTR_PUSH */
static
int parse_seq( struct parser* p , struct emitter* em , int rtk ) {
  struct tokenizer* tk = &(p->tk);
  /* check if it is an empty list */
  if( tk->tk == rtk ) {
    tk_move(tk);
    return 0;
  }
  do {
    CALLE(parse_expr(p,em));
    EMIT0(em,VM_ATTR_PUSH);
    if( tk->tk == TK_COMMA ) {
      tk_move(tk);
      continue;
    } else if( tk->tk == rtk ) {
      tk_move(tk);
      break;
    } else {
      parser_rpt_err(p,"Unknown token:%s,expect \",\" or \"%s\"!",
          tk_get_name(tk->tk),
          tk_get_name(rtk));
      return -1;
    }
  } while(1);
  return 0;
}

static
int parse_list( struct parser* p , struct emitter* em ) {
  struct tokenizer* tk = &(p->tk);
  assert(tk->tk == TK_LSQR);
  tk_move(tk);
  EMIT0(em,VM_LLIST); /* load an empty list onto the stack */
  return parse_seq(p,em,TK_RSQR);
}

/* Tuple is ambigious by its self !
 * varible1 = (1+2);
 * Whether we need to treat the (1+2) as an singleton tuple or an expression with
 * parentheses , we don't know. Python indicates this by forcing you uses a comma
 * appending after the each element which makes it context free. We need to parse
 * this sort of thing when we see a leading parenthenses ! Take an extra step to
 * look ahead */
enum {
  TUPLE,
  EXPR
};

static
int parse_tuple_or_subexpr( struct parser* p , struct emitter* em , int* tp ) {
  /* We cannot look back and I don't want to patch the code as well.
   * So we have to spend an extra instruction on top of it in case
   * it is an tuple */
  int instr = EMIT_PUT(em,0);
  struct tokenizer* tk = &(p->tk);

  assert(tk->tk == TK_LPAR);
  tk_move(tk);

  if( tk->tk == TK_RPAR ) {
    /* empty tuple/list */
    tk_move(tk);
    EMIT0_AT(em,instr,VM_LLIST);
    *tp = TUPLE;
    return 0;
  }

  /* Parsing the expression here at first */
  CALLE(parse_expr(p,em));

  /* Checking if we have pending comma or not */
  if( tk->tk == TK_COMMA ) {
    tk_move(tk);
    /* this is an tuple here */
    EMIT0_AT(em,instr,VM_LLIST);
    EMIT0(em,VM_ATTR_PUSH);
    *tp = TUPLE;
    return parse_seq(p,em,TK_RPAR);
  } else {
    CONSUME(TK_RPAR);
    /* Emit a NOP initially since we don't have a real tuple */
    EMIT0_AT(em,instr,VM_NOP0);
    *tp = EXPR;
    return 0;
  }
}

static
int parse_dict( struct parser* p , struct emitter* em ) {
  struct tokenizer* tk = &(p->tk);

  assert(tk->tk == TK_LBRA);
  tk_move(tk);

  /* load an empty dictionary on the stack */
  EMIT0(em,VM_LDICT);

  if( tk->tk == TK_RBRA ) {
    tk_move(tk);
    return 0;
  }

  do {
    /* get the key */
    CALLE(parse_expr(p,em));
    /* eat the colon */
    CONSUME(TK_COLON);
    /* get value */
    CALLE(parse_expr(p,em));
    /* generate code push them into the dict */
    EMIT0(em,VM_ATTR_SET);

    if(tk->tk == TK_COMMA) {
      tk_move(tk);
      continue;
    } else if( tk->tk == TK_RBRA ) {
      tk_move(tk);
      break;
    } else {
      parser_rpt_err(p,"Unknown token:%s,expect \",\" or \"}\"!",
          tk_get_name(tk->tk));
      return -1;
    }
  } while(1);
  return 0;
}

static
int parse_var_prefix( struct parser* p , struct emitter* em ,
    struct string* var ) {
  int idx;
  /* Check whether the variable is a local variable or
   * at least could be */
  if((idx=lex_scope_get(p,var,NULL))<0) {
    /* Not a local variable, must be an upvalue */
    int const_idx;
    const_idx=program_const_str(em->prg,var,1);
    /* Now emit a UPVALUE instructions */
    EMIT1(em,VM_UPVALUE_GET,const_idx);
  } else {
    /* Local variables: just load the onto top of the stack */
    EMIT1(em,VM_BPUSH,idx);
    /* Don't forget to release the var's buffer */
    string_destroy(var);
  }
  return 0;
}

/* Function call in AJJ
 * For our underlying object model, we don't support function as first class
 * element. In our model, functions are functions, they are not value type so
 * they cannot be captured on top of the stack. This simplifies our underlying
 * object model design, but it makes calling a function becomes kind of verbose.
 * Bascially, user could use 2 syntax to call a function ,but they generate
 * different VM codes.
 *
 * 1. Call a global functions :
 *
 * func1(1,b,2) or pipe : a | func2(b,c)
 * This sort of calling function will result in stack layout as follow :
 * [ par1, par2 , .... ] , and the calling VM instructaion will be
 * VM_CALL, which takes 2 parameters, one as an index to constant table for function
 * name and the other as the function parameter count. VM_CALL will try to resolve
 * this symbol at runtime by looking at the current * template object's and its
 * inheritance chain ; if not found, then go to global environment to resolve it.
 *
 * 2. Call a object's method :
 *
 * If user want to call a function for a specific object, then he/she could write
 * code like: my_object.func1(1,2,3).
 * This calling syntax will generate VM codes entirely different with the global one.
 * Because fun1 is actually a method for object my_object, not a global function, so
 * it will result in following stack layout :
 * [ par1, par2, par3, par4, OBJECT ].
 * And the result instruction will be VM_ATTR_CALL, which still takes 2 parameters,
 * one is the name index and the other is the parameters conut.
 *
 */

static
int parse_invoke_par( struct parser* p , struct emitter* em ) {
  struct tokenizer* tk = &(p->tk);
  int num = 0;
  /* method call */
  assert(tk->tk == TK_LPAR);
  tk_move(tk);

  if( tk->tk == TK_RPAR ) {
    tk_move(tk);
    return 0;
  }

  do {
    CALLE(parse_expr(p,em));
    ++num;
    if( num > AJJ_FUNC_ARG_MAX_SIZE ) {
      /* do a check here ? */
      parser_rpt_err(p,"Function passing to many parameters!"
          "At most %d is allowed!",AJJ_FUNC_ARG_MAX_SIZE);
      return -1;
    }
    if( tk->tk == TK_COMMA ) {
      tk_move(tk);
    } else {
      if( tk->tk == TK_RPAR ) {
        tk_move(tk);
        break;
      } else {
        parser_rpt_err(p,"Function/Method call must be closed"
              " by \")\"!");
        return -1;
      }
    }
  } while(1);
  return num;
}

static
int parse_attr_or_methodcall( struct parser* p, struct emitter* em ,
    struct string* prefix /* owned */) {
  struct tokenizer* tk = &(p->tk);
  int comp_tk = tk->tk;
  assert( tk->tk == TK_LSQR || tk->tk == TK_DOT );

  if( !string_null(prefix) ) {
    /* We have a symbol name as prefix, we need to load the object
     * whose name is this prefix on to the stack first and then parse
     * the expression and then retrieve the attributes */
    CALLE(parse_var_prefix(p,em,prefix));
  }

  /* Move tokenizer */
  tk_move(tk);

  if( comp_tk == TK_LSQR ) {
    CALLE(parse_expr(p,em));
    EMIT0(em,VM_ATTR_GET);
    CONSUME(TK_RSQR);
  } else {
    struct string comp;
    int idx;

    EXPECT_VARIABLE();
    CALLE(symbol(p,&comp));
    idx=program_const_str(em->prg,&comp,1);
    tk_move(tk);
    /* Until now, we still don't know we are calling a member function or
     * reference an object on to the stack.We need to lookahead one more
     * token to decide */
    if( tk->tk == TK_LPAR ) {
      int num;
      CALLE((num = parse_invoke_par(p,em))<0);
      EMIT2(em,VM_ATTR_CALL,idx,num);
    } else {
      EMIT1(em,VM_LSTR,idx);
      EMIT0(em,VM_ATTR_GET); /* look up the attributes */
    }
  }

  return 0;
}

/* Emit function call
 * The calling convention for a function is as follow:
 * 1. Since each function is on different memory page, the VM will save
 *    2 main registers , EBP (base pointer for local variables) and ESP
 *    ( end of the stack ). No need to push PC onto stack.
 * 2. After jumping to a new function, EBP points to the start of stack
 *    of target callee. The first element on this stack is function par
 *    numbers. Then follows corresponding number of calling parameters.
 */

static
int parse_funccall_or_pipe( struct parser* p , struct emitter* em ,
    struct string* prefix , int pipe ) {
  int idx;
  int num ;
  assert(p->tk.tk == TK_LPAR);

  idx=program_const_str(em->prg,prefix,1);
  CALLE((num=parse_invoke_par(p,em))<0); /* generate call parameter for function */
  num += pipe;
  EMIT2(em,VM_CALL,idx,num); /* call the function based on current object */
  return 0;
}

/* This function handles the situation that the function is a pipe command shortcut */
static
int parse_pipecmd( struct parser* p , struct emitter* em ,
    struct string* cmd ) {
  int idx;
  idx=program_const_str(em->prg,cmd,1);
  EMIT2(em,VM_CALL,idx,1);
  return 0;
}

static
int parse_attr( struct parser* p, struct emitter* em ,
    struct string* prefix ) {
  struct tokenizer* tk = &(p->tk);

  if( tk->tk == TK_LPAR ) {
    CALLE(parse_funccall_or_pipe(p,em,prefix,0));
  } else {
    if( tk->tk == TK_DOT || tk->tk == TK_LSQR ) {
      CALLE(parse_attr_or_methodcall(p,em,prefix));
    } else {
      /* Just a variable name */
      if( !string_null(prefix) )
        CALLE(parse_var_prefix(p,em,prefix));
    }
  }
  do {
    if( tk->tk == TK_DOT || tk->tk == TK_LSQR ) {
      CALLE(parse_attr_or_methodcall(p,em,&NULL_STRING));
    } else {
      break;
    }
  } while(1);
  return 0;
}

/* Parse the is / is not prefixed expression */
static
int parse_is( struct parser* p , struct emitter* em ) {
  int num = 1;
  int fn_idx;
  struct tokenizer* tk;
  struct string fn;
  int op;

  tk = &(p->tk);
  assert(tk->tk == TK_IS || tk->tk == TK_ISN );

  if(tk->tk == TK_IS)
    op = VM_TEST;
  else
    op = VM_TESTN;

  tk_move(tk);

  /* Here our tokenizer will not treate True/true False/false None/none
   * as valid variable name since this will make other parsing part
   * ambigious. But we really need to treat these value as variable
   * which enable user to do test like :
   * a is True ; b is False ; c is None. Which may not be really useful,
   * just in case people loves this style. */
  if(tk_expect_id(tk)) {
    /* try to check whether we have True,False or None */
    switch(tk->tk) {
      case TK_TRUE:
        fn = string_dup(&TRUE_STRING);
        break;
      case TK_FALSE:
        fn = string_dup(&FALSE_STRING);
        break;
      case TK_NONE:
        fn = string_dup(&NONE_STRING);
        break;
      default:
        parser_rpt_err(p,"Unexpected token here:%s, expect:Variable/"
            "False/True/None!",
            tk_get_name(tk->tk));
        return -1;
    }
  } else {
    /* an oridinary variable name */
    CALLE(symbol(p,&fn)); /* get the test name */
  }
  fn_idx = program_const_str(em->prg,&fn,1);
  tk_move(tk);
  if( tk->tk == TK_LPAR ) {
    num += parse_invoke_par(p,em);
  }
  EMIT2(em,op,fn_idx,num);
  return 0;
}

static
int parse_test( struct parser* p , struct emitter* em ) {
  CALLE(parse_is(p,em));
  do {
    switch(p->tk.tk) {
      case TK_IS:
      case TK_ISN:
        CALLE(parse_is(p,em));
        break;
      default:
        return 0;
    }
  } while(1);
  return 0;
}

static
int parse_atomic( struct parser* p, struct emitter* em ) {
  struct tokenizer* tk = &(p->tk);
  int idx;
  struct string str;
  int tp;

  tk_expect_id(tk); /* rewrite the tokenizer to get more
                     * keyword candidate */
  switch(tk->tk) {
    case TK_VARIABLE:
      CALLE(symbol(p,&str)); /* get the symbol name */
      tk_move(tk); /* move the token */
      CALLE(parse_attr(p,em,&str));
      break;
    case TK_STRING:
      strbuf_move(&(tk->lexeme),&str);
      idx=program_const_str(em->prg,&str,1);
      EMIT1(em,VM_LSTR,idx);
      tk_move(tk);
      break;
    case TK_TRUE:
      EMIT0(em,VM_LTRUE);
      tk_move(tk);
      break;
    case TK_FALSE:
      EMIT0(em,VM_LFALSE);
      tk_move(tk);
      break;
    case TK_NONE:
      EMIT0(em,VM_LNONE);
      tk_move(tk);
      break;
    case TK_NUMBER:
      idx=program_const_num(em->prg,tk->num_lexeme);
      EMIT1(em,VM_LNUM,idx);
      tk_move(tk);
      break;
    case TK_LPAR:
      CALLE(parse_tuple_or_subexpr(p,em,&tp));
      if(tp == TUPLE)
        CALLE(parse_attr(p,em,&NULL_STRING));
      break;
    case TK_LSQR:
      CALLE(parse_list(p,em));
      CALLE(parse_attr(p,em,&NULL_STRING));
      break;
    case TK_LBRA:
      CALLE(parse_dict(p,em));
      break;
    default:
      parser_rpt_err(p,"Unexpected token here:%s!",
          tk_get_name(tk->tk));
      return -1;
  }
  if( tk->tk == TK_IS || tk->tk == TK_ISN )
    CALLE(parse_test(p,em));
  return 0;
}

static
int parse_unary( struct parser* p, struct emitter* em ) {
  unsigned char op[1024]; /* At most we support 1024 unary operators */
  int op_len = 0;
  int i;
  struct tokenizer* tk = &(p->tk);

#define PUSH_OP(T) \
  do { \
    if( op_len == 1024 ) { \
      parser_rpt_err(p,"Too much unary operators, more than 1024!"); \
      return -1; \
    } \
    op[op_len++] = (T); \
  } while(0)

  do {
    switch(tk->tk) {
      case TK_NOT: PUSH_OP(VM_NOT); break;
      case TK_SUB: PUSH_OP(VM_NEG); break;
      case TK_LEN: PUSH_OP(VM_LEN); break;
      case TK_ADD: break;
      default:
        goto done;
    }
    tk_move(tk);
  } while(1);

done:
  /* start to parse the thing here */
  CALLE(parse_atomic(p,em));
  for( i = 0 ; i < op_len ; ++i ) {
    EMIT0(em,op[i]);
  }
  return 0;
}
#undef PUSH_OP

/* The binary operation parsing is not traditional table lookup
 * way with its precendence but handle unrolled function. The
 * reason is because we don't have AST so I assume it would be hard
 * to make all the semantic action has similar syntax. So I unroll
 * the loop in each sub function and call it manually .. :) */
static
int parse_factor( struct parser* p, struct emitter* em ) {
  struct tokenizer* tk = &(p->tk);
  CALLE(parse_unary(p,em));
  do {
    int op;
    switch(tk->tk) {
      case TK_MUL: op = VM_MUL; break;
      case TK_DIV: op = VM_DIV; break;
      case TK_DIVTRUCT: op = VM_DIVTRUCT; break;
      case TK_MOD: op = VM_MOD; break;
      case TK_POW: op = VM_POW; break;
        break;
      default:
        goto done;
    }
    tk_move(tk);
    CALLE(parse_unary(p,em));
    EMIT0(em,op);
  } while(1);

done: /* finish */
  return 0;
}

static
int parse_term( struct parser* p, struct emitter* em ) {
  struct tokenizer* tk = &(p->tk);
  CALLE(parse_factor(p,em));
  do {
    int op;
    switch(tk->tk) {
      case TK_ADD: op = VM_ADD; break;
      case TK_SUB: op = VM_SUB; break;
      case TK_IN: op = VM_IN; break;
      case TK_NIN: op = VM_NIN; break;
      case TK_CAT: op = VM_CAT; break;
      default: goto done;
    }
    tk_move(tk);
    CALLE(parse_factor(p,em));
    EMIT0(em,op);
  } while(1);
done:
  return 0;
}

static
int parse_cmp( struct parser* p, struct emitter* em ) {
  struct tokenizer* tk = &(p->tk);
  CALLE(parse_term(p,em));
  do {
    int op;
    switch(tk->tk) {
      case TK_EQ: op = VM_EQ; break;
      case TK_NE: op = VM_NE; break;
      case TK_LT: op = VM_LT; break;
      case TK_LE: op = VM_LE; break;
      case TK_GT: op = VM_GT; break;
      case TK_GE: op = VM_GE; break;
        op = tk->tk;
        break;
      default:
        goto done;
    }
    tk_move(tk);
    CALLE(parse_term(p,em));
    EMIT0(em,op);
  } while(1);
done:
  return 0;
}

static
int parse_logic( struct parser* p, struct emitter* em ) {
  int jmp_tb[1024];
  int bc_tb[1024];
  int sz = 0;
  int i;
  int pos;
  struct tokenizer* tk = &(p->tk);

#define PUSH_JMP(X) \
  do { \
    if( sz == 1024 ) { \
      parser_rpt_err(p,"Too much logic component,you have more than 1024!"); \
      return -1; \
    } \
    jmp_tb[sz] = EMIT_PUT(em,1); \
    bc_tb[sz] = (X); \
    ++sz; \
  } while(0)

  CALLE(parse_cmp(p,em));
  do {
    switch(tk->tk) {
      case TK_AND: PUSH_JMP(VM_JLF); break;
      case TK_OR:  PUSH_JMP(VM_JLT); break;
      default:
        if( sz != 0 ) {
          /* for the last value, we just booleanize
           * it to true and false */
          EMIT0(em,VM_BOOL);
        }
        goto done;
    }
    tk_move(tk);
    CALLE(parse_cmp(p,em));
  } while(1);
done:
  pos = emitter_label(em);
  for( i = 0 ; i < sz ; ++i ) {
    EMIT1_AT(em,jmp_tb[i],bc_tb[i],pos);
  }
  return 0;
}
#undef PUSH_JMP

static
int parse_pipe( struct parser* p , struct emitter* em ) {
  struct tokenizer* tk = &(p->tk);
  struct string prefix;
  assert( tk->tk == TK_PIPE );
  tk_move(tk);
  EXPECT_VARIABLE();
  CALLE(symbol(p,&prefix));
  tk_move(tk);
  if( tk->tk == TK_LPAR ) {
    CALLE(parse_funccall_or_pipe(p,em,&prefix,1));
  } else if( tk->tk != TK_DOT && tk->tk != TK_LSQR ) {
    CALLE(parse_pipecmd(p,em,&prefix));
  } else {
    parser_rpt_err(p,"Cannot pipe to a method call or object call "
        "syntax. Pipe can only work with free function syntax!");
    return -1;
  }
  return 0;
}

static
int parse_pipe_list( struct parser* p , struct emitter* em ) {
  CALLE(parse_logic(p,em));
  while(p->tk.tk == TK_PIPE)
    CALLE(parse_pipe(p,em));
  return 0;
}

/* Expression.
 * We support value1 if cond else value2 format.
 * The code generation is not simple at all, however. Since the condition is not
 * the first component, the parser needs to look ahead unlimited tokens to say
 * whether this is a ternary format or not. We don't have AST, this becomes very
 * hard. So how to generate the code ?
 * We inject a jump point before the expression. We turn this instruction into
 * NOP when it is not ternary. If it is, this instruction will just jump to the
 * condition part. Then at the end of this expression, we have another jump to
 * skip the alternative value path. Condition part will follow by a ternary byte
 * code which simply jumps to corresponding position based on the evaluation.
 * The NOP is removed during the peephole optimization phase
 */
static
int parse_expr( struct parser* p, struct emitter* em ) {
  int fjmp = EMIT_PUT(em,1);
  int val1_l = emitter_label(em);
  struct tokenizer* tk = &(p->tk);
  CALLE(parse_pipe_list(p,em));

  /* Check if we have a pending if or not */
  if( tk->tk == TK_IF ) {
    int cond_l, end_l;
    int val1_jmp;
    tk_move(tk);

    /* An unconditional jump to skip the alternative path and condition */
    val1_jmp = EMIT_PUT(em,1);

    /* Parse the condition part */
    cond_l = emitter_label(em);
    CALLE(parse_pipe_list(p,em));
    EMIT1(em,VM_JT,val1_l); /* jump to value1 when condition is true */

    CONSUME(TK_ELSE);

    /* Parse the alternative value */
    CALLE(parse_pipe_list(p,em));
    end_l = emitter_label(em);

    /* Patch the jump */
    EMIT1_AT(em,fjmp,VM_JMP,cond_l);
    EMIT1_AT(em,val1_jmp,VM_JMP,end_l);
  } else {
    /* nothing serious, just issue a NOP */
    EMIT1_AT(em,fjmp,VM_NOP1,0);
  }
  return 0;
}

/* ============================== ConstExpression =======================
 * We need const expression evaluator for default value. Pay attention, we
 * only support const expression for default arguments for functions. During
 * the parsing, the value's memory are owned by the global scope. They are
 * not gonna be GC through scope mechanism by default */

static int
parse_constexpr( struct parser* p , struct ajj_value* output );

static int
parse_constseq( struct parser* p , int ltk , int rtk ,
    struct ajj_value* output ) {
  struct tokenizer* tk = &(p->tk);
  struct ajj_object* l;

  assert(tk->tk == ltk);
  tk_move(tk);

  /* set the output to a list */
  l = ajj_object_create_list(p->a,p->root_gc);
  *output = ajj_value_assign(l);

  if( tk->tk == rtk ) {
    tk_move(tk);
    return 0;
  }

  do {
    struct ajj_value val;
    CALLE(parse_constexpr(p,&val));
    builtin_list_push(p->a,l,&val);
    if( tk->tk == TK_COMMA ) {
      tk_move(tk);
      continue;
    } else if( tk->tk == rtk ) {
      tk_move(tk);
      break;
    } else {
      parser_rpt_err(p,"constexpr: unexpected token here:%s "
          "expect \",\" or \"%s\"",
          tk_get_name(ltk),
          tk_get_name(rtk));
      return -1;
    }
  } while(1);

  return 0;
}

#define parse_constlist(P,OUTPUT) parse_constseq(P,TK_LSQR,TK_RSQR,OUTPUT)
#define parse_consttuple(P,OUTPUT) parse_constseq(P,TK_LPAR,TK_RPAR,OUTPUT)

static int
parse_constdict( struct parser* p , struct ajj_value* output ) {
  struct tokenizer* tk = &(p->tk);
  struct ajj_object* d;
  assert( tk->tk == TK_LBRA );
  tk_move(tk);

  d = ajj_object_create_dict(p->a,p->root_gc);
  *output = ajj_value_assign(d);

  if( tk->tk == TK_RBRA ) {
    tk_move(tk);
    return 0;
  }

  do {
    struct ajj_value key , val;
    CALLE(parse_constexpr(p,&key));

    /* check if key is string */
    if( key.type != AJJ_VALUE_STRING ) {
      parser_rpt_err(p,"constexpr: dictionary key must be string!");
      return -1;
    }

    if( tk->tk == TK_COLON ) {
      tk_move(tk);
    } else {
      parser_rpt_err(p,
          "constexpr: unexpected token:%s, expect \":\" or \"}\"",
          tk_get_name(tk->tk));
      return -1;
    }
    CALLE(parse_constexpr(p,&val));

    /* insert the key into the list */
    builtin_dict_insert(p->a,d,&key,&val);

    /* delete a string promptly */
    ajj_value_delete_string(p->a,&key);

    if( tk->tk == TK_COMMA ) {
      tk_move(tk);
      continue;
    } else if( tk->tk == TK_RBRA ) {
      tk_move(tk);
      break;
    } else {
      parser_rpt_err(p,"constexpr: unexpected token:%s,expect \",\" or \"}\"",
          tk_get_name(tk->tk));
      return -1;
    }

  } while(1);

  return 0;
}

static int
parse_constexpr( struct parser* p , struct ajj_value* output ) {
  int neg = 0;
  struct tokenizer* tk = &(p->tk);

  if( tk->tk == TK_SUB ) {
    neg = 1; /* this is the only allowed unary operator */
    tk_move(tk);
  }

  switch(tk->tk) {
    case TK_NUMBER:
      *output = ajj_value_number( neg ? -tk->num_lexeme : tk->num_lexeme);
      break;
    case TK_STRING:
      if( neg ) {
        parser_rpt_err(p,"constexpr: string literal cannot work with unary "
            "operator \"-\"!");
        return -1;
      } else {
        *output = ajj_value_assign(
            ajj_object_create_string(p->a,p->root_gc,
              tk->lexeme.str,tk->lexeme.len,0));
      }
      break;
    case TK_NONE:
      if( neg ) {
        parser_rpt_err(p,"constexpr: none cannot work with unary "
            "operator \"-\"!");
        return -1;
      } else {
        *output = AJJ_NONE;
      }
      break;
    case TK_TRUE:
      if( neg ) {
        *output = AJJ_FALSE;
      } else {
        *output = AJJ_TRUE;
      }
      break;
    case TK_FALSE:
      if( neg ) {
        *output = AJJ_TRUE;
      } else {
        *output = AJJ_FALSE;
      }
      break;
    case TK_LSQR:
      if( neg ) {
        parser_rpt_err(p,"constexpr: list literal cannot work with unary "
            "operator \"-\"!");
        return -1;
      }
      return parse_constlist(p,output);
    case TK_LPAR:
      if( neg ) {
        parser_rpt_err(p,"constexpr: tuple literal cannot work with unary "
            "operator \"-\"!");
        return -1;
      }
      return parse_consttuple(p,output);
    case TK_LBRA:
      if( neg ) {
        parser_rpt_err(p,"constexpr: dictionary literal cannot work with unary "
            "operator \"-\"!");
        return -1;
      }
      return parse_constdict(p,output);
    default:
      parser_rpt_err(p,"constexpr: unknown token:%s!",
          tk_get_name(tk->tk));
      return -1;
  }
  tk_move(tk);
  return 0;
}

/* ================================== STRUCTURE =================================== */
/* enter_scope indicates whether this function will enter a new lexical_scope
 * or not */
static
int parse_scope( struct parser* , struct emitter* em ,
    int enter_scope , int emit_gc , int pop_num );

static
int parse_print( struct parser* p, struct emitter* em ) {
  struct tokenizer* tk = &(p->tk);
  assert(tk->tk == TK_LEXP);
  tk_move(tk);
  if( tk->tk == TK_REXP ) {
    tk_move(tk);
  } else {
    CALLE(parse_expr(p,em));
    EMIT0(em,VM_PRINT);
    CONSUME(TK_REXP);
  }
  return 0;
}

static
int parse_branch ( struct parser* p, struct emitter* em ) {
  int cond_jmp = -1;
  int end_jmp [ 1024 ];
  int jmp_sz = 0;
  int i;
  int has_else = 0;
  struct tokenizer* tk = &(p->tk);

#define PUSH_JMP(P) \
  do { \
    if( jmp_sz == 1024 ) { \
      parser_rpt_err(p,"Too much if-elif-else branch, more than 1024!"); \
      return -1; \
    } \
    end_jmp[jmp_sz++] = (P); \
  } while(0)

  assert( tk->tk == TK_IF );
  tk_move(tk);
  CALLE(parse_expr(p,em));
  /* condition failed jump */
  cond_jmp = EMIT_PUT(em,1);

  CONSUME(TK_RSTMT);

  CALLE(parse_scope(p,em,1,1,0));
  /* jump out of the scope */
  PUSH_JMP(EMIT_PUT(em,1));
  do {
    switch(tk->tk) {
      case TK_ENDIF:
        { /* end of the if scope */
          if( cond_jmp > 0 )
            EMIT1_AT(em,cond_jmp,VM_JF,emitter_label(em));
          tk_move(tk);
          CONSUME(TK_RSTMT);
          goto done;
        }
      case TK_ELIF:
        { /* elif scope */
          if( has_else ) {
            parser_rpt_err(p,"Expect endfor since else tag is seen before!");
            return -1;
          }
          tk_move(tk);
          assert(cond_jmp > 0 );
          /* modify the previous conditional jmp */
          EMIT1_AT(em,cond_jmp,VM_JF,emitter_label(em));
          CALLE(parse_expr(p,em));

          cond_jmp = EMIT_PUT(em,1);
          CONSUME(TK_RSTMT);
          break;
        }
      case TK_ELSE:
        { /* else scope */
          tk_move(tk);
          assert( cond_jmp > 0 );
          EMIT1_AT(em,cond_jmp,VM_JF,emitter_label(em));
          cond_jmp = -1;
          CONSUME(TK_RSTMT);
          has_else = 1;
          break;
        }
      default:
        parser_rpt_err(p,"Unexpected token in branch scope:%s",
            tk_get_name(tk->tk));
        return -1;
    }
    CALLE(parse_scope(p,em,1,1,0));
    PUSH_JMP(EMIT_PUT(em,1));
  } while(1);
done:
  assert(jmp_sz >= 1);
  /* patch all rest of the cool things. We DON"T patch the last
   * PUSH_JMP since the following block is naturally followed by
   * this branch , no jmp really needed */
  for( i = 0 ; i < jmp_sz-1 ; ++i ) {
    EMIT1_AT(em,end_jmp[i],VM_JMP,emitter_label(em));
  }
  /* patch the last one as a NOP1 */
  EMIT1_AT(em,end_jmp[jmp_sz-1],VM_NOP1,0);
  return 0;
}
#undef PUSH_JMP

/* Function
 * What function ? Jinja doesn't have functions !
 * Finally each jinja building block needs to be mapped to some existed
 * or well definied unit that can be executed. A functions here is the
 * basic building block for:
 * 1) Macros
 * 2) Blocks
 * 3) For loop
 * 4) Main function
 * It forms the basic way for executing those jinja template codes.
 *
 * When we enter into the function, the value stack should already
 * have some value there. EBP register points to position that is
 * owned by us, but there're values there already.
 * EBP --> parameter_num
 *           par1
 *           par2
 *           ...
 * ESP -->  <end-of-stack>
 * However, those value has been named by local variables.
 * NOTES: parse_func_prolog is always not generating ANY scope based
 * instructions. These instructions should be generated by caller
 * function.
 */

static
int parse_func_prolog( struct parser* p , struct emitter* em ) {
  size_t i;
  for( i = 0 ; i < em->prg->par_size ; ++i ) {
    /* Generate rest of named parameters */
    CALLE(lex_scope_set(p,&(em->prg->par_list[i].name))==-2);
  }
  return 0;
}

/* This function is used to allocate the builtin variable for each
 * function. The builtin variables for each function has 4, they are:
 * 1. __argnum__: number of parameters passed into this function.
 * 2. __func__  : name of the function
 * 3. vargs     : a list of parameters that is not marked for function.
 * 4. caller    : the caller's function name
 * These 4 arguments will be placed right after the last argument passed
 * by the caller so it is before every arguments defined inside of the
 * function. The order do matter */

static
int alloc_func_builtin_var( struct parser* p ) {
  CALLE(lex_scope_set(p,&ARGNUM)==-2);
  CALLE(lex_scope_set(p,&FUNC)==-2);
  CALLE(lex_scope_set(p,&VARGS)==-2);
  CALLE(lex_scope_set(p,&CALLER)==-2);
  CALLE(lex_scope_set(p,&SELF)==-2);
  return 0;
}

static int
parse_func_body( struct parser* p , struct emitter* em ) {
#ifndef NDEBUG
  struct lex_scope* scp = lex_scope_top(p);
#endif

  CALLE(lex_scope_jump(p) == NULL);
  assert( lex_scope_top(p)->parent == NULL );
  assert( lex_scope_top(p)->len == 0 );
  assert( lex_scope_top(p)->end == 0 );

  CALLE(parse_func_prolog(p,em)); /* Parsing the prolog */
  CALLE(alloc_func_builtin_var(p)); /* builtin vars */

  /* start to parse the function body ,which is just
   * another small code scope */
  CALLE(parse_scope(p,em,1,1,0));

  /* Generate return instructions */
  EMIT0(em,VM_RET);

  /* Notes, after calling this function, the tokenizer should still
   * have tokens related to end of the callin scope */
  lex_scope_exit(p);
  assert( lex_scope_top(p) == scp ); /* Check */
  return 0;
}

/* Parse function declaration, although we only used it in MACRO right
 * now. But it may be extended to be used some other places as well */
static int
parse_func_prototype( struct parser* p , struct program* prg ) {
  struct tokenizer* tk = &(p->tk);

  assert( tk->tk == TK_LPAR );
  tk_move(tk);
  if( tk->tk == TK_RPAR ) {
    tk_move(tk);
    return 0;
  } else {
    int see_def = 0;
    do {
      struct string par_name;
      struct ajj_value def_val = AJJ_NONE;
      /* Building prototype of function */
      EXPECT_VARIABLE();
      CALLE(symbol(p,&par_name));
      tk_move(tk);

      /* If we have already seen def , all the value after the first
       * default value MUST be companied with default value */
      if( see_def ) {
        if(tk->tk != TK_ASSIGN) {
          parser_rpt_err(p,"Expect a default value for this argument," \
              "this argument is the argument after the first argument that "\
              "has a default value,so it must have a default value!");
          return -1;
        }
      }
      /* Check if we need to parse the default const expression value */
      if( tk->tk == TK_ASSIGN ) {
        /* Turn of see_def */
        see_def = 1;
        /* Eat the assignment */
        tk_move(tk);
        /* We have a default value , parse it through constexpr */
        CALLE(parse_constexpr(p,&def_val));
      }
      /* Add it into the function prototype */
      CALLE(program_add_par(prg,&par_name,1,&def_val));
      /* Check if we can exit or continue */
      if( tk->tk == TK_COMMA ) {
        tk_move(tk);
        continue;
      } else if( tk->tk == TK_RPAR ) {
        tk_move(tk);
        break;
      }
    } while(1);
    return 0;
  }
}

static int
parse_macro( struct parser* p , struct emitter* em ) {
  struct emitter new_em;
  struct program* new_prg;
  struct string name;
  struct tokenizer* tk = &(p->tk);

  UNUSE_ARG(em); /* Make caller easy to coordinate */

  assert(tk->tk == TK_MACRO);
  tk_move(tk);
  /* We need the name of the macro then we can get a new
   * program objects */
  EXPECT_VARIABLE();
  CALLE(symbol(p,&name));
  assert(p->tpl->val.obj.fn_tb);
  /* Add a new function into the table */
  new_prg = func_table_add_jj_macro(
      p->tpl->val.obj.fn_tb,&name,1);
  tk_move(tk); /* eat function name */
  emitter_init(&new_em,new_prg);

  /* Parsing the prototype */
  CALLE(parse_func_prototype(p,new_prg));
  CONSUME(TK_RSTMT);

  /* Parsing the rest of the body */
  CALLE(parse_func_body(p,&new_em));
  CONSUME(TK_ENDMACRO);
  CONSUME(TK_RSTMT);
  return 0;
}

/* Block inside of Jinja is still an functions, the difference is that a
 * block will be compiled into a ZERO-parameter functions and automatically
 * gets called when we are not have extends instructions */
static int
parse_block( struct parser* p , struct emitter* em ) {
  struct string name;
  struct program* new_prg;
  struct emitter new_em;
  struct tokenizer* tk = &(p->tk);
  int num = 0;

  assert(tk->tk == TK_BLOCK);
  tk_move(tk);
  EXPECT_VARIABLE();
  CALLE(symbol(p,&name));
  tk_move(tk);

  if( tk->tk != TK_RSTMT && !(p->extends) ) {
    EXPECT(TK_LPAR);
    num = parse_invoke_par(p,em);
  }
  CONSUME(TK_RSTMT);

  if( !(p->extends) ) {
    int idx;
    idx=program_const_str(em->prg,&name,0);
    EMIT2(em,VM_BCALL,idx,num);
    EMIT1(em,VM_POP,1); /* pop the return value */
  }
  assert(p->tpl->val.obj.fn_tb);
  new_prg = func_table_add_jj_block(
      p->tpl->val.obj.fn_tb,&name,1);
  emitter_init(&new_em,new_prg);

  /* Parsing the functions */
  CALLE(parse_func_body(p,&new_em));
  /* Parsing the rest of the body */
  CONSUME(TK_ENDBLOCK);
  CONSUME(TK_RSTMT);
  return 0;
}

/* Call block.
 * Each call block will be compiled into a anonymous macro. The id
 * of this anonymous macro is passed by setting up the __caller__
 * upvalue and then invoke the corresponding macro. Inside of the
 * macro , it will call this function by using caller directory */
static int
parse_call( struct parser* p , struct emitter* em ) {
  struct tokenizer* tk = &(p->tk);
  struct program* new_prg;
  struct emitter new_em;
  struct string name = random_name(p,'c');
  int name_idx;
  int caller_idx;
  struct string macro_name;

  assert(tk->tk == TK_CALL);
  tk_move(tk);

  /* compile the parse_call as an anonymous macro */
  new_prg = func_table_add_jj_macro(
      p->tpl->val.obj.fn_tb,&name,1);
  emitter_init(&new_em,new_prg);
  if( tk->tk == TK_LPAR ) {
    CALLE(parse_func_prototype(p,new_prg)); /* parse the prototype */
  }

  /* now generate the code for setting it up as a upvalue */
  name_idx = program_const_str(em->prg,&name,0);
  caller_idx = program_const_str(em->prg,&CALLER_STUB,0);
  EMIT1(em,VM_LSTR,name_idx); /* load the name onto stack */
  EMIT1(em,VM_UPVALUE_SET,caller_idx); /* set the value into caller_idx */

  /* generate call stub here  */
  EXPECT_VARIABLE();
  strbuf_move(&(tk->lexeme),&macro_name);
  tk_move(tk);
  EXPECT(TK_LPAR); /* must be a function call */
  CALLE(parse_funccall_or_pipe(p,em,&macro_name,0)); /* generate call */
  CONSUME(TK_RSTMT); /* eat the %} */
  /* generate pop to pop out the return value */
  EMIT1(em,VM_POP,1);

  /* now parse the function body */
  CALLE(parse_func_body(p,&new_em));
  CONSUME(TK_ENDCALL);
  CONSUME(TK_RSTMT);

  /* remove upvalue */
  EMIT1(em,VM_UPVALUE_DEL,caller_idx);
  return 0;
}

/* for loop
 * We don't support RECURSIVE, this is anti-human. I heavily doubt its
 * usability and it adds too much complexity for my implementation. I
 * used to support it by entirely compile the loop body as a internal
 * function and call it in C code, but it won't work since the lexical
 * scope is entirely messed up and we don't support upvalue/closure.
 * For simplicity, we don't support recursive anymore. This makes our
 * life way easier and make loop body straitforward. If user wants to
 * have recursive loop ( I doubt since it is not intuitive ), he/she
 * can just use built-in function to FLATTEN the list/dictionary */

#define parse_loop_cond(P,EM) parse_pipe_list(P,EM)

/* This function compiles the for body into the closure. What is for
 * body:
 * {% for symbolname in expr (filter) %}
 * We will compile the filter + body of for loop as a closure function.
 * This function will have one parameter which is same as symbolname.
 * Optionally, for accept a comma separated key,value name as this :
 * {% for key,value in map (filter) %}
 */

static int parse_for_body( struct parser* p ,
    struct emitter* em ,
    struct string* key,
    struct string* val ) {
  int else_jmp;
  int loop_jmp;
  int filter_jmp = -1;
  int loop_body_jmp = -1;
  int brk_jmp = -1;

  int loop_cond_pos;

#ifndef NDEBUG
  int local_idx;
#endif

  int deref_tp;
  size_t i;
  struct string obj_name;
  struct string itr_name;
  struct lex_scope* scp;

  obj_name = random_name(p,'i');
  itr_name = random_name(p,'i');

  /* before parsing the loop condition , we push an empty slot
   * here on the stack named loop for the VM to initialize the
   * loop objects */
  EMIT0(em,VM_LNONE); /* loop object */
  CALLE(parse_loop_cond(p,em)); /* map/list object */

  CALLE((scp=lex_scope_enter(p,1))==NULL);

  /* for loop object */
#ifndef NDEBUG
  CALLE((local_idx = lex_scope_set(p,&LOOP)) == -2);
  assert(local_idx == -1);
#else
  CALLE(lex_scope_set(p,&LOOP) == -2);
#endif

  /* for object itself */
#ifndef NDEBUG
  CALLE((local_idx=lex_scope_set(p,&obj_name))==-2);
  assert(local_idx == -1);
#else
  CALLE(lex_scope_set(p,&LOOP) == -2);
#endif

  /* for iterator */
#ifndef NDEBUG
  CALLE((local_idx=lex_scope_set(p,&itr_name))==-2);
  assert(local_idx == -1);
#else
  CALLE(lex_scope_set(p,&LOOP) == -2);
#endif

  string_destroy(&obj_name); /* object name */
  string_destroy(&itr_name); /* iterator name */

  /* Adding empty test. We always perform a proactive
   * empty test in case we have a else branch code body.
   * And it doesn't hurt us too much also avoid us to
   * call iter_start which is costy than simple test whether
   * it is empty or not */
  EMIT1(em,VM_TPUSH,1);
  else_jmp = EMIT_PUT(em,1);

  /* Enter into the scope of loop body */
  ENTER_SCOPE();

  EMIT0(em,VM_ITER_START); /* start the iterator */

  /* Now the top 3 stack elements are
   * 1. iterator
   * 2. object
   * 3. loop object */
  loop_cond_pos = emitter_label(em);
  EMIT0(em,VM_ITER_HAS);
  loop_jmp = EMIT_PUT(em,1); /* loop jump */

  /* Dereferencing the key and value */
  deref_tp = -1;
  if( !string_null(key) && !string_null(val) ) {
    deref_tp = ITERATOR_KEYVAL;
    if( lex_scope_set(p,key) != -1 ) {
      parser_rpt_err(p,"Cannot set up local variable:%s for loop!",
          key->str);
      string_destroy(key);
      string_destroy(val);
      return -1;
    }
    if( lex_scope_set(p,val) != -1 ) {
      parser_rpt_err(p,"Cannot set up local variable:%s for loop!",
          val->str);
      string_destroy(key);
      string_destroy(val);
      return -1;
    }
  } else if( !string_null(val) ) {
    deref_tp = ITERATOR_VAL;
    if( lex_scope_set(p,val) != -1 ) {
      parser_rpt_err(p,"Cannot set up local variable:%s for loop!",
          val->str);
      string_destroy(val);
      return -1;
    }
  } else if( !string_null(key) ) {
    deref_tp = ITERATOR_KEY;
    if( lex_scope_set(p,key) != -1 ) {
      parser_rpt_err(p,"Cannot set up local variable:%s for loop!",
          val->str);
      string_destroy(key);
      return -1;
    }
  }

  string_destroy(val);
  string_destroy(key);

  if( deref_tp != -1 ) {
    EMIT1(em,VM_ITER_DEREF,deref_tp);
  }

  /* Now check if we have a filter or not */
  if( p->tk.tk == TK_IF ) {
    /* got a filter */
    tk_move(&(p->tk));
    /* parse the filter onto the stack */
    CALLE(parse_expr(p,em));
    /* now check whether filter is correct or not */
    filter_jmp = EMIT_PUT(em,1);
  }
  CONSUME(TK_RSTMT);

  /* change stack pointer which is used for break/continue */
  scp->lctrl->stk_pos = scp->end;
  /* parse the scope */
  CALLE(parse_scope(p,em,0,0,0));

  assert( lex_scope_top(p)->is_loop && lex_scope_top(p)->in_loop );

  /* patch the continue jump table here */
  for( i = 0 ; i < lex_scope_top(p)->lctrl->conts_len ; ++i ) {
    EMIT2_AT(em,lex_scope_top(p)->lctrl->conts[i].code_pos,
        VM_JMPC,
        lex_scope_top(p)->lctrl->conts[i].enter_cnt,
        emitter_label(em));
  }

  /* filter jumps to here */
  if( filter_jmp >0 )
    EMIT1_AT(em,filter_jmp,VM_JF,emitter_label(em));

  /* generate code for poping the dereferenced value */
  if(deref_tp!=-1) {
    if(deref_tp == ITERATOR_KEYVAL) {
      EMIT1(em,VM_POP,2);
    } else {
      EMIT1(em,VM_POP,1);
    }
  }

  /* move the iterator */
  EMIT0(em,VM_ITER_MOVE);

  /* jump to the condition check code */
  EMIT1(em,VM_JMP,loop_cond_pos);

  /* here is the jump position that is corresponding to
   * loop condition test failure */
  EMIT1_AT(em,loop_jmp,VM_JF,
      emitter_label(em)); /* patch the jmp */

  /* patch the break jump table here */
  if( lex_scope_top(p)->lctrl->brks_len ) {
    /* generate a jump here to make sure normal execution flow
     * will skip the following ONE pop instruction */
    brk_jmp = EMIT_PUT(em,1);
  }

  for( i = 0 ; i < lex_scope_top(p)->lctrl->brks_len ; ++i ) {
    EMIT2_AT(em,lex_scope_top(p)->lctrl->brks[i].code_pos,
        VM_JMPC,
        lex_scope_top(p)->lctrl->brks[i].enter_cnt,
        emitter_label(em));
  }

  if( i ) {
    /* generate pop instruction for dereferenced value */
    if(deref_tp!=-1) {
      if(deref_tp == ITERATOR_KEYVAL) {
        EMIT1(em,VM_POP,2);
      } else {
        EMIT1(em,VM_POP,1);
      }
    }

    assert( brk_jmp >0 );
    EMIT1_AT(em,brk_jmp,VM_JMP,emitter_label(em)); /* patch the jump to skip
                                                    * the POP instruction above */
  }

  /* pop the iterator on top of the stack */
  EMIT1(em,VM_POP,1);

  /* emit the body exit */
  EMIT0(em,VM_EXIT);

  /* Exit the lexical scope */
  lex_scope_exit(p);

  /* Check if we have an else branch */
  if( p->tk.tk == TK_ELSE ) {
    /* Set up the loop_body_jmp hooks */
    loop_body_jmp = EMIT_PUT(em,1);

    /* Patch the else_jmp */
    EMIT1_AT(em,else_jmp,VM_JEPT,emitter_label(em));

    tk_move(&(p->tk));
    CONSUME(TK_RSTMT);
    /* Parse the else scope */
    CALLE(parse_scope(p,em,1,1,0));
  } else {
    /* Patch the else_jmp */
    EMIT1_AT(em,else_jmp,VM_JEPT,emitter_label(em));
  }

  if( loop_body_jmp >= 0 ) {
    EMIT1_AT(em,loop_body_jmp,VM_JMP,emitter_label(em));
  }

  /* pop the map/list object and the loop object if we
   * have used on top of the stack */
  EMIT1(em,VM_POP,2);

  /* consume remained tokens */
  CONSUME(TK_ENDFOR);
  CONSUME(TK_RSTMT);
  return 0;
}

/* Parsing the for loop body.
 * This function act as a parse prolog by only parsing the symbol inside of
 * for loop : {% for symbol1,symbol2 in ...
 * Rest of the parse code is in parse_for_body. For symbol that has name "_"
 * we treat it is not used there. */
static
int parse_for( struct parser* p , struct emitter* em ) {
  struct string key = NULL_STRING;
  struct string val = NULL_STRING;
  struct tokenizer* tk = &(p->tk);

  assert(tk->tk == TK_FOR);
  tk_move(tk);
  /* We don't support tuple in our code. So we don't really have unpacked
   * semantic here. We support following semantic :
   * key
   * _,value
   * key,_
   * So at most 2 symbols can be provided here */
  EXPECT_VARIABLE();
  strbuf_move(&(tk->lexeme),&key);
  tk_move(tk);
  if( tk->tk == TK_COMMA ) {
    tk_move(tk);
    EXPECT_VARIABLE();
    strbuf_move(&(tk->lexeme),&val);
    tk_move(tk);
  } else {
    val = key;
    key = NULL_STRING;
  }
  CONSUME(TK_IN);
  /* Ignore underscore since they serves as placeholder */
  if( !string_null(&key) && string_cmpcl(&key,"_",1) == 0 ) {
    string_destroy(&key);
    key = NULL_STRING;
  }
  if( !string_null(&val) && string_cmpcl(&val,"_",1) == 0 ) {
    string_destroy(&val);
    val = NULL_STRING;
  }
  return parse_for_body(p,em,&key,&val);
}

/* Filter Scope */
static int
parse_filter( struct parser* p , struct emitter* em ) {
  struct string name;
  struct string text;
  int vm_lstr = -1;
  int text_idx;
  struct tokenizer* tk = &(p->tk);

  assert(tk->tk == TK_FILTER);
  tk_move(tk);
  CALLE(symbol(p,&name));
  tk_move(tk);
  vm_lstr = EMIT_PUT(em,1);

  if( tk->tk == TK_LPAR ) {
    CALLE(parse_funccall_or_pipe(p,em,&name,1));
  } else {
    CALLE(parse_pipecmd(p,em,&name));
  }
  CONSUME(TK_RSTMT);
  strbuf_move(&(tk->lexeme),&text);
  text_idx=program_const_str(em->prg,&text,1);
  EMIT1_AT(em,vm_lstr,VM_LSTR,text_idx);
  tk_move(tk);
  CALLE(finish_scope_tag(p,TK_ENDFILTER));
  return 0;
}

/* Set Scope */
static
int parse_assign( struct parser* p , struct emitter* em ,
    int idx ) {
  struct tokenizer* tk = &(p->tk);
  assert(tk->tk == TK_ASSIGN);
  tk_move(tk);
  CALLE(parse_expr(p,em));
  if( idx >= 0 ) {
    EMIT1(em,VM_STORE,idx);
  }
  return 0;
}

static
int parse_set( struct parser* p, struct emitter* em ) {
  int var_idx;
  struct tokenizer* tk = &(p->tk);
  struct string sym;

  assert( tk->tk == TK_SET );
  tk_move(tk);

  EXPECT_VARIABLE();
  sym = strbuf_tostring(&(tk->lexeme));
  CALLE((var_idx=lex_scope_set(p,&sym))==-2);

  tk_move(tk); /* Move forward */

  /* check if we have an pending expression or just end of the SET scope*/
  if( tk->tk == TK_RSTMT ) {
    int txt_idx;
    struct string str;
    /* end of the set scope , so it is a scop based set */
    tk_move(tk);
    EXPECT(TK_TEXT);
    strbuf_move(&(tk->lexeme),&str);
    txt_idx=program_const_str(em->prg,&str,1);
    tk_move(tk);
    /* load the text on to stack */
    EMIT1(em,VM_LSTR,txt_idx);
    /* we need to move this static text into the position of that
     * variable there */
    if( var_idx >= 0 ) {
      EMIT1(em,VM_STORE,var_idx);
    }
    /* set up the var based on the stack */
    CALLE(finish_scope_tag(p,TK_ENDSET));
    return 0;
  } else if( tk->tk == TK_ASSIGN ) {
    CALLE(parse_assign(p,em,var_idx));
    CONSUME(TK_RSTMT);
    return 0;
  } else {
    parser_rpt_err(p,"Set scope either uses \"=\" to indicate "
        "a one line assignment,"
        "or uses a scope based set!");
    return -1;
  }
}

/* Do */
static int
parse_do( struct parser* p , struct emitter* em ) {
    struct tokenizer* tk = &(p->tk);
  assert( tk->tk == TK_DO );
  tk_move(tk);
  CALLE(parse_expr(p,em));
  EMIT1(em,VM_POP,1);
  CONSUME(TK_RSTMT);
  return 0;
}

/* Move */
static int
parse_move( struct parser* p , struct emitter* em ) {
  /* Move is simple, just find 2 memory position and then move the value */
  int dst_idx;
  int src_idx;
  int dst_level;
  int src_level;
  struct string sym;

  struct tokenizer* tk = &(p->tk);

  assert(tk->tk == TK_MOVE);
  tk_move(tk);

  EXPECT_VARIABLE(); /* dest variable */

  sym = strbuf_tostring(&(tk->lexeme));
  if( (dst_idx=lex_scope_get(p,
          &sym,
          &dst_level))<0 ) {
    parser_rpt_err(p,"In move statement, the target variable:%s is not defined!",
        strbuf_tostring(&(tk->lexeme)).str);
    return -1;
  }

  tk_move(tk);
  CONSUME(TK_ASSIGN); /* consume the assign */
  EXPECT_VARIABLE(); /* target variable */

  sym = strbuf_tostring(&(tk->lexeme));
  if( (src_idx=lex_scope_get(p,
          &sym,
          &src_level))<0 ) {
    parser_rpt_err(p,"In move statement, the source variable:%s is not defined!",
        strbuf_tostring(&(tk->lexeme)).str);
    return -1;
  }
  tk_move(tk);
  if( src_level >= dst_level ) {
    EMIT2(em,VM_MOVE,dst_idx,src_idx);
  } else {
    /* Lift source value to outer scope */
    EMIT2(em,VM_LIFT,src_idx,dst_level-src_level);
    /* Move source value to target position */
    EMIT2(em,VM_MOVE,dst_idx,src_idx);
  }
  CONSUME(TK_RSTMT);

  return 0;
}

/* Loop control statements */
static int
parse_break( struct parser* p , struct emitter* em ) {
  struct tokenizer* tk = &(p->tk);
  int pos;
  struct lex_scope* lscp;
  assert(tk->tk == TK_BREAK);
  tk_move(tk);

  assert( lex_scope_top(p)->in_loop );
  if( lex_scope_top(p)->lctrl->brks_len == MAX_LOOP_CTRL_SIZE ) {
    parser_rpt_err(p,"Cannot have more break statements in this loop!");
    return -1;
  }
  /* Generate code for break instruction. Since when break happend,
   * the control flow will be changed, the stack status cannot be
   * maintained properly here. We need to generate POP code to make
   * stack consistent */
  lscp = lex_scope_top(p);
  assert(lscp->end >= lscp->lctrl->stk_pos);
  if(lscp->end > lscp->lctrl->stk_pos) {
    EMIT1(em,VM_POP,lscp->end - lscp->lctrl->stk_pos);
  }
  pos = lex_scope_top(p)->lctrl->brks_len;
  lex_scope_top(p)->lctrl->brks[pos].code_pos = EMIT_PUT(em,2);
  lex_scope_top(p)->lctrl->brks[pos].enter_cnt=
    lex_scope_top(p)->lctrl->cur_enter - 1;
  ++lex_scope_top(p)->lctrl->brks_len;
  CONSUME(TK_RSTMT);
  return 0;
}

static int
parse_continue( struct parser* p , struct emitter* em ) {
  struct tokenizer* tk = &(p->tk);
  int pos;
  struct lex_scope* lscp;
  assert(tk->tk == TK_CONTINUE);
  tk_move(tk);

  assert( lex_scope_top(p)->in_loop );
  if( lex_scope_top(p)->lctrl->conts_len == MAX_LOOP_CTRL_SIZE ) {
    parser_rpt_err(p,"Cannot have more continue statements in this loop!");
    return -1;
  }
  lscp = lex_scope_top(p);
  assert(lscp->end >= lscp->lctrl->stk_pos);
  if(lscp->end > lscp->lctrl->stk_pos) {
    EMIT1(em,VM_POP,lscp->end - lscp->lctrl->stk_pos);
  }
  pos = lex_scope_top(p)->lctrl->conts_len;
  lex_scope_top(p)->lctrl->conts[pos].code_pos = EMIT_PUT(em,2);
  lex_scope_top(p)->lctrl->conts[pos].enter_cnt=
    lex_scope_top(p)->lctrl->cur_enter - 1;
  ++lex_scope_top(p)->lctrl->conts_len;
  CONSUME(TK_RSTMT);
  return 0;
}

/* With statment.
 * For with statment, we will generate a lex_scope for it */
static int
parse_with( struct parser* p , struct emitter* em ) {
  struct tokenizer* tk = &(p->tk);
  assert(tk->tk == TK_WITH);
  tk_move(tk); tk_expect_id(tk);

  if( tk->tk == TK_VARIABLE ) {
    int idx;
    struct string sym;
    /* We have shortcut writing, so we need to generate a new lexical
     * scope */
    sym = strbuf_tostring(&(tk->lexeme));
    CALLE(lex_scope_enter(p,0) == NULL);
    CALLE((idx=lex_scope_set(p,&sym))==-2);
    tk_move(tk);
    ENTER_SCOPE(); /* enter the scope */
    EXPECT(TK_ASSIGN); /* check wether we have an assignment operator */
    CALLE(parse_assign(p,em,idx));
    CONSUME(TK_RSTMT);
    /* Now parsing the whole scope body */
    CALLE(parse_scope(p,em,0,0,1));
    EMIT0(em,VM_EXIT); /* exit the scope */
    CONSUME(TK_ENDWITH);
    CONSUME(TK_RSTMT);
    /* exit the lexical scope */
    lex_scope_exit(p);
  } else {
    CONSUME(TK_RSTMT);
    CALLE(parse_scope(p,em,1,1,0));
    CONSUME(TK_ENDWITH);
    CONSUME(TK_RSTMT);
  }
  return 0;
}

/* Return
 * return statements is used to return a value from the current execution
 * flow. It could be used in macro/block and main functions. It can optionally
 * accepts an expression */
static
int parse_return( struct parser* p ,struct emitter* em ) {
  struct tokenizer* tk = &(p->tk);
  assert(tk->tk == TK_RETURN);
  tk_move(tk);

  if(tk->tk == TK_RSTMT) {
    /* empty return statements */
    EMIT0(em,VM_LNONE); /* return None */
    EMIT0(em,VM_RET); /* return */
  } else {
    /* evaluate the expression */
    CALLE(parse_expr(p,em));
    EMIT0(em,VM_RET);
  }
  CONSUME(TK_RSTMT);
  return 0;
}

/* Context
 * Context is used to setup some upvalue while including/extending other templates.
 * This is useful when you want to customize the behavior of the engine. We support
 * context in include and extends statements by allowing user to set up a context
 * scope to setup the upvalue. The genenral grammar is like this:
 * {% set key=value (optional)/(override) %}
 * {% set key=value (optional)/(override) %}
 * ...
 * The context are pushed on to stack and the correpsonding include/extends instruction
 * should take care of these values */

static int
parse_context_body( struct parser* p , struct emitter* em ) {
  int cnt = 0; /* count of the context value */
  struct tokenizer* tk = &(p->tk);
  do {

    if( tk->tk == TK_TEXT )
      tk_move(tk); /* skip the text */

    CONSUME(TK_LSTMT);
    if( tk->tk == TK_SET ) {
      int var_idx;
      int opt = UPVALUE_OVERRIDE;
      struct string str;
      CONSUME(TK_SET);
      EXPECT_VARIABLE();
      /* this is a context statment */
      strbuf_move(&(tk->lexeme),&str);
      tk_move(tk); /* move the variable name */
      CONSUME(TK_ASSIGN); /* skip the assignment */
      var_idx = program_const_str(em->prg,&str,1);
      /* for each context value, 3 corresponding attributes will
       * be pushed onto the stack. They are:
       * 1. variable name index in constant string table
       * 2. value of this variable needs to be setup
       * 3. attributes for this value. */

      EMIT1(em,VM_LIMM,var_idx); /* symbol name index */
      CALLE(parse_expr(p,em));   /* value */
      if( tk->tk == TK_OPTIONAL ) {
        opt = UPVALUE_OPTIONAL;
        tk_move(tk);
      } else if( tk->tk == TK_OVERRIDE ) {
        tk_move(tk);
      }
      EMIT1(em,VM_LIMM,opt); /* attribute for this value */
      CONSUME(TK_RSTMT);  /* eat the %} */
      ++cnt;
    } else {
      /* skip the text */
      /* return from here when unknown token comes */
      return cnt;
    }
  } while(1);
  UNREACHABLE();
}

/* Include
 * Include allows user to include another rendered template. In jinja2, include
 * supports those specifier to add some context information. Here, we don't support
 * the concept in Jinaj2 , but we support something new here. We allow user to
 * customize the running environment and also the execution thing by using upvalue.
 * Upvalue is a kind of value that will be resolved delayed on the runtime. Anyone
 * could use upvalue to wrap a method call or other things. Include support using
 * upvalue setting locally _OR_ using json based the text to setup upvalue.
 * Basically, include supports 3 types of grammar as follow:
 * {% include template %}
 *
 * {% include template upvalue %}
 *   {% set name=value (optional)/(override) %}
 *   {% set name=value (optional)/(override) %}
 * {% endinclude %}
 *
 * {% include template json jsonfile %}
 *   {% set name=value (optional)/(override) %}
 * {% endinclude %}
 *
 */

static int
parse_include( struct parser* p , struct emitter* em ) {
  int cnt = 0;
  int opt = INCLUDE_UPVALUE;
  struct tokenizer* tk = &(p->tk);

  assert(tk->tk == TK_INCLUDE);
  tk_move(tk);
  CALLE(parse_expr(p,em));

  if( tk->tk == TK_RSTMT ) {
    /* line inclusion */
    tk_move(tk);
  } else if( tk->tk == TK_UPVALUE ) {
    tk_move(tk);
    CONSUME(TK_RSTMT);
    cnt = parse_context_body(p,em);
    CONSUME(TK_ENDINCLUDE);
    CONSUME(TK_RSTMT);
    opt = INCLUDE_UPVALUE;
  } else if( tk->tk == TK_JSON ) {
    tk_move(tk);
    /* json template name */
    CALLE(parse_expr(p,em));
    CONSUME(TK_RSTMT);
    cnt = parse_context_body(p,em);
    CONSUME(TK_ENDINCLUDE);
    CONSUME(TK_RSTMT);
    opt = INCLUDE_JSON;
  } else {
    parser_rpt_err(p,"Unexpected token here in include scope:%s!",
        tk_get_name(tk->tk));
    return -1;
  }
  EMIT2(em,VM_INCLUDE,opt,cnt);
  return 0;
}

/* Import
 * In jinja2, user could import specific name/macro and then alias it
 * into different other name. In our implementation, for simplicity,
 * we don't allow this feature. Everytime, if user trys to do importing,
 * every macro will be imported into the calling scope.
 *
 * Also , jinja2 forces the method with underscore prefix cannot be even
 * imported. We don't have such constraints since if you really don't want
 * that function just don't call it. This level of private is really kind
 * of confusing and useless.
 *
 * EG:
 * {% import template_path as object %}
 *
 * Import will be parsed as follow, the bottom 2 elements of the stack are
 * imported file path and also the name/symbol for the imported objects.
 */

static int
parse_import( struct parser* p , struct emitter* em ) {
  struct string name;
  int name_idx;
  struct tokenizer* tk = &(p->tk);

  assert(tk->tk == TK_IMPORT);
  tk_move(tk);

  /* Get the filepath as an expression */
  CALLE(parse_expr(p,em));
  CONSUME(TK_AS);

  /* Get the symbol name onto stack as a string literal */
  CALLE(symbol(p,&name));
  tk_move(tk);

  /* Get the index */
  name_idx=program_const_str(em->prg,&name,1);
  CONSUME(TK_RSTMT);

  /* emit the import instruction */
  EMIT1(em,VM_IMPORT,name_idx);
  return 0;
}

/* Inheritance */
static int
parse_extends( struct parser * p , struct emitter* em ) {
  struct tokenizer* tk = &(p->tk);
  assert(tk->tk == TK_EXTENDS);
  tk_move(tk);
  CALLE(parse_expr(p,em));
  EMIT0(em,VM_EXTENDS);
  CONSUME(TK_RSTMT);
  ++(p->extends);
  return 0;
}

/* ========================================================
 * Parser dispatch loop
 * ======================================================*/
/* non-loop body */
static int
parse_scope( struct parser* p , struct emitter* em ,
    int enter_scope ,
    int emit_gc ,
    int pop_num ) {
  struct tokenizer* tk = &(p->tk);
  int stk_start; /* record the stack position before compiling
                  * any code in this scope */

  /* only when we are in global scope and also we see extends
   * we switch ourself into a strict extends mode that refuses
   * to parse any statements in global scope except block */
  int only_extends = is_in_main(p) && (p->extends > 0);

  if( enter_scope )
    CALLE(lex_scope_enter(p,0) == NULL);

  stk_start = lex_scope_top(p)->end;

  if( emit_gc )
    ENTER_SCOPE();

#define HANDLE_CASE(T,t) \
    case TK_##T: \
      CALLE(parse_##t(p,em)); \
      break; \
    case TK_END##T: \
      goto done;

  do {
    if( tk->tk == TK_TEXT ) {
      if( only_extends ) {
        tk_move(tk); /* skip the text if we have seen extends */
      } else {
        /* just print the text out */
        struct string text;
        int text_id;
        strbuf_move(&(tk->lexeme),&text);
        text_id = program_const_str(em->prg,&text,1);
        EMIT1(em,VM_LSTR,text_id);
        EMIT0(em,VM_PRINT);
        tk_move(tk);
      }
    }

    if( tk->tk == TK_LSTMT ) {
      tk_move(tk);
      if( only_extends ) {
        /* We only allow block instructions in here */
        if( tk->tk == TK_BLOCK )
          CALLE(parse_block(p,em));
        else if( tk->tk == TK_ENDBLOCK )
          goto done;
        else
          goto fail;
      } else {
        switch(tk->tk) {
          HANDLE_CASE(IF,branch)
          HANDLE_CASE(FOR,for)
          HANDLE_CASE(SET,set)
          HANDLE_CASE(FILTER,filter)
          HANDLE_CASE(CALL,call)
          HANDLE_CASE(MACRO,macro)
          HANDLE_CASE(BLOCK,block)
          HANDLE_CASE(WITH,with)
          case TK_INCLUDE:
            if( is_in_main(p) )
              CALLE(parse_include(p,em));
            else
              goto fail;
            break;
          case TK_IMPORT:
            if( is_in_main(p) )
              CALLE(parse_import(p,em));
            else
              goto fail;
            break;
          case TK_EXTENDS:
            if( is_in_main(p) )
              CALLE(parse_extends(p,em));
            else
              goto fail;
            break;
          case TK_ELIF:
          case TK_ELSE:
            goto done;
          case TK_DO:
            CALLE(parse_do(p,em));
            break;
          case TK_MOVE:
            CALLE(parse_move(p,em));
            break;
          case TK_BREAK:
            if( lex_scope_top(p)->in_loop ) {
              CALLE(parse_break(p,em));
            } else {
              goto fail;
            }
            break;
          case TK_CONTINUE:
            if( lex_scope_top(p)->in_loop ) {
              CALLE(parse_continue(p,em));
            } else {
              goto fail;
            }
            break;
          case TK_RETURN:
            CALLE(parse_return(p,em));
            break;
          default:
            goto fail;
        }
      }
    } else if( tk->tk == TK_LEXP ) {
      CALLE(parse_print(p,em));
    } else {
      if( tk->tk != TK_EOF )
        goto fail;
      else
        break;
    }
  } while(1);

#undef HANDLE_CASE

done:
  assert( lex_scope_top(p)->end >= stk_start );
  pop_num += lex_scope_top(p)->end - stk_start;
  /* generate code for poping temporary value on stack */
  if(pop_num) {
    EMIT1(em,VM_POP,pop_num);
  }

  if( emit_gc )
    EMIT0(em,VM_EXIT);

  if( enter_scope ) {
    lex_scope_exit(p);
  }
  return 0;

fail:
  parser_rpt_err(p,"Unexpected token in scope:%s!",
      tk_get_name(tk->tk));
  return -1;
}

/* ========================================
 * Public Interface
 * ======================================*/
struct ajj_object*
parse( struct ajj* a, const char* key,
    const char* src, int own ) {
  struct ajj_object* tmpl;
  struct parser p;
  struct emitter em;
  struct program* prg;
  struct gc_scope temp_scp; /* temporary gc scope , which enable us
                             * to delete all the garbage if we parse
                             * failed */

  tmpl = ajj_new_template(a,key,src,own);
  assert(tmpl);

  /* init the temporary gc scope */
  gc_init_temp(&temp_scp,tmpl->scp);
  /* start parsing */
  parser_init(&p,key,src,a,tmpl,&temp_scp);
  /* enter the lexical scope for function */
  CHECK(lex_scope_jump(&p)!=NULL);
  /* STARTS for parsing main */
  CHECK((prg = func_table_add_jj_main(
          tmpl->val.obj.fn_tb,&MAIN,0)));
  /* initialize code emitter */
  emitter_init(&em,prg);
  /* reserve space for builtin values */
  alloc_func_builtin_var(&p);
  if(parse_scope(&p,&em,1,1,0)) {
    /* delete all the data in temporary gc scope */
    gc_scope_exit(a,&temp_scp);
    /* destroy the parser, it will delete all the stacked
     * lexical scope  as well */
    parser_destroy(&p);
    /* we can only delete *this* template */
    ajj_delete_template(a,key);
    return NULL;
  }
  /* EMIT a return instruction */
  emitter_emit0(&em,p.tk.pos,VM_RET);

  /* merge memory in temporary gc to its corresponding
   * gc scope */
  gc_scope_merge(tmpl->scp,&temp_scp);

  /* destroy the parser which will destroy all the lexical
   * scope it creates internally */
  parser_destroy(&p);
  return tmpl;
}
