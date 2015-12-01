#ifndef _AJJ_H_
#define _AJJ_H_
#include <stddef.h>

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

typedef int (*ajj_function)( struct ajj* ,
    void* ,
    struct ajj_value*,
    size_t ,
    struct ajj_value* );

typedef int (*ajj_method)( struct ajj* , /* execution context */
    struct ajj_object* obj,
    struct ajj_value*,
    size_t ,
    struct ajj_value* );

typedef void* (*ajj_class_ctor)( struct ajj* ,
    void* ,
    struct ajj_value*,
    size_t );

typedef void (*ajj_class_dtor)( struct ajj* ,
    void* udata , void* object );

struct ajj_class {
  char name[ AJJ_SYMBOL_NAME_MAX_SIZE ]; /* name of the symbol */

  ajj_class_ctor ctor;
  ajj_class_dtor dtor;

  struct {
    ajj_method method;
    char name[AJJ_SYMBOL_NAME_MAX_SIZE];
  } * mem_func;

  size_t mem_func_len; /* length of member function */
  void* udata; /* user data shared by all the functions */
};

struct ajj_value {
  union {
    int boolean; /* This one must be first to make C89 initializer
                  * happy since a union can only be initialized with
                  * its first field/entry. */
    double number;
    struct ajj_object* object;
  } value;
  int type;
};

enum {
  AJJ_VALUE_NOT_USE = 0,
  AJJ_VALUE_NUMBER,
  AJJ_VALUE_BOOLEAN,
  AJJ_VALUE_NONE,
  AJJ_VALUE_STRING,
  AJJ_VALUE_LIST,
  AJJ_VALUE_DICT,
  AJJ_VALUE_OBJECT,

  AJJ_VALUE_SIZE
};

const char*
ajj_value_get_type_name( const struct ajj_value* );

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

/* List API =============================== */
void ajj_list_push( const struct ajj_value*, const struct ajj_value* );
void ajj_list_clear( const struct ajj_value* );
size_t ajj_list_size( const struct ajj_value* );
struct ajj_value* ajj_list_index( struct ajj_value* , size_t );

/* Dict API ============================== */
int ajj_dict_insert( const struct ajj_value* , const char* key ,
    const struct ajj_value* val );
const struct ajj_value*
ajj_dict_find( const struct ajj_value* , const char* key );
int ajj_dict_delete( const struct ajj_value* , const char* key );

/* AJJ ============================= */
struct ajj* ajj_create();
void ajj_destroy( struct ajj* );
#endif /* _AJJ_H_ */
