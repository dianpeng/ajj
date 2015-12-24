#ifndef _AJJ_H_
#define _AJJ_H_
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>

#define AJJ_SYMBOL_NAME_MAX_SIZE 32
#define AJJ_GLOBAL_SYMBOL_MAX_SIZE 256
#define AJJ_LOCAL_CONSTANT_SIZE 128
#define AJJ_EXTENDS_MAX_SIZE 8
#define AJJ_FUNC_LOCAL_BUF_SIZE 16
#define AJJ_FUNC_ARG_MAX_SIZE 16

/* Indicate how many functions you could recursively call */
#define AJJ_MAX_CALL_STACK 128
#define AJJ_MAX_VALUE_STACK_SIZE 1024

struct ajj;
struct ajj_value;
struct ajj_object;
struct ajj_io;

#define AJJ_EXEC_OK   0
#define AJJ_EXEC_FAIL -1

typedef int (*ajj_function)( struct ajj* ,
    void* ,
    struct ajj_value*,
    size_t ,
    struct ajj_value* );

typedef int (*ajj_method)( struct ajj* , /* execution context */
    struct ajj_value* obj,
    struct ajj_value*,
    size_t ,
    struct ajj_value* );

typedef int (*ajj_class_ctor)( struct ajj* ,
    void* ,
    struct ajj_value*,
    size_t ,
    void** ret ,
    int* type );

typedef void (*ajj_class_dtor)( struct ajj* ,
    void* udata , void* object );

/* SLOT protocol is used to provide some function that is directly
 * recognized by the VM. All the iterator function is directly called
 * by VM without issuing a function name and do function lookup. With
 * slot function implemented, an registered object is able to be interated in
 * the for loop */
struct ajj_slot {
  int (*iter_start) ( struct ajj* , const struct ajj_value* );
  int (*iter_move ) ( struct ajj* , const struct ajj_value* , int );
  int (*iter_has  ) ( struct ajj* , const struct ajj_value* , int );
  struct ajj_value (*iter_get_key)( struct ajj* , const struct ajj_value* , int );
  struct ajj_value (*iter_get_val)( struct ajj* , const struct ajj_value* , int );
  size_t (*len) ( struct ajj* , const struct ajj_value* );
  int (*empty)( struct ajj* , const struct ajj_value* );
  /* attributes modifier */
  struct ajj_value (*attr_get) ( struct ajj* , const struct ajj_value* ,
          const struct ajj_value* );
  void (*attr_set)( struct ajj* , struct ajj_value* ,
      const struct ajj_value* k , const struct ajj_value* v);
  /* attributes push */
  void (*attr_push)( struct ajj* , struct ajj_value* ,
      const struct ajj_value* v );
  /* move */
  void (*move) ( struct ajj* , struct ajj_value* obj );
  /* print */
  const char* (*display)(struct ajj*, const struct ajj_value*,size_t*);
  /* comparison.
   * All the comparision operation COULD FAIL, the return value is a
   * status indicator not the result of the comparision */
  int (*in) ( struct ajj* , const struct ajj_value* ,
      const struct ajj_value* , int* );
  int (*eq)(struct ajj* , const struct ajj_value* ,
      const struct ajj_value* , int* );
  int (*ne)(struct ajj* , const struct ajj_value* ,
      const struct ajj_value* , int* );
  int (*lt)(struct ajj* , const struct ajj_value* ,
      const struct ajj_value* , int* );
  int (*le)(struct ajj* , const struct ajj_value* ,
      const struct ajj_value* , int* );
  int (*gt)(struct ajj* , const struct ajj_value* ,
      const struct ajj_value* , int* );
  int (*ge)(struct ajj* , const struct ajj_value* ,
      const struct ajj_value* , int* );
};

struct ajj_class_method {
  ajj_method method;
  char name[AJJ_SYMBOL_NAME_MAX_SIZE];
};

struct ajj_class {
  char name[ AJJ_SYMBOL_NAME_MAX_SIZE ]; /* name of the symbol */
  ajj_class_ctor ctor;
  ajj_class_dtor dtor;
  struct ajj_class_method* mem_func;
  size_t mem_func_len; /* length of member function */
  struct ajj_slot slot;
  void* udata; /* user data shared by all the functions */
};

struct ajj_value {
  union {
    int boolean; /* This one must be first to make C89 initializer
                  * happy since a union can only be initialized with
                  * its first field/entry. */
    double number;
    struct ajj_object* object;
    void* __private__; /* For carry private data usage, user don't need
                        * to use it and know it at all */
  } value;
  int type;
};

enum {
  AJJ_VALUE_NOT_USE = 0,
  AJJ_VALUE_NUMBER,
  AJJ_VALUE_BOOLEAN,
  AJJ_VALUE_NONE,
  AJJ_VALUE_STRING,
  AJJ_VALUE_OBJECT,

  /* always the last one to define */
  AJJ_VALUE_SIZE
};

/* Any user defined object's type should be larger than
 * this extension tag value */
#define AJJ_USER_DEFINE_EXTENSION (AJJ_VALUE_SIZE+100)

