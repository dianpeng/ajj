#include "ajj-priv.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>

/* emitter for byte codes */
struct emitter {
  struct program* prg;
  size_t cd_cap;
};

struct parser {
  struct ajj* a;
  struct ajj_object* tpl;
  unsigned short unname_cnt;
};

static struct string THIS = { "this" , 4 };

#define MINIMUM_CODE_PAGE_SIZE 256

static inline
void reserve_code_page( struct emitter* em , size_t cap ) {
  void* nc;
  assert( em->cd_cap < cap );
  if( em->prg->len == 0 ) {
    /* for the first time, we allocate a large code page */
    cap = MINIMUM_CODE_PAGE_SIZE;
  }
  nc = malloc(cap);
  if( em->prg->codes ) {
    memcpy(nc,em->prg->codes,em->prg->len);
    free(em->prg->codes);
  }
  em->cd_cap = cap;
}


static inline
void emit0( struct emitter* em , int bc ) {
  if( em->cd_cap <= em->prg->len + 9 ) {
    reserve_code_page(em,2*em->cd_cap);
  }
  *((unsigned char*)(em->prg->codes) + em->prg->len)
    = (unsigned char)(bc);
  ++(em->prg->len);
}

static inline
void emit_int( struct emitter* em , int arg ) {
  int l;
  assert( em->cd_cap > em->prg->len + 4 ); /* ensure we have enough space to run */
  l = (em->prg->len) & 4;
  switch(l) {
    case 0:
      *((int*)(em->prg->codes) + em->prg->len) = arg;
      break;
    case 1:
      {
        int lower = (arg & 0xff000000) >> 24;
        int higher= (arg & 0x00ffffff);
        *((unsigned char*)(em->prg->codes) + em->prg->len)
          = (unsigned char)lower;
        *((int*)((unsigned char*)(em->prg->codes) + em->prg->len+1))
          = higher;
        break;
      }
    case 2:
      {
        int lower = (arg & 0xffff0000) >> 16;
        int higher= (arg & 0x0000ffff);
        *((int*)((unsigned char*)(em->prg->codes) + em->prg->len))
          = lower;
        *((int*)((unsigned char*)(em->prg->codes) + em->prg->len+2))
          = higher;
        break;
      }
    case 3:
      {
        int lower = (arg & 0xffffff00) >> 8;
        int higher= (arg & 0x000000ff);
        *((int*)((unsigned char*)(em->prg->codes) + em->prg->len))
          = lower;
        *((int*)((unsigned char*)(em->prg->codes) + em->prg->len+3))
          = higher;
        break;
      }
    default:
      UNREACHABLE();
      break;
  }
  em->prg->len += 4;
}

static inline
void emit1( struct emitter* em , int bc , int a1 ) {
  emit0(em,bc);
  emit_int(em,a1);
}

static inline
void emit2( struct emitter* em , int bc , int a1 , int a2 ) {
  emit1(em,bc,a1);
  emit_int(em,a2);
}

static inline
int put( struct emitter* em , int arg_sz ) {
  int ret;
  size_t add;
  assert( arg_sz == 0 || arg_sz == 1 || arg_sz == 2 );
  ret = em->prg->len;
  add = arg_sz * 4 + 1;
  if( em->cd_cap <= em->prg->len + add ) {
    reserve_code_page(em,em->cd_cap * 2 );
  }
  em->prg->len += add;
  return ret;
}

static inline
void emit0_at( struct emitter* em , int pos , int bc ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emit0(em,bc);
  em->prg->len = save;
}

static inline
void emit1_at( struct emitter* em , int pos , int bc , int a1 ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emit1(em,bc,a1);
  em->prg->len = save;
}

static inline
void emit2_at( struct emitter* em , int pos , int bc , int a1 , int a2 ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emit2(em,bc,a1,a2);
  em->prg->len = save;
}

static inline
int emitter_label( struct emitter* em ) {
  return (int)(em->prg->len);
}

static
void report_error( struct parser* , const char* , ... );

