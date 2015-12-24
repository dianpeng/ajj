#ifndef _BUILTIN_H_
#define _BUILTIN_H_
#include <stddef.h>

struct ajj;
void ajj_builtin_load( struct ajj* );

/* ==============================
 * LOOP
 * ============================*/

/* This function is required by VM to make LOOP object
 * works */
struct ajj_value;
void builtin_loop_move( struct ajj_value* loop );


#endif /* _BUILTIN_H_ */
