#ifndef _VM_H_
#define _VM_H_
#include "ajj.h"
#include "util.h"
#include <stdio.h>

struct ajj;
struct ajj_object;
struct func_table;
struct gc_scope;


/* A program is a structure represents the code compiled from Jinja template.
 * It is the concrete entity that VM gonna execute. A program is alwyas held
 * by an object's function table objects. */

struct program {
  int* codes;
  int* spos;
  size_t len;

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


/* Our VM is always trying to call different functions, no matter it is a C
 * function or a scripted function ( which is struct program ). Each function,
 * during runtime will form a func_frame object which is stacked by the VM.
 * VM is always executing the top of that func_frame stack. By walk that stack,
 * we can know the stack traces of current execution. One thing to note, the
 * frame is also used to represent the C function execution */

struct func_frame {
  const struct function* entry; /* Function entry */

  /* the following fields are only used when the function is a script
   * function. Otherwise they are all set to 0 */

  int ebp; /* EBP register */
  int esp; /* ESP register */
  size_t pc ; /* Program counter register */
  size_t ppc; /* Previous PC */

  struct string name; /* function name , a pointer that just points to the
                       * name inside of entry */

  int par_cnt ;  /* parameter count when calling this function */

  struct ajj_object* obj; /* if it is an object call, then the object pointer
                           * to the corresponding caller object */
};


/* Runtime object is the internal VM data structure which contains all the VM
 * execution resources. It has a pointer points to the main jinja template and
 * contains a value stack + an function frame stack. Also a gc_scope pointer always
 * points to the current gc scope and a output FILE points to where the output
 * of the jinja template should go to. */

struct runtime {
  struct ajj_object* cur_obj; /* current calling object */
  struct func_frame call_stk[ AJJ_MAX_CALL_STACK ];
  int cur_call_stk; /* Current stk position */
  struct ajj_value val_stk[AJJ_MAX_VALUE_STACK_SIZE]; /* current value stack size */
  struct gc_scope* cur_gc; /* current gc scope */
  struct ajj_io* output;
};

static inline
void program_init( struct program* prg ) {
  prg->codes = NULL;
  prg->spos = NULL;
  prg->len = 0;

  prg->str_len = 0;
  prg->str_cap = AJJ_LOCAL_CONSTANT_SIZE;
  prg->str_tbl = malloc(sizeof(struct string)*AJJ_LOCAL_CONSTANT_SIZE);

  prg->num_len = 0;
  prg->num_cap = AJJ_LOCAL_CONSTANT_SIZE;
  prg->num_tbl = malloc(sizeof(double)*AJJ_LOCAL_CONSTANT_SIZE);

  prg->par_size =0;
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
          0,
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
        0,
        &(prg->num_cap));
  }
  for( i = 0 ; i < prg->num_len ; ++i ) {
    if( num == prg->num_tbl[i] )
      return i;
  }
  prg->num_tbl[prg->num_len] = num;
  return prg->num_len++;
}

/* helper function for converting the ajj_value to specific type */
int vm_to_number( const struct ajj_value* , double* );
int vm_to_integer( const struct ajj_value* , int* );
/* boolean conversion will NEVER fail */
int vm_to_boolean( const struct ajj_value* );

static
int vm_is_true( const struct ajj_value* val ) {
  return vm_to_boolean(val) == 1;
}

static
int vm_is_false( const struct ajj_value* val ) {
  return vm_to_boolean(val) == 0;
}

int vm_to_string( const struct ajj_value* val ,
        struct string* str , int* own );

/* =============================================
 * Interfaces
 * ===========================================*/

int vm_run_jinja( struct ajj* a, struct ajj_object* jj,
    struct ajj_io* output );

#endif /* _VM_H_ */

