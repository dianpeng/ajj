#include "ajj-priv.h"
#include "util.h"
#include "parse.h"
#include "lex.h"
#include "vm.h"

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define EXPECT(TK) \
  do { \
    if( p->tk->tk != (TK) ) { \
      report_error(p,"Unexpected token :%s expect :%s", \
          tk_get_name(p->tk->tk), \
          tk_get_name((TK))); \
      return -1; \
    } \
  } while(0)

#define CONSUME(TK) \
  do { \
    EXPECT(TK); \
    tk_move(p->tk); \
  } while(0)

#define CALLE(P) \
  do { \
    if( (P) ) { \
      return -1; \
    } \
  } while(0)

#define emit0(em,BC) emitter_emit0(em,p->tk->pos,BC)
#define emit1(em,BC,A1) emitter_emit1(em,p->tk->pos,BC,A1)
#define emit2(em,BC,A1,A2) emitter_emit2(em,p->tk->pos,BC,A1,A2)
#define emit0_at(em,P,BC) emitter_emit0_at(em,p->tk->pos,P,BC)
#define emit1_at(em,P,BC,A1) emitter_emit1_at(em,p->tk->pos,P,BC,A1)
#define emit2_at(em,P,BC,A1,A2) emitter_emit2_at(em,p->tk->pos,P,BC,A1,A2)

/* Lexical Scope
 * Lexical scope cannot cross function boundary. Once a new function
 * enters, a new set of lexical scope must be initialized */
#define MAX_BREAK_SIZE 16

struct loop_ctrl {
  int cur_enter; /* Count for current accumulated enter
                  * instructions. If means , if we have to shfit
                  * control flow across the scope, then we need
                  * to account for this count as well */
  struct {
    int code_pos;
    int enter_cnt;
  } brks[ MAX_BREAK_SIZE ];
  size_t len;
};

struct lex_scope {
  struct lex_scope* parent; /* parent scope */
  struct {
    char name[ AJJ_SYMBOL_NAME_MAX_SIZE ];
    int idx;
  } lsys[ AJJ_LOCAL_CONSTANT_SIZE ];
  size_t len;
  int end   ; /* Maximum stack offset taken by this scope.
               * This value must be derived from its nested
               * scope serves as the base index to store local
               * variables */

  int in_loop:1;  /* If it is in a loop */
  int is_loop:1;  /* If this scope is a loop */
  struct loop_ctrl* lctrl; /* Loop control if this scope is a loop
                            * scope */
};

#define MAX_NESTED_FUNCTION_DEFINE 128

struct parser {
  struct tokenizer* tk;
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
};

#define PTOP() (p->cur_scp[p->scp_tp])

static
int is_in_main( struct parser* p ) {
  return p->scp_tp == 0;
}

static
int lex_scope_top_index( struct parser* P ) {
  assert( PTOP()->end != 0 );
  return PTOP()->end - 1;
}

static
void report_error( struct parser* p , const char* format, ... ) {
  va_list vl;
  int len;
  int pos,ln;
  size_t i;
  char cs[CODE_SNIPPET_SIZE];
  struct tokenizer* tk = p->tk;

  /* get the position and line number of tokenizer */
  pos = ln = 1;
  for( i = 0 ; i < tk->pos && tk->src[i] ; ++i ) {
    if( tk->src[i] == '\n' ) {
      ++ln;pos =0;
    } else {
      ++pos;
    }
  }

  tk_get_current_code_snippet(tk,cs);

  /* output the prefix message */
  len = snprintf(p->a->err,1024,
      "[Parser:(%d,%d)] at:%s!\nMessage:",pos,ln,cs);
  assert( len >0 && len < 1024 );

  /* output the rest messge even it is truncated */
  va_start(vl,format);
  len = snprintf(p->a->err+len-1,
      ERROR_BUFFER_SIZE+1-len, format,vl);
}


/* Lexical scope operation for parsing */
static
struct lex_scope* lex_scope_enter( struct parser* p , int is_loop ) {
  struct lex_scope* scp;
  scp = malloc(sizeof(*scp));
  scp->parent = PTOP();
  scp->end = PTOP()->end;
  scp->len = 0;
  if( is_loop ) {
    /* This scope IS a loop scope */
    scp->in_loop = 1;
    scp->lctrl = malloc(sizeof(struct loop_ctrl));
    scp->lctrl->len = 0;
    scp->lctrl->cur_enter = 0;
    scp->is_loop = 1;
  } else {
    scp->in_loop = PTOP()->in_loop;
    scp->lctrl = PTOP()->lctrl;
    scp->is_loop = 0;
  }
  return (PTOP() = scp);
}

static
struct lex_scope* lex_scope_jump( struct parser* p ) {
  struct lex_scope* scp;
  scp = malloc(sizeof(*scp));
  if( p->scp_tp == MAX_NESTED_FUNCTION_DEFINE ) {
    report_error(p,"Too much nested functoin definition!");
    return NULL;
  } else {
    ++p->scp_tp;
    PTOP() = scp;
    scp->parent = NULL;
    scp->len = 0;
    scp->end = 0;
    scp->in_loop = 0;
    scp->is_loop = 0;
    scp->lctrl = NULL;
    return PTOP();
  }
}

static
struct lex_scope* lex_scope_exit( struct parser*  p ) {
  struct lex_scope* scp;
  assert( PTOP() != NULL );
  /* move the current ptop to its parent */
  scp = PTOP()->parent;
  if( PTOP()->is_loop ) {
    free(PTOP()->lctrl);
  }
  free( PTOP() );
  if( scp == NULL ) {
    PTOP() = NULL; /* don't forget to set it to NULL */
    /* exit the function scope */
    assert( p->scp_tp != 0 );
    --p->scp_tp;
  } else {
    PTOP() = scp;
  }
  return PTOP();
}

#define ENTER_SCOPE() \
  do { \
    if( PTOP()->in_loop ) { \
      PTOP()->lctrl->cur_enter++; \
    } \
    emit0(em,VM_ENTER); \
  } while(0)

/* This function is not an actual set but a set if not existed.
 * Because most of the local symbol definition has such semantic.
 * This function returns -2 represent error,
 * returns -1 represent a new symbol is set up
 * and non-negative number represents the name is found */
static
int lex_scope_set( struct parser* p , const char* name ) {
  int i;
  struct lex_scope* scp = PTOP();

  /* Try to find symbol:name on local scope */
  for( i = 0 ; i < scp->len ; ++i ) {
    if( strcmp(scp->lsys[i].name,name) == 0 ) {
      /* Find one, why we need to define it */
      return scp->lsys[i].idx;
    }
  }

  if( scp->len == AJJ_LOCAL_CONSTANT_SIZE ) {
    report_error(p,"Too much local constant in a scope!More than :%d",
        AJJ_LOCAL_CONSTANT_SIZE);
    return -2; /* We cannot set any more constant */
  }
  assert( strlen(name) < AJJ_SYMBOL_NAME_MAX_SIZE );
  ++scp->len;
  strcpy(scp->lsys[scp->len].name,name);
  scp->lsys[scp->len].idx = scp->end++;
  return -1; /* Indicate we have a new one */
}

