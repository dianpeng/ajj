#include "vm.h"
#include "parse.h"
#include "ajj-priv.h"
#include "object.h"
#include "ajj-priv.h"
#include "bc.h"
#include "opt.h"

static void do_test( const char* src ) {
    struct ajj* a = ajj_create();
    struct ajj_object* jinja;
    const struct program* prg;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    jinja = parse(a,"Hello World",src,0);
    if(!jinja) {
      fprintf(stderr,"%s",a->err);
      assert(0);
    }
    prg = ajj_object_jinja_main(jinja);
    dump_program(a,src,prg,output);
    if(vm_run_jinja(a,jinja,output)) {
      fprintf(stderr,"%s",a->err);
      assert(0);
    }

    assert(!optimize(a,jinja));
    dump_program(a,src,prg,output);
    assert(!vm_run_jinja(a,jinja,output));

    ajj_io_destroy(output);
    ajj_destroy(a); /* all memory should gone */
}

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
    const char* src = "{% for index,val in xrange(1000) %}" \
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
                      "{% for i,_ in xrange(10000) %}" \
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
                      "{% for i,_ in xrange(10) %}" \
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
    const char* src = "{% for i in xrange(10) if i%2 ==0 %}" \
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
    const char* src = "{% for i in xrange(10) if i % 2 != 0 %}" \
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
    const char* src = "{% for i in xrange(10) %}" \
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
                      "{% for i in xrange(10) %}" \
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
  {
    struct ajj* a = ajj_create();
    const char* src = "{% set i = 0 %}" \
                      "{% for x in xrange(99999) %}" \
                      "{% set idx = i+1 %}" \
                      "{% move i=idx %}" \
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
  { /* nested loop */
    struct ajj* a = ajj_create();
    const char* src = "{% for a in xrange(10) %}" \
                        "{% for b in xrange(10) %}" \
                          "{{ (a,'+',b,'=',a+b) }}\n" \
                        "{% endfor %}" \
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
  { /* nested loop with control */
    struct ajj* a = ajj_create();
    const char* src = "{% for a in xrange(10) %}" \
                      "{% if a % 3 %}{% continue %}{% endif %}" \
                        "{% for b in xrange(10) %}" \
                          "{% if b > 6 %}{% break %} {% endif %}" \
                          "{% if b % 2 %}{% continue %}{% endif %}" \
                          "{{ (a,'+',b,'=',a+b) }}\n" \
                        "{% endfor %}" \
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
  { /* nested loop with some move semantic and local variable */
    struct ajj* a = ajj_create();
    const char* src = "{% set outside = 0 %}" \
                      "{% for a in xrange(10) %}" \
                      "{% set invar = 100 %}" \
                        "{% for b in xrange(10) %}" \
                          "{% set i2 = outside+1 %}" \
                          "{% move outside=i2 %}" \
                          "{{ (a,'+',b,'=',a+b) }}\n" \
                        "{% endfor %}" \
                        "{{ ('L',l1,'\n') }}" \
                      "{% endfor %}"\
                      "{{ outside }}\n";
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
  /* other misc member functions testing */
  {
    struct ajj* a = ajj_create();
    const char* src = "{% set arr = [1,2,3,4,{'A':[123,45],'B':'Hell','C':[]}] %}" \
                      "{% for a in arr %}" \
                      "{{ 'Length'+'='+ #arr}}\n" \
                      "{{ a }}\n" \
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
    const char* src = "static const char* STRING_TABLE[] = {\n" \
                      "{% for i in xrange(256) %}" \
                      "{ {{ i }},{{ 0 }} },\n" \
                      "{% endfor %}"\
                      "};";
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
    const char* src = "{% set str = 'AbcdEfgHIJK' %}" \
                      "{% for a in xrange(#str) %}" \
                      "{{ str[a] }}\n" \
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

void test_branch() {
#if 0
  {
    struct ajj* a = ajj_create();
    const char* src = "{% if 101+123 >= 245 %}" \
                      "{{ True }}\n" \
                      "{% else %}" \
                      "{{ False }}\n" \
                      "{% endif %}";
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
    const char* src = "{% set l = xrange(100) %}" \
                      "{% for a in l %}" \
                      "{{ 'List='+#l }}\n" \
                      "{% if a % 10 == 0 %}" \
                      "{{ '1'+':'+a }}\n" \
                      "{% elif a % 10 == 1 %}" \
                      "{{ '2'+':'+a }}\n" \
                      "{% elif a % 10 == 3 %}" \
                      "{{ '3'+':'+a }}\n" \
                      "{% elif a % 10 == 4 %}" \
                      "{{ '4'+':'+a }}\n" \
                      "{% elif a % 10 == 5 %}" \
                      "{{ '5'+':'+a }}\n" \
                      "{% elif a % 10 == 6 %}" \
                      "{{ '6'+':'+a }}\n" \
                      "{% else %}" \
                      "{{ 'Rest'+':'+a }}\n" \
                      "{% endif %}" \
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

/* MOVE operations */
void test_move() {
  {
    struct ajj* a = ajj_create();
    const char* src = "{% set L = 0 %}" \
                      "{{ 'L='+L }}\n" \
                      "{% with %}"
                      "{% with %}" \
                      "{% set MyVar = [1,2,3,4] %}" \
                      "{% move L = MyVar %}" \
                      "{% endwith %}" \
                      "{% endwith %}"
                      "{{ 'L=' }}{{ L }}\n";
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
  { /* move within same scope/lexical scope */
    struct ajj* a = ajj_create();
    const char* src = "{% set L = 0 %}" \
                      "{{ 'L='+L }}\n" \
                      "{% set MyVar = [1,2,3,4] %}" \
                      "{% move L = MyVar %}" \
                      "{{ 'L=' }}{{ L }}\n";
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

/* WITH */
void test_with() {
  { /* move within same scope/lexical scope */
    struct ajj* a = ajj_create();
    const char* src = "{% with L = None %}" \
                      "{% with L = 200 %}" \
                        "{{ 'InnerL'+L }}\n" \
                      "{% endwith %}" \
                      "{{ 'OuterL'+L }}\n" \
                      "{% endwith %}";
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

/* ============================
 * Function/Macro
 * ==========================*/

void test_macro() {
#if 0
  /* simple macro */
  do_test("{% macro User(Name,Value) %}" \
            "Name={{Name}};Value={{Value}}\n"\
          "{% endmacro %}"\
          "{% for a in xrange(100) %}" \
          "{% do User('YourName',100) %}"
          "{% endfor %}"
          );
  do_test("{% macro User(Name,Value,Def=['123',456],UUVV='345') %}" \
          "Name={{Name}}\nValue={{Value}}\nDef={{Def}}\nUUVV={{UUVV}}\n" \
          "{% endmacro %}" \
          "{% for a in xrange(2) %}" \
          "{% do User('Hulu'+a,a) %}\n"\
          "{% endfor %}");
#endif
  do_test(
      "{% macro Foo(my_foo,bar='123') %}" \
      "ARGNUM:{{__argnum__}}\n" \
      "FUNC:{{__func__}}\n" \
      "{% for a in xrange(2) %}" \
      "my_foo:{{a}}={{my_foo}}\n"\
      "{% do Bar(a,bar) %}"\
      "{% endfor %}" \
      "{% endmacro %}" \
      "{% macro Bar(arg,def={'abc':[1,2,3,[123123123,23]],'A':'B'}) %}"\
      "ARGNUM:{{__argnum__}}\n" \
      "FUNC:{{__func__}}\n" \
      "{% for p in xrange(2) %}" \
      "arg:{{p}}={{arg}}\n" \
      "def={{def}}\n"\
      "{% endfor %}" \
      "{% endmacro %}"\
      "{% do Foo('Hi') %}"
      );
}

int main() {
  do_test("{% set my_dict = {'abc':123} %}"\
          "{{ my_dict.abc }}\n" \
          "{{ my_dict['abc'] }}\n");
}
