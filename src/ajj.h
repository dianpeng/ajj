#ifndef _AJJ_H_
#define _AJJ_H_
#include <stddef.h>

struct ajj_object;
struct ajj_value;

#define AJJ_METHOD_PARAMETER_LIST_MAX_SIZE 12
#define AJJ_SYMBOL_NAME_MAX_SIZE 32
#define AJJ_GLOBAL_SYMBOL_MAX_SIZE 64

typedef int (*ajj_method)( struct ajj* ,
    void* ,
    struct ajj_value[AJJ_METHOD_PARAMETER_LIST_MAX_SIZE] ,
    size_t ,
    struct ajj_value* );


struct ajj_value {
  union {
    struct ajj_object* object;
    double number;
    int boolean;
  } value;
  int type;
};

enum {
  AJJ_VALUE_NUMBER,
  AJJ_VALUE_BOOLEAN,
  AJJ_VALUE_UNDEFINED,
  AJJ_VALUE_STRING,
  AJJ_VALUE_DICT,
  AJJ_VALUE_ARRAY,
  AJJ_VALUE_TUPLE,
  AJJ_VALUE_METHOD
};

struct ajj* ajj_create();
void ajj_destroy( struct ajj* );

/* misc function to help develop ajj */
int ajj_register_object( struct ajj* , const char* , const struct ajj_value* );
int ajj_delete_object( struct ajj* , const char* , const struct ajj_value* );
struct ajj_value* ajj_find_object( struct ajj* , const char* );

/* misc function for creating new object*/
int ajj_new_array( struct ajj* , struct ajj_value* );
int ajj_new_dict ( struct ajj* , struct ajj_value* );
int ajj_new_tuple( struct ajj* , struct ajj_value* , int size );
int ajj_new_method(struct ajj* , struct ajj_value* , ajj_method , void* );

/* render the text */
char* ajj_render( struct ajj* , const char* input , size_t* len );

#endif /* _AJJ_H_ */
