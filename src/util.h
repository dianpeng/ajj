#ifndef _UTIL_H_
#define _UTIL_H_

#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>

#ifndef UNREACHABLE
#define UNREACHABLE() assert(!"UNREACHABLE!")
#endif /* UNREACHABLE */

#ifndef UNUSE_ARG
#define UNUSE_ARG(X) (void)(X)
#endif /* UNUSE_ARG */

#define STRBUF_MOVE_THRESHOLD 1024

#define STRING_HASH_SEED 1771
#define DICT_MAX_SIZE (1<<29)

#define ARRAY_SIZE(X) (sizeof(X)/sizeof((X)[0]))

#define INITIAL_MEMORY_SIZE 128
#define BOUNDED_MEMORY_SIZE 1024*64

#ifndef NDEBUG
#define CHECK assert
#else
#define CHECK(X) \
  do { \
    if( !(X) ) { \
      fprintf(stderr,"assertion failed:"#X); \
      abort(); \
    } \
  } while(0)
#endif /* NDEBUG */

/* Helper macro to insert into double linked list */
#define LINIT(X) \
  do { \
    (X)->next = (X); \
    (X)->prev = (X); \
  } while(0)

#define LINSERT(X,T) \
  do { \
    (X)->next = (T); \
    (X)->prev = (T)->prev; \
    (T)->prev->next = (X); \
    (T)->prev = (X); \
  } while(0)

#define LREMOVE(X) \
  do { \
    (X)->prev->next = (X)->next; \
    (X)->next->prev = (X)->prev; \
  } while(0)

/* To make print size_t portable, the most conservative way is to
 * cast it into long long int and use llu. Modifier z is not a part
 * of C90 standard */

#ifndef C99
#define SIZEP(S) (unsigned long)(S)
#define SIZEF "%lu"
#else
#define SIZEP(S) (S)
#define SIZEF "%zu"
#endif /* C99 */

#ifndef MIN
#define MIN(X,Y) ((X)<(Y) ? (X) : (Y))
#endif

/* grow a piece of memory to a new memory. The size_t obj_sz is used represent
 * the length for each elements and the length is the size of the current array,
 * the new_cap is used to indicate the target capacity of the memory.
 * assert new_cap > len
 * and the size for the new memory is obj_sz * length
 * After calling this function, the input mem will be freeed */

void* mem_grow( void* , size_t obj_sz , size_t append, size_t* old_cap );

/* =======================================================
 * String
 * It is a one time composes object,
 * you could create one by using several routines,
 * however , after creating it, you are not supposed
 * to modify it or recreate it on same string slot.
 * String doesn't support modification at all. If you
 * want to modify , use strbuf
 * ======================================================*/

struct string {
  const char* str;
  size_t len;
};

extern struct string NULL_STRING;
extern struct string EMPTY_STRING; /* contains a "" string , good for display */
extern struct string TRUE_STRING;
extern struct string FALSE_STRING;
extern struct string NONE_STRING;

#define CONST_STRING(X) { X , ARRAY_SIZE(X)-1 }

struct string string_dup( const struct string* str );
struct string string_dupc( const char* str );

/* Do not call string_destroy on constant string */
struct string string_const( const char* str , size_t len );

int string_eq( const struct string* l , const struct string* r );

int string_eqc( const struct string* l , const char* str );

int string_cmp( const struct string* l , const struct string* r );

void string_destroy( struct string* str );

/* string null means the pointer to the str is NULL */
#define string_null(S) ((S)->str == NULL)

/* string empty means string is not NULL but it just doesn't
 * contain any content */
#define string_empty(S)(!string_null(S) && string_eq(S,&EMPTY_STRING))

struct string
string_concate( const struct string* l , const struct string* r );

struct string
string_multiply( const struct string* l , int times );


/* ====================================================
 * String buffer
 * Support pushing back and buffer manipulation
 * ===================================================*/
struct strbuf {
  char* str;
  size_t len;
  size_t cap;
};

void strbuf_reserve( struct strbuf* buf , size_t cap );

void strbuf_init( struct strbuf* buf );

void strbuf_init_cap( struct strbuf* buf , size_t cap );

void strbuf_push( struct strbuf* buf , char c );

void strbuf_append( struct strbuf* buf , const char* str , size_t len );

void strbuf_destroy( struct strbuf* buf );

char strbuf_index( const struct strbuf* buf , int idx );

void strbuf_reset( struct strbuf* buf );

#define strbuf_clear strbuf_reset

void strbuf_move( struct strbuf* buf , struct string* output );

struct string strbuf_tostring( struct strbuf* buf );


/* ===========================================
 * Map
 * =========================================*/

struct map_entry {
  struct string key;
  unsigned int hash; /* fullhash for this key */
  unsigned int next : 29; /* next resolved collision */
  unsigned int more: 1 ;  /* more collision ? */
  unsigned int used: 1 ;  /* whether this one is empty */
  unsigned int del  : 1 ; /* whether this one is deleted
                           * Please be sure that , if the empty is set to 0,
                           * it will never be reset back to 1 even if it is
                           * deleted somehow. The delete is just a marker says
                           * that in the future, this piece of memory could be
                           * used */
};

struct map {
  struct map_entry* entry;
  void* value;

  size_t cap;
  size_t len;
  size_t obj_sz;
};

struct map_pair {
  const struct string* key;
  void* val;
};

void map_create( struct map* d , size_t obj_sz ,
    size_t cap );

void map_destroy(struct map* d);

/* XXX_c APIs are used to help dealing with the public interfaces ,since public
 * interfaces are not using struct string objects */
int map_insert( struct map* , const struct string* , int own , const void* val );
int map_insert_c( struct map* , const char* key , const void* val );
int map_remove( struct map* , const struct string* , void* val );
int map_remove_c( struct map* , const char* key , void* val );
void* map_find  ( struct map* , const struct string* );
void* map_find_c( struct map* , const char* key );
void map_clear( struct map* );
#define map_size(d) ((d)->len)
/* iterator for mapionary */
int map_iter_start( const struct map* );
#define map_iter_has(D,I) ((size_t)(I) < (D)->cap)
int map_iter_move ( const struct map* , int itr );
struct map_pair map_iter_deref( struct map* d, int itr );

/* ========================================
 * Slab
 * ======================================*/
struct chunk {
  struct chunk* next;
};

struct freelist {
  struct freelist* next;
};

struct slab {
  struct chunk* ck;
  struct freelist* fl;
  size_t cur_cap;
  size_t obj_sz;
};

void slab_init( struct slab* , size_t cap , size_t obj_sz );
void slab_destroy(struct slab* );
void* slab_malloc( struct slab* );
void slab_free( struct slab* , void* );

/* =========================================
 * Other helper functions
 * =======================================*/
int is_int( double val );
char* dtoc( double val , size_t* len );
const char* const_cstr( char c );

#endif /* _UTIL_H_ */