static
int random_name( struct parser* p , char name[AJJ_SYMBOL_NAME_MAX_SIZE] ) {
  if(p->unname_cnt == USHRT_MAX) {
    report_error(p,"Too much scope/blocks,more than:%d!",USHRT_MAX);
    return -1;
  }
  sprintf(name,"@%d",p->unname_cnt);
  ++p->unname_cnt;
  return 0;
}

static
int const_str( struct parser* p , struct program* prg, struct string str , int own ) {
  if( str.len > 128 ) {
insert:
    if( prg->str_len == AJJ_LOCAL_CONSTANT_SIZE ) {
      report_error(p,"Too much local string literals!");
      if( own ) free(str.str); /* own this string */
      return -1;
    } else {
      if(own) {
        prg->str_tbl[prg->str_len] = str;
      } else {
        prg->str_tbl[prg->str_len].str = strdup(str.str);
        prg->str_tbl[prg->str_len].len = str.len;
      }
      return prg->str_len++;
    }
  } else {
    /* do a linear search here */
    size_t i = 0 ;
    for( i ; i < prg->str_len ; ++i ) {
      if( prg->str_tbl[i].len == str.len &&
          strcmp(prg->str_tbl[i].str,str.str) == 0 ) {
        if(own) free(str.str);
        return i;
      }
    }
    goto insert;
  }
}

static inline
int const_num( struct parser* p , struct program* prg, double num ) {
  if( prg->str_len == AJJ_LOCAL_CONSTANT_SIZE ) {
    report_error(p,"Too much local number literals!");
    return -1;
  } else {
    prg->num_tbl[prg->str_len] = num;
    return prg->num_len++;
  }
}
/* do the parse */
static
int parse_expr( struct parser* , struct emitter* , struct tokenizer* );

