#ifndef _BUILTIN_H_
#define _BUILTIN_H_
#include <stddef.h>

struct ajj;
struct ajj_value;
struct ajj_object;
struct gc_scope;


void ajj_builtin_load( struct ajj* );

/* ==============================
 * LOOP
 * ============================*/

/* This function is required by VM to make LOOP object
 * works */
void builtin_loop_move( struct ajj_value* loop );

struct ajj_object*
json_parse( struct ajj* a , struct gc_scope* scp,
    const char* filename , const char* func );

/* helper function for our VM to cast an object to the
 * internal map object */
int object_is_map( struct ajj_value* );
struct map* object_cast_to_map( struct ajj_value* );


#endif /* _BUILTIN_H_ */
