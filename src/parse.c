#include "ajj-priv.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#define EXPECT(TK) \
  do { \
    if( tk->tk != (TK) ) { \
      report_errpr(p,tk,"Unexpected token :%s expect :%s", \
          tk_get_name(tk->tk), \
          tk_get_name((TK))); \
      return -1; \
    } \
  } while(0)

#define CALLE(P) \
  do { \
    if( (P) ) { \
      return -1; \
    } \
  } while(0)


/* Lexical Scope
 * Lexical scope cannot cross function boundary. Once a new function
 * enters, a new set of lexical scope must be initialized */
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
};

#define MAX_NESTED_FUNCTION_DEFINE 128

struct parser {
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

static inline
void parse_init( struct parser* p , struct ajj* a );

static
void report_error( struct parser* , const struct tokenizer* , const char* , ... );

#define PTOP() (p->cur_scp[p->scp_tp])

/* Lexical scope operation for parsing */
static inline
struct lex_scope* lex_scope_enter( struct parser* p ) {
  struct lex_scope* scp;
  scp = malloc(sizeof(*scp));
  scp->parent = PTOP();
  scp->end = PTOP()->end;
  scp->len = 0;
  return PTOP();
}

static inline
struct lex_scope* lex_scope_jump( struct parser* p ) {
  struct lex_scope* scp;
  scp = malloc(sizeof(*scp));
  if( p->scp_tp == MAX_NESTED_FUNCTION_DEFINE ) {
    report_errpr(p,tk,"Too much nested functoin definition!");
    return NULL;
  } else {
    ++p->scp_tp;
    PTOP() = scp;
    scp->parent = NULL;
    scp->len = 0;
    scp->end = 0;
    return PTOP();
  }
}

static inline
struct lex_scope* lex_scope_exit( struct parser*  p ) {
  struct lex_scope* scp;
  assert( PTOP() != NULL );
  /* move the current ptop to its parent */
  scp = PTOP()->parent;
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

/* This function is not an actual set but a set if not existed.
 * Because most of the local symbol definition has such semantic.
 * This function returns -2 represent error,
 * returns -1 represent a new symbol is set up
 * and non-negative number represents the name is found */
static inline
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
    report_errpr(p,tk,"Too much local constant in a scope!More than :%d",
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
int lex_scope_get( struct parser* p , const char* name ) {
  struct lex_scope* cur = PTOP();
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
  } while(cur);
  return -1;
}

static
struct string
random_name( struct parser* p , char l ) {
  char name[1024];
  int result;
  if(p->unname_cnt == USHRT_MAX) {
    report_errpr(p,tk,"Too much scope/blocks,more than:%d!",USHRT_MAX);
    return NULL_STRING;
  }
  result = sprintf(name,"@%c%d",l,p->unname_cnt);
  assert(result < AJJ_SYMBOL_NAME_MAX_SIZE );
  ++p->unname_cnt;
  return string_dupc(name);
}

static
int const_str( struct parser* p , struct program* prg, struct string* str , int own ) {
  if( str->len > 128 ) {
insert:
    if( prg->str_len == AJJ_LOCAL_CONSTANT_SIZE ) {
      report_errpr(p,tk,"Too much local string literals!");
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

static inline
int const_num( struct parser* p , struct program* prg, double num ) {
  if( prg->str_len == AJJ_LOCAL_CONSTANT_SIZE ) {
    report_errpr(p,tk,"Too much local number literals!");
    return -1;
  } else {
    prg->num_tbl[prg->str_len] = num;
    return prg->num_len++;
  }
}
/* Parser helpers */
static
int finish_scope_tag( struct parser* p, struct tokenizer* tk , int token  ) {
  EXPECT(TK_LSTMT);
  tk_consume(tk);

  EXPECT(token);
  tk_consume(tk);

  EXPECT(TK_RSTMT);
  tk_consume(tk);
  return 0;
}

static
int symbol( struct parser* p , struct tokenizer* tk , struct string* output ) {
  assert(tk->tk == TK_VARIABLE);
  if( tk->lexme.len >= AJJ_SYMBOL_NAME_MAX_SIZE ) {
    report_errpr(p,tk,"Local symbol is too long ,longer than:%d",
        AJJ_SYMBOL_NAME_MAX_SIZE);
    return -1;
  } else {
    strbuf_move(&(tk->lexme),output);
    return 0;
  }
}

/* Expr ============================== */
static
int parse_expr( struct parser* , struct emitter* , struct tokenizer* );

/* List literal.
 * List literal are like this [ expr , expr , expr ].
 * The problem is how to generate code that create the list for you, to do this
 * we will use instruction VM_ATTR_PUSH */
static int parse_seq( struct parser* p , struct emitter* em ,
    struct tokenizer* tk , int ltk , int rtk ) {
  assert(tk->tk == token);
  tk_consume(tk);

  /* load an empty list onto the stack */
  emit0(em,VM_LLIST);

  /* check if it is an empty list */
  if( tk->tk == rtk ) {
    tk_consume(tk);
    return 0;
  }

  do {
    CALLE(parse_expr(p,em,tk));
    emit0(em,VM_ATTR_PUSH);
    if( tk->tk == TK_COMMA ) {
      tk_consume(tk);
      continue;
    } else if( tk->tk == rtk ) {
      tk_consume(tk);
      break;
    } else {
      report_error(p,tk,"Unknown token:%s,expect \",\" or \"%s\"",
          tk_get_name(tk->tk),
          tk_get_name(rtk));
      return -1;
    }
  } while(1);
  return 0;
}

static
int parse_list( struct parser* p , struct emitter* em , struct tokenizer* tk ) {
  return parse_seq(p,em,tk,TK_LSQR,TK_RSQR);
}

/* tuple is just an alias of the list */
static
int parse_tuple( struct parser* p , struct emitter* em , struct tokenizer* tk ) {
  return parse_seq(p,em,tk,TK_LPAR,TK_RPAR);
}

static
int parse_dict( struct parser* p , struct emitter* em , struct tokenizer* tk ) {
  assert(tk->tk == TK_LBRA);
  tk_consume(tk);

  /* load an empty dictionary on the stack */
  emit0(em,VM_LDICT);

  if( tk->tk == TK_RBRA ) {
    tk_consume(tk);
    return 0;
  }

  do {
    /* get the key */
    CALLE(parse_expr(p,em,tk));
    /* eat the colon */
    EXPECT(TK_COLON); tk_consume(tk);
    /* get value */
    CALLE(parse_expr(p,em,tk));
    /* generate code push them into the dict */
    emit0(em,VM_ATTR_SET);

    if(tk->tk == TK_COMMA) {
      tk_consume(tk);
      continue;
    } else if( tk->tk == TK_RBRA ) {
      tk_consume(tk);
      continue;
    } else {
      report_error(p,tk,"Unknown token:%s,expect \",\" or \"}\"!",
          tk_get_name(tk->tk));
      return -1;
    }
  } while(1);
  return 0;
}

static
int parse_literals( struct parser* p, struct emitter* em , struct tokenizer* tk ) {
  int idx;
  switch(tk->tk) {
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
      CALLE((idx=const_num(p,em->prg,tk->num_lexme))<0);
      emit1(em,VM_LNUM,idx);
      break;
    case TK_STRING:
      CALLE((idx=const_str(p,em->prg,strbuf_move(&(tk->lexme)),1))<0);
      emit1(em,VM_LSTR,idx);
      break;
    case TK_LSQR:
      return parse_list(p,em,tk);
    case TK_LPAR:
      return parse_tuple(p,em,tk);
    case TK_LBRA:
      return parse_dict(p,em,tk);
    default:
      UNREACHABLE();
      return -1;
  }
  tk_consume(tk);
  return 0;
}

static inline
int parse_var_prefix( struct parser* p , struct emitter* em , struct string* val ) {
  /* Check whether the variable is a local variable or
   * at least could be */
  if((idx=lex_scope_get(p,var->str))<0) {
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

static inline
int parse_var( struct parser* p, struct emitter* em , struct tokenizer* tk ) {
  struct string var;
  int idx;
  assert(tk->tk == TK_VARIABLE);
  CALLE(symbol(p,tk,&var));
  tk_consume(tk);
  return parse_var_prefix(p,em,&var);
}

static
int parse_attr( struct parser* p, struct emitter* em , struct tokenizer* tk,
    struct string* prefix /* owned */) {
  int idx;
  int comp_tk;
  assert( tk->tk == TK_LSQR || tk->tk == TK_DOT );

  if( string_null(prefix) ) {
    /* We have a symbol name as prefix, we need to load the object
     * whose name is this prefix on to the stack first and then parse
     * the expression and then retrieve the attributes */
    CALLE(parse_var_prefix(p,em,prefix));
  }

  /* Move tokenizer */
  comp_tk = tk->tk;
  tk_consume(tk);

  if( comp_tk == TK_LSQR ) {
    CALLE(parse_expr(p,em,tk));
  } else {
    EXPECT(TK_VARIABLE);
    CALLE((idx=const_str(p,em->prg,prefix,strbuf_move(&(tk->lexme)),1))<0);
    tk_consume(tk); /* move forward */
    emit1(em,VM_ATTR_GET,idx); /* look up the attributes */
  }

  /* Now try to parse the end of this component lookup */
  if( comp_tk == TK_LSQR ) {
    EXPECT(TK_RSQR);
    tk_consume(tk);
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
    struct tokenizer* tk , struct string* prefix , int pipe ) {
  int idx;
  int num = pipe; /* If this function call is a pipe call, then pipe will set to 1.
                   * which makes our function has one more parameters comes from
                   * pipe. But the caller of this function must set up the code
                   * correctly. [ object , pipe_data ]. This typically involes prolog
                   * for moving data around */
  int pipe_limm = -1;
  int pipe_tmove= -1;
  int func_limm = -1;
  assert(tk->tk == TK_LPAR);
  tk_consume(tk);
  if( !string_null(prefix) ) {
    CALLE((idx=const_str(p,em->prg,prefix,1))<0);
    /* NOTES: When we have the prefix as a inemitter_put for funccall, it means we
     * are calling a free function. But in our VM, we can only do
     * member function (method) calling, it means we need to load the
     * current scope onto the stack. This instruction is emitted by
     * our callee parse_prefix function*/
  }
  if(pipe) {
    /* We need to take consideration of PIPE. PIPE is just a function call
     * with some special parameter manipulation. If this function call is
     * a pipe call. */
    emit1(em,VM_TPUSH,-1); /* Duplicate the value on top of the stack */
    /* Now we reserve following code to move the functions parameters to the
     * original places of first parameters.
     * VM_LIMM(parameter_num) Load parameter num on top of stack
     * VM_TMOVE(-3) Move the top element of stack to the third top element.
     */
    pipe_limm = emitter_put(em,1);
    pipe_tmove= emitter_put(em,1);
  } else {
    func_limm = emitter_put(em,1);
  }

  /* Populating the function parameters */
  if( tk->tk != TK_RPAR ) {
    do {
      CALLE((parse_expr(p,em,tk)));
      ++num;
      if( tk->tk == TK_COMMA ) {
        tk_consume(tk);
      } else {
        if( tk->tk == TK_RPAR ) {
          tk_consume(tk);
          break;
        } else {
          report_errpr(p,tk,"Function call must be closed by \")\"!");
          return -1;
        }
      }
    } while(1);
  }
  emit1(em,VM_CALL,idx); /* call the function based on current object */

  /* patch the instructions */
  if(pipe) {
    assert( pipe_limm > 0 && pipe_tmove > 0 );
    emit1_at(em,pipe_limm,VM_LIMM,num);
    emit1_at(em,pipe_tmove,VM_TLOAD,-3);
  } else {
    assert( func_limm > 0 );
    emit1_at(em,func_limm,VM_LIMM,num);
  }
  return 0;
}

/* This function handles the situation that the function is a pipe command shortcut */
static inline
int parse_pipecmd( struct parser* p , struct emitter* em ,
    struct tokenizer* tk , struct string* cmd ) {
  int idx;
  emit1(em,VM_TPUSH,-1);
  emit1(em,VM_LIMM,1);
  emit1(em,VM_TLOAD,-3);
  CALLE((idx=const_str(p,em->prg,cmd,1))<0);
  emit1(em,VM_CALL,idx);
  return 0;
}

static inline
int parse_prefix( struct parser* p, struct emitter* em , struct tokenizer* tk ) {
  struct string prefix;
  assert(tk->tk == TK_VARIABLE);
  CALLE(symbol(p,tk,&prefix));
  tk_consume(tk);
  if( tk->tk == TK_RPAR ) {
    emit0(em,VM_SCOPE); /* load scope on stack */
    CALLE(parse_funccall(p,em,tk,&prefix,0));
  } else {
    if( tk->tk == TK_DOT || tk->tk == TK_LSQR ) {
      CALLE(parse_attr(p,em,tk,&prefix));
    } else {
      /* Just a variable name */
      return parse_var(p,em,tk);
    }
  }
  do {
    if( tk->tk == TK_RPAR ) {
      CALLE(parse_funccall(p,em,tk,&NULL_STRING,0));
    } else if( tk->tk == TK_DOT || tk->tk == TK_LSQR ) {
      CALLE(parse_attr(p,em,tk,&NULL_STRING));
    } else if( tk->tk == TK_PIPE ) {
      tk_consume(tk);
      EXPECT(TK_VARIABLE);
      CALLE(symbol(p,tk,&prefix));
      tk_consume(tk);
      if( tk->tk == TK_LPAR ) {
        CALLE(parse_funccall_or_pipe(p,em,tk,&prefix,1));
      } else {
        CALLE(parse_pipecmd(p,em,tk,&prefix));
      }
    } else {
      break;
    }
  } while(1);
  return 0;
}

static inline
int parse_atomic( struct parser* p, struct emitter* em , struct tokenizer* tk ) {
  switch(tk->tk) {
    case TK_VARIABLE:
      return parse_prefix(p,em,tk);
    case TK_STRING:
    case TK_TRUE:
    case TK_FALSE:
    case TK_NONE:
    case TK_NUMBER:
    case TK_LSQR:
    case TK_LBRA:
      return parse_literals(p,em,tk);
    case TK_LPAR:
      tk_consume(tk);
      if(parse_expr(p,em,tk))
        return -1;
      EXPECT(TK_RPAR);
      tk_consume(tk);
      return 0;
    default:
      report_errpr(p,tk,"Unexpect token here:%s",tk_get_name(tk->tk));
      return -1;
  }
}

static inline
int parse_unary( struct parser* p, struct emitter* em , struct tokenizer* tk ) {
  unsigned char op[1024]; /* At most we support 1024 unary operators */
  int op_len = 0;
  int i;

#define PUSH_OP(T) \
  do { \
    if( op_len == 1024 ) { \
      report_errpr(p,tk,"Too much unary operators, more than 1024!"); \
      return -1; \
    } \
    op[op_len++] = (T); \
  } while(0)

  do {
    switch(tk->tk) {
      case TK_NOT: PUSH_OP(TK_NOT); break;
      case TK_SUB: PUSH_OP(TK_SUB); break;
      case TK_ADD:
      default:
        break;
    }
  } while(1);

  /* start to parse the thing here */
  CALLE(parse_atomic(p,em,tk));
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
int parse_factor( struct parser* p, struct emitter* em , struct tokenizer* tk ) {
  CALLE(parse_unary(p,em,tk));
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
    CALLE(parse_unary(p,em,tk));
    emit0(em,op);
  } while(1);

done: /* finish */
  return 0;
}

static
int parse_term( struct parser* p, struct emitter* em , struct tokenizer* tk ) {
  CALLE(parse_factor(p,em,tk));
  do {
    int op;
    switch(tk->tk) {
      case TK_ADD: op = VM_ADD; break;
      case TK_SUB: op = VM_SUB; break;
      default: goto done;
    }
    CALLE(parse_factor(p,em,tk));
    emit0(em,op);
  } while(1);
done:
  return 0;
}

static
int parse_cmp( struct parser* p, struct emitter* em , struct tokenizer* tk ) {
  CALLE(parse_term(p,em,tk));
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
    CALLE(parse_term(p,em,tk));
    emit0(em,op);
  } while(1);
done:
  return 0;
}

static
int parse_logic( struct parser* p, struct emitter* em , struct tokenizer* tk ) {
  CALLE(parse_cmp(p,em,tk));
  do {
    int op;
    switch(tk->tk) {
      case TK_AND: op = VM_AND; break;
      case TK_OR:  op = VM_OR ; break;
      default:
        goto done;
    }
    CALLE(parse_cmp(p,em,tk));
    emit0(em,op);
  } while(1);
done:
  return 0;
}

static
int parse_expr( struct parser* p, struct emitter* em , struct tokenizer* tk ) {
  /* expression support tenary in a 'expr if cond else expr' way, which doesn't
   * really follow the generation of bytecode sequence. basically we want the
   * condition generated at first, so we use a VM_SWAP operation to swap the
   * top 2 position in stack to make VM_TENARY works correctly */
  CALLE(parse_logic(p,em,tk));
  /* Check if we have a pending if or not */
  if( tk->tk == TK_IF ) {
    tk_consume(tk);
    CALLE(parse_logic(p,em,tk));
    /* Swap the top 2 value here */
    emit2(em,VM_TSWAP,-1,-2); /* The stack is growing up , ESP(assume we have) is
                               * pointing to the first unused element in the stack,
                               * and we use ESP to calculate the position of stack,
                               * so ESP-1 : is the top most used element, the ESP-2
                               * is the second most used element. */
    EXPECT(TK_ELSE);
    tk_consume(tk);

    emit0(em,VM_TENARY);
  }
}

/* ============================== ConstExpression =======================
 * We need const expression evaluator for default value. Pay attention, we
 * only support const expression for default arguments for functions. During
 * the parsing, the value's memory are owned by the global scope. They are
 * not gonna be GC through scope mechanism by default */

static int
parse_constexpr( struct parser* p , struct tokenizer* tk ,
    struct ajj_value* output );

static int
parse_constseq( struct parser* p , struct tokenizer* tk ,
    int ltk , int rtk ,
    struct ajj_value* output );


/* structures */

/* enter_scope indicates whether this function will enter a new lexical_scope
 * or not */
static
int parse_scope( struct parser* , struct emitter* em ,
    struct tokenizer* tk , int enter_scope , int rm_ws );

static
int parse_print( struct parser* p,
    struct emitter* em , struct tokenizer* tk ) {
  CALLE(parse_expr(p,em,tk));

  emit0(em,VM_PRINT);
  EXPECT(TK_REXP);
  tk_consume(tk);
  return 0;
}

static
int parse_branch ( struct parser* p,
    struct emitter* em , struct tokenizer* tk ) {
  int cond_jmp = -1;
  int end_jmp [ 1024 ];
  int jmp_sz = 0;
  int i;
  int has_else = 0;

#define PUSH_JMP(P) \
  do { \
    if( jmp_sz == 1024 ) { \
      report_errpr(p,tk,"Too much if-elif-else branch, more than 1024!"); \
      return -1; \
    } \
    end_jmp[jmp_sz++] = (P); \
  } while(0)

  assert( tk->tk == TK_IF );
  tk_consume(tk);
  CALLE(parse_expr(p,em,tk));
  /* condition failed jump */
  cond_jmp = emitter_put(em,1);

  EXPECT(TK_RSTMT);
  tk_consume(tk);

  CALLE(parse_scope(p,em,tk,1));
  /* jump out of the scope */
  PUSH_JMP(emitter_put(em,1));
  do {
    switch(tk->tk) {
      case TK_ENDFOR:
        { /* end of the for scope */
          if( cond_jmp > 0 )
            emit1_at(em,cond_jmp,VM_JMP,emitter_label(em));
          tk_consume(tk);
          EXPECT(TK_RSTMT);
          tk_consume(tk);
          goto done;
        }
      case TK_ELIF:
        { /* elif scope */
          if( has_else ) {
            report_errpr(p,tk,"Expect endfor since else tag is seen before!");
            return -1;
          }
          tk_consume(tk);
          assert(cond_jmp > 0 );
          /* modify the previous conditional jmp */
          emit1_at(em,cond_jmp,VM_JMP,emitter_label(em));
          CALLE(parse_expr(p,em,tk));

          cond_jmp = emitter_put(em,1);
          EXPECT(TK_RSTMT);
          tk_consume(tk);
          break;
        }
      case TK_ELSE:
        { /* else scope */
          tk_consume(tk);
          assert( cond_jmp > 0 );
          emit1_at(em,cond_jmp,VM_JMP,emitter_label(em));
          cond_jmp = -1;
          EXPECT(TK_RSTMT);
          tk_consume(tk);
          has_else = 1;
          break;
        }
      default:
        report_errpr(p,tk,"Unexpected token in branch scope:%s",
            tk_get_name(tk->tk));
        return -1;
    }
    CALLE(parse_scope(p,em,tk,1));
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

static void
parse_func_prolog( struct parser* p , struct emitter* em ) {
  size_t i;
  /* Generate __argnum__ */
  CALLE(lex_scope_set(p,ARGNUM)==-2);
  for( i = 0 ; i < em->prg->par_size ; ++i ) {
    /* Generate rest of named parameters */
    CALLE(lex_scope_set(p,em->prg->par_list[i])==-2);
  }
}

static int
parse_func_body( struct parser* p , struct emitter* em ,
    struct tokenizer* tk ) {
  struct lex_scope* scp = PTOP();
  assert( PTOP()->parent == NULL );
  assert( PTOP()->len == 0 );
  assert( PTOP()->end == 0 );

  CALLE(lex_scope_jump(p) != NULL);

  parse_func_prolog(p,em,tk); /* Parsing the prolog */
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
parse_func_prototype( struct parser* p , struct tokenizer* tk ,
    struct program* prg ) {
  assert( tk->tk == TK_LPAR );
  tk_consume(tk);
  if( tk->tk == TK_RPAR ) {
    tk_consume(0);
    return 0;
  } else {
    do {
      struct string par_name;
      struct ajj_value def_val = AJJ_NONE;
      /* Building prototype of function */
      EXPECT(TK_VARIABLE);
      CALLE(symbol(p,tk,&par_name));
      tk_consume(tk);
      if( tk->tk == TK_EQ ) {
        tk_consume(tk);
        /* We have a default value , parse it through constexpr */
        CALLE(parse_constexpr(p,tk,&def_val));
      }
      /* Add it into the function prototype */
      CALLE(program_add_par(prg,&par_name,&def_val));
      /* Check if we can exit or continue */
      if( tk->tk == TK_COMMA ) {
        tk_consume(tk);
        continue;
      } else if( tk->tk == TK_RPAR ) {
        tk_consume(tk);
        break;
      }
    } while(1);
    return 0;
  }
}

static int
parse_macro( struct parser* p , struct tokenizer* tk ) {
  struct emitter new_em;
  struct program* new_prg;
  struct string name;

  assert(tk->tk == TK_MACRO);
  tk_consume(tk);
  /* We need the name of the macro then we can get a new
   * program objects */
  EXPECT(TK_VARIABLE);
  CALLE(symbol(p,tk,&name));
  assert(p->tpl->val.obj.fn_tb);
  /* Add a new function into the table */
  new_prg = func_table_add_jj_macro(
      p->tpl->val.obj.fn_tb,&name,1);
  new_em.prg = new_prg;
  /* Parsing the prototype */
  CALLE(parse_func_prototype(p,tk,new_prg));
  EXPECT(TK_RSTMT); tk_consume(tk);

  /* Parsing the rest of the body */
  CALLE(parse_func_body(p,tk,&new_em));
  EXPECT(TK_ENDMACRO); tk_consume(tk);
  EXPECT(TK_RSTMT); tk_consume(tk);
  return 0;
}

/* Block inside of Jinja is still an functions, the difference is that a
 * block will be compiled into a ZERO-parameter functions and automatically
 * gets called when we are not have extends instructions */
static int
parse_block( struct parser* p , struct emitter* em , struct tokenizer* tk ) {
  struct string name;
  struct new_prg* new_prg;
  struct emitter new_em;
  assert(tk->tk == TK_BLOCK);
  tk_consume(tk);
  EXPECT(TK_VARIABLE);
  CALLE(symbol(p,tk,&name));
  if( p->extends == 0 ) {
    int idx;
    CALLE((idx=const_str(p,em->prg,&name,0))<0);
    /* Generate caller code since we haven't seen any extends
     * instructions yet */
    emit1(em,VM_LIMM,0);
    emit1(em,VM_CALL,idx);
  }
  assert(p->tpl->val.obj.fn_tb);
  new_prg = func_table_add_jj_block(
      p->tpl->val.obj.fn_tb,&name,1);
  new_em.prg = new_prg;
  /* Parsing the functions */
  CALLE(parse_func_body(p,&new_em,tk));
  /* Parsing the rest of the body */
  EXPECT(TK_ENDBLOCK);tk_consume(tk);
  EXPECT(TK_RSTMT);tk_consume(tk);
  return 0;
}

/* for loop
 * In jinja, for loop is actually kind of complicated. We will use some
 * trick here to perform the for loop implementation and take advantage
 * of our scope based variable lookup.
 *
 * How to parse for loop ?
 * We have special instructions to do loop , VM_LOOP and VM_LOOPREC , it
 * accpets an iterable objects and a compiled closure.
 */

static inline
int parse_loop_cond( struct parser* p , struct emitter* em ,
    struct tokenizer* tk ) {
  return parse_logic(p,em,tk);
}

/* Why another parse_scope, since in loop we have loop control. For function
 * parse_loop_scope , it allows you to put continue/break in body */
static
void parse_loop_scope( struct parser* p , struct emitter* em ,
    struct tokenizer* tk );

/* This function compiles the for body into the closure. What is for
 * body:
 * {% for symbolname in expr (filter) (recursive) %}
 * We will compile the filter + body of for loop as a closure function.
 * This function will have one parameter which is same as symbolname.
 * Optionally, for accept a comma separated key,value name as this :
 * {% for key,value in map (filter) (recursive) %}
 */

static int parse_for_body( struct parser* p ,
    struct emitter* em ,
    struct tokenizer* tk ,
    struct string* key,
    struct string* val ) {
  struct string name;
  int idx;
  int kv_idx;
  struct emitter cl_em; /* new program's emitter */
  struct program* new_prg; /* new program */
  int recur = 0;
  int jmp_pos;
  int ft_jmp_pos = -1;

  /* get unique name */
  CALLE((name=random_name(p,'l')) != NULL_STRING);
  /* get unique name index in CURRENT scope */
  CALLE((idx=const_str(p,em->prg,name,0)));
  /* compile the loop target onto stack */
  CALLE(parse_loop_cond(p,em,tk));
  /* Since a for can follow by an else when the object is empty,
   * we need to insert a jmp instruction here. This jump instruction
   * is a special instruction , it will jump when the value of the
   * top stack is empty . First we duplicate the stack value, since
   * jump will consume one value, then we do a jump */
  emit1(em,VM_PUSH,-1);
  jmp_pos = emitter_put(em,1); /* setup the tag for jmp */

  /* We cannot generate VM_LOOP/VM_LOOPREC until we see the recursive
   * keyword. However we could defer this operation until we finish the
   * potential filter. The reason is that those are 2 different code
   * buffers. */

  /* Create a new program into the existed objects */
  assert( p->tpl->tp == AJJ_VALUE_OBJECT );
  assert( p->tpl->val.obj.fn_tb != NULL  );
  /* Set up the new program scopes */
  cl.prg = new_prg = func_table_add_jj_block(
      p->tpl->val.obj.fn_tb,name);

  /* Add function parameters into its prototype definition */
  if( string_null(key) ) {
    CALLE((kv_idx=const_str(p,new_prg,key,1))<0);
    CALLE(program_add_par(new_prg,kv_idx,AJJ_NONE));
  }
  if( string_null(val) ) {
    CALLE((kv_idx=const_str(p,new_prg,val,1))<0);
    CALLE(program_add_par(new,kv_idx,AJJ_NONE));
  }
  /* Add prolog for functions */
  CALLE(parse_func_prolog(p,&cl_em));

  /* Enter into the new scope for parsing */
  CALLE(lex_scope_jump(p) != NULL);
  /* Generate filter if we have one */
  if( tk->tk == TK_IF ) {
    tk_consume(tk);
    CALLE(parse_expr(p,&cl_em,tk));
    ft_jmp_pos = emitter_put(&cl_em,1); /* Reserve a  place for jumping */
  }

  /* Now we should left with keyword recursive or %} */
  if( tk->tk == TK_RECURSIVE ) {
    recur = 1;
    tk_consume(tk);
  }
  EXPECT(TK_RSTMT);
  tk_consume(tk);

  /* parse the closure itself */
  CALLE(parse_loop_body(p,&cl_em,tk));
  /* if we have a filter, we will direct the jump here */
  if( ft_jmp_pos >= 0 ) {
    emit1_at(&cl_em,ft_jmp_pos,VM_JT,emitter_label(&cl_em));
  }
  /* generate prolog for function call */
  emit1(&cl_em,VM_LIMM,LOOP_CONTINUE);
  emit0(&cl_em,VM_RET);
  /* leave the current parsing scope */
  lex_scope_exit(p);

  /* Now we need to generate code that actually calls into
   * each closure during the loop phase */
  if(recur) {
    emit1(em,VM_LOOPRECUR,idx);
  } else {
    emit1(em,VM_LOOP,idx);
  }
  /* Unrecognized end scope tag */
  if( tk->tk == TK_ELSE ) {
    /* We enter into the for-else */
    tk_consume(tk); EXPECT(TK_RSTMT) ; tk_consume(tk);
    /* Set up the jmp tag here */
    emit1_at(em,jmp_pos,VM_JEPT,emitter_label(em));
    jmp_pos = -1;
    /* Now generate code for else branch */
    CALLE(parse_scope(p,em,tk,1));
  }
  EXPECT(TK_ENDFOR); tk_consume(tk);
  EXPECT(TK_RSTMT); tk_consume(tk);

  /* We may not enter into an else scope, so it is possible
   * that the jmp_pos is not patched correctly here . We
   * need to patch jmp_pos if we don't have else scope as
   * well */
  if( jmp_pos > 0 ) {
    emit1_at(em,jmp_pos,VM_JEPT,emitter_label(em));
  }

  return 0;
}

/* Parsing the for loop body.
 * This function act as a parse prolog by only parsing the symbol inside of
 * for loop : {% for symbol1,symbol2 in ...
 * Rest of the parse code is in parse_for_body. For symbol that has name "_"
 * we treat it is not used there. */
static
int parse_for( struct parser* p , struct emitter* em , struct tokenizer* tk ) {
  int idx;
  struct string key = NULL_STRING;
  struct string val = NULL_STRING;

  assert(tk->tk == TK_FOR);
  tk_consume(tk);
  /* We don't support tuple in our code. So we don't really have unpacked
   * semantic here. We support following semantic :
   * value
   * key
   * _,value
   * key,_
   * So at most 2 symbols can be provided here */
  EXPECT(TK_VARIABLE);
  strbuf_move(&(tk->lexme),&key);
  tk_consume(tk);
  if( tk->tk == TK_COMMA ) {
    tk_consume(tk);
    EXPECT(TK_VARIABLE);
    strbuf_move(&(tk->lexme),&val);
    tk_consume(tk);
  } else {
    val = key;
    key = NULL_STRING;
  }
  EXPECT(TK_IN);
  tk_consume(tk);
  /* Ignore underscore since they serves as placeholder */
  if( strcmp(key->str,"_") == 0 ) {
    string_destroy(&key);
    key = NULL_STRING;
  }
  if( strcmp(val->str,"_") == 0 ) {
    string_destroy(&val);
    val = NULL_STRING;
  }
  return parse_for_body(p,em,tk,&key,&val);
}

/* Call
 * We don't support passing function from MACRO to CALL, since this looks wired
 * and also makes implementation complicated because it introduces some specific
 * situations.
 */
static int
parse_call( struct parser* p , struct emitter* em , struct tokenizer* tk ) {
  struct string name;
  struct string text;
  int text_idx;
  int caller_idx;
  int vm_lstr = -1;
  int vm_upvalue_set = -1;
  assert(tk->tk == TK_CALL);
  tk_consume(tk);
  EXPECT(TK_VARIABLE);
  CALLE(symbol(p,tk,&name));
  tk_consume(tk);

  /* Because we cannot know the text we want to insert as an upvalue
   * before parsing the function call. We need to reserve instructions
   * spaces in current code page.
   * VM_LSTR(idx);
   * VM_UPVALUE_SET(idx); */
  vm_lstr = emitter_put(em,1);
  vm_upvalue_set = emitter_put(em,1);
  /* Now we start to parse the func call */
  CALLE(parse_funccall_or_pipe(p,em,tk,&name,0));
  EXPECT(TK_RSTMT);tk_consume(tk);
  EXPECT(TK_TEXT);
  /* Get text index in string pool */
  strbuf_move(&(tk->lexme),&text);
  tk_consume(tk);
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
  CALLE(finish_scope_tag(p,tk,TK_ENDCALL));
  return 0;
}

/* Filter Scope */
static int
parse_filter( struct parser* p , struct emitter* em , struct tokenizer* tk ) {
  struct string name;
  struct string text;
  int vm_lstr = -1;
  int text_idx;

  assert(tk->tk == TK_FILTER);
  tk_consume(tk);
  CALLE(symbol(p,tk,&name));
  tk_consume(tk);
  vm_lstr = emitter_put(em,1);

  if( tk->tk == TK_LPAR ) {
    CALLE(parse_funccall_or_pipe(p,em,tk,&name,1));
  } else {
    CALLE(parse_pipecmd(p,em,tk,&name));
  }
  EXPECT(TK_RSTMT); tk_consume(tk);
  strbuf_move(&(tk->lexme),&text);
  CALLE((text_idx=const_str(p,em->prg,&text,1))<0);
  emit1_at(em,vm_lstr,VM_LSTR,text_idx);
  CALLE(finish_scope_tag(p,tk,TK_ENDFILTER));
}

/* Set Scope */
static
int parse_assign( struct parser* p , struct emitter* em ,
    struct tokenizer* tk , int idx ) {
  assert(tk->tk == TK_ASSIGN);
  tk_consume(tk);
  CALLE(parse_expr(p,em,tk));
  if( idx >= 0 ) {
    /* not new variable */
    emit1(em,VM_BSTORE,var_idx);
  }
  return 0;
}

static
int parse_set( struct parser* p, struct emitter* em , struct tokenizer* tk ) {
  int ret = 0;
  int var_idx;

  assert( tk->tk == TK_SET );
  tk_consume(tk);

  EXPECT(TK_VARIABLE);
  CALLE((var_idx=lex_scope_set(p,strbuf_tostring(tk->lexme)))==-2);
  tk_consume(tk); /* Move forward */

  /* check if we have an pending expression or just end of the SET scope*/
  if( tk->tk == TK_RSTMT ) {
    int txt_idx;
    /* end of the set scope , so it is a scop based set */
    tk_consume(tk);
    EXPECT(TK_TEXT);
    CALLE((txt_idx=const_str(p,em->prg,strbuf_move(&(tk->lexme)),1))<0);
    tk_consume(tk);
    /* load the text on to stack */
    emit1(em,VM_LSTR,txt_idx);
    /* we need to move this static text into the position of that
     * variable there */
    if( var_idx >= 0 ) {
      /* This is a setting operations, since this symbol is somehow been used */
      emit1(em,VM_BSTORE,var_idx);
    } else {
      /* This is a no-op since the current top of the stack has been
       * assigend to that local symbol */
    }
    /* set up the var based on the stack */
    CALLE(finish_scope_tag(p,tk,TK_SET));
    return 0;
  } else if( tk->tk == TK_ASSIGN ) {
    CALLE(parse_assign(p,em,tk,var_idx));
    EXPECT(TK_RSTMT); tk_consume(tk);
    return 0;
  } else {
    report_errpr(p,tk,"Set scope either uses \"=\" to indicate a one line assignment,\
        or uses a scope based set!");
    return -1;
  }
}

/* Do */
static int
parse_do( struct parser* p , struct emitter* em , struct tokenizer* tk ) {
  assert( tk->tk == TK_DO );
  tk_consume(tk);
  CALLE(parse_expr(p,em,tk));
  EXPECT(TK_RSTMT);
  tk_consume(tk);
  return 0;
}

/* Loop control statements.
 * Our loop body is actually compiled into a function, and later on the
 * code will call into this function each time. To control this sort of
 * thing , we will use return value from that function. The trick is as
 * follow:
 * The general loop body will end up with returning LOOP_CONTINUE, which
 * is actually 0.
 * The continue will generate a return LOOP_CONTINUE same as the normal
 * exit , and the break will generate return LOOP_BREAK which has value
 * 1. Using this protocol, the VM could know how to handle the break or
 * continue of the loop body.
 */

static int
parse_continue( struct parser* p , struct emitter* em , struct tokenizer* tk ) {
  assert(tk->tk == TK_CONTINUE);
  tk_consume(tk);
  emit1(em,VM_LIMM,LOOP_CONTINUE);
  emit0(em,VM_RET);
  EXPECT(TK_RSTMT);
  tk_consume(tk);
  return 0;
}

static int
parse_break( struct parser* p , struct emitter* em , struct tokenizer* tk ) {
  assert(tk->tk == TK_BREAK );
  tk_consume(tk);
  emit1(em,VM_LIMM,LOOP_BREAK);
  emit0(em,VM_RET);
  EXPECT(TK_RSTMT);
  tk_consume(tk);
  return 0;
}

/* With statment.
 * For with statment, we will generate a lex_scope for it */
static int
parse_with( struct parser* p , struct emitter* em , struct tokenizer* tk ) {
  assert(tk->tk == TK_WITH);
  tk_consume(tk);

  if( tk->tk == TK_VARIABLE ) {
    /* We have shortcut writing, so we need to generate a new lexical
     * scope */
    CALLE(lex_scope_enter(p) != NULL);
    CALLE(parse_assign(p,em,tk));
    EXPECT(TK_RSTMT);
    tk_consume(tk);
    /* Now parsing the whole scope body */
    CALLE(parse_scope(p,em,tk,0));
    EXPECT(TK_ENDWITH); tk_consume(tk);
    EXPECT(TK_RSTMT); tk_consume(tk);
    /* exit the lexical scope */
    lex_scope_exit(p,em,tk);
  } else {
    EXPECT(TK_RSTMT); tk_consume(tk);
    CALLE(parse_scope(p,em,tk,1));
    EXPECT(TK_ENDWITH);tk_consume(tk);
    EXPECT(TK_RSTMT);tk_consume(tk);
  }
  return 0;
}

/* Include
 * Include allows user to include another rendered template. But, we don't
 * support any benaivor specifier from Jinja2. However, we have one extension
 * that allows the user to setup context variable/upvalue for rendering that
 * specific included template which allows user to do customization.
 * {% include template_name upvalue %}
 *   {% upvalue name expression (override)/(fix) %}
 *   {% upvalue name expression (voerride)/(fix) %}
 * {% endinclude %}
 * Or the old way to do include like this:
 * {% include template_name %}
 *
 * Implementation notes:
 * Well, we can just setup the up value in the current code page and then
 * we are done, but we don't do this since potentially user may want to
 * override these values in some other ways which we may not know. So we
 * pass upvalue as parameters to builtin function __include__ to let it
 * decide what behavior it wants to achieve. __include__ just need to return
 * me a string text for rendered , then its done */

static int
parse_include( struct parser* p , struct emitter* em , struct tokenizer* tk ) {
  int cnt = 1;

  assert(tk->tk == TK_INCLUDE);
  tk_consume(tk);
  CALLE(parse_expr(p,em,tk));
  if( tk->tk == TK_UPVALUE ) {
    /* We have an up value scope */
    tk_consume(tk);
    EXPECT(TK_RSTMT);tk_consume(tk);
    do {
      EXPECT(TK_LSTMT); tk_consume(tk);
      if( tk->tk == TK_UPVALUE ) {
        int var_idx;
        struct string var;
        int imm;

        tk_consume(tk);
        EXPECT(TK_VARIABLE);
        CALLE(symbol(p,em,&var));
        CALLE((var_idx=const_str(p,em->prg,&var,1))<0);
        emit1(em,VM_LSTR,var_idx); /* Load the upvalue name onto the stack */
        CALLE(parse_expr(p,em,tk));/* Generate the value corresponding to that upvalue */
        if( tk->tk == TK_FIX ) {
          imm = 1;
          tk_consume(tk);
        } else {
          if( tk->tk == TK_OVERRIDE ) {
            imm = 0;
            tk_consume(tk);
          } else if( tk->tk == TK_RSTMT ) {
            imm = 0;
          } else {
            report_errpr(p,tk,"Upvalue specifier can only be override/fix!");
            return -1;
          }
          EXPECT(TK_RSTMT); tk_consume(tk);
          emit1(em,VM_LIMM,imm);
          cnt += 3;
        }
      } else if( tk->tk == TK_ENDINCLUDE ) {
        break;
      } else {
        report_errpr(p,tk,"Unknown token:%s!",tk_get_name(tk->tk));
        return -1;
      }
    } while(1);
    /* generate the calling instructions */
    emit1(em,VM_INCLUDE,cnt);
    tk_consume(tk); /* eat TK_ENDINCLUDE */
    EXPECT(TK_RSTMT); tk_consume(tk);
    return 0;
  } else {
    EXPECT(TK_RSTMT);tk_consume(tk);
    emit1(em,VM_INCLUDE,cnt);
  }
  return 0;
}

/* Import
 * Import has 2 types, one starts with import and the other starts with from.
 * These 2 will result in different VM instruction generated. The intenral
 * object representation is kind of complicated for import , since we need to
 * introduce new symbol into the current scope. The new symbol is introduced
 * as upvalue */
static int
parse_import( struct parser* p , struct emitter* em , struct tokenizer* tk ) {
  struct string name;
  int name_idx;

  assert(tk->tk == TK_IMPORT);
  tk_consume(tk);
  /* Get the filepath as an expression */
  CALLE(parse_expr(p,em,tk));
  EXPECT(TK_AS); tk_consume(tk);
  /* Get the symbol name onto stack as a string literal */
  CALLE(symbol(p,tk,&name));
  tk_consume(tk);
  /* Get the index */
  CALLE((name_idx=const_str(p,em->prg,&name,1))<0);
  emit1(em,VM_IMPORT,name_idx);

  EXPECT(TK_RSTMT); tk_consume(tk);
  return 0;
}

static int
parse_from( struct parser* p , struct emitter* em , struct tokenizer* tk ) {
  struct string name;
  int name_idx;
  int alias_cnt = 0;

  assert(tk->tk == TK_FROM);
  tk_consume(tk);

  CALLE(parse_expr(p,em,tk));
  EXPECT(TK_IMPORT);tk_consume(tk);

  EXPECT(TK_VARIABLE);
  CALLE(symbol(p,tk,&name));
  tk_consume(tk);

  CALLE((name_idx=const_str(p,em->prg,&name,1))<0);
  /* For import_from, we need to put 2 defualt parameter on to the stack.
   * One is the template filepath, the other is the symbol name, later on
   * we need to set up upvalue based on the symbol name */
  emit1(em,VM_LSTR,name_idx);

  /* Now we start to parse the alias name */
  EXPECT(TK_AS); tk_consume(tk);

  do {
    struct string alias;
    int alias_idx;
    /* at least one symbol is needed */
    EXPECT(TK_VARIABLE); tk_consume(tk);
    CALLE(symbol(p,tk,&alias));
    CALLE((alias_idx = const_str(p,em->prg,&alias,1))<0);
    /* load it into stack */
    emit1(em,VM_LSTR,alias_idx);
    ++alias_cnt;
    if( tk->tk == TK_COMMA ) {
      tk_consume(tk);
      continue;
    } else if( tk->tk == TK_RSTMT ) {
      tk_consume(tk);
      break;
    } else {
      report_errpr(p,tk,"Unknown token:%s,expect \",\" or \"%}\"!",
          tk_get_name(tk->tk));
      return -1;
    }
  } while(1);

  /* Generate the import instructions */
  emit1(em,VM_IMPORT_SYMBOL,alias_cnt);
  return 0;
}


































