#ifndef _OPT_H_
#define _OPT_H_

struct program;

/* A very peephole optimizer for following reason :
 * 1. remove redundancy instruction (NOPX)
 * 2. constant folding */
int optimize( struct program* );


#endif /* _OPT_H_ */
