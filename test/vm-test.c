#include <vm.h>
#include <parse.h>
#include <ajj-priv.h>
#include <object.h>
#include <ajj-priv.h>
#include <bc.h>
#include <opt.h>
#include <stdlib.h>

static void do_test( const char* src ) {
  struct ajj* a = ajj_create();
  struct ajj_object* jinja;
  const struct program* prg;
  const struct program* another_prg;
  struct ajj_io* output = ajj_io_create_file(a,stdout);
  const struct string NAME = { "Input" , 5 };
  jinja = parse(a,"Hello World",src,0);
  if(!jinja) {
    fprintf(stderr,"%s",a->err);
    abort();
  }
  prg = ajj_object_jinja_main(jinja);
  dump_program(a,src,prg,output);
  assert(!optimize(a,jinja));
  dump_program(a,src,prg,output);
  if(vm_run_jinja(a,jinja,output)) {
    fprintf(stderr,"%s",a->err);
    abort();
  }

  ajj_io_destroy(output);
  ajj_destroy(a); /* all memory should gone */
}

static
void test_simple() {
  {
    const char* src = "ABCDEF {{ yyy }} {{ __func__ }} {{ __argnum__*2 if __argnum__ > 0 else 1989 }} BCDEF\n" \
                       "{% set var='abc123' %}\n" \
                       "{% if __argnum__ != 0 %} __argnum__ : {{ __argnum__ }} \n" \
                       "{% elif var >= 1*2-3/4 %} __argnum__ : TRUE {{ __argnum__ }} \n" \
                       "{% else %} __argnum__ : {{ False }} False {{ 8*8*8 }} \n" \
                       "{% endif %}\n";
    do_test(src);
  }
  {
    const char* src = "ABCDEF {{ yyy }} {{ __func__ }} {{ __argnum__*2 if __argnum__ > 0 else 1989 }} BCDEF\n" \
                       "{% set var = 'hello_world' %}\n" \
                       "{% set vb = true %}\n" \
                       "{% if var == 'hello_world' %} {{ 'Hello World' }} {% endif %}\n" \
                       "{% if vb %} {{ 'TRUE' }} {% endif %}\n";
    do_test(src);
  }
}

/* ==================================
 * TEST EXPRESSION
 * ================================*/

#define TEST(A,B) \
  do { \
    do_test("{% do assert_expr( A == B , 'A == B')%}"); \
  } while(0)

static
void test_expr() {
  /* Arithmatic */
  TEST(1+2*3,7);
  TEST(1+2**3,9);
  TEST(1+4,5);
  TEST(1*5,5);
  TEST(100*100,10000);
  TEST(5/100,0.02);
  TEST(5%6,1);
  do_test("{% do assert_expr(5//5==1,'5//2==1') %}");
  TEST(1*2**3 && 4/5 or 1,True);
  TEST(1+2*3/4 >= 5 and 90,False);
  TEST(3/4 if True == False else 4**2,4**2);
  {
    const char* src = "{% for index,val in xrange(1000) %}" \
                       "{{ ':'+index }}\n"\
                       "{{ False }}\n" \
                       "{% endfor %}";
    do_test(src);
  }
  {
    const char* src = "{% for a in [1,2,3,4,5,100,1000] %}" \
                       "{{ (a,a,a,a*a) }}\n" \
                       "{% endfor %}";
    do_test(src);
  }
  {
    const char* src = "{% set a = [] %}" \
                       "{% for i,_ in xrange(10000) %}" \
                       "{% do a.append(i) %}" \
                       "{{ i }} " \
                       "{% endfor %}\n"\
                       "{{ a }}\n";
    do_test(src);
  }
  {
    const char* src = "{% set a = {} %}" \
                       "{% for i,_ in xrange(10) %}" \
                       "{% do a.set('HelloWorld'+i,i) %}" \
                       "{{ 'HelloWorld'+i+'='+i }} " \
                       "{% endfor %}\n" \
                       "{% do a.update('HelloWorld'+0,1000) %}" \
                       "{{ a }}";
    do_test(src);
  }
  {
    const char* src = "{% for i in xrange(10) if i%2 ==0 %}" \
                       "{{ i }}\n"\
                       "{% endfor %}";
    do_test(src);
  }

  do_test("{% set my_dict = {'abc':123} %}"\
          "{% do assert_expr(my_dict.abc == 123,'_') %}" \
          "{% do assert_expr(my_dict['abc']==123,'_') %}");

  do_test("{% set my_dict = {'abc':[1,2,3] , 'u'*3:'f'*3 } %}" \
          "{% do assert_expr(my_dict.abc[1] == 2,'_') %}" \
          "{% do assert_expr(my_dict.abc[0] == 1,'_') %}" \
          "{% do assert_expr(my_dict.uuu == 'fff','_') %}" \
          "{% do assert_expr(my_dict['uuu'] == 'fff','_') %}" \
          "{% set dict2 = { 'new_dict' : my_dict , 'vv':[1,2,3,4] } %}"\
          "{% do assert_expr(dict2.new_dict.uuu == 'fff','_') %}" \
          "{% do assert_expr(dict2.new_dict.abc[2] == 3,'_') %}" \
          "{% do assert_expr(dict2.vv[3] == 4,'_') %}");

  do_test("{% set my_list = [] %}" \
          "{% for i in xrange(1000) if i % 3 == 0 %}" \
          "{% do my_list.append(i) %}" \
          "{% endfor %}" \
          "{% set cnt = 0 %}" \
          "{% for i in xrange(1000) if i % 3 == 0 %}" \
          "{% set idx = cnt+1 %}" \
          "{{ 'idx'+':'+idx }}\n"\
          "{% move cnt = idx %}" \
          "{{ 'idx'+':'+idx }}\n"\
          "{% do assert_expr(my_list[idx-1]==i,'_'+i+':'+idx) %}" \
          "{% endfor %}");

  TEST(2 is odd,False);
  TEST(3 is odd,True);
  TEST(4 is even,True);
  TEST(NotDefine is not defined,True);
}

