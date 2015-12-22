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
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"Hello World",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));

    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
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
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"Hello World",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));

    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
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
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
  }

  {
    struct ajj* a = ajj_create();
    const char* src = "{{ 1+2*3**4%3 }}\n";
    struct ajj_object* jinja;
    const struct program* prg;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
  }

  {
    struct ajj* a = ajj_create();
    const char* src = "{{ 1+2*3**4%3 > 0 and 1>=1+2*3 or -9 < 0}}\n";
    struct ajj_object* jinja;
    const struct program* prg;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
  }
  {
    struct ajj* a = ajj_create();
    const char* src = "{{ 3//4 if True == False else 4**2 }}";
    struct ajj_object* jinja;
    const struct program* prg;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
  }
  {
    struct ajj* a = ajj_create();
    const char* src = "{{ 3//4 if not True == False else 4**2 }}\n";
    struct ajj_object* jinja;
    const struct program* prg;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
  }
  {
    struct ajj* a = ajj_create();
    const char* src = "{% for index,val in xlist(1000) %}" \
                      "{{ ':'+index }}\n"\
                      "{{ False }}\n" \
                      "{% endfor %}";
    struct ajj_object* jinja;
    const struct program* prg;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
  }
  {
    struct ajj* a = ajj_create();
    const char* src = "{% for a in [1,2,3,4,5,100,1000] %}" \
                      "{{ (a,a,a,a*a) }}\n" \
                      "{% endfor %}";
    struct ajj_object* jinja;
    const struct program* prg;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
  }
  {
    struct ajj* a = ajj_create();
    const char* src = "{% set a = [] %}" \
                      "{% for i,_ in xlist(10000) %}" \
                      "{% do a.append(i) %}" \
                      "{{ i }} " \
                      "{% endfor %}\n"\
                      "{{ a }}\n";
    struct ajj_object* jinja;
    const struct program* prg;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
  }
  {
    struct ajj* a = ajj_create();
    const char* src = "{% set a = {} %}" \
                      "{% for i,_ in xlist(10) %}" \
                      "{% do a.set('HelloWorld'+i,i) %}" \
                      "{{ 'HelloWorld'+i+'='+i }} " \
                      "{% endfor %}\n" \
                      "{% do a.update('HelloWorld'+0,1000) %}" \
                      "{{ a }}";
    struct ajj_object* jinja;
    const struct program* prg;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
  }
#endif
  {
    struct ajj* a = ajj_create();
    const char* src = "{% for i in xlist(10) if i%2 ==0 %}" \
                      "{{ i }}\n"\
                      "{% endfor %}";
    struct ajj_object* jinja;
    const struct program* prg;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
  }
}

void test_loop() {
#if 0
  {
    struct ajj* a = ajj_create();
    const char* src = "{% for i in xlist(10) if i % 2 != 0 %}" \
                      "{{ ('Value='+i,'\n') }}" \
                      "{% if i < 3 %}"\
                      "{{ (i+100,'\n') }}"\
                      "{% else %}" \
                      "{{ (-i,'\n') }}"\
                      "{% endif %}"\
                      "{% endfor %}";
    struct ajj_object* jinja;
    const struct program* prg;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
  }
  {
    struct ajj* a = ajj_create();
    const char* src = "{% for i in xlist(10) %}" \
                      "{% if i % 2 != 0 %}{% continue %}{% endif %}"\
                      "{{ 'Value='+i }}\n" \
                      "{% if i >= 5 %}{% break %}{% endif %}" \
                      "{% endfor %}";
    struct ajj_object* jinja;
    const struct program* prg;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
  }
  {
    struct ajj* a = ajj_create();
    const char* src = "{% set a = { 'UUV':123 , 'VVU':456 } %}"\
                      "{% set i = 0 %}" \
                      "{% for k,v in a %}" \
                      "{% set idx = i + 1 %}"\
                      "{% set uuv = 1232 %}"\
                      "{% set ppp = 333 %}" \
                      "{% if k == 'VVU' %}{% break %}{%endif%}" \
                      "{{ k }} {{ '=' }} {{ v }}\n" \
                      "{{ 'Index:'+idx+ppp+uuv }}\n"\
                      "{% endfor %}" \
                      "{% for i in xlist(10) %}" \
                      "{{ i }}\n" \
                      "{% endfor %}";
    struct ajj_object* jinja;
    const struct program* prg;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
  }
#endif
  {
    struct ajj* a = ajj_create();
    const char* src = "{% set i = 0 %}" \
                      "{% for x in xlist(10) %}" \
                      "{% set idx = i+1 %}" \
                      "{% move i idx %}" \
                      "{{ idx }}\n"\
                      "{% endfor %}";
    struct ajj_object* jinja;
    const struct program* prg;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"_",src,0);
    assert(jinja);
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));
  }

}

int main() {
  test_loop();
}
