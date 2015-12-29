#ifndef _VM_H_
#define _VM_H_
#include "ajj.h"
#include "util.h"
#include "parse.h" /* for MAX_LOOP_CTRL_SIZE */
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

  struct ajj_object* obj; /* if it is an object call, then the object pointer
                           * to the corresponding caller object */
  int par_cnt : 31;  /* parameter count that resides on the caller's stack,
                      * it is not the actual number of arguments number for
                      * called function. Called function arguments number
                      * is always the defined number */
  int method  : 1 ;  /* indicate whether this call is a method call or not */

  /* Optimization for LOOP object */
  struct ajj_object* loops[ MAX_LOOP_CTRL_SIZE ];
  size_t cur_loops;

  /* Entry GC pointer, used to clean the gc scope when an early return is
   * executed */
  struct gc_scope* enter_gc;
};

/* Runtime object is the internal VM data structure which contains all the VM
 * execution resources. It has a pointer points to the main jinja template and
 * contains a value stack + an function frame stack. Also a gc_scope pointer always
 * points to the current gc scope and a output FILE points to where the output
 * of the jinja template should go to. */
struct runtime {
  /* Runtime inheritance chain when extends happened */
  struct runtime* prev;
  struct runtime* next;
  
  size_t inc_cnt; /* nested inclusion count, if too much include is
                   * met, we just return falure. This avoid crash on
                   * stack overflow */
  struct ajj_object* jinja; /* jinja template related to this runtime */
  struct func_frame call_stk[ AJJ_MAX_CALL_STACK ];
  int cur_call_stk; /* Current stk position */
  struct ajj_value val_stk[AJJ_MAX_VALUE_STACK_SIZE]; /* current value stack size */
  struct gc_scope* cur_gc; /* current gc scope */
  struct gc_scope* root_gc;/* root gc scope for *this* jinja template */
  struct ajj_io* output;
  struct upvalue_table* global; /* Per template based global value. This make
                                 * sure each template is executed in its own
                                 * global variable states */
};

#define runtime_root_gc(rt) ((rt)->root_gc)

void program_init( struct program* prg );
void program_destroy( struct program* prg );
int program_add_par( struct program* prg , struct string* name ,
    int own, const struct ajj_value* val );
int program_const_str( struct program* prg , struct string* str ,
    int own );
int program_const_num( struct program* prg , double num );
/* helper function for converting the ajj_value to specific type */
int vm_to_number( const struct ajj_value* , double* );
int vm_to_integer( const struct ajj_value* , int* );
/* boolean conversion will NEVER fail */
int vm_to_boolean( const struct ajj_value* );
#define vm_is_true(V) (vm_to_boolean(V) == 1)
#define vm_is_false(V) (vm_to_boolean(V)==0)
int vm_to_string( const struct ajj_value* val ,
        struct string* str , int* own );

/* Get builtin variable from the current stack frame */
int vm_get_argnum( struct ajj* a );
const struct string* vm_get_func( struct ajj* a );
const struct ajj_value* vm_get_vargs( struct ajj* a );
const struct string* vm_get_caller( struct ajj* a );
const struct ajj_object* vm_get_self( struct ajj* a );

/* VM BUILTIN function */
int vm_caller( struct ajj* ,
    void*,
    struct ajj_value*,size_t,
    struct ajj_value*);

int vm_super( struct ajj* ,
    void*,
    struct ajj_value* ,
    size_t,
    struct ajj_value*);

/* =============================================
 * Interfaces
 * ===========================================*/

int vm_run_jinja( struct ajj* a, struct ajj_object* jj,
    struct ajj_io* output );

#endif /* _VM_H_ */