#define EXPECT(TK) \
  do { \
    if( tk->tk != (TK) ) { \
      report_error(p,"Unexpected token :%s expect :%s", \
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

#define CALLC(P,C) \
  do { \
    if( (P) ) { \
      C; \
      return -1; \
    } \
  } while(0)


/* constant */

/* Parsing a list literals into an object at top of the stack. */
static
int parse_list( struct parser* p , struct emitter* em , struct tokenizer* tk ) {

}

static
int parse_dict( struct parser* p , struct emitter* em , struct tokenizer* tk ) {

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
int parse_var( struct parser* p, struct emitter* em , struct tokenizer* tk ) {
  int idx;
  assert(tk->tk == TK_VARIABLE);
  CALLE((idx=const_str(p->a,em->prg,strbuf_move(&(tk->lexme)),1))<0);
  emit1(em,VM_VAR_LOAD,idx);
  return 0;
}

static
int parse_attr( struct parser* p, struct emitter* em , struct tokenizer* tk,
    struct string prefix /* owned */) {
  int idx;
  int comp_tk;

  assert( tk->tk == TK_LSQR || tk->tk == TK_DOT );
  if( prefix.str != NULL ) {
    /* We have a symbol name as prefix, we need to load the object
     * whose name is this prefix on to the stack first and then parse
     * the expression and then retrieve the attributes */
    CALLE((idx=const_str(p,em->prg,prefix,1))<0);
    /* Emit code load this object on the stack */
    emit1(em,VM_VAR_LOAD,idx);
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
    emit1(em,VM_ATTR_LOAD,idx); /* look up the attributes */
  }

  /* Now try to parse the end of this component lookup */
  if( comp_tk == TK_LSQR ) {
    EXPECT(TK_RSQR);
    tk_consume(tk);
  }
  return 0;
}

static
int parse_funccall( struct parser* p , struct emitter* em ,
    struct tokenizer* tk , struct string prefix , int pipe ) {
  int idx;
  int num = pipe; /* If this function call is a pipe call, then pipe will set to 1.
                   * which makes our function has one more parameters comes from
                   * pipe. But the caller of this function must set up the code
                   * correctly. [ object , pipe_data ]. This typically involes prolog
                   * for moving data around */
  assert(tk->tk == TK_LPAR);
  tk_consume(tk);
  if( prefix.str != NULL ) {
    CALLE((idx=const_str(p,em->prg,prefix,1))<0);
    /* NOTES: When we have the prefix as a input for funccall, it means we
     * are calling a free function. But in our VM, we can only do
     * member function (method) calling, it means we need to load the
     * current scope onto the stack. This instruction is emitted by
     * our callee parse_prefix function*/
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
          report_error(p,"Function call must be closed by \")\"!");
          return -1;
        }
      }
    } while(1);
  }
  emit1(em,VM_LIMM,num); /* load the number of function parameter on to stack */
  emit1(em,VM_CALL,idx); /* call the function based on current object */
  return 0;
}

static inline
int parse_prefix( struct parser* p, struct emitter* em , struct tokenizer* tk ) {
  struct string prefix;
  assert(tk->tk == TK_VARIABLE);
  prefix = strbuf_move(&(tk->lexme));
  if( tk->tk == TK_RPAR ) {
    emit0(em,VM_SCOPE); /* load scope on stack */
    CALLE(parse_funccall(p,em,tk,prefix,0));
  } else {
    if( tk->tk == TK_DOT || tk->tk == TK_LSQR ) {
      CALLE(parse_attr(p,em,tk,prefix));
    }
  }
  do {
    if( tk->tk == TK_RPAR ) {
      CALLE(parse_funccall(p,em,tk,NULL_STRING,0));
    } else if( tk->tk == TK_DOT || tk->tk == TK_LSQR ) {
      CALLE(parse_attr(p,em,tk,NULL_STRING));
    } else if( tk->tk == TK_PIPE ) {
      /* Now we emit the pipe prolog:
       * If we reach here, we know that on our stack, the previous
       * call's result are on top of the stack, but it must be used
       * as the first parameter of the pipe functions. And also, we
       * need to push the current scope object onto the stack as well.
       * This can be achieved by a special instruction. VM_PIPE */
      emit0(em,VM_PIPE);
      /* Now we try to check wether the pipe operator follows a function
       * call or not. A pipe operator can only fllows a function call !*/
      tk_consume(tk);
      EXPECT( TK_VARIABLE );
      prefix = strbuf_move(&(tk->lexme));
      tk_consume(tk);
      if( tk->tk == TK_LPAR ) {
        /* Now call the followed functions */
        CALLE(parse_funccall(p,em,tk,prefix,1));
      } else {
        /* a sinlge pipe command as shortcut */
        int idx;
        CALLE((idx=const_str(p,em->prg,prefix,1))<0);
        emit1(em,VM_LIMM,1);
        emit1(em,VM_CALL,idx);
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
      report_error(p,"Unexpect token here:%s",tk_get_name(tk->tk));
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
      report_error(p,"Too much unary operators, more than 1024!"); \
      return -1; \
    } \
    op[op_len++] = (T); \
  } while(0)

  do {
    switch(tk->tk) {
      case TK_NOT:
        PUSH_OP(TK_NOT);
        break;
      case TK_SUB:
        PUSH_OP(TK_SUB);
        break;
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
      case TK_MUL:
      case TK_DIV:
      case TK_DIVTRUCT:
      case TK_MOD:
      case TK_POW:
        op = tk->tk;
        break;
      default:
        goto done;
    }
    CALLE(parse_unary(p,em,tk));
    switch(op) {
      case TK_MUL:
        emit0(em,VM_MUL);
        break;
      case TK_DIV:
        emit0(em,VM_DIV);
        break;
      case TK_DIVTRUCT:
        emit0(em,VM_DIVTRUCT);
        break;
      case TK_MOD:
        emit0(em,VM_MOD);
        break;
      case TK_POW:
        emit0(em,VM_POW);
        break;
      default:
        UNREACHABLE();
        return -1;
    }
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
      case TK_ADD:
      case TK_SUB:
        op = tk->tk;
      default: goto done;
    }
    CALLE(parse_factor(p,em,tk));
    switch(op) {
      case TK_ADD:
        emit0(em,VM_ADD);
        break;
      case TK_SUB:
        emit0(em,VM_SUB);
        break;
      default:
        UNREACHABLE();
        return -1;
    }
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
      case TK_EQ:
      case TK_NE:
      case TK_LT:
      case TK_LE:
      case TK_GT:
      case TK_GE:
        op = tk->tk;
        break;
      default:
        goto done;
    }
    CALLE(parse_term(p,em,tk));
    switch(op) {
      case TK_EQ:
        emit0(em,VM_EQ);
        break;
      case TK_NE:
        emit0(em,VM_NE);
        break;
      case TK_LT:
        emit0(em,VM_LT);
        break;
      case TK_LE:
        emit0(em,VM_LE);
        break;
      case TK_GT:
        emit0(em,VM_GT);
        break;
      case TK_GE:
        emit0(em,VM_GE);
        break;
      default:
        UNREACHABLE();
        return -1;
    }
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
      case TK_AND:
      case TK_OR:
        op = tk->tk;
        break;
      default:
        goto done;
    }
    CALLE(parse_cmp(p,em,tk));
    switch(op) {
      case TK_AND:
        emit0(em,VM_AND);
        break;
      case TK_OR:
        emit0(em,VM_OR);
        break;
      default:
        UNREACHABLE();
        return -1;
    }
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
    emit2(em,VM_SWAP,-1,-2); /* The stack is growing up , ESP(assume we have) is
                              * pointing to the first unused element in the stack,
                              * and we use ESP to calculate the position of stack,
                              * so ESP-1 : is the top most used element, the ESP-2
                              * is the second most used element. */
    EXPECT(TK_ELSE);
    tk_consume(tk);

    CALLE(parse_logic(p,em,tk));
    emit0(em,VM_TENARY);
  }
}

