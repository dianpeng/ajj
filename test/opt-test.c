#include "ajj-priv.h"
#include "parse.h"
#include "object.h"
#include "bc.h"
#include "opt.h"

#include <stdlib.h>
#include <stdio.h>

#define FATAL(X,MSG,...) \
    do { \
        if(!(X)) { \
            fprintf(stderr,MSG,__VA_ARGS__); \
            abort(); \
        } \
    } while(0)

static int check_same( struct ajj_io* l , struct ajj_io* r ) {
  size_t lsz ;
  void* lc;
  size_t rsz;
  void* rc;

  lc = ajj_io_get_content(l,&lsz);
  rc = ajj_io_get_content(r,&rsz);
  return lsz == rsz && (memcmp(lc,rc,lsz)==0);
}

static void print( int n, struct ajj_io* l ) {
  size_t sz;
  void* c;
  char* p;

  c = ajj_io_get_content(l,&sz);
  p = malloc(sz+1);
  memcpy(p,c,sz);
  p[sz] = 0;
  printf("%d:\n%s\n",n,p);
  free(p);
}

static int COUNT = 0;

/* For optimization test, we use self testing. Basically it runs the template
 * without optimization once and then run it with optimization on once. After
 * that, start comparing these 2 results to see whether we have difference or
 * not */

void do_test( const char* src ) {
    struct ajj* a;
    struct ajj_object* jinja;
    struct ajj_io* nopt_io;
    struct ajj_io* opt_io;

    a = ajj_create();
    nopt_io = ajj_io_create_mem(a,1024);
    opt_io = ajj_io_create_mem(a,1024);

    jinja = parse(a,"<test>",src,0);
    FATAL(jinja,"%s",a->err);
    FATAL(!vm_run_jinja(a,jinja,nopt_io),"%s",a->err);
    /* optimize it */
    FATAL(!optimize(a,jinja),"%s",a->err);
    /* run again */
    FATAL(!vm_run_jinja(a,jinja,opt_io),"%s",a->err);
    /* check whether same */
    FATAL(check_same(nopt_io,opt_io),"%s","Optimization change the semantic!");
    print(COUNT,nopt_io);
    ++COUNT;
}

/* Expression based on constant folding */
void test_expr() {
  /* numeric */
  do_test("{{ 1+2 }}"); /* folding add */
  do_test("{{ 1-2 }}"); /* folding sub */
  do_test("{{ 1*2 }}"); /* folding mul */
  do_test("{{ 1/2 }}"); /* folding div */
  do_test("{{ 3//3}}"); /* folding div */
  do_test("{{ 8%4 }}"); /* folding remainder */
  do_test("{{ 3**2}}"); /* folding pow */
  do_test("{{ 3 >= 2 }}"); /* folding comparison */
  do_test("{{ 3 < 2 }}" );
  do_test("{{ 1 == 1 }}");
  do_test("{{ 1 != 1 }}");
  do_test("{{ 2> 3 }}");
  do_test("{{ 2>=3 }}");
  do_test("{{ -5 }}");
  do_test("{{ not 5 }}");

  /* string */
  do_test("{{ 'hello' + 'world' }}");
  do_test("{{ 'h'*3 }}");
  do_test("{{ 'hello' > 'h' }}");
  do_test("{{ 'hello' == 'hello' }}");
  do_test("{{ 'you' < 'y' }}");
  do_test("{{ 'you' <= 'you' }}");
  do_test("{{ 'key' >= 'k' }}");

  /* composition */
  do_test("{{ 1+2*3/4 }}");
  do_test("{{ 1>=2 and 3<=4 or not -5 }}");
  do_test("{{ 3 if 5 >= 7 else 1+2*3/4 }}");
  do_test("{{ 77**1 >= 2 and 3 <= 1 or (1+2<3*4+5//6/7) }}");

  /* other sort of complicated crap */
  do_test("{{ (-3 | abs) + 3*5/12 }}");
  do_test("{{ ( 2 is odd ) != False }}");
  do_test("{{ default(1+2*3/4 < 3/4 ,'not default') }}");
  do_test("{{ [1+2*3,4/5,100*2/23>=7 and 90] }}");
  do_test("{{ { 'ab'+'cd': 'U'*3 , 'PP'*2 : [1,2*3,4] } }}");
}

int main() {
  test_expr();
  return 0;
}
