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
  size_t (*length) ( struct ajj* , const struct ajj_value* );
  int (*empty)( struct ajj* , const struct ajj_value* );
  int (*in) ( struct ajj* , const struct ajj_value* , const struct ajj_value* );
  /* attributes modifier */
  struct ajj_value (*attr_get) ( struct ajj* , const struct ajj_value* ,
          const struct ajj_value* );
  void (*attr_set)( struct ajj* , struct ajj_value* ,
      const struct ajj_value* k , const struct ajj_value* v);
  /* attributes push */
  void (*attr_push)( struct ajj* , struct ajj_value* ,
      const struct ajj_value* v );
  /* for print */
  const char* (*display)(struct ajj*, const struct ajj_value*,size_t*);
  /* comparison */
  int (*eq)(struct ajj* , const struct ajj_value* ,
      const struct ajj_value* );
  int (*ne)(struct ajj* , const struct ajj_value* ,
      const struct ajj_value* );
  int (*lt)(struct ajj* , const struct ajj_value* ,
      const struct ajj_value* );
  int (*le)(struct ajj* , const struct ajj_value* ,
      const struct ajj_value* );
  int (*gt)(struct ajj* , const struct ajj_value* ,
      const struct ajj_value* );
  int (*ge)(struct ajj* , const struct ajj_value* ,
      const struct ajj_value* );
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
  AJJ_VALUE_CLASS = -1,
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

static
const char*
ajj_value_get_type_name( const struct ajj_value* v ) {
  switch(v->type) {
    case AJJ_VALUE_NOT_USE:
      return NULL;
    case AJJ_VALUE_NUMBER:
      return "number";
    case AJJ_VALUE_BOOLEAN:
      return "boolean";
    case AJJ_VALUE_NONE:
      return "none";
    case AJJ_VALUE_STRING:
      return "string";
    case AJJ_VALUE_OBJECT:
      return "object";
    default:
      return NULL;
  }
}

extern struct ajj_value AJJ_TRUE;
extern struct ajj_value AJJ_FALSE;
extern struct ajj_value AJJ_NONE;

static inline
struct ajj_value ajj_value_number( double val ) {
  struct ajj_value value;
  value.type = AJJ_VALUE_NUMBER;
  value.value.number = val;
  return value;
}
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

/* This function will create a specific value based on the type
 * you tells it and the corresponding parameters you put into it.
 * The return value , if it is a heap based object , will live on
 * the scope that CURRENTLY the vm on. But if you call it while no
 * executiong is performing, the value you get is on the global gc
 * scope which will only be released when you delete the corresponding
 * ajj object. */

struct ajj_value
ajj_value_new( struct ajj* a , int type , ... );

/* AJJ ============================= */
struct ajj* ajj_create();
void ajj_destroy( struct ajj* );
void ajj_error ( struct ajj* , const char* format , ... );

/* Register object ======================*/
void ajj_env_add_value( struct ajj* a , const char* , int type , ... );
int ajj_env_del_value( struct ajj* a , const char* );

/* IO ============================= */
/* We used to decide just use FILE* as our IO abstration layer since we could use it handle
 * in memory output. However, since FILE*'s memory buffer version doesn't support dynamic
 * buffer growth so we have to roll our own wheels. This one is really just a very thing
 * warpper on top of the memory and FILE* */

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
