#ifndef _PARSE_H_
#define _PARSE_H_
#include "conf.h"
#include <time.h>

#define MAX_LOOP_CTRL_SIZE 32

/* Parse the source from src into an compiled object stored inside of
 * output. The compiled object's memory is owned by the correpsonding
 * garbage collector scope */
struct ajj_object;
struct ajj;

struct ajj_object*
parse( struct ajj* , const char* key, const char* src, int own , time_t ts );

#endif /* _PARSE_H_ */