/* this function load this pointer onto stack. The this pointer represent this template
 * objects. It is set up manually during the parsing phase */
static inline
int load_this( struct parser* p,
    struct emitter* em ,
    struct tokenizer* tk ) {
  int idx;
  UNUSE_ARG(tk);
  CALLE((idx=const_str(p,THIS,0))<0);
  emit1(em,VM_VAR_LOAD,idx);
  return 0;
}

static
int finish_scope_tag( struct parser* p,
    struct emitter* em ,
    struct tokenizer* tk ,
    int token  ) {
  UNUSE_ARG(em);

  EXPECT(TK_LSTMT);
  tk_consume(tk);

  EXPECT(token);
  tk_consume(tk);

  EXPECT(TK_RSTMT);
  tk_consume(tk);

  return 0;
}

/* structures */

static
int parse_scope( struct parser* ,
    struct emitter* em , struct tokenizer* tk , int gen_scope );

/* parse the following scope as a unnamed closure*/
static
int parse_set( struct parser* p,
    struct emitter* em , struct tokenizer* tk ) {
  int idx;
  assert( tk->tk == TK_SET );
  tk_consume(tk);

  EXPECT(TK_VARIABLE);
  CALLE((idx=const_str(p,em->prg,strbuf_move(&(tk->lexme)),1))<0);

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

    /* set up the var based on the stack */
    emit1(em,VM_VAR_SET,idx);
    CALLE(finish_scope_tag(p,em,tk,TK_SET));
    return 0;
  } else if( tk->tk == TK_ASSIGN ) {
    tk_consume(tk);
    CALLE(parse_expr(p,em,tk));
    emit1(em,VM_VAR_SET,idx);
    return 0;
  } else {
    report_error(p,"Set scope either uses \"=\" to indicate a one line assignment,\
        or uses a scope based set!");
    return -1;
  }
}