void test_loop() {
  {
    const char* src = "{% for i in xrange(10) if i % 2 != 0 %}" \
                       "{{ ('Value='+i,'\n') }}" \
                       "{% if i < 3 %}"\
                       "{{ (i+100,'\n') }}"\
                       "{% else %}" \
                       "{{ (-i,'\n') }}"\
                       "{% endif %}"\
                       "{% endfor %}";
    do_test(src);
  }
  {
    const char* src = "{% for i in xrange(10) %}" \
                       "{% if i % 2 != 0 %}{% continue %}{% endif %}"\
                       "{{ 'Value='+i }}\n" \
                       "{% if i >= 5 %}{% break %}{% endif %}" \
                       "{% endfor %}";
    do_test(src);
  }
  {
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
    do_test(src);
  }
  {
    const char* src = "{% set i = 0 %}" \
                       "{% for x in xrange(99999) %}" \
                       "{% set idx = i+1 %}" \
                       "{% move i=idx %}" \
                       "{{ idx }}\n"\
                       "{% endfor %}";
    do_test(src);
  }
  { /* nested loop */
    const char* src = "{% for a in xrange(10) %}" \
                       "{% for b in xrange(10) %}" \
                       "{{ (a,'+',b,'=',a+b) }}\n" \
                       "{% endfor %}" \
                       "{% endfor %}";
    do_test(src);
  }
  { /* nested loop with control */
    const char* src = "{% for a in xrange(10) %}" \
                       "{% if a % 3 %}{% continue %}{% endif %}" \
                       "{% for b in xrange(10) %}" \
                       "{% if b > 6 %}{% break %} {% endif %}" \
                       "{% if b % 2 %}{% continue %}{% endif %}" \
                       "{{ (a,'+',b,'=',a+b) }}\n" \
                       "{% endfor %}" \
                       "{% endfor %}";
    do_test(src);
  }
  { /* nested loop with some move semantic and local variable */
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
    do_test(src);
  }
  /* other misc member functions testing */
  {
    const char* src = "{% set arr = [1,2,3,4,{'A':[123,45],'B':'Hell','C':[]}] %}" \
                       "{% for a in arr %}" \
                       "{{ 'Length'+'='+ #arr}}\n" \
                       "{{ a }}\n" \
                       "{% endfor %}";
    do_test(src);
  }
  {
    const char* src = "static const char* STRING_TABLE[] = {\n" \
                       "{% for i in xrange(256) %}" \
                       "{ {{ i }},{{ 0 }} },\n" \
                       "{% endfor %}"\
                       "};";
    do_test(src);
  }
  {
    const char* src = "{% set str = 'AbcdEfgHIJK' %}" \
                       "{% for a in xrange(#str) %}" \
                       "{{ str[a] }}\n" \
                       "{% endfor %}";
    do_test(src);
  }
  do_test("{% for i in xrange(100) %}" \
      "{{ loop }}\n" \
      "i={{i}}\n" \
      "{% endfor %}");
}