static
int lex_scope_get( struct parser* p , const char* name , int* lvl ) {
  struct lex_scope* cur = PTOP();
  int l = 0;
  assert( cur ) ;
  assert( strlen(name) < AJJ_LOCAL_CONSTANT_SIZE );
  do {
    int i;
    for( i = 0 ; i < cur->len ; ++i ) {
      if( strcmp(name,cur->lsys[i].name) == 0 ) {
        return cur->lsys[i].idx;
      }
    }
    cur = cur->parent;
    ++l;
  } while(cur);
  if( lvl ) *lvl = l;
  return -1;
}

static
struct string
random_name( struct parser* p , char l ) {
  char name[1024];
  int result;
  if(p->unname_cnt == USHRT_MAX) {
    report_error(p,"Too much scope/blocks,more than:%d!",USHRT_MAX);
    return NULL_STRING;
  }
  result = sprintf(name,"@%c%d",l,p->unname_cnt);
  assert(result < AJJ_SYMBOL_NAME_MAX_SIZE );
  ++p->unname_cnt;
  return string_dupc(name);
}

static
int const_str( struct parser* p , struct program* prg,
    struct string* str , int own ) {
  if( str->len > 128 ) {
insert:
    if( prg->str_len == AJJ_LOCAL_CONSTANT_SIZE ) {
      report_error(p,"Too much local string literals!");
      if( own ) string_destroy(str);
      return -1;
    } else {
      if(own) {
        prg->str_tbl[prg->str_len] = str;
      } else {
        prg->str_tbl[prg->str_len] = string_dup(str);
      }
      return prg->str_len++;
    }
  } else {
    /* do a linear search here */
    size_t i = 0 ;
    for( i ; i < prg->str_len ; ++i ) {
      if( string_eq(prg->str_tbl+i,str) ) {
        if(own) string_destroy(str);
        return i;
      }
    }
    goto insert;
  }
}

