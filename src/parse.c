#include "ajj-priv.h"
#include <stdlib.h>
/* emitter for byte codes */
struct emitter {
  struct program* prg;
  size_t cd_cap;
};

static inline
void reserve_code_page( struct emitter* em , size_t cap ) {
  void* nc;

  assert( em->cd_cap < cap );
  nc = malloc(cap);

  if( em->prg->codes ) {
    memcpy(nc,em->prg->codes,em->prg->len);
    free(em->prg->codes);
  }

  em->cd_cap = cap;
}

#define MINIMUM_CODE_PAGE_SIZE 1024 /* 1KB */

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
int label( struct emitter* em ) {
  return (int)(em->prg->len);
}

static
void report_error( struct ajj* , const char* , ... );

static
int const_str( struct ajj* a , struct program* prg, struct string str , int own ) {
  if( str.len > 128 ) {
insert:
    if( prg->str_len == AJJ_LOCAL_CONSTANT_SIZE ) {
      report_error(a,"Too much local string literals!");
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
int const_num( struct ajj* a , struct program* prg, double num ) {
  if( prg->str_len == AJJ_LOCAL_CONSTANT_SIZE ) {
    report_error(a,"Too much local number literals!");
    return -1;
  } else {
    prg->num_tbl[prg->str_len] = num;
    return prg->num_len++;
  }
}
/* do the parse */
static
int parse_expr( struct ajj* , struct emitter* , struct tokenizer* );

#define EXPECT(TK) \
  do { \
    if( tk->tk != (TK) ) { \
      report_error(a,"Unexpected token :%s expect :%s", \
          tk_get_name(tk->tk), \
          tk_get_name((TK))); \
      return -1; \
    } \
  } while(0)


/* constant */
static
int parse_const( struct ajj* a, struct emitter* em , struct tokenizer* tk ) {
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
      if((idx = const_num(a,em->prg,tk->num_lexme)) <0) {
        return -1;
      }
      emit1(em,VM_LNUM,idx);
      break;
    case TK_STRING:
      if((idx = const_str(a,em->prg,strbuf_move(&(tk->lexme)),1)) <0) {
        return -1;
      }
      emit1(em,VM_LSTR,idx);
      break;
    default:
      UNREACHABLE();
      return -1;
  }
  tk_consume(tk);
  return 0;
}

static inline
int parse_var( struct ajj* a , struct emitter* em , struct tokenizer* tk ) {
  int idx;
  assert(tk->tk == TK_VARIABLE);
  if( (idx = const_str(a,em->prg,strbuf_move(&(tk->lexme)),1)) <0 ) {
    return -1;
  }
  emit1(em,VM_VAR_LOAD,idx);
  return 0;
}

static
int parse_attr( struct ajj* a , struct emitter* em , struct tokenizer* tk,
    struct string prefix /* owned */) {
  int idx;
  int comp_tk;

  assert( tk->tk == TK_LSQR || tk->tk == TK_DOT );
  if( prefix.str != NULL ) {
    /* We have a symbol name as prefix, we need to load the object
     * whose name is this prefix on to the stack first and then parse
     * the expression and then retrieve the attributes */
    if((idx = const_str(a,em->prg,prefix,1)) <0) {
      return -1;
    }
    /* Emit code load this object on the stack */
    emit1(em,VM_VAR_LOAD,idx);
  }

  /* Move tokenizer */
  comp_tk = tk->tk;
  tk_consume(tk);

  if( comp_tk == TK_LSQR ) {
    if( parse_expr(a,em,tk) )
      return -1;
  } else {
    EXPECT(TK_VARIABLE);
    if( (idx = const_str(a,em->prg,prefix,strbuf_move(&(tk->lexme)),1)) < 0 ) {
      return -1;
    }
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
int parse_funccall( struct ajj* a, struct emitter* em ,
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
    if( (idx = const_str(a,em->prg,prefix,1)) < 0 ) {
      return -1;
    }
    /* NOTES: When we have the prefix as a input for funccall, it means we
     * are calling a free function. But in our VM, we can only do
     * member function (method) calling, it means we need to load the
     * current scope onto the stack. This instruction is emitted by
     * our callee parse_prefix function*/
  }

  /* Populating the function parameters */
  if( tk->tk != TK_RPAR ) {
    do {
      if( parse_expr(a,em,tk) )
        return -1;
      ++num;
      if( tk->tk == TK_COMMA ) {
        tk_consume(tk);
      } else {
        if( tk->tk == TK_RPAR ) {
          tk_consume(tk);
          break;
        } else {
          report_error(a,"Function call must be closed by \")\"!");
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
int parse_prefix( struct ajj* a , struct emitter* em , struct tokenizer* tk ) {
  struct string prefix;
  struct string null_prefix;
  null_prefix.str = NULL;
  null_prefix.len = 0;

  assert(tk->tk == TK_VARIABLE);
  prefix = strbuf_move(&(tk->lexme));

  if( tk->tk == TK_RPAR ) {
    emit0(em,VM_SCOPE); /* load scope on stack */
    if(parse_funccall(a,em,tk,prefix,0))
      return -1;
  } else {
    if( tk->tk == TK_DOT || tk->tk == TK_LSQR ) {
      if(parse_attr(a,em,tk,prefix))
        return -1;
    }
  }

  do {
    if( tk->tk == TK_RPAR ) {
      if(parse_funccall(a,em,tk,null_prefix,0))
        return -1;
    } else if( tk->tk == TK_DOT || tk->tk == TK_LSQR ) {
      if(parse_attr(a,em,tk,null_prefix))
        return -1;
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
        if(parse_funccall(a,em,tk,prefix,1))
          return -1;
      } else {
        /* a sinlge pipe command as shortcut */
        int idx;
        if( (idx = const_str(a,em->prg,prefix,1)) < 0 )
          return -1;
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
int parse_atomic( struct ajj* a, struct emitter* em , struct tokenizer* tk ) {
  switch(tk->tk) {
    case TK_VARIABLE:
      return parse_prefix(a,em,tk);
    case TK_STRING:
    case TK_TRUE:
    case TK_FALSE:
    case TK_NONE:
    case TK_NUMBER:
      return parse_const(a,em,tk);
    case TK_LPAR:
      tk_consume(tk);
      if(parse_expr(a,em,tk))
        return -1;
      EXPECT(TK_RPAR);
      tk_consume(tk);
      return 0;
    default:
      report_error(a,"Unexpect token here:%s",tk_get_name(tk->tk));
      return -1;
  }
}

static inline
int parse_unary( struct ajj* a, struct emitter* em , struct tokenizer* tk ) {
  unsigned char op[1024]; /* At most we support 1024 unary operators */
  int op_len = 0;
  int i;

#define PUSH_OP(T) \
  do { \
    if( op_len == 1024 ) { \
      report_error(a,"Too much unary operators, more than 1024!"); \
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
  if(parse_atomic(a,em,tk))
    return -1;
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
int parse_factor( struct ajj* a, struct emitter* em , struct tokenizer* tk ) {
  if(parse_unary(a,em,tk))
    return -1;
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
    if(parse_unary(a,em,tk))
      return -1;
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
int parse_term( struct ajj* a , struct emitter* em , struct tokenizer* tk ) {
  if(parse_factor(a,em,tk))
    return -1;
  do {
    int op;
    switch(tk->tk) {
      case TK_ADD:
      case TK_SUB:
        op = tk->tk;
      default: goto done;
    }
    if(parse_factor(a,em,tk))
      return -1;
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
int parse_cmp( struct ajj* a, struct emitter* em , struct tokenizer* tk ) {
  if(parse_term(a,em,tk))
    return -1;
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
    if(parse_term(a,em,tk))
      return -1;
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
int parse_logic( struct ajj* a, struct emitter* em , struct tokenizer* tk ) {
  if(parse_cmp(a,em,tk))
    return -1;
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
    if(parse_cmp(a,em,tk))
      return -1;
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
int parse_expr( struct ajj* a, struct emitter* em , struct tokenizer* tk ) {
  /* We allow a if cond : expr else: expr structure as a more readable
   * tenary operation. */
  if( tk->tk != TK_IF ) {
    /* simple expression */
    return parse_logic(a,em,tk);
  } else {
    tk_consume(tk); /* consume if */

    /* parsing the condition */
    if( parse_logic(a,em,tk) )
      return -1;

    /* parsing the left operand */
    EXPECT(TK_COLON);
    tk_consume(tk);

    if( parse_logic(a,em,tk) )
      return -1;

    /* parsing the right operand */
    EXPECT(TK_ELSE);
    tk_consume(tk);

    EXPECT(TK_COLON);
    tk_consume(tk);

    if( parse_logic(a,em,tk) )
      return -1;
    /* emit the instructions */
    emit0(em,VM_TENARY);
    return 0;
  }
}

static
int finish_end_scope( struct ajj* a,
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
int parse_scope( struct ajj* a, struct ajj_object* temp,
    struct emitter* em , struct tokenizer* tk );

static
int parse_set( struct ajj* a, struct ajj_object* temp ,
    struct emitter* em , struct tokenizer* tk ) {
  int idx;

  UNUSE_ARG(temp);

  assert( tk->tk == TK_SET );
  tk_consume(tk);

  EXPECT(TK_VARIABLE);
  if( (idx = const_str(a,em->prg,strbuf_move(&(tk->lexme)),1)) <0 )
    return -1;

  /* check if we have an pending expression or just end of the SET scope*/
  if( tk->tk == TK_RSTMT ) {
    int txt_idx;
    /* end of the set scope , so it is a scop based set */
    tk_consume(tk);

    EXPECT(TK_TEXT);
    if( (txt_idx = const_str(a,em->prg,strbuf_move(&(tk->lexme)),1)) < 0 )
      return -1;
    tk_consume(tk);

    /* load the text on to stack */
    emit1(em,VM_LSTR,txt_idx);

    /* set up the var based on the stack */
    emit1(em,VM_VAR_SET,idx);
    if(finish_end_scope(a,em,tk,TK_SET))
      return -1;
    return 0;
  } else if( tk->tk == TK_ASSIGN ) {
    tk_consume(tk);
    if(parse_expr(a,em,tk))
      return -1;
    emit1(em,VM_VAR_SET,idx);
    return 0;
  } else {
    report_error(a,"Set scope either uses \"=\" to indicate a one line assignment,\
        or uses a scope based set!");
    return -1;
  }
}

static
int parse_do( struct ajj* a, struct ajj_temp* temp,
    struct emitter* em , struct tokenizer* tk ) {
  UNUSE_ARG(temp);
  assert( tk->tk == TK_DO );
  tk_consume(tk);

  if( parse_expr(a,em,tk) )
    return -1;

  /* We left the expression output on the stack, which is not
   * what we want. Therefore we emit a VM_POP instructions to
   * clear the stack */
  emit0(em,VM_POP);

  EXPECT(TK_RSTMT);
  tk_consume(tk);

  return 0;
}

static
int parse_print( struct ajj* a , struct ajj_object* temp,
    struct emitter* em , struct tokenizer* tk ) {
  UNUSE_ARG(temp);
  if(parse_expr(a,em,tk))
    return -1;

  emit0(em,VM_PRINT);
  EXPECT(TK_REXP);
  tk_consume(tk);
  return 0;
}

static
int parse_branch ( struct ajj* a , struct ajj_object* temp ,
    struct emitter* em , struct tokenizer* tk ) {
  int cond_jmp = -1;
  int end_jmp [ 1024 ];
  int jmp_sz = 0;
  int i;
  int has_else = 0;

#define PUSH_JMP(P) \
  do { \
    if( jmp_sz == 1024 ) { \
      report_error(a,"Too much if-elif-else branch, more than 1024!"); \
      return -1; \
    } \
    end_jmp[jmp_sz++] = (P); \
  } while(0)

  UNUSE_ARG(temp);
  assert( tk->tk == TK_IF );
  tk_consume(tk);

  if( parse_expr(a,em,tk) )
    return -1;

  /* condition failed jump */
  cond_jmp = put(em,1);

  EXPECT(TK_RSTMT);
  tk_consume(tk);

  if( parse_scope(a,temp,em,tk) )
    return -1;

  /* jump out of the scope */
  PUSH_JMP(put(em,1));

  do {
    switch(tk->tk) {
      case TK_ENDFOR:
        { /* end of the for scope */
          if( cond_jmp > 0 )
            emit1_at(em,cond_jmp,VM_JMP,label(em));
          tk_consume(tk);
          EXPECT(TK_RSTMT);
          tk_consume(tk);
          goto done;
        }
      case TK_ELIF:
        { /* elif scope */
          if( has_else ) {
            report_error(a,"Expect endfor since else tag is seen before!");
            return -1;
          }

          tk_consume(tk);
          assert(cond_jmp > 0 );

          /* modify the previous conditional jmp */
          emit1_at(em,cond_jmp,VM_JMP,label(em));

          if(parse_expr(a,em,tk))
            return -1;

          cond_jmp = put(em,1);
          EXPECT(TK_RSTMT);
          tk_consume(tk);
          break;
        }
      case TK_ELSE:
        { /* else scope */
          tk_consume(tk);
          assert( cond_jmp > 0 );

          emit1_at(em,cond_jmp,VM_JMP,label(em));
          cond_jmp = -1;

          EXPECT(TK_RSTMT);
          tk_consume(tk);
          has_else = 1;
          break;
        }
      default:
        report_error(a,"Unexpected token in branch scope:%s",
            tk_get_name(tk->tk));
        return -1;
    }
    if( parse_scope(a,temp,em,tk) )
      return -1;
    if( !has_else )
      PUSH_JMP(put(em,1));
  } while(1);
done:
  for( i = 0 ; i < jmp_sz ; ++i ) {
    emit1_at(em,end_jmp[i],VM_JMP,label(em));
  }
  return 0;
}
#undef PUSH_JMP









