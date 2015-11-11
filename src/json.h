#ifndef _JSON_H_
#define _JSON_H_
#include "util.h"

struct ajj;
struct gc_scope;
struct ajj_value;

/* =================================
 * Json
 * ================================*/

/* Parse a json document into a OBJECT object. The json and ajj object
 * has a 1:1 mapping. The internal mapping is as follow :
 * json:string --> string
 * json:null   --> None
 * json:boolean--> boolean
 * json:number --> number
 * json:list   --> list
 * json:object --> dictionary */

int json_decode ( struct ajj* , struct gc_scope* ,
    const char* , struct ajj_value* );



#endif /* _JSON_H_ */
