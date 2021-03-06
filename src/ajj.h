#ifndef _AJJ_H_
#define _AJJ_H_
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>

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
    struct ajj_value* ,
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
    void** ,
    int* );

/* Signature represents a registered object's destructor
 * function call.
 * Arg1: ajj engine pointer
 * Arg2: the opaque pointer user registered for this class
 * Arg3: the object's pointer */
typedef void (*ajj_class_dtor)( struct ajj* , void* , void* );

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
  void (*attr_set)( struct ajj* , struct ajj_value* , const struct ajj_value* ,
      const struct ajj_value* );

  /* This function is used to push an attributes into the specific object */
  void (*attr_push)( struct ajj* , struct ajj_value* , const struct ajj_value* );

  /* This function is used to help the GC algorithm works. It will be triggered
   * when a user indicate a move operations. And internally, for any intenrally
   * referenced value that comes from AJJ, user needs to call ajj_value_move on
   * those value to ensure the reference is correct to VM , otherwise those value
   * can be garbage collected. If any object doesn't reference any ajj value from
   * AJJ world, then no need to provide the implementation */
  void (*move) ( struct ajj* , struct ajj_value* );

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
ajj_value_get_type_name( const struct ajj_value* );

/* Static variable for AJJ_TRUE/AJJ_FALSE/AJJ_NONE */
extern struct ajj_value AJJ_TRUE;
extern struct ajj_value AJJ_FALSE;
extern struct ajj_value AJJ_NONE;

/* Convert a double to ajj_value */
struct ajj_value ajj_value_number( double );

/* Convert a boolean to ajj_value */
struct ajj_value ajj_value_boolean( int );

/* Construct a string to ajj_value */
struct ajj_value ajj_value_new_string( struct ajj* , const char* , size_t );

/* Construct a constant string to ajj_value. The constant
 * string means the string is not garbage collected by VM */
struct ajj_value ajj_value_new_const_string( struct ajj* , const char* ,size_t );

/* Convert a ajj_value to its corresponding boolean value.
 * The ajj_value must be boolean type */
#define ajj_value_to_boolean(V) ((V)->value.boolean)

/* Convert a ajj_value to its corresponding number value.
 * The ajj_value must be number type */
#define ajj_value_to_number(V) ((V)->value.number)

/* Convert a ajj_value to its string value. The ajj_value
 * must be string type */
const char* ajj_value_to_str( const struct ajj_value* , size_t* );

/* Construct an object based on its class name.
 * Arg1: ajj engine pointer
 * Arg2: name of the class
 * Arg3: list of arguments
 * Arg4: size of arguments
 */
struct ajj_value
ajj_value_new_object( struct ajj* , const char* , struct ajj_value* , size_t );

/* Call an object's specific method.
 * Arg1: ajj engine pointer
 * Arg2: the object's pointer
 * Arg3: the method name
 * Arg4: list of arguments
 * Arg5: size of arguments
 * Arg6: return value pointer */
int ajj_value_call_object_method( struct ajj* , struct ajj_value* , const char* ,
    struct ajj_value* , size_t , struct ajj_value* );

/* New a list object */
struct ajj_value ajj_value_new_list( struct ajj* );

/* Push a value into the list object */
void ajj_value_list_push( struct ajj* , struct ajj_value* , struct ajj_value* );

/* Get the size of the list object */
size_t ajj_value_list_size( struct ajj* , struct ajj_value* );

/* Index a value from the list object */
struct ajj_value
ajj_value_list_index( struct ajj* , struct ajj_value* ,int );

/* Clear the list */
void ajj_value_list_clear( struct ajj* , struct ajj_value* );

/* New a dict object */
struct ajj_value ajj_value_new_dict( struct ajj* );

/* Insert a key value pair into the dictionary */
void ajj_value_dict_insert( struct ajj* , struct ajj_value* ,
    struct ajj_value* , struct ajj_value* );

/* Find a key inside of the dictionary */
struct ajj_value
ajj_value_dict_find( struct ajj* , struct ajj_value* , struct ajj_value* );

/* Remove a entry in the dictionary */
int ajj_value_dict_remove( struct ajj* ,struct ajj_value* ,struct ajj_value* );

/* Clear the dictionary */
void ajj_value_dict_clear( struct ajj* , struct ajj_value* );

