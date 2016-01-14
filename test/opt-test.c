#include "opt.h"
#include "ajj-priv.h"
#include "parse.h"
#include "object.h"
#include "bc.h"
#include "opt.h"

static
void test1() {
#if 0
  {
    const char* src = "{{ 1+2*3/4 * True - False + 19988*33 and 1+3 }}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"HelloWorld",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    dump_program(a,src,prg,stdout);
    assert( !optimize(a,obj) );
    prg = ajj_object_jinja_main(obj);
    dump_program(a,src,prg,stdout);
  }

  {
    const char* src = "{{ -1 + not True + not False + -(1+2*3)}}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"HelloWorld",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    dump_program(a,src,prg,stdout);
    assert( !optimize(a,obj) );
    prg = ajj_object_jinja_main(obj);
    dump_program(a,src,prg,stdout);
  }
#endif

  {
    const char* src = "{% if 1+2+3+4*5/6//7**8%9 %}\n" \
                      "{{ pp }}\n" \
                      "{% else %}\n" \
                      "{{ UU+11+22+33*44 }}\n" \
                      "{% endif %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"HelloWorld",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    dump_program(a,src,prg,stdout);
    assert( !optimize(a,obj) );
    prg = ajj_object_jinja_main(obj);
    dump_program(a,src,prg,stdout);
  }
}

int main() {
  test1();
  return 0;
}