/* AJJ VALUE ============================== */

const char*
ajj_value_get_type_name( const struct ajj_value* v );

extern struct ajj_value AJJ_TRUE;
extern struct ajj_value AJJ_FALSE;
extern struct ajj_value AJJ_NONE;

/* Function helps us to compose of a new ajj_value */

static
struct ajj_value ajj_value_number( double val ) {
  struct ajj_value value;
  value.type = AJJ_VALUE_NUMBER;
  value.value.number = val;
  return value;
}

static
struct ajj_value
ajj_value_boolean( int boolean ) {
  struct ajj_value ret;
  assert(boolean ==0 || boolean ==1);
  ret.type = AJJ_VALUE_BOOLEAN;
  ret.value.boolean = boolean;
  return ret;
}

struct ajj_value ajj_value_new_string( struct ajj* a,
    const char* str, size_t len );

struct ajj_value ajj_value_new_const_string( struct ajj* a,
    const char* str ,size_t len );

struct ajj_value ajj_value_new_object( struct ajj* a,
    const char* name ,
    size_t arg_num ,
    struct ajj_value* args );

/* Function convert ajj_value to specific c type */
static inline
int ajj_value_to_boolean( const struct ajj_value* val ) {
  assert(val->type == AJJ_VALUE_BOOLEAN);
  return val->value.boolean;
}
static inline
double ajj_value_to_number( const struct ajj_value* val ) {
  assert(val->type == AJJ_VALUE_NUMBER);
  return val->value.number;
}
const char* ajj_value_to_str( const struct ajj_value* val );

/* AJJ value operators */
int ajj_value_in ( struct ajj* a,
    const struct ajj_value* obj,
    const struct ajj_value* target,
    int* result);

int ajj_value_eq( struct ajj* a,
    const struct ajj_value* L , const struct ajj_value* R,
    int* result );

int ajj_value_ne( struct ajj* a,
    const struct ajj_value* L , const struct ajj_value* R,
    int* result);

int ajj_value_lt( struct ajj* a,
    const struct ajj_value* L , const struct ajj_value* R,
    int* result);

int ajj_value_le( struct ajj* a,
    const struct ajj_value* L , const struct ajj_value* R,
    int* result);

int ajj_value_ge( struct ajj* a,
    const struct ajj_value* L , const struct ajj_value* R,
    int* result);

int ajj_value_gt( struct ajj* a,
    const struct ajj_value* L , const struct ajj_value* R,
    int* result);

int ajj_value_len( struct ajj* a,
    const struct ajj_value* obj ,
    int* result );

int ajj_value_empty( struct ajj* a,
    const struct ajj_value* obj ,
    int* result );

/* AJJ ============================= */
struct ajj* ajj_create();
void ajj_destroy( struct ajj* );
void ajj_error ( struct ajj* , const char* format , ... );

/* Register object ======================*/

/* Used to add a value to global environment */
void ajj_env_add_value( struct ajj* a , const char* , int type , ... );

/* This function is used to add a class to the global environment.*/
void ajj_env_add_class( struct ajj* a , const struct ajj_class* );

/* This function is used to add a function to the global environment*/
void ajj_env_add_function( struct ajj* a, const char* ,
    ajj_function entry, void* );

/* This function is used to delete something from the global environment */
int ajj_env_del( struct ajj* a , const char* );

/* The following upvalue function will be only useful when the VM is
 * executing the code. And these functions can only be used during the
 * registered callback function gets called */

/* This function is used to add a value to the CURRENT scope */
void ajj_upvalue_add_value( struct ajj* a,
    const char* , int type , ... );

void ajj_upvalue_add_class( struct ajj* a,
    const struct ajj_class* );

void ajj_upvalue_add_function( struct ajj* a, const char*,
    ajj_function entry, void* );

void ajj_upvalue_del( struct ajj* a, const char* );

/* Clear the *all* the environment variables */
void ajj_env_clear( struct ajj* a );

/* IO ============================= */
/* We used to decide just use FILE* as our IO abstration layer since we
 * could use it handle in memory output. However, since FILE*'s memory
 * buffer version doesn't support dynamic buffer growth so we have to
 * roll our own wheels. This one is really just a very thin warpper on
 * top of the memory and FILE* */

struct ajj_io* ajj_io_create_file( struct ajj* a , FILE* );
struct ajj_io* ajj_io_create_mem ( struct ajj* a , size_t size );
void ajj_io_destroy( struct ajj_io* );
int ajj_io_printf( struct ajj_io* , const char* fmt , ... );
int ajj_io_vprintf( struct ajj_io* , const char* fmt , va_list );
int ajj_io_write( struct ajj_io* , const void* mem , size_t len );
void ajj_io_flush( struct ajj_io* );

/* MISC ============================= */
const char* ajj_display( struct ajj* a,
    const struct ajj_value* v ,
    size_t* length ,
    int* own );

void* ajj_load_file( struct ajj* , const char* , size_t* );

#endif /* _AJJ_H_ */