/* ===========================================================
 * The following function is proxy function to call
 * a specific object's slot function if it has one.
 * If the target function is existed and worked correctly
 * it return AJJ_EXEC_OK, otherwise AJJ_EXEC_FAIL is returend
 * =========================================================*/

int ajj_value_attr_get( struct ajj* , const struct ajj_value* ,
    const struct ajj_value* , struct ajj_value* );

int ajj_value_attr_set( struct ajj* , struct ajj_value* ,
    const struct ajj_value* , const struct ajj_value* );

int ajj_value_attr_push( struct ajj* , struct ajj_value* ,
    const struct ajj_value* );

const char* ajj_display( struct ajj* , const struct ajj_value* ,
    size_t* , int* );

struct ajj_value ajj_value_move( struct ajj* , const struct ajj_value* ,
    struct ajj_value* );

int ajj_value_iter_start( struct ajj* , const struct ajj_value* , int* );

int ajj_value_iter_move( struct ajj* , const struct ajj_value* ,
    int , int* );

int ajj_value_iter_has ( struct ajj* , const struct ajj_value* ,
    int , int* );

int ajj_value_iter_get_key( struct ajj* , const struct ajj_value* ,
    int , struct ajj_value* );

int ajj_value_iter_get_val( struct ajj* , const struct ajj_value* ,
    int , struct ajj_value* );

int ajj_value_in ( struct ajj* , const struct ajj_value* ,
    const struct ajj_value* , int* );

int ajj_value_eq( struct ajj* , const struct ajj_value* ,
    const struct ajj_value* , int* );

int ajj_value_ne( struct ajj* , const struct ajj_value* ,
    const struct ajj_value* , int* );

int ajj_value_lt( struct ajj* , const struct ajj_value* ,
    const struct ajj_value* , int* );

int ajj_value_le( struct ajj* , const struct ajj_value* ,
    const struct ajj_value* , int* );

int ajj_value_ge( struct ajj* , const struct ajj_value* ,
    const struct ajj_value* , int* );

int ajj_value_gt( struct ajj* , const struct ajj_value* ,
    const struct ajj_value* , int* );

int ajj_value_len( struct ajj* , const struct ajj_value* ,
    size_t* );

int ajj_value_empty( struct ajj* , const struct ajj_value* ,
    int* );

/* ===============================================================
 * AJJ file system abstraction layer
 * ==============================================================*/

/* Ajj itself doesn't take care of the file system but expecting the
 * user to provide API to allow Ajj access underlying file system.
 * This allow user to do various file path mapping and also caching. */
struct ajj_vfs {
  /* Function that perform loading of the file.
   * The first argument is the name of the file in the request ;
   * the second argument is for output of the size of the file ;
   * the third argument is in and out argument, the caller will put
   * a timestamp to its best knowledege when the file gets modified,
   * and expect the latest timestamp gets updated in that area.
   * The memory returned by this function is taken ownership by the
   * library and will be *freed* by the library .
   *
   * NOTES: The returned memory *MUST* be null terminated, even if
   * it is UTF encoded, user needs to append an extra 0 at the end
   * of the buffer. Also the size returned by the function *DO NOT*
   * need to reflect this null terminator. It means user actually
   * allocate one bytes more than the file size but the length it
   * returns is same as the file size
   *
   */
  void* (*vfs_load)( struct ajj*, const char* , size_t* , time_t* , void* );

  /* Function that perform retrieving timestame of the target path .
   * returns -1 means failed , otherwise returns 0 */
  int (*vfs_timestamp)( struct ajj* , const char* , time_t* , void* );

  /* Function to check whether this timestamp is the latest one ,
   * returns -1 means fail, returns 0 means false, otherwise returns 1*/
  int (*vfs_timestamp_is_current)( struct ajj* , const char* , time_t , void* );
};

extern struct ajj_vfs AJJ_DEFAULT_VFS;

/* Create an ajj engine. Before rendering any templates, a ajj
 * engine pointer must be created and it serves as the environment
 * and resource holder for all the template rendering happened inside
 * of it . User should never mix different ajj engine pointer togeteher */
struct ajj* ajj_create( struct ajj_vfs* , void* );

/* Destroy an ajj engine pointer */
void ajj_destroy( struct ajj* );

/* Set a user specified pointer to ajj */
void ajj_set_udata( struct ajj* , void* );
void*ajj_get_udata( struct ajj* );