static
int const_num( struct parser* p , struct program* prg, double num ) {
  if( prg->str_len == AJJ_LOCAL_CONSTANT_SIZE ) {
    report_error(p,"Too much local number literals!");
    return -1;
  } else {
    prg->num_tbl[prg->str_len] = num;
    return prg->num_len++;
  }
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
  struct tokenizer* tk = p->tk;
  assert(tk->tk == TK_VARIABLE);
  if( tk->lexeme.len >= AJJ_SYMBOL_NAME_MAX_SIZE ) {
    report_error(p,"Local symbol is too long ,longer than:%d",
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
static int parse_seq( struct parser* p , struct emitter* em , int rtk ) {
  struct tokenizer* tk = p->tk;
  /* check if it is an empty list */
  if( tk->tk == rtk ) {
    tk_move(tk);
    return 0;
  }

  do {
    CALLE(parse_expr(p,em,tk));
    emit0(em,VM_ATTR_PUSH);
    if( tk->tk == TK_COMMA ) {
      tk_move(tk);
      continue;
    } else if( tk->tk == rtk ) {
      tk_move(tk);
      break;
    } else {
      report_error(p,"Unknown token:%s,expect \",\" or \"%s\"",
          tk_get_name(tk->tk),
          tk_get_name(rtk));
      return -1;
    }
  } while(1);
  return 0;
}

static
int parse_list( struct parser* p , struct emitter* em ) {
  struct tokenizer* tk = p->tk;
  assert(tk->tk == TK_LSQR);
  tk_move(tk);
  emit0(em,VM_LLIST); /* load an empty list onto the stack */
  return parse_seq(p,em,TK_RSQR);
}

/* Tuple is ambigious by its self !
 * varible1 = (1+2);
 * Whether we need to treat the (1+2) as an singleton tuple or an expression with
 * parentheses , we don't know. Python indicates this by forcing you uses a comma
 * appending after the each element which makes it context free. We need to parse
 * this sort of thing when we see a leading parenthenses ! Take an extra step to
 * look ahead */
static
int parse_tuple_or_subexpr( struct parser* p , struct emitter* em ) {
  /* We cannot look back and I don't want to patch the code as well.
   * So we have to spend an extra instruction on top of it in case
   * it is an tuple */
  int instr = emitter_put(em,0);
  struct tokenizer* tk = p->tk;

  assert(tk->tk == TK_LPAR);
  tk_move(tk);

  /* Parsing the expression here at first */
  CALLE(parse_expr(p,em));

  /* Checking if we have pending comma or not */
  if( tk->tk == TK_COMMA ) {
    /* this is an tuple here */
    emit0_at(em,instr,VM_LLIST);
    emit0(em,VM_ATTR_PUSH);
    return parse_seq(p,em,TK_RPAR);
  } else {
    CONSUME(TK_RPAR);
    /* Emit a NOP initially since we don't have a real tuple */
    emit0_at(em,instr,VM_NOP0);
    return 0;
  }
}

static
int parse_dict( struct parser* p , struct emitter* em ) {
  struct tokenizer* tk = p->tk;

  assert(tk->tk == TK_LBRA);
  tk_move(tk);

  /* load an empty dictionary on the stack */
  emit0(em,VM_LDICT);

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
    emit0(em,VM_ATTR_SET);

    if(tk->tk == TK_COMMA) {
      tk_move(tk);
      continue;
    } else if( tk->tk == TK_RBRA ) {
      tk_move(tk);
      continue;
    } else {
      report_error(p,"Unknown token:%s,expect \",\" or \"}\"!",
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
  if((idx=lex_scope_get(p,var->str,NULL))<0) {
    /* Not a local variable, must be an upvalue */
    int const_idx;
    CALLE((const_idx=const_str(p,em->prg,var,1))<0);
    /* Now emit a UPVALUE instructions */
    emit1(em,VM_UPVALUE_GET,const_idx);
  } else {
    /* Local variables: just load the onto top of the stack */
    emit1(em,VM_BPUSH,idx);
    /* Don't forget to release the var's buffer */
    string_destroy(var);
  }
  return 0;
}

static
int parse_var( struct parser* p, struct emitter* em ) {
  struct string var;
  struct tokenizer* tk = p->tk;
  assert(tk->tk == TK_VARIABLE);
  CALLE(symbol(p,&var));
  tk_move(tk);
  return parse_var_prefix(p,em,&var);
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
  struct tokenizer* tk = p->tk;
  int num = 0;
  /* method call */
  assert(tk->tk == TK_LPAR);
  tk_move(tk);

  if( tk->tk == TK_RPAR ) {
    tk_move(tk);
    return 0;
  }

  do {
    CALLE((parse_expr(p,em)));
    ++num;
    if( tk->tk == TK_COMMA ) {
      tk_move(tk);
    } else {
      if( tk->tk == TK_RPAR ) {
        tk_move(tk);
        break;
      } else {
        report_error(p,"Function/Method call must be closed by \")\"!");
        return -1;
      }
    }
  } while(1);
  return num;
}

static
int parse_attr_or_methodcall( struct parser* p, struct emitter* em ,
    struct string* prefix /* owned */) {
  struct tokenizer* tk = p->tk;
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
    emit0(em,VM_ATTR_GET);
    CONSUME(TK_RSQR);
  } else {
    struct string comp;
    int idx;

    EXPECT(TK_VARIABLE);
    CALLE(symbol(p,&comp));
    CALLE((idx=const_str(p,em->prg,prefix,&comp,1))<0);
    tk_move(tk);
    /* Until now, we still don't know we are calling a member function or
     * reference an object on to the stack.We need to lookahead one more
     * token to decide */
    if( tk->tk == TK_LPAR ) {
      int num;
      CALLE((num = parse_invoke_par(p,em))>=0);
      emit2(em,VM_ATTR_CALL,idx,num);
    } else {
      emit1(em,VM_LSTR,idx);
      emit0(em,VM_ATTR_GET); /* look up the attributes */
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
  int num = pipe; /* If this function call is a pipe call, then pipe will set to 1.
                   * which makes our function has one more parameters comes from
                   * pipe. But the caller of this function must set up the code
                   * correctly. [ object , pipe_data ]. This typically involes prolog
                   * for moving data around */
  struct tokenizer* tk = p->tk;
  assert(tk->tk == TK_LPAR);

  CALLE((idx=const_str(p,em->prg,prefix,1))<0);
  num += parse_invoke_par(p,em); /* generate call parameter for function */
  emit2(em,VM_CALL,idx,num); /* call the function based on current object */
  return 0;
}

/* This function handles the situation that the function is a pipe command shortcut */
static
int parse_pipecmd( struct parser* p , struct emitter* em ,
    struct string* cmd ) {
  int idx;
  CALLE((idx=const_str(p,em->prg,cmd,1))<0);
  emit2(em,VM_CALL,idx,1);
  return 0;
}

static
int parse_prefix( struct parser* p, struct emitter* em ) {
  struct tokenizer* tk = p->tk;
  struct string prefix;
  assert(tk->tk == TK_VARIABLE);
  CALLE(symbol(p,&prefix));
  tk_move(tk);
  if( tk->tk == TK_RPAR ) {
    CALLE(parse_funccall_or_pipe(p,em,&prefix,0));
  } else {
    if( tk->tk == TK_DOT || tk->tk == TK_LSQR ) {
      CALLE(parse_attr_or_methodcall(p,em,&prefix));
    } else {
      /* Just a variable name */
      return parse_var(p,em);
    }
  }
  do {
    if( tk->tk == TK_DOT || tk->tk == TK_LSQR ) {
      CALLE(parse_attr_or_methodcall(p,em,&NULL_STRING));
    } else if( tk->tk == TK_PIPE ) {
      tk_move(tk);
      EXPECT(TK_VARIABLE);
      CALLE(symbol(p,&prefix));
      tk_move(tk);
      if( tk->tk == TK_LPAR ) {
        CALLE(parse_funccall_or_pipe(p,em,&prefix,1));
      } else {
        CALLE(parse_pipecmd(p,em,&prefix));
      }
    } else {
      break;
    }
  } while(1);
  return 0;
}

static
int parse_atomic( struct parser* p, struct emitter* em ) {
  struct tokenizer* tk = p->tk;
  int idx;

  switch(tk->tk) {
    case TK_VARIABLE:
      return parse_prefix(p,em);
    case TK_STRING:
      CALLE((idx=const_str(p,em->prg,strbuf_move(&(tk->lexeme)),1))<0);
      emit1(em,VM_LSTR,idx);
      break;
    case TK_TRUE:
      emit0(em,VM_LTRUE);
      break;
    case TK_FALSE:
      emit0(em,VM_LFALSE);
      break;
    case TK_NONE:
      emit0(em,VM_LNONE);
      break;
    case TK_NUMBER:
      CALLE((idx=const_num(p,em->prg,tk->num_lexeme))<0);
      emit1(em,VM_LNUM,idx);
      break;
    case TK_LPAR:
      return parse_tuple_or_subexpr(p,em);
    case TK_LSQR:
      return parse_list(p,em);
    case TK_LBRA:
      return parse_dict(p,em);
    default:
      report_error(p,"Unexpect token here:%s",tk_get_name(tk->tk));
      return -1;
  }
  tk_move(tk);
  return 0;
}

static
int parse_unary( struct parser* p, struct emitter* em ) {
  unsigned char op[1024]; /* At most we support 1024 unary operators */
  int op_len = 0;
  int i;
  struct tokenizer* tk = p->tk;

#define PUSH_OP(T) \
  do { \
    if( op_len == 1024 ) { \
      report_error(p,"Too much unary operators, more than 1024!"); \
      return -1; \
    } \
    op[op_len++] = (T); \
  } while(0)

  do {
    switch(tk->tk) {
      case TK_NOT: PUSH_OP(TK_NOT); break;
      case TK_SUB: PUSH_OP(TK_SUB); break;
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
    if( op[i] == TK_NOT ) {
      emit0(em,VM_NOT);
    } else {
      emit0(em,VM_NEG);
    }
  }
  return 0;
}
#undef PUSH_OP

static
int parse_factor( struct parser* p, struct emitter* em ) {
  struct tokenizer* tk = p->tk;
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
    emit0(em,op);
  } while(1);

done: /* finish */
  return 0;
}

static
int parse_term( struct parser* p, struct emitter* em ) {
  struct tokenizer* tk = p->tk;
  CALLE(parse_factor(p,em));
  do {
    int op;
    switch(tk->tk) {
      case TK_ADD: op = VM_ADD; break;
      case TK_SUB: op = VM_SUB; break;
      default: goto done;
    }
    tk_move(tk);
    CALLE(parse_factor(p,em));
    emit0(em,op);
  } while(1);
done:
  return 0;
}

static
int parse_cmp( struct parser* p, struct emitter* em ) {
  struct tokenizer* tk = p->tk;
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
    emit0(em,op);
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
  struct tokenizer* tk = p->tk;

#define PUSH_JMP(X) \
  do { \
    if( sz == 1024 ) { \
      report_error(p,"Too much logic component,you have more than 1024!"); \
      return -1; \
    } \
    jmp_tb[sz] = emitter_put(em,1); \
    bc_tb[sz] = (X); \
    ++sz; \
  } while(0)

  CALLE(parse_cmp(p,em));
  do {
    switch(tk->tk) {
      case TK_AND: PUSH_JMP(VM_JLF); break;
      case TK_OR:  PUSH_JMP(VM_JLT); break;
      default:
        goto done;
    }
    tk_move(tk);
    CALLE(parse_cmp(p,em));
  } while(1);
  /* fallback position means we evaluate it to true , since
   * if all component passes jump, we end up having nothing
   * on stack. So we need to load true value on stack here */
  emit0(em,VM_LTRUE);
done:
  pos = emitter_label(em);
  for( i = 0 ; i < sz ; ++i ) {
    emit1_at(em,jmp_tb[i],bc_tb[i],pos);
  }
  return 0;
}
#undef PUSH_JMP

/* Expression.
 * We support value1 if cond else value2 format.
 * The code generation is not simple at all, however. Since the condition is not
 * the first component, the parser needs to look ahead unlimited tokens to say
 * whether this is a tenary format or not. We don't have AST, this becomes very
 * hard. So how to generate the code ?
 * We inject a jump point before the expression. We turn this instruction into
 * NOP when it is not tenary. If it is, this instruction will just jump to the
 * condition part. Then at the end of this expression, we have another jump to
 * skip the alternative value path. Condition part will follow by a tenary byte
 * code which simply jumps to corresponding position based on the evaluation.
 * The NOP is removed during the peephole optimization phase
 */
static
int parse_expr( struct parser* p, struct emitter* em ) {
  int fjmp = emitter_put(em,1);
  int val1_l = emitter_label(em);
  struct tokenizer* tk = p->tk;
  CALLE(parse_logic(p,em));
  /* Check if we have a pending if or not */
  if( tk->tk == TK_IF ) {
    int cond_l, end_l;
    int val1_jmp;
    tk_move(tk);

    /* An unconditional jump to skip the alternative path and condition */
    val1_jmp = emitter_put(em,1);

    /* Parse the condition part */
    cond_l = emitter_label(em);
    CALLE(parse_logic(p,em));
    emit1(em,VM_JT,val1_l); /* jump to value1 when condition is true */

    CONSUME(TK_ELSE);

    /* Parse the alternative value */
    CALLE(parse_logic(p,em));
    end_l = emitter_label(em);

    /* Patch the jump */
    emit1_at(em,fjmp,VM_JMP,cond_l);
    emit1_at(em,val1_jmp,VM_JMP,end_l);
  } else {
    /* nothing serious, just issue a NOP */
    emit1_at(em,fjmp,VM_NOP1,0);
  }
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
  struct tokenizer* tk = p->tk;
  assert(tk->tk == ltk);
  tk_move(tk);

  /* set the output to a list */
  *output = ajj_value_assign(
      ajj_object_create_list(p->a,gc_root(p->a)));

  do {
    struct ajj_value val;
    CALLE(parse_constexpr(p,&val));
    ajj_list_push(output,&val);
    if( tk->tk == TK_COMMA ) {
      tk_move(tk);
      continue;
    } else if( tk->tk == rtk ) {
      tk_move(tk);
      break;
    } else {
      report_error(p,"constexpr: unknown token here:%s "
          "expect \",\" or \"%s\"",
          tk_get_name(ltk),
          tk_get_name(rtk));
      return -1;
    }
  } while(1);

  return 0;
}

static int
parse_constlist( struct parser* p , struct ajj_value* output ) {
  return parse_constseq(p,TK_LSQR,TK_RSQR,output);
}

static int
parse_consttuple( struct parser* p , struct ajj_value* output ) {
  return parse_constseq(p,TK_LPAR,TK_RPAR,output);
}

static int
parse_constdict( struct parser* p , struct ajj_value* output ) {
  struct tokenizer* tk = p->tk;
  assert( tk->tk == TK_LPAR );
  tk_move(tk);

  *output = ajj_value_assign(
      ajj_object_create_dict(p->a,gc_root(p->a)));

  do {
    struct ajj_value key , val;
    CALLE(parse_constexpr(p,&key));

    /* check if key is string */
    if( key.type != AJJ_VALUE_STRING ) {
      report_error(p,"constexpr: dictionary key must be string!");
      return -1;
    }

    if( tk->tk == TK_COLON ) {
      tk_move(tk);
    } else {
      report_error(p,
          "constexpr: unknown token:%s, expect \":\" or \"}\"",
          tk_get_name(tk->tk));
      return -1;
    }
    CALLE(parse_constexpr(p,&val));

    /* insert the key into the list */
    ajj_dict_insert(output,ajj_value_to_str(&key),&val);

    /* remove key/string from memory */
    ajj_value_destroy(p->a,&key);

    if( tk->tk == TK_COMMA ) {
      tk_move(tk);
      continue;
    } else if( tk->tk == TK_RPAR ) {
      tk_move(tk);
      break;
    } else {
      report_error(p,"constexpr: unknown token:%s,expect \",\" or \"}\"",
          tk_get_name(tk->tk));
      return -1;
    }

  } while(1);

  return 0;
}

static int
parse_constexpr( struct parser* p , struct ajj_value* output ) {
  int neg = 0;
  struct tokenizer* tk = p->tk;

  if( tk->tk == TK_SUB ) {
    neg = 1; /* this is the only allowed unary operator */
    tk_move(tk);
  }

  switch(tk->tk) {
    case TK_NUMBER:
      *output = ajj_value_number( neg ? -tk->num_lexeme : tk->num_lexeme);
      return 0;
    case TK_STRING:
      if( neg ) {
        report_error(p,"constexpr: string literal cannot work with unary "
            "operator \"-\"!");
        return -1;
      } else {
        *output = ajj_value_assign(
            ajj_object_create_string(p->a,gc_root(p->a),
              tk->lexeme.str,tk->lexeme.len,0));
        return 0;
      }
    case TK_NONE:
      if( neg ) {
        report_error(p,"constexpr: none cannot work with unary "
            "operator \"-\"!");
        return -1;
      } else {
        *output = AJJ_NONE;
        return 0;
      }
    case TK_TRUE:
      if( neg ) {
        *output = AJJ_FALSE;
      } else {
        *output = AJJ_TRUE;
      }
      return 0;
    case TK_FALSE:
      if( neg ) {
        *output = AJJ_FALSE;
      } else {
        *output = AJJ_TRUE;
      }
      return 0;
    case TK_LSQR:
      if( neg ) {
        report_error(p,"constexpr: list literal cannot work with unary "
            "operator \"-\"!");
        return -1;
      }
      return parse_constlist(p,output);
    case TK_LPAR:
      if( neg ) {
        report_error(p,"constexpr: tuple literal cannot work with unary "
            "operator \"-\"!");
        return -1;
      }
      return parse_consttuple(p,output);
    case TK_LBRA:
      if( neg ) {
        report_error(p,"constexpr: dictionary literal cannot work with unary "
            "operator \"-\"!");
        return -1;
      }
      return parse_constdict(p,output);
    default:
      report_error(p,"constexpr: unknown token:%s!",
          tk_get_name(tk->tk));
      return -1;
  }
  return 0;
}

/* ================================== STRUCTURE =================================== */

/* enter_scope indicates whether this function will enter a new lexical_scope
 * or not */

static
int parse_scope( struct parser* , struct emitter* em ,
    int enter_scope );

static
int parse_print( struct parser* p, struct emitter* em ) {
  struct tokenizer* tk = p->tk;
  assert(tk->tk == TK_LEXP);
  tk_move(tk);
  CALLE(parse_expr(p,em));
  emit0(em,VM_PRINT);
  CONSUME(TK_REXP);
  return 0;
}

static
int parse_branch ( struct parser* p, struct emitter* em ) {
  int cond_jmp = -1;
  int end_jmp [ 1024 ];
  int jmp_sz = 0;
  int i;
  int has_else = 0;
  struct tokenizer* tk = p->tk;

#define PUSH_JMP(P) \
  do { \
    if( jmp_sz == 1024 ) { \
      report_error(p,"Too much if-elif-else branch, more than 1024!"); \
      return -1; \
    } \
    end_jmp[jmp_sz++] = (P); \
  } while(0)

  assert( tk->tk == TK_IF );
  tk_move(tk);
  CALLE(parse_expr(p,em));
  /* condition failed jump */
  cond_jmp = emitter_put(em,1);

  CONSUME(TK_RSTMT);

  CALLE(parse_scope(p,em,1));
  /* jump out of the scope */
  PUSH_JMP(emitter_put(em,1));
  do {
    switch(tk->tk) {
      case TK_ENDFOR:
        { /* end of the for scope */
          if( cond_jmp > 0 )
            emit1_at(em,cond_jmp,VM_JMP,emitter_label(em));
          tk_move(tk);
          CONSUME(TK_RSTMT);
          goto done;
        }
      case TK_ELIF:
        { /* elif scope */
          if( has_else ) {
            report_error(p,"Expect endfor since else tag is seen before!");
            return -1;
          }
          tk_move(tk);
          assert(cond_jmp > 0 );
          /* modify the previous conditional jmp */
          emit1_at(em,cond_jmp,VM_JMP,emitter_label(em));
          CALLE(parse_expr(p,em));

          cond_jmp = emitter_put(em,1);
          CONSUME(TK_RSTMT);
          break;
        }
      case TK_ELSE:
        { /* else scope */
          tk_move(tk);
          assert( cond_jmp > 0 );
          emit1_at(em,cond_jmp,VM_JMP,emitter_label(em));
          cond_jmp = -1;
          CONSUME(TK_RSTMT);
          has_else = 1;
          break;
        }
      default:
        report_error(p,"Unexpected token in branch scope:%s",
            tk_get_name(tk->tk));
        return -1;
    }
    CALLE(parse_branch_body(p,em));
    if( !has_else )
      PUSH_JMP(emitter_put(em,1));
  } while(1);
done:
  /* patch all rest of the cool things */
  for( i = 0 ; i < jmp_sz ; ++i ) {
    emit1_at(em,end_jmp[i],VM_JMP,emitter_label(em));
  }
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

static int
parse_func_prolog( struct parser* p , struct emitter* em ) {
  size_t i;
  for( i = 0 ; i < em->prg->par_size ; ++i ) {
    /* Generate rest of named parameters */
    CALLE(lex_scope_set(p,em->prg->par_list[i])==-2);
  }
}

static int
parse_func_body( struct parser* p , struct emitter* em ) {
  struct lex_scope* scp = PTOP();
  assert( PTOP()->parent == NULL );
  assert( PTOP()->len == 0 );
  assert( PTOP()->end == 0 );

  CALLE(lex_scope_jump(p,0) != NULL);

  CALLE(parse_func_prolog(p,em)); /* Parsing the prolog */
  /* start to parse the function body ,which is just
   * another small code scope */
  CALLE(parse_scope(p,em,tk,1));
  /* Generate return instructions */
  emit0(em,VM_RET);
  /* Notes, after calling this function, the tokenizer should still
   * have tokens related to end of the callin scope */

  lex_scope_exit(p);
  assert( PTOP() == scp ); /* Check */
  return 0;
}

/* Parse function declaration, although we only used it in MACRO right
 * now. But it may be extended to be used some other places as well */
static int
parse_func_prototype( struct parser* p , struct program* prg ) {
  struct tokenizer* tk = p->tk;

  assert( tk->tk == TK_LPAR );
  tk_move(tk);
  if( tk->tk == TK_RPAR ) {
    tk_move(0);
    return 0;
  } else {
    do {
      struct string par_name;
      struct ajj_value def_val = AJJ_NONE;
      /* Building prototype of function */
      EXPECT(TK_VARIABLE);
      CALLE(symbol(p,&par_name));
      tk_move(tk);
      if( tk->tk == TK_EQ ) {
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
parse_macro( struct parser* p ) {
  struct emitter new_em;
  struct program* new_prg;
  struct string name;
  struct tokenizer* tk = p->tk;

  assert(tk->tk == TK_MACRO);
  tk_move(tk);
  /* We need the name of the macro then we can get a new
   * program objects */
  EXPECT(TK_VARIABLE);
  CALLE(symbol(p,&name));
  assert(p->tpl->val.obj.fn_tb);
  /* Add a new function into the table */
  new_prg = func_table_add_jj_macro(
      p->tpl->val.obj.fn_tb,&name,1);
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
  struct tokenizer* tk = p->tk;

  assert(tk->tk == TK_BLOCK);
  tk_move(tk);
  EXPECT(TK_VARIABLE);
  CALLE(symbol(p,&name));
  if( p->extends == 0 ) {
    int idx;
    CALLE((idx=const_str(p,em->prg,&name,0))<0);
    emit2(em,VM_CALL,idx,0);
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

static inline
int parse_loop_cond( struct parser* p , struct emitter* em ) {
  return parse_logic(p,em);
}

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

  int loop_cond_pos;
  int itr_idx;
  int obj_idx;
  int deref_tp;
  int i;
  struct string obj_name;
  struct string itr_name;
  struct lex_scope* scp;

  obj_name = random_name(p,'i');
  itr_name = random_name(p,'i');

  CALLE(parse_loop_cond(p,em));

  CALLE((scp=lex_scope_enter(p,1)));

  CALLE((obj_idx=lex_scope_set(p,obj_name.str))!=-2);
  assert(obj_idx == -1);

  CALLE((itr_idx=lex_scope_set(p,itr_name.str))!=-2);
  assert(itr_idx == -1);

  string_destroy(&obj_name); /* object name */
  string_destroy(&itr_name); /* iterator name */

  /* Adding empty test. We always perform a proactive
   * empty test in case we have a else branch code body.
   * And it doesn't hurt us too much also avoid us to
   * call iter_start which is costy than simple test whether
   * it is empty or not */
  emit1(em,VM_TPUSH,1);
  else_jmp = emitter_put(em,1);

  emit0(em,VM_ITER_START); /* start the iterator */

  /* Now the top 2 stack elements are
   * 1. iterator
   * 2. object */
  loop_cond_pos = emitter_label(em);
  emit0(em,VM_ITER_HAS);
  loop_jmp = emitter_put(em,1); /* loop jump */

  /* Enter into the scope of loop body */
  ENTER_SCOPE();

  /* Dereferencing the key and value */
  deref_tp = 0;
  if( key && val ) {
    deref_tp = 2;
    if( lex_scope_set(p,scp,key->str) != -1 ) {
      report_error(p,"Cannot set up local variable:%s for loop!",
          key->str);
      string_destroy(key);
      string_destroy(val);
      return -1;
    }
    if( lex_scope_set(p,scp,val->str) != -1 ) {
      report_error(p,"Cannot set up local variable:%s for loop!",
          val->str);
      string_destroy(key);
      string_destroy(val);
      return -1;
    }
  } else if( val ) {
    deref_tp = 1;
    if( lex_scope_set(p,scp,val->str) != -1 ) {
      report_error(p,"Cannot set up local variable:%s for loop!",
          val->str);
      string_destroy(val);
      return -1;
    }
  }

  string_destroy(val);
  string_destroy(key);

  if( deref_tp != 0 ) {
    emit0(em,VM_ITER_DEREF);
  }

  /* Now check if we have a filter or not */
  if( tk->tk == TK_IF ) {
    /* got a filter */
    tk_move(tk);
    /* parse the filter onto the stack */
    CALLE(parse_expr(p,em));
    /* now check whether filter is correct or not */
    filter_jmp = emitter_put(em,1);
  }
  CONSUME(TK_RSTMT);

  /* parse the loop body */
  brk_jmp.len = 0;

  CALLE(parse_scope(p,em,0));

  /* filter jumps to here */
  if( filter_jmp >0 )
    emit1_at(em,filter_jmp,VM_JPT,emitter_label(em));

  /* pop the temporary key/value pair */
  emit1(em,VM_POP,deref_tp);

  /* move the iterator */
  emit0(em,VM_ITER_MOVE);

  /* jump to the condition check code */
  emit1(em,VM_JMP,loop_cond_pos);

  /* here is the jump position that is corresponding to
   * loop condition test failure */
  emit1_at(em,loop_jmp,VM_JMP,emitter_label(em)); /* patch the jmp */

  /* patch the break jump table here */
  assert( PTOP()->is_loop && PTOP()->in_loop );

  for( i = 0 ; i < PTOP()->lctrl->len ; ++i ) {
    emit2_at(em,PTOP()->lctrl->brk[i].code_pos,
        VM_JMPC,
        PTOP()->lctrl->brk[i].enter_cnt,
        emitter_label(em));
  }

  /* pop the iterator on top of the stack */
  emit1(em,VM_POP,1);

  /* emit the body exit */
  emit0(em,VM_EXIT);

  /* Exit the lexical scope */
  lex_scope_exit(p);

  /* Patch the else_jmp */
  emit1_at(em,else_jmp,VM_JMP,emitter_label(em));

  /* Check if we have an else branch */
  if( tk->tk == TK_ELSE ) {
    /* Set up the loop_body_jmp hooks */
    loop_body_jmp = emitter_put(em,1);
    tk_move(tk);
    CONSUME(TK_RSTMT);
    /* Parse the else scope */
    CALLE(parse_scope(p,em,1));
  }

  if( loop_body_jmp ) {
    emit1_at(em,loop_body_jmp,VM_JMP,emitter_label(em));
  }

  /* pop the map/list object on top of the stack */
  emit1(em,VM_POP,1);

  return 0;
}

/* Parsing the for loop body.
 * This function act as a parse prolog by only parsing the symbol inside of
 * for loop : {% for symbol1,symbol2 in ...
 * Rest of the parse code is in parse_for_body. For symbol that has name "_"
 * we treat it is not used there. */
static
int parse_for( struct parser* p , struct emitter* em ) {
  int idx;
  struct string key = NULL_STRING;
  struct string val = NULL_STRING;
  struct tokenizer* tk = p->tk;

  assert(tk->tk == TK_FOR);
  tk_move(tk);
  /* We don't support tuple in our code. So we don't really have unpacked
   * semantic here. We support following semantic :
   * value
   * key
   * _,value
   * key,_
   * So at most 2 symbols can be provided here */
  EXPECT(TK_VARIABLE);
  strbuf_move(&(tk->lexeme),&key);
  tk_move(tk);
  if( tk->tk == TK_COMMA ) {
    tk_move(tk);
    EXPECT(TK_VARIABLE);
    strbuf_move(&(tk->lexeme),&val);
    tk_move(tk);
  } else {
    val = key;
    key = NULL_STRING;
  }
  CONSUME(TK_IN);
  /* Ignore underscore since they serves as placeholder */
  if( strcmp(key.str,"_") == 0 ) {
    string_destroy(&key);
    key = NULL_STRING;
  }
  if( strcmp(val.str,"_") == 0 ) {
    string_destroy(&val);
    val = NULL_STRING;
  }
  return parse_for_body(p,em,&key,&val);
}

/* Call
 * We don't support passing function from MACRO to CALL, since this looks wired
 * and also makes implementation complicated because it introduces some specific
 * situations.
 */
static int
parse_call( struct parser* p , struct emitter* em ) {
  struct string name;
  struct string text;
  int text_idx;
  int caller_idx;
  int vm_lstr = -1;
  int vm_upvalue_set = -1;
  struct tokenizer* tk = p->tk;

  assert(tk->tk == TK_CALL);
  tk_move(tk);
  EXPECT(TK_VARIABLE);
  CALLE(symbol(p,tk,&name));
  tk_move(tk);

  /* Because we cannot know the text we want to insert as an upvalue
   * before parsing the function call. We need to reserve instructions
   * spaces in current code page.
   * VM_LSTR(idx);
   * VM_UPVALUE_SET(idx); */
  vm_lstr = emitter_put(em,1);
  vm_upvalue_set = emitter_put(em,1);
  /* Now we start to parse the func call */
  CALLE(parse_funccall_or_pipe(p,em,&name,0));
  CONSUME(TK_RSTMT);
  EXPECT(TK_TEXT);
  /* Get text index in string pool */
  strbuf_move(&(tk->lexeme),&text);
  tk_move(tk);
  CALLE((text_idx=const_str(p,em->prg,&text,1))<0);
  /* Get caller index in string pool */
  CALLE((caller_idx=const_str(p,em->prg,&CALLER,0))<0);
  emit1_at(em,vm_lstr,VM_LSTR,text_idx);
  /* Set the caller intenral string , this string will be
   * exposed by a builtin wrapper function called caller,
   * then user could use caller() to get the content of this
   * upvalue */
  emit1_at(em,vm_upvalue_set,VM_UPVALUE_SET,caller_idx);
  /* Clear the upvalue */
  emit1(em,VM_UPVALUE_DEL,caller_idx);
  CALLE(finish_scope_tag(p,TK_ENDCALL));
  return 0;
}

/* Filter Scope */
static int
parse_filter( struct parser* p , struct emitter* em ) {
  struct string name;
  struct string text;
  int vm_lstr = -1;
  int text_idx;
  struct tokenizer* tk = p->tk;

  assert(tk->tk == TK_FILTER);
  tk_move(tk);
  CALLE(symbol(p,tk,&name));
  tk_move(tk);
  vm_lstr = emitter_put(em,1);

  if( tk->tk == TK_LPAR ) {
    CALLE(parse_funccall_or_pipe(p,em,&name,1));
  } else {
    CALLE(parse_pipecmd(p,em,&name));
  }
  CONSUME(TK_RSTMT);
  strbuf_move(&(tk->lexeme),&text);
  CALLE((text_idx=const_str(p,em->prg,&text,1))<0);
  emit1_at(em,vm_lstr,VM_LSTR,text_idx);
  CALLE(finish_scope_tag(p,TK_ENDFILTER));
}

/* Set Scope */
static
int parse_assign( struct parser* p , struct emitter* em ,
    int idx ) {
  struct tokenizer* tk = p->tk;
  assert(tk->tk == TK_ASSIGN);
  tk_move(tk);
  CALLE(parse_expr(p,em));
  if( idx >= 0 ) {
    emit1(em,VM_STORE,idx);
  }
  return 0;
}

static
int parse_set( struct parser* p, struct emitter* em ) {
  int ret = 0;
  int var_idx;
  struct tokenizer* tk = p->tk;

  assert( tk->tk == TK_SET );
  tk_move(tk);

  EXPECT(TK_VARIABLE);
  CALLE((var_idx=lex_scope_set(p,strbuf_tostring(tk->lexeme)))==-2);
  tk_move(tk); /* Move forward */

  /* check if we have an pending expression or just end of the SET scope*/
  if( tk->tk == TK_RSTMT ) {
    int txt_idx;
    /* end of the set scope , so it is a scop based set */
    tk_move(tk);
    EXPECT(TK_TEXT);
    CALLE((txt_idx=const_str(p,em->prg,strbuf_move(&(tk->lexeme)),1))<0);
    tk_move(tk);
    /* load the text on to stack */
    emit1(em,VM_LSTR,txt_idx);
    /* we need to move this static text into the position of that
     * variable there */
    if( var_idx >= 0 ) {
      emit1(em,VM_STORE,var_idx);
    } else {
      /* This is a no-op since the current top of the stack has been
       * assigend to that local symbol */
    }
    /* set up the var based on the stack */
    CALLE(finish_scope_tag(p,TK_SET));
    return 0;
  } else if( tk->tk == TK_ASSIGN ) {
    CALLE(parse_assign(p,em,var_idx));
    CONSUME(TK_RSTMT);
    return 0;
  } else {
    report_error(p,"Set scope either uses \"=\" to indicate " \
        "a one line assignment," \
        "or uses a scope based set!");
    return -1;
  }
}

/* Do */
static int
parse_do( struct parser* p , struct emitter* em ) {
    struct tokenizer* tk = p->tk;
  assert( tk->tk == TK_DO );
  tk_move(tk);
  CALLE(parse_expr(p,em));
  emit1(em,VM_POP,1);
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

  struct lex_scope* p_scp;
  struct tokenizer* tk = p->tk;

  assert(tk->tk == TK_MOVE);
  tk_move(tk);

  EXPECT(TK_VARIABLE); /* dest variable */

  /* Finding the dest variable in the previous scope */
  p_scp = PTOP()->parent;

  if( p_scp == NULL ) {
    p_scp = PTOP();
  }

  if( (dst_idx=lex_scope_get(p_scp,
          strbuf_tostring(&(tk->lexeme)).str,
          &dst_level))<0 ) {
    report_error(p,"In move statement, the target variable:%s is not defined!",
        strbuf_tostring(&(tk->lexeme)).str);
    return -1;
  }

  tk_move(tk);

  EXPECT(TK_VARIABLE); /* target variable */

  if( (src_idx=lex_scope_get(PTOP(),
          strbuf_tostring(&(tk->lexeme)).str,
          &src_level))<0 ) {
    report_error(p,"In move statement, the source variable:%s is not defined!",
        strbuf_tostring(&(tk->lexeme)).str);
    return -1;
  }
  tk_move(tk);

  if( src_idx >= dst_idx ) {
    emit2(em,VM_MOVE,dst_idx,src_idx);
  } else {
    /* Lift source value to outer scope */
    emit2(em,VM_LIFT,src_idx,dst_idx-src_idx);
    /* Move source value to target position */
    emit2(em,VM_MOVE,dst_idx,src_idx);
  }
  CONSUME(TK_RSTMT);

  return 0;
}

/* Upvalue */
static int
parse_upvalue( struct parser* p , struct emitter* em ) {
  int var_idx;
  struct tokenizer* tk = p->tk;

  assert( tk->tk == TK_UPVALUE );
  tk_move(tk);
  EXPECT(TK_VARIABLE);
  CALLE((var_idx=const_str(p,em->prg,strbuf_move(&(tk->lexeme)),1))<0);
  tk_move(tk);
  CALLE(parse_expr(p,em));
  emit1(em,VM_UPVALUE_SET,var_idx);
  EXPECT(TK_RSTMT);

  /* Start to parsing the body */
  CALLE(parse_scope(p,em,1));

  /* Generate deletion for this scope */
  emit1(em,VM_UPVALUE_DEL,var_idx);

  CONSUME(TK_ENDVALUE);
  CONSUME(TK_RSTMT);
  return 0;
}

/* Loop control statements */
static int
parse_continue( struct parser* p , struct emitter* em ) {
  struct tokenizer* tk = p->tk;
  int pos;
  assert(tk->tk == TK_CONTINUE);
  tk_move(tk);

  assert( PTOP()->in_loop );
  if( PTOP()->lctrl->len == MAX_BREAK_SIZE ) {
    report_error(p,"Cannot have more break statements in this loop!");
    return -1;
  }
  pos = PTOP()->lctrl->len;
  PTOP()->ctrl->brk[pos].code_pos = emitter_put(em,2);
  PTOP()->ctrl->brk[pos].enter_cnt= PTOP()->lctrl->cur_enter;
  ++PTOP()->ctrl->len;
  CONSUME(TK_RSTMT);
  return 0;
}

/* BREAK actually will do NOTHING at all but just eat up all the
 * tokens, the caller will manage the code emittion to jump to
 * the end of the scope */
static void
parse_break( struct parser* p , struct emitter* em ) {
  struct tokenizer* tk = p->tk;
  assert(tk->tk == TK_BREAK );
  tk_move(tk);
  CONSUME(TK_RSTMT);
}

/* With statment.
 * For with statment, we will generate a lex_scope for it */
static int
parse_with( struct parser* p , struct emitter* em ) {
  struct tokenizer* tk = p->tk;
  assert(tk->tk == TK_WITH);
  tk_move(tk);

  if( tk->tk == TK_VARIABLE ) {
    /* We have shortcut writing, so we need to generate a new lexical
     * scope */
    CALLE(lex_scope_enter(p,0) != NULL);
    CALLE(parse_assign(p,em));
    CONSUME(TK_RSTMT);
    /* Now parsing the whole scope body */
    CALLE(parse_scope(p,em,0));
    CONSUME(TK_ENDWITH);
    CONSUME(TK_RSTMT);
    /* exit the lexical scope */
    lex_scope_exit(p,em);
  } else {
    CONSUME(TK_RSTMT);
    CALLE(parse_scope(p,em,1));
    CONSUME(TK_ENDWITH);
    CONSUME(TK_RSTMT);
  }
  return 0;
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
 *   {% upvalue name value (fix)/(override) %}
 *   {% upvalue name value (fix)/(override) %}
 * {% endinclude %}
 *
 * {% include template json jsonfile %}
 *   {% upvalue name value (fix)/(override) %}
 * {% endinclude %}
 *
 */

static int
parse_include_body( struct parser* p , struct emitter* em ) {
  int cnt = 0;
  struct tokenizer* tk = p->tk;
  do {
    CONSUME(TK_LSTMT);
    if( tk->tk == TK_UPVALUE ) {
      int var_idx;
      int opt = INCLUDE_UPVALUE_OVERRIDE;
      tk_move(tk);
      CALLE((var_idx=const_str(p,em->prg,
              strbuf_move(&(tk->lexeme)),1))<0);
      tk_move(tk); /* move the symbol name */
      emit1(em,VM_LIMM,var_idx); /* symbol name index */
      CALLE(parse_expr(p,em,tk));
      /* parsing the options for this upvalue */
      if( tk->tk == TK_FIX ) {
        opt = INCLUDE_UPVALUE_FIX;
        tk_move(tk);
      } else if (tk->tk == TK_OVERRIDE) {
        opt = INCLUDE_UPVALUE_OVERRIDE;
        tk_move(tk);
      }
      CONSUME(TK_RSTMT);
      emit1(em,VM_LIMM,opt);
    } else {
      /* Do not check whether this is a endinclude
       * or not, let include body handle it */
      return cnt;
    }
  } while(1);
}

static int
parse_include( struct parser* p , struct emitter* em ) {
  int cnt = 0;
  int opt = INCLUDE_NONE;
  struct tokenizer* tk = p->tk;

  assert(tk->tk == TK_INCLUDE);
  tk_move(tk);
  CALLE(parse_expr(p,em));

  if( tk->tk == TK_RSTMT ) {
    /* line inclusion */
    tk_move(tk);
  } else if( tk->tk == TK_UPVALUE ) {
    tk_move(tk);
    CONSUME(TK_RSTMT);
    cnt = parse_include_body(p,em);
    CONSUME(TK_ENDINCLUDE);
    CONSUME(TK_RSTMT);
    opt = INCLUDE_UPVALUE;
  } else if( tk->tk == TK_JSON ) {
    tk_move(tk);
    /* json template name */
    CALLE(parse_expr(p,em));
    CONSUME(TK_RSTMT);
    cnt = parse_include_body(p,em);
    CONSUME(TK_ENDINCLUDE);
    CONSUME(TK_RSTMT);
    opt = INCLUDE_JSON;
  } else {
    report_error(p,"Unknown token here in include scope:%s",
        tk_get_name(tk));
    return -1;
  }
  emit2(em,VM_INCLUDE,opt,cnt);
  return 0;
}

/* Import
 * Import has 2 types, one starts with import and the other starts with from.
 * We only support first type, doesn't support second type with keyword from.
 */
static int
parse_import( struct parser* p , struct emitter* em ) {
  struct string name;
  int name_idx;
  struct tokenizer* tk = p->tk;

  assert(tk->tk == TK_IMPORT);
  tk_move(tk);
  /* Get the filepath as an expression */
  CALLE(parse_expr(p,em));
  CONSUME(TK_AS);
  /* Get the symbol name onto stack as a string literal */
  CALLE(symbol(p,&name));
  tk_move(tk);
  /* Get the index */
  CALLE((name_idx=const_str(p,em->prg,&name,1))<0);
  emit1(em,VM_IMPORT,name_idx);

  CONSUME(TK_RSTMT);
  return 0;
}

/* Inheritance
 *
 * We support inheritance in Jinaj2. And we support multiple inheritance as well.
 * Internally any jinja template will be compiled into a n AJJ object, this object
 * will have its functions and variables. And each object will automatically have
 * a main function serves as the entry for rendering this template. Also each object
 * is able to inherit some other objects as well. The extends instruction will end
 * up with a function call that calls its parent's __main__ function right in the
 * children's template. By this we could render parent template's content into this
 * template. And because function lookup are followed by the inheritance chain ,so
 * user will see the overrided block been rendered.
 *
 *
 * The trick part is the super() call. This call will be handled by a specialized
 * C function on the context. It will skip the current object but call the function
 * of its parent's corresponding part. The super call is actually handed as a special
 * instructions internally , VM will not dispatch any function has such name
 */

static int
parse_extends( struct parser * p , struct emitter* em ) {
  struct tokenizer* tk = p->tk;
  assert(tk->tk == TK_EXTENDS);
  tk_consume(tk);
  CALLE(parse_expr(p,em));
  emit0(em,VM_EXTENDS);
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
    struct break_jmp* ) {
  struct tokenizer* tk = p->tk;
  int cont_jmp[ MAX_BREAK_SIZE ];
  int cont_len = 0;
  int i;

#define PUSH_CONT() \
  do { \
    assert(PTOP()->in_loop); \
    if( cont_len == MAX_BREAK_SIZE ) { \
      report_error(p,"Cannot have more continue in loop body!") \
      return -1; \
    } \
    cont_jmp[cont_len] = emitter_put(em,1); \
    ++cont_len; \
  } while(0)

  if( enter_scope ) {
    CALLE(lex_scope_enter(p,0) != NULL);
  }
  ENTER_SCOPE();

#define HANDLE_CASE(T,t) \
    case TK_##T: \
      CALLE(parse_##t(p,em)); \
      break; \
    case TK_END##T: \
      goto done;

  do {
    if( tk->tk == TK_TEXT ) {
      struct string text;
      int text_id;
      text = strbuf_move(&(tk->lexeme));
      CALLE((text_id == const_str(p,em->prg,text,1))<0);
      emit1(em,VM_LSTR,text_id);
      emit0(em,VM_PRINT);
      tk_move(tk);
    }

    if( tk->tk == TK_LSTMT ) {
      tk_move(tk);
      if( p->extends ) {
        /* We only allow block instructions in here */
        if( tk->tk == TK_BLOCK )
          CALLE(parse_block(p,em));
        else
          goto fail;
      } else {
        switch(tk->tk) {
          HANDLE_CASE(IF,branch)
            HANDLE_CASE(For,for)
            HANDLE_CASE(Set,set)
            HANDLE_CASE(Filter,filter)
            HANDLE_CASE(Call,call)
            HANDLE_CASE(MACRO,macro)
            HANDLE_CASE(BLOCK,block)
            HANDLE_CASE(UPVALUE,upvalue)
          case TK_INCLUDE:
            if( is_in_main(p) )
              CALLE(parse_include(p,em));
            else
              goto fail;
          case TK_IMPORT:
            if( is_in_main(p) )
              CALLE(parse_import(p,em));
            else
              goto fail;
          case TK_EXTENDS:
            if( is_in_main(p) )
              CALLE(parse_extends(p,em));
            else
              goto fail;
          case TK_ELIF:
          case TK_ELIF:
            goto done;
          case TK_DO:
            CALLE(parse_do(p,em));
          case TK_WITH:
            CALLE(parse_with(p,em));
          case TK_MOVE:
            CALLE(parse_move(p,em));
          case TK_BREAK:
            if( PTOP()->in_loop ) {
              parse_break(p,em);
              PUSH_CONT();
            } else {
              goto fail;
            }
          case TK_CONTINUE:
            if( PTOP()->in_loop ) {
              CALLE(parse_break(p,em));
            } else {
              goto fail;
            }
          default:
              goto fail;
        }
      }
    } else if( tk->tk == TK_LEXP ) {
      return parse_print(p,em);
    } else {
      goto fail;
    }
  } while(1);

#undef HANDLE_CASE

done:
  /* patch the continue operations */
  for( i = 0 ; i < cont_len ; ++i ) {
    emit1_at(em,cont_jmp[i],VM_JMP,
        emitter_label(em));
  }

  emit0(em,VM_EXIT);
  if( enter_scope ) {
    lex_scope_exit(p);
  }
  return 0;

fail:
  report_error(p,"Unkonwn token in scope:%s!",
      tk_get_name(tk->tk));
  return -1;
}
