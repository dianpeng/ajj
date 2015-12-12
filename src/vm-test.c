#include "vm.h"
#include "parse.h"
#include "ajj-priv.h"
#include "object.h"
#include "ajj-priv.h"
#include "bc.h"
#include "opt.h"

static
void test1() {
  {
    struct ajj* a = ajj_create();
    const char* src = "ABCDEF {{ yyy }} {{ __func__ }} {{ __argnum__*2 if __argnum__ > 0 else 1989 }} BCDEF\n" \
                       "{% set var='abc123' %}\n" \
                       "{% if __argnum__ != 0 %} __argnum__ : {{ __argnum__ }} \n" \
                       "{% elif var >= 1*2-3/4 %} __argnum__ : TRUE {{ __argnum__ }} \n" \
                       "{% else %} __argnum__ : {{ False }} False {{ 8*8*8 }} \n" \
                       "{% endif %}\n";
    struct ajj_object* jinja;
    const struct program* prg;
    jinja = parse(a,"Hello World",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));

    assert(!optimize(a,jinja));
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));
  }
  {
    struct ajj* a = ajj_create();
    const char* src = "ABCDEF {{ yyy }} {{ __func__ }} {{ __argnum__*2 if __argnum__ > 0 else 1989 }} BCDEF\n" \
                      "{% set var = 'hello_world' %}\n" \
                      "{% set vb = true %}\n" \
                      "{% if var == 'hello_world' %} {{ 'Hello World' }} {% endif %}\n" \
                      "{% if vb %} {{ 'TRUE' }} {% endif %}\n";
    struct ajj_object* jinja;
    const struct program* prg;
    jinja = parse(a,"Hello World",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));

    assert(!optimize(a,jinja));
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));
  }
}

/* ==================================
 * TEST EXPRESSION
 * ================================*/

static
void test_expr() {
#if 0
  {
    struct ajj* a = ajj_create();
    const char* src = "{{ 1+2*3/4 }}\n";
    struct ajj_object* jinja;
    const struct program* prg;
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));
    assert(!optimize(a,jinja));
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));
  }

  {
    struct ajj* a = ajj_create();
    const char* src = "{{ 1+2*3**4%3 }}\n";
    struct ajj_object* jinja;
    const struct program* prg;
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));
    assert(!optimize(a,jinja));
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));
  }

  {
    struct ajj* a = ajj_create();
    const char* src = "{{ 1+2*3**4%3 > 0 and 1>=1+2*3 or -9 < 0}}\n";
    struct ajj_object* jinja;
    const struct program* prg;
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));
    assert(!optimize(a,jinja));
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));
  }
  {
    struct ajj* a = ajj_create();
    const char* src = "{{ 3//4 if True == False else 4**2 }}";
    struct ajj_object* jinja;
    const struct program* prg;
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));
    assert(!optimize(a,jinja));
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));
  }
  {
    struct ajj* a = ajj_create();
    const char* src = "{{ 3//4 if not True == False else 4**2 }}\n";
    struct ajj_object* jinja;
    const struct program* prg;
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));
    assert(!optimize(a,jinja));
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));
  }
#endif
  {
    struct ajj* a = ajj_create();
    const char* src = "{% set a = 'UUVV' %}\n" \
                      "{{ a + 'ppVV'*3 }}\n";
    struct ajj_object* jinja;
    const struct program* prg;
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));
    assert(!optimize(a,jinja));
    dump_program(src,prg,stdout);
    assert(!vm_run_jinja(a,jinja,stdout));
  }
}

int main() {
  test_expr();
}

































