#ifndef _VM_H_
#define _VM_H_
#include "ajj.h"
#include "util.h"
#include <stdio.h>

struct ajj;
struct ajj_object;
struct func_table;
struct gc_scope;

struct program {
  void* codes;
  size_t len;
  int* spos;
  size_t spos_len;

  struct string* str_tbl;
  size_t str_len;
  size_t str_cap;

  double*num_tbl;
  size_t num_len;
  size_t num_cap;

  /* parameter prototypes. Program is actually a script based
   * function routine. Each program will have a prototypes */
  struct {
    struct string name;
    struct ajj_value def_val; /* Default value for function parameters.
                               * These values are owned by the scope that owns
                               * this template object. It is typically global
                               * scope */
  } par_list[ AJJ_FUNC_ARG_MAX_SIZE ];
  size_t par_size;
};

/* Execution context */
struct func_frame {
  const struct function* entry; /* Function entry */
  int ebp; /* EBP register */
  int esp; /* ESP register */
  size_t pc ; /* Program counter register */
  struct string name; /* function name */
  int par_cnt : 16 ;
  int method  : 1;    /* whether this call is a method call */
};

struct runtime {
  struct ajj_object* cur_tmpl;   /* current jinja template */
  struct func_frame call_stk[ AJJ_MAX_CALL_STACK ];
  size_t cur_call_stk; /* Current stk position */
  struct ajj_value val_stk[AJJ_MAX_VALUE_STACK_SIZE]; /* current value stack size */
  struct gc_scope* cur_gc; /* current gc scope */
  FILE* output;
};

static inline
void program_init( struct program* prg ) {
  prg->codes = NULL;
  prg->len = 0;

  prg->str_len = 0;
  prg->str_cap = AJJ_LOCAL_CONSTANT_SIZE;
  prg->str_tbl = malloc(sizeof(struct string)*AJJ_LOCAL_CONSTANT_SIZE);

  prg->num_len = 0;
  prg->num_cap = AJJ_LOCAL_CONSTANT_SIZE;
  prg->num_tbl = malloc(sizeof(double)*AJJ_LOCAL_CONSTANT_SIZE);

  prg->par_size =0;
  prg->spos = NULL;
  prg->spos_len = 0;
}

void program_destroy( struct program* prg );

static inline
int program_add_par( struct program* prg , struct string* name ,
    int own, const struct ajj_value* val ) {
  if( prg->par_size == AJJ_FUNC_ARG_MAX_SIZE )
    return -1;
  else {
    assert(name->len < AJJ_SYMBOL_NAME_MAX_SIZE );
    prg->par_list[prg->par_size].def_val = *val; /* owned the value */
    prg->par_list[prg->par_size].name = own ? *name : string_dup(name);
    ++prg->par_size;
    return 0;
  }
}

static
int program_const_str( struct program* prg , struct string* str ,
    int own ) {
  if( str->len > 128 ) {
insert:
    if( prg->str_len == prg->str_cap ) {
      prg->str_tbl = mem_grow(prg->str_tbl, sizeof(struct string),
          &(prg->str_cap));
    }
    if(own) {
      prg->str_tbl[prg->str_len] = *str;
    } else {
      prg->str_tbl[prg->str_len] = string_dup(str);
    }
    return prg->str_len++;
  } else {
    size_t i = 0 ;
    for( ; i < prg->str_len ; ++i ) {
      if( string_eq(prg->str_tbl+i,str) ) {
        if(own) string_destroy(str);
        return i;
      }
    }
    goto insert;
  }
}

static
int program_const_num( struct program* prg , double num ) {
  size_t i;
  if( prg->num_len== prg->num_cap ) {
    prg->num_tbl = mem_grow(prg->num_tbl,sizeof(double),
        &(prg->num_cap));
  }
  for( i = 0 ; i < prg->num_len ; ++i ) {
    if( num == prg->num_tbl[i] )
      return i;
  }
  prg->num_tbl[prg->num_len] = num;
  return prg->num_len++;
}

/* =============================================
 * Interfaces
 * ===========================================*/

int vm_run_func( struct ajj* ,
    struct ajj_object* tp,
    const struct string* name,
    struct ajj_value*,
    size_t len,
    struct ajj_value* output );

int vm_run_jj( struct ajj* a,
    struct ajj_object* tp ,
    struct ajj_value*,
    size_t len,
    struct ajj_value* output );

#endif /* _VM_H_ */

