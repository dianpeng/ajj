#ifndef _AJJ_H_
#define _AJJ_H_
#include <stddef.h>

#define AJJ_METHOD_PARAMETER_LIST_MAX_SIZE 12
#define AJJ_SYMBOL_NAME_MAX_SIZE 32
#define AJJ_GLOBAL_SYMBOL_MAX_SIZE 256
#define AJJ_LOCAL_CONSTANT_SIZE 128
#define AJJ_EXTENDS_MAX_SIZE 8
#define AJJ_FUNC_LOCAL_BUF_SIZE 16
#define AJJ_FUNC_PAR_MAX_SIZE 16

struct ajj_value;
struct ajj_context;

typedef int (*ajj_method)( struct ajj_context* , /* execution context */
    void* , /* user data */
    struct ajj_value[AJJ_METHOD_PARAMETER_LIST_MAX_SIZE] ,
    size_t ,
    struct ajj_value* );

struct ajj_object_proto {
  char name[ AJJ_SYMBOL_NAME_MAX_SIZE ]; /* name of the symbol */
  ajj_method ctor;
  ajj_method dtor;

  struct {
    ajj_method method;
    char name[AJJ_SYMBOL_NAME_MAX_SIZE];
  } * mem_func;

  size_t mem_func_len; /* length of member function */
  void* udata; /* user data shared by all the functions */
};

struct ajj_value {
  union {
    void* object;
    double number;
    int boolean;
    int itr_idx;
    void* itr_ptr;
  } value;
  int type;
};

extern ajj_value AJJ_TRUE;
extern ajj_value AJJ_FALSE;
extern ajj_value AJJ_NONE;

enum {
  AJJ_VALUE_NUMBER,
  AJJ_VALUE_BOOLEAN,
  AJJ_VALUE_NONE,
  AJJ_VALUE_STRING,
  AJJ_VALUE_LIST,
  AJJ_VALUE_DICT,
  AJJ_VALUE_OBJECT,

  AJJ_VALUE_SIZE
};

struct ajj* ajj_create();
void ajj_destroy( struct ajj* );

/* casting an ajj pointer to the ajj_context pointer */
static inline
struct ajj_context* ajj_to_context( struct ajj* a ) {
  return (struct ajj_context*)(a);
}

/* misc function to help develop ajj */
void ajj_new_dict( struct ajj_context* , struct ajj_value* );
void ajj_new_tuple(struct ajj_context* , struct ajj_value* );
void ajj_new_list( struct ajj* , struct ajj_value* );

int
ajj_dict_add( struct ajj* , struct ajj_value* , const struct ajj_value* , int overwrite );
const struct ajj_value* 
ajj_dict_find( struct ajj* , const struct ajj_value* , const char* );
size_t
ajj_dict_len( const struct ajj_value* );

int ajj_tuple_add(struct ajj* , struct ajj_value* , const struct ajj_value* );
const struct ajj_value*
ajj_tuple_index(struct ajj* , const struct ajj_value* , int index );
size_t
ajj_tuple_len( const struct ajj_value* );

int ajj_list_add(struct ajj* , struct ajj_value* , const struct ajj_value* );
const struct ajj_value*
ajj_list_index(struct ajj*, const struct ajj_value* , int index );
size_t
ajj_list_len( const struct ajj_value* );

int ajj_register_number( struct ajj* , const char* key , double value );
int ajj_register_string( struct ajj* , const char* key , const char* );
int ajj_register_boolean( struct ajj* , const char* key  , int );
int ajj_register_none( struct ajj* , const char* key );
int ajj_register_dict( struct ajj* , const char* key , const struct ajj_value* );
int ajj_register_list( struct ajj* , const char* key , const struct ajj_value* );
int ajj_register_tuple(struct ajj* , const char* key , const struct ajj_value* );

int ajj_unregister( struct ajj* , const char* key );

/* the memory for const struct ajj_object_method* must be persistent during the execution
 * of process. AJJ will just use it instead of copying the value . General method is to
 * use a static array for this piece of memory */
int ajj_register_object(struct ajj* , const struct ajj_object_proto* );

/* render the text */
char* ajj_render( struct ajj* , const char* input , size_t* len );

#endif /* _AJJ_H_ */
