#ifndef _AJJ_H_
#define _AJJ_H_
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>

struct ajj;
struct ajj_value;
struct ajj_object;
struct ajj_io;

/* Status code indicate whether the user defined function
 * works correctly or not. Extra error information may be
 * provided by using ajj_error */
#define AJJ_EXEC_OK   0
#define AJJ_EXEC_FAIL -1

/* Signature represent a global function call .
 * Arg1: ajj engine pointer
 * Arg2: the opaque pointer user registered
 * Arg3: list of arguments
 * Arg4: size of the arguments
 * Arg5: return value pointer , user use it to provide
 *       return value.
 */
typedef int (*ajj_function)( struct ajj* ,
    void* ,
    struct ajj_value*,
    size_t ,
    struct ajj_value* );

/* Signature represents a registered object's member function call
 * Arg1: ajj engine pointer
 * Arg2: the object pointer , or self/this pointer
 * Arg3: list of arguments,
 * Arg4: size of arguments
 * Arg5: return value pointer
 */
typedef int (*ajj_method)( struct ajj* , /* execution context */
    struct ajj_value* obj,
    struct ajj_value*,
    size_t ,
    struct ajj_value* );

/* Signature represents a registered object's constructor
 * function call.
 * Arg1: ajj engine pointer
 * Arg2: the opaque pointer user registered for this class
 * Arg3: the object pointer, or self/this pointer
 * Arg4: list of arguments
 * Arg5: size of arguments
 * Arg6: the instantiated object's pointer
 * Arg7: A unique type value associated with this object.
 *       This value must be larger than AJJ_USER_DEFINE_EXTENSION.
 *       Otherwise behavior is undefined */
typedef int (*ajj_class_ctor)( struct ajj* ,
    void* ,
    struct ajj_value*,
    size_t ,
    void** ret ,
    int* type );

/* Signature represents a registered object's destructor
 * function call.
 * Arg1: ajj engine pointer
 * Arg2: the opaque pointer user registered for this class
 * Arg3: the object's pointer */
typedef void (*ajj_class_dtor)( struct ajj* ,
    void* udata , void* object );

/* Slot represents all the builtin functions for each objects. This
 * function is recognized by virtual machine directly and serves as
 * the protocol between user defined objects and AJJ virtual machine,
 * user doesn't have to define all function pointer , to ignore one
 * just put NULL */
struct ajj_slot {
  /* =================================================================
   * The following 5 functions are about the iterability of an object
   * and must register all of them if the object is iterable. In our
   * code base, the iterator _MUST_ be an integer.
   * ===============================================================*/

   /* iter_start function is used to initialize the interator  */
  int (*iter_start) ( struct ajj* , const struct ajj_value* );

  /* This function is used to move the iterator once forward */
  int (*iter_move ) ( struct ajj* , const struct ajj_value* , int );

  /* This function is used to test whether the current iterator is valid
   * or not */
  int (*iter_has  ) ( struct ajj* , const struct ajj_value* , int );

  /* This function is used to get the key from current iterator if it has,
   * if the target object doesn't contain the corresponding key, return
   * AJJ_NONE */
  struct ajj_value (*iter_get_key)( struct ajj* , const struct ajj_value* , int );

  /* This function is used to get the value from current iterator if it has ,
   * if the target object doesn't contain the correpsonding value, return
   * AJJ_NONE */
  struct ajj_value (*iter_get_val)( struct ajj* , const struct ajj_value* , int );

  /* =================================================================
   * Property function for objects
   * ===============================================================*/

  /* This function is used to tell the length of the target object */
  size_t (*len) ( struct ajj* , const struct ajj_value* );

  /* This function is used to test whether the target object is empty */
  int (*empty)( struct ajj* , const struct ajj_value* );

  /* ============================================================================
   * Attributes function. Those function are used to access attributes from
   * objects. Attributes are typically accessed through dot operator or index
   * operator. Eg: a["some_key"] or a.some_key will trigger function call on
   * attributes function. All the user registered function only needs to provide
   * attr_get. The attr_set/attr_push are only used for internal objects currently.
   * ===========================================================================*/

  /* This function is used to get a attribute from the target object based on
   * the key from script */
  struct ajj_value (*attr_get) ( struct ajj* , const struct ajj_value* ,
          const struct ajj_value* );

  /* This function is used to set a attribute for the target object */
  void (*attr_set)( struct ajj* , struct ajj_value* ,
      const struct ajj_value* k , const struct ajj_value* v);