static
int parse_do( struct parser* p,
    struct emitter* em , struct tokenizer* tk ) {
  assert( tk->tk == TK_DO );
  tk_consume(tk);

  CALLE(parse_expr(p,em,tk));

  /* We left the expression output on the stack, which is not
   * what we want. Therefore we emit a VM_POP instructions to
   * clear the stack */
  emit0(em,VM_POP);

  EXPECT(TK_RSTMT);
  tk_consume(tk);
  return 0;
}

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
      report_error(p,"Too much if-elif-else branch, more than 1024!"); \
      return -1; \
    } \
    end_jmp[jmp_sz++] = (P); \
  } while(0)

  assert( tk->tk == TK_IF );
  tk_consume(tk);

  CALLE(parse_expr(p,em,tk));

  /* condition failed jump */
  cond_jmp = put(em,1);

  EXPECT(TK_RSTMT);
  tk_consume(tk);

  CALLE(parse_scope(p,em,tk,1));

  /* jump out of the scope */
  PUSH_JMP(put(em,1));

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
            report_error(p,"Expect endfor since else tag is seen before!");
            return -1;
          }

          tk_consume(tk);
          assert(cond_jmp > 0 );

          /* modify the previous conditional jmp */
          emit1_at(em,cond_jmp,VM_JMP,emitter_label(em));
          CALLE(parse_expr(p,em,tk));

          cond_jmp = put(em,1);
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
        report_error(p,"Unexpected token in branch scope:%s",
            tk_get_name(tk->tk));
        return -1;
    }
    CALLE(parse_scope(p,em,tk,1));
    if( !has_else )
      PUSH_JMP(put(em,1));
  } while(1);
done:
  /* patch all rest of the cool things */
  for( i = 0 ; i < jmp_sz ; ++i ) {
    emit1_at(em,end_jmp[i],VM_JMP,emitter_label(em));
  }
  return 0;
}
#undef PUSH_JMP

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

static inline
int parse_loop_filter( struct parser* p , struct emitter* em ,
    struct tokenizer* tk ) {
  /* Filter for loop is like this: if condition */
  if( tk->tk == TK_IF ) {
    tk_consume(tk);
    CALLE(parse_loop_cond(p,em,tk));
  }
  return 0;
}

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
    struct string key,
    struct string val ) {
  char name[AJJ_SYMBOL_NAME_MAX_SIZE]; /* random name buffer */
  int idx;
  int kv_idx;
  struct emitter cl_em; /* new program's emitter */
  struct program* new_prg; /* new program */
  int recur = 0;
  int jmp_pos;

  /* get unique name */
  CALLE(random_name(p,name));
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
  jmp_pos = put(em,1); /* setup the tag for jmp */

  /* We cannot generate VM_LOOP/VM_LOOPREC until we see the recursive
   * keyword. However we could defer this operation until we finish the
   * potential filter. The reason is that those are 2 different code
   * buffers. */

  /* Create a new program into the existed objects */
  assert( p->tpl->tp == AJJ_VALUE_OBJECT );
  assert( p->tpl->val.obj.fn_tb != NULL  );
  /* Set up the new program scopes */
  cl.prg = new_prg = func_table_add_jj_block(tb,name);
  /* Add function parameters here */
  if( key_name.str ) {
    CALLE((kv_idx=const_str(p,new_prg,key_name,1))<0);
    CALLE(program_add_par(new_prg,kv_idx,AJJ_NONE));
  }
  if( val_name.str ) {
    CALLE((kv_idx=const_str(p,new_prg,val_name,1))<0);
    CALLE(program_add_par(new,kv_idx,AJJ_NONE));
  }
  /* Generate code for filters */
  CALLE(parse_loop_filter(p,&cl_em,tk));

  /* Now we should left with keyword recursive or %} */
  if( tk->tk == TK_RECURSIVE ) {
    recur = 1;
    tk_consume(tk);
  }
  EXPECT(TK_RSTMT);
  tk_consume(tk);

  /* parse the closure itself */
  CALLE(parse_body(p,&cl_em,tk,0));

  /* generate epilog for function call */
  emit0(&cl_em,VM_RET);

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
  EXPECT(TK_ENDFOR);
  tk_consume(tk);

  EXPECT(TK_RSTMT);
  tk_consume(tk);

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
 * This function act as a parser prolog by only parsing the symbol inside of
 * for loop : {% for symbol1,symbol2 in ...
 * Rest of the parser code is in parse_for_body. For symbol that has name "_"
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
  key = strbuf_move(&(tk->lexme));
  tk_consume(tk);
  if( tk->tk == TK_COMMA ) {
    tk_consume(tk);
    EXPECT(TK_VARIABLE);
    val = strbuf_move(&(tk->lexme));
    tk_consume(tk);
  } else {
    val = key;
    key = NULL_STRING;
  }
  EXPECT(TK_IN);
  tk_consume(tk);
  return parse_for_body(p,em,tk,key,val);
}















