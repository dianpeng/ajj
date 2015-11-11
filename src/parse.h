#ifndef _PARSE_H_
#define _PARSE_H_
#include "ajj.h"
#include "util.h"

#define LOOP_CONTINUE 0
#define LOOP_BREAK 1

int parse( struct ajj* , struct gc_scope* ,
    const char* src, const char* key, ajj_value* output );


#endif /* _PARSE_H_ */
