#ifndef _PARSE_H_
#define _PARSE_H_
#include "ajj.h"
#include "util.h"

/* Parse the source from src into an compiled object stored inside of
 * output. The compiled object's memory is owned by the correpsonding
 * garbage collector scope */
struct ajj_object;

struct ajj_object*
parse( struct ajj* , const char* , const char* , int );

#endif /* _PARSE_H_ */