void test_branch() {
  {
    struct ajj* a = ajj_create();
    const char* src = "{% if 101+123 >= 245 %}" \
                       "{{ True }}\n" \
                       "{% else %}" \
                       "{{ False }}\n" \
                       "{% endif %}";
    do_test(src);
  }
  {
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
    do_test(src);
  }
  do_test("{% set c = cycler('a','b','c','UUV',[1,2,3,4,{'b':true,'f':false}]) %}" \
      "{% for x in xrange(1000) %}" \
      "x={{x}}\n" \
      "current = {{ c.next() }}\n" \
      "{% endfor %}");
}

/* MOVE operations */
void test_move() {
  {
    const char* src = "{% set L = 0 %}" \
                       "{{ 'L='+L }}\n" \
                       "{% with %}"
                       "{% with %}" \
                       "{% set MyVar = [1,2,3,4] %}" \
                       "{% move L = MyVar %}" \
                       "{% endwith %}" \
                       "{% endwith %}"
                       "{{ 'L=' }}{{ L }}\n";
    do_test(src);
  }
  { /* move within same scope/lexical scope */
    const char* src = "{% set L = 0 %}" \
                       "{{ 'L='+L }}\n" \
                       "{% set MyVar = [1,2,3,4] %}" \
                       "{% move L = MyVar %}" \
                       "{{ 'L=' }}{{ L }}\n";
    do_test(src);
  }
}

/* WITH */
void test_with() {
  { /* move within same scope/lexical scope */
    const char* src = "{% with L = None %}" \
                       "{% with L = 200 %}" \
                       "{{ 'InnerL'+L }}\n" \
                       "{% endwith %}" \
                       "{{ 'OuterL'+L }}\n" \
                       "{% endwith %}";
    do_test(src);
  }
}

/* ============================
 * Function/Macro
 * ==========================*/

void test_macro() {
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

static
void test_random_1() {
  do_test("{% macro Input(title,class='dialog') %}" \
      "CallerStub=\n{{ caller('MyBoss','Overwrite') }}\n" \
      "{% do caller('MyBoos','Overwrite','Watch Me','Whip','Nae Nae') %}" \
      "{% endmacro %}" \
      "{% call(args,def='123') Input('Hello World','Caller') %}" \
      "argnum={{__argnum__}}\n" \
      "VARGS={{vargs}}\n" \
      "{% endcall %}");
  do_test("{% import 'import.html' as T %}" \
      "{% import 'import.html' as T2%}" \
      "{{ T.Test('Hello World') }}" \
      "{{ T2.Test('Hello World2') }}");
  do_test("{{ to_jsonc('{ \"Y\":[] , \
    \"V\":{},  \
      \"Hello\" : \
      \"World\" , \
      \"None\":null,\
      \"True\":true,\
      \"False\":\
      false,\
      \"UUV\":[1,2,3,4,5] \
      }') }}\n");
  do_test("{% for x in xrange(10)%} III={{x}}\n {% endfor %}" \
      "{% extends 'base.html' %}" \
      "{% block Test3 %} vargs={{vargs}} {% endblock %}");
  do_test("{% include 'include.html' %}"
      "Hello World From Child!\n");
  do_test("{% include rm_trail(shell('pwd')) + '/include.html' %}"
      "{% import 'include.html' as Lib %}"
      "{% set arr = [1,2,3,4,5,6,7,8,9] %}"
      "Array = {{arr}}\n"
      "Sum = {{ Lib.array_sum(arr) }}\n"
      "Odd = {{ Lib.odd_filter(arr)}}\n"
      );
  do_test("{{ -3*2>7-1998 | abs | abs | abs | abs }}");
}

int main() {
#if 0
  test_simple();
  test_expr();
  test_loop();
  test_with();
  test_move();
  test_branch();
  test_macro();
  test_random_1();
#endif
  do_test("{% for i in xrange(1000) if i % 3 == 0 %}" \
          "{% set idx = cnt+1 %}" \
          "{{ idx }}\n" \
          "{% endfor %}");
}