  /* This function is used to push an attributes into the specific object */
  void (*attr_push)( struct ajj* , struct ajj_value* ,
      const struct ajj_value* v );

  /* This function is used to help the GC algorithm works. It will be triggered
   * when a user indicate a move operations. And internally, for any intenrally
   * referenced value that comes from AJJ, user needs to call ajj_value_move on
   * those value to ensure the reference is correct to VM , otherwise those value
   * can be garbage collected. If any object doesn't reference any ajj value from
   * AJJ world, then no need to provide the implementation */
  void (*move) ( struct ajj* , struct ajj_value* obj );

  /* This function is used to provide a string representation when the print
   * instruction is issued towards this object. A print instruction is issued
   * when {{ expression }} is emitted. */
  const char* (*display)(struct ajj*, const struct ajj_value*,size_t*);

  /* =========================================================================
   * Comparison operators.
   * All these functions will be triggered when the script has corresponding
   * comparison operation on top of them
   * =======================================================================*/

  /* This function is used to test whether an object is *IN* another object */
  int (*in) ( struct ajj* , const struct ajj_value* ,
      const struct ajj_value* , int* );
  /* Test whether the target object is equal with this object */
  int (*eq)(struct ajj* , const struct ajj_value* ,
      const struct ajj_value* , int* );

  /* Test whether the target object is not equal with this object */
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

/* =============================================================
 * User defined class prototype.
 * The ajj_class represents a type that user registered into the AJJ.
 * The instantiation of the ajj_class will resulint the corresponding
 * object.
 * ===========================================================*/

/* Method prototype for user defined objects */
struct ajj_class_method {
  ajj_method method;
  const char* name;
};

/* Class prototype */
struct ajj_class {
  /* Name of this class */
  const char* name;
  /* Constructor of the this class. Suppose the class's name is SomeObject,
   * then the ctor will be triggered when SomeObject(...) is issued in jinja2.
   * This will result in the object gets created */
  ajj_class_ctor ctor;

  /* Destructor of this class. This function will only be triggered/called
   * when the corresponding gabarge collector scope exits */
  ajj_class_dtor dtor;

  /* List of member function */
  struct ajj_class_method* mem_func;
  size_t mem_func_len;

  /* Slot objects which contains all the protocol function this class
   * defined for our VM */
  struct ajj_slot slot;

  /* Opaque pointer for user data . This opaque pointer will only be used
   * by ctor and dtor function */
  void* udata;
};

/* This structure represents a value in AJJ. */
struct ajj_value {
  union {
    /* Represent boolean value . This must be the first in
     * this union. This is a primitive type so it has value
     * semantic which means copy by its value */
    int boolean;
    /* Represent the number value if this value is a number type.
     * Also this is a primitive type as well and it is copy by
     * value */
    double number;
    /* Represent a heap based object and it is managed by our GC algorithm */
    struct ajj_object* object;
    /* An opaque pointer used internally */
    void* __private__;
  } value;
  /* Type of this ajj_value.
   * Notes: If it is NONE type, then we don't have actualy value field
   * for it since we know it by this type tag */
  int type;
};

/* Different type of ajj_value */
enum {
  AJJ_VALUE_NOT_USE = 0,
  AJJ_VALUE_NUMBER,
  AJJ_VALUE_BOOLEAN,
  AJJ_VALUE_NONE,
  AJJ_VALUE_STRING,
  AJJ_VALUE_OBJECT,