/* Dump an error information into the ajj engine and be consumed by the
 * user */
void ajj_error ( struct ajj* , const char* , ... );

/* Get last error descriptive string */
const char* ajj_last_error( struct ajj* );

/* Add a value into the ajj environment. The environment is *SHARED* by
 * all the template rendered inside of the same ajj engine */
void ajj_env_add_value( struct ajj* , const char* , int , ... );

/* Add a class into ajj environment */
void ajj_env_add_class( struct ajj* , const struct ajj_class* );

/* Add a function into ajj environment */
void ajj_env_add_function( struct ajj* , const char* , ajj_function entry,
    void* );

/* Add a filter function */
#define ajj_env_add_filter ajj_env_add_function

/* Add a test function into ajj environment. Test function has exactly
 * same prototype as normal function. But a test function *MUST* return
 * true/false */
void ajj_env_add_test( struct ajj* , const char* , ajj_function entry,
    void* );

/* Check whether a name is existed inside of ajj environment */
int ajj_env_has( struct ajj* , const char* );

/* Delete a name from ajj environment. This is *NOT* for deleting
 * an class or function during the template is executing. If calling
 * this function during template rendering, the behavior is undefined */
int ajj_env_del( struct ajj* , const char* );

/* ==============================================================
 * Operation on upvalue table for specific template. The following
 * functions can only be called during the template is rendering
 * ============================================================*/

/* Add a value into the upvalue table for a specific template */
void ajj_upvalue_add_value( struct ajj* , const char* , int , ... );

/* Add a class into the upvalue table */
void ajj_upvalue_add_class( struct ajj* , const struct ajj_class* );

/* Add a function into the upvalue table */
void ajj_upvalue_add_function( struct ajj* , const char*,
    ajj_function entry, void* );

/* Add a filter function into the upvalue table */
#define ajj_upvalue_add_filter ajj_upvalue_add_function

/* Delete a name from the upvalue. The deletion operation is generally
 * not safe at all. If user delete a builtin upvalue or a upvalue that
 * is setup by the script while script is using it, the behavior is not
 * defined :) */
void ajj_upvalue_del( struct ajj* , const char* );

/* Check whether a name inside of upvalue table is existed or not */
int ajj_upvalue_has( struct ajj* , const char* );

/* Clear the *all* the environment variables */
void ajj_env_clear( struct ajj* );

/* =============================================================
 * Runtime specified user object
 * ============================================================*/

/* This function will retrieve the opaque pointer user sets when calls
 * the ajj_render_XXX function . So it is only available during the
 * execution of the template scripts */
void* ajj_runtime_get_udata( struct ajj* );

/* =============================================================
 * IO object for holding the rendering output
 * ===========================================================*/

/* Create an IO from an existed FILE* structure */
struct ajj_io* ajj_io_create_file( struct ajj* , FILE* );

/* Create an IO from memory with size */
struct ajj_io* ajj_io_create_mem ( struct ajj* , size_t );

/* Destroy an IO object , this won't result in the FILE* handler been
 * closed, user needs to call fclose on the handler if the ajj_io object
 * is a file handler IO object */
void ajj_io_destroy( struct ajj* , struct ajj_io* );

/* Printf to an IO object */
int ajj_io_printf( struct ajj_io* , const char* , ... );

/* va_list based printf to an IO object */
int ajj_io_vprintf( struct ajj_io* , const char* , va_list );

/* write to IO object with memory */
int ajj_io_write( struct ajj_io* , const void* , size_t );

/* flush IO object */
void ajj_io_flush( struct ajj_io* );

/* Get intenral content. Only works with memory based IO */
void* ajj_io_get_content( struct ajj_io* , size_t* );

/* Get the internal content in a caller owned pointer. The pointer
 * returned must be freed after using it. Only works with memory based
 * IO */
void* ajj_io_detach( struct ajj_io* , size_t* );

/* ===============================================================
 * Rendering API
 * =============================================================*/

/* Render a file into an IO object with given file name. The file must
 * be UTF encoded */
int ajj_render_file( struct ajj* , struct ajj_io*, const char* , void* );

/* Render a in memory data into an IO object. The content must be UTF
 * encoded */
int ajj_render_data( struct ajj* , struct ajj_io*, const char* , const char* ,
    void* );

#endif /* _AJJ_H_ */
