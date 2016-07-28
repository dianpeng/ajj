#ifndef _GC_H_
#define _GC_H_
#include "conf.h"
#include "object.h"


/* Garbage Collection
 * A key feature for our implementation is that we don't have a *GOOD*
 * gc implementation. Why that is a feature ? Because we don't pay for
 * a real GC. In CPython , it uses reference counting to implement the
 * GC with a check routine to ensure reference counting works correctly.
 * In many other language a real GC is implemented, mark and sweep. In
 * our code, we don't support these fancy algorithms, but just strictly
 * do memory management on lexical scope. Each lexical scope will have
 * a gc_scope object which is responsible for all the heap objects in
 * that scope. We emit VM_ENTER instruction on each lexical scope to help
 * VM allocate new gc_scope object. Because Jinja grammar doesn't allow
 * explicit assign value to outer scope, this means we are entirely safe
 * for managing those heap memory since they cannot "escape" from the
 * current scope. Once a lexical scope is exit, we emit a VM_EXIT instruction
 * to destroy the corresponding gc_scope which contains all the memory.
 *
 * What if user wants to move one memory to another scope ? Each object
 * , if it is heap allocated , will contain a scope pointer to its OWNED
 * scope. By simply comparing the target scope object and the current
 * scope object, we can easily decide the ownership of this object.
 * Example:
 *
 * {
 *   object1 = U;
 *   {
 *      object2 = V;
 *   }
 * }
 *
 * Suppose you want to move object2 to object1, and they are living in
 * different lexical scope. What we need to do is just move object2 to
 * object1's scope. Why we need to move, because object1's scope has
 * longer lifespan than the scope of object2.
 *
 * In our implementation, because we allow branch and other loop control
 * statement, those statments CAN break the exeuction flow then result in
 * exit not been executed. Especially for break in loop. To resolve this,
 * the parser will generate special jump instruction when this jump do
 * cross the lexical scope boundary. And this jump instruction also carries
 * information that how many scope objects needs to be cleared after the
 * jump performed. This information can be retrieved by simply counting
 * the lexical scope while parsing.
 *
 * With all above, we have a nearly deterministic GC alrogithm like reference
 * count but we don't pay cost to invoke some mark and sweep or cycle
 * reference detection to "FIX" the buffer thing. But this thing only works
 * in jinja since it has a very strict variable assignment rules.
 * So happy to not spend 1000 lines of code to craft a mark-sweep algorithm :)
 */

struct ajj;

struct gc_scope {
  struct ajj_object gc_tail; /* tail of the GC objects list */
  struct gc_scope* parent;   /* parent scope */
  unsigned int scp_id;       /* scope id */
};

#define gc_root_init(S,I) \
  do { \
    LINIT(&((S)->gc_tail)); \
    (S)->parent = NULL; \
    (S)->scp_id = (I); \
  } while(0)

#define gc_init_temp(S,T) \
  do { \
    LINIT(&((S)->gc_tail)); \
    (S)->parent = (T)->parent; \
    (S)->scp_id = (T)->scp_id; \
  } while(0)

/* Merge the object from temporary GC scope to destination GC scope
 * and this require us to rewrite the scp field in each object as well */
void gc_scope_merge( struct gc_scope* , struct gc_scope* );

struct gc_scope*
gc_scope_create( struct ajj* , struct gc_scope* );


void gc_scope_exit( struct ajj* , struct gc_scope* );

/* This function will destroy all the gc scope allocated memory and also
 * the gc_scope object itself */
void gc_scope_destroy( struct ajj* , struct gc_scope* );

#endif /* _GC_H_ */