  /* Size of the ajj_value type */
  AJJ_VALUE_SIZE
};

/* User registered object/class type value must be larger than
 * this value */
#define AJJ_USER_DEFINE_EXTENSION (AJJ_VALUE_SIZE+100)

/* Get the ajj_value type's human readable string for this */
const char*
ajj_value_get_type_name( const struct ajj_value* v );

/* Static variable for AJJ_TRUE/AJJ_FALSE/AJJ_NONE */
extern struct ajj_value AJJ_TRUE;
extern struct ajj_value AJJ_FALSE;
extern struct ajj_value AJJ_NONE;

/* Convert a double to ajj_value */
struct ajj_value ajj_value_number( double val );
/* Convert a boolean to ajj_value */
struct ajj_value ajj_value_boolean( int boolean );
/* Construct a string to ajj_value */
struct ajj_value ajj_value_new_string( struct ajj* a,
    const char* str, size_t len );
/* Construct a constant string to ajj_value. The constant
 * string means the string is not garbage collected by VM */
struct ajj_value ajj_value_new_const_string( struct ajj* a,
    const char* str ,size_t len );

/* Convert a ajj_value to its corresponding boolean value.
 * The ajj_value must be boolean type */
#define ajj_value_to_boolean(V) ((V)->value.boolean)

/* Convert a ajj_value to its corresponding number value.
 * The ajj_value must be number type */
#define ajj_value_to_number(V) ((V)->value.number)

/* Convert a ajj_value to its string value. The ajj_value
 * must be string type */
const char* ajj_value_to_str( const struct ajj_value* val ,
    size_t* len );

/* Construct an object based on its class name.
 * Arg1: ajj engine pointer
 * Arg2: name of the class
 * Arg3: list of arguments
 * Arg4: size of arguments
 */
struct ajj_value
ajj_value_new_object( struct ajj* a,
    const char* name,
    struct ajj_value* arg,
    size_t arg_len );

/* Call an object's specific method.
 * Arg1: ajj engine pointer
 * Arg2: the object's pointer
 * Arg3: the method name
 * Arg4: list of arguments
 * Arg5: size of arguments
 * Arg6: return value pointer */
int ajj_value_call_object_method( struct ajj* a,
    struct ajj_value* obj,
    const char* name,
    struct ajj_value* arg,
    size_t arg_len,
    struct ajj_value* ret );

/* New a list object */
struct ajj_value ajj_value_new_list( struct ajj* a );

/* Push a value into the list object */
void ajj_value_list_push( struct ajj* a, struct ajj_value* ,
    struct ajj_value* val );

/* Get the size of the list object */
size_t ajj_value_list_size( struct ajj* a, struct ajj_value* );

/* Index a value from the list object */
struct ajj_value
ajj_value_list_index( struct ajj* a, struct ajj_value* ,int index );

/* Clear the list */
void ajj_value_list_clear( struct ajj* a, struct ajj_value* );

/* New a dict object */
struct ajj_value ajj_value_new_dict( struct ajj* a );

/* Insert a key value pair into the dictionary */
void ajj_value_dict_insert( struct ajj* a, struct ajj_value* ,
    struct ajj_value* key, struct ajj_value* val );

/* Find a key inside of the dictionary */
struct ajj_value
ajj_value_dict_find( struct ajj* a, struct ajj_value* ,
    struct ajj_value* key );

/* Remove a entry in the dictionary */
int ajj_value_dict_remove( struct ajj* a, struct ajj_value* ,
    struct ajj_value* key );

/* Clear the dictionary */
void ajj_value_dict_clear( struct ajj* a, struct ajj_value* );

/* ===========================================================
 * The following function is proxy function to call
 * a specific object's slot function if it has one.
 * If the target function is existed and worked correctly
 * it return AJJ_EXEC_OK, otherwise AJJ_EXEC_FAIL is returend
 * =========================================================*/

int ajj_value_attr_get( struct ajj* a,
    const struct ajj_value* obj,
    const struct ajj_value* key,
    struct ajj_value* ret );

int ajj_value_attr_set( struct ajj* a,
    struct ajj_value* obj,
    const struct ajj_value* key,
    const struct ajj_value* val);

int ajj_value_attr_push( struct ajj* a,
    struct ajj_value* obj,
    const struct ajj_value* val);

const char* ajj_display( struct ajj* a,
    const struct ajj_value* v ,
    size_t* length ,
    int* own );

struct ajj_value ajj_value_move( struct ajj* a,
    const struct ajj_value* self,
    struct ajj_value* tar );

int ajj_value_iter_start( struct ajj* a,
    const struct ajj_value* obj,
    int* itr );

int ajj_value_iter_move( struct ajj* a,
    const struct ajj_value* obj,
    int itr, int* itr_ret );

int ajj_value_iter_has ( struct ajj* a,
    const struct ajj_value* obj,
    int itr, int* result );

int ajj_value_iter_get_key( struct ajj* a,
    const struct ajj_value* obj,
    int itr,
    struct ajj_value* key );

int ajj_value_iter_get_val( struct ajj* a,
    const struct ajj_value* obj,
    int itr,
    struct ajj_value* val );

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
    size_t* result );

int ajj_value_empty( struct ajj* a,
    const struct ajj_value* obj ,
    int* result );


/* Create an ajj engine. Before rendering any templates, a ajj
 * engine pointer must be created and it serves as the environment
 * and resource holder for all the template rendering happened inside
 * of it . User should never mix different ajj engine pointer togeteher */
struct ajj* ajj_create();

/* Destroy an ajj engine pointer */
void ajj_destroy( struct ajj* );

/* Dump an error information into the ajj engine and be consumed by the
 * user */
void ajj_error ( struct ajj* , const char* format , ... );

/* Get last error descriptive string */
const char* ajj_last_error( struct ajj* a );

/* Add a value into the ajj environment. The environment is *SHARED* by
 * all the template rendered inside of the same ajj engine */
void ajj_env_add_value( struct ajj* a , const char* , int type , ... );

/* Add a class into ajj environment */
void ajj_env_add_class( struct ajj* a , const struct ajj_class* );

/* Add a function into ajj environment */
void ajj_env_add_function( struct ajj* a, const char* ,
    ajj_function entry, void* );

/* Add a filter function */
#define ajj_env_add_filter ajj_env_add_function

/* Add a test function into ajj environment. Test function has exactly
 * same prototype as normal function. But a test function *MUST* return
 * true/false */
void ajj_env_add_test( struct ajj* a, const char* ,
    ajj_function entry, void* );

/* Check whether a name is existed inside of ajj environment */
int ajj_env_has( struct ajj* a, const char* );

/* Delete a name from ajj environment. This is *NOT* for deleting
 * an class or function during the template is executing. If calling
 * this function during template rendering, the behavior is undefined */
int ajj_env_del( struct ajj* a , const char* );

/* ==============================================================
 * Operation on upvalue table for specific template. The following
 * functions can only be called during the template is rendering
 * ============================================================*/

/* Add a value into the upvalue table for a specific template */
void ajj_upvalue_add_value( struct ajj* a,
    const char* , int type , ... );

/* Add a class into the upvalue table */
void ajj_upvalue_add_class( struct ajj* a,
    const struct ajj_class* );

/* Add a function into the upvalue table */
void ajj_upvalue_add_function( struct ajj* a, const char*,
    ajj_function entry, void* );

#define ajj_upvalue_add_filter ajj_upvalue_add_function

/* Delete a name from the upvalue. The deletion operation is generally
 * not safe at all. If user delete a builtin upvalue or a upvalue that
 * is setup by the script while script is using it, the behavior is not
 * defined :) */
void ajj_upvalue_del( struct ajj* a, const char* );

/* Check whether a name inside of upvalue table is existed or not */
int ajj_upvalue_has( struct ajj* a , const char* );

/* Clear the *all* the environment variables */
void ajj_env_clear( struct ajj* a );

/* =============================================================
 * IO object for holding the rendering output
 * ===========================================================*/

/* Create an IO from an existed FILE* structure */
struct ajj_io* ajj_io_create_file( struct ajj* a , FILE* );
/* Create an IO from memory with size */
struct ajj_io* ajj_io_create_mem ( struct ajj* a , size_t size );
/* Destroy an IO object , this won't result in the FILE* handler been
 * closed, user needs to call fclose on the handler if the ajj_io object
 * is a file handler IO object */
void ajj_io_destroy( struct ajj_io* );
/* Printf to an IO object */
int ajj_io_printf( struct ajj_io* , const char* fmt , ... );
/* va_list based printf to an IO object */
int ajj_io_vprintf( struct ajj_io* , const char* fmt , va_list );
/* write to IO object with memory */
int ajj_io_write( struct ajj_io* , const void* mem , size_t len );
/* flush IO object */
void ajj_io_flush( struct ajj_io* );
/* Get intenral content. Only works with memory based IO */
void* ajj_io_get_content( struct ajj_io* , size_t* size );
/* Get the internal content in a caller owned pointer. The pointer
 * returned must be freed after using it. Only works with memory based
 * IO */
void* ajj_io_detach( struct ajj_io* , size_t* size );

/* ===============================================================
 * Rendering API
 * =============================================================*/

/* Render a file into an IO object with given file name. The file must
 * be UTF encoded */
int ajj_render_file( struct ajj* ,
    struct ajj_io*,
    const char* file );

/* Render a in memory data into an IO object. The content must be UTF
 * encoded */
int ajj_render_data( struct ajj* ,
    struct ajj_io*,
    const char* src,
    const char* key);

#endif /* _AJJ_H_ */
