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
    const char* src = "{% set idx = 0 %}" \
                      "{% for index,val in xrange(1000) %}" \
                      "{% set i = idx + 1 %}" \
                      "{% do assert_expr(index == i-1) %}" \
                      "{% move idx = i %}" \
                      "{% endfor %}";
    do_test(src);
  }
  {
    const char* src = "{% set arr = [1,2,3,4,5,100,1000 ] %}" \
                      "{% for i,val in arr %}" \
                      "{% do assert_expr(val == arr[i]) %}" \
                      "{% endfor %}";
    do_test(src);
  }
  {
    const char* src = "{% set a = [] %}" \
                       "{% for i,_ in xrange(10000) %}" \
                       "{% do a.append(i) %}" \
                       "{% endfor %}\n"\
                       "{% for i in xrange(10000) %}" \
                       "{% do assert_expr(a[i] == i) %}" \
                       "{% endfor %}";
    do_test(src);
  }
  {
    const char* src = "{% set a = {} %}" \
                      "{% for i,_ in xrange(10) %}" \
                      "{% do a.set( 'HelloWorld'+i , i ) %}" \
                      "{% endfor %}" \
                      "{% set idx = 0 %}" \
                      "{% for i,v in a %}" \
                      "{% set num = idx + 1 %}" \
                      "{% move idx = num %}" \
                      "{% do assert_expr( i == 'HelloWorld'+(num-1) ) %}" \
                      "{% do assert_expr( v == num-1 ) %}" \
                      "{% endfor %}";
    do_test(src);
  }
  {
    const char* src =  "{% set arr = [] %}" \
                       "{% for i in xrange(10) if i%2 ==0 %}" \
                       "{% do arr.append(i) %}" \
                       "{% endfor %}" \
                       "{% for i,v in arr %}" \
                       "{% do assert_expr(v == i*2) %}" \
                       "{% do assert_expr(arr[i] == v ) %}" \
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
          "{% move cnt = idx %}" \
          "{% do assert_expr(my_list[idx-1]==i,'_'+i+':'+idx) %}" \
          "{% endfor %}");

  TEST(2 is odd,False);
  TEST(3 is odd,True);
  TEST(4 is even,True);
  TEST(NotDefine is not defined,True);
  /* tenary operations */
  TEST(3 if True != False else 4,4);
  TEST('HH'*3 if 'f'*3 == 'fff' else 'UUV','HH'*3);
  TEST(5%2 == 0 if 3%2 == 0 else True,True);
  /* in , not in and # operators */
  do_test("{% do assert_expr( #'HelloWorld' == 10 ) %}" \
          "{% do assert_expr( #[1,2,3] == 3 ) %}" \
          "{% do assert_expr( #{'U':'V','W':'X','Y':'Z'} == 3 ) %}" \
          "{% do assert_expr( #1 == 1 ) %}" \
          "{% do assert_expr( #None == 0 ) %}" \
          "{% do assert_expr( #False == 1 ) %}" \
          "{% do assert_expr( #True == 1 ) %}");
  do_test("{% do assert_expr( 'a' in { 'a' :123 } == True ) %}" \
          "{% do assert_expr( 'a' in { 'B' : None } == False ) %}" \
          "{% do assert_expr( 1 in [1,2,3] ) %}" \
          "{% do assert_expr( 3 in [1,2,3,4] ) %}" \
          "{% do assert_expr( 998 not in [1,234] ) %}" \
          "{% do assert_expr( None in [ None, None , None ] ) %}" \
          "{% do assert_expr( True in [True , True ] ) %}" \
          "{% do assert_expr( False in [True, False] ) %}");
}

void test_loop() {
  do_test("{% set cat1 = [] %}" \
          "{% set cat2 = [] %}" \
          "{% set cat3 = [] %}" \
          "{% for i in xrange(1000) %}" \
          "{% if i % 3 == 0 %}" \
          "{% do cat1.append(i) %}" \
          "{% elif i % 3 == 1 %}" \
          "{% do cat2.append(i) %}" \
          "{% else %}" \
          "{% do cat3.append(i) %}" \
          "{% endif %}" \
          "{% endfor %}" \
          "{% for i,v in cat1 %}" \
          "{% do assert_expr(cat1[i]==i*3) %}" \
          "{% do assert_expr(cat1[i]==v) %}" \
          "{% endfor %}" \
          "{% for i,v in cat2 %}" \
          "{% do assert_expr(cat2[i]==i*3+1) %}" \
          "{% do assert_expr(cat2[i]==v) %}" \
          "{% endfor %}"
          "{% for i,v in cat3 %}" \
          "{% do assert_expr(cat3[i]==i*3+2) %}" \
          "{% do assert_expr(cat3[i] == v) %}" \
          "{% endfor %}");

  do_test("{% set arr = [] %}" \
          "{% for i in xrange(10) if i % 3 == 0 %}" \
          "{% do arr.append(i) %}" \
          "{% endfor %}" \
          "{% for i,v in arr %}" \
          "{% do assert_expr(v == i*3) %}" \
          "{% do assert_expr(v == arr[i]) %}" \
          "{% endfor %}");

  do_test("{% set arr = [] %}" \
          "{% for i in xrange(10) %}" \
          "{% if i % 2 != 0 %}" \
          "{% continue %}" \
          "{% endif %}" \
          "{% do arr.append(i) %}" \
          "{% endfor %}" \
          "{% for i,v in arr %}" \
          "{% do assert_expr(v == i*2) %}" \
          "{% do assert_expr(v == arr[i]) %}" \
          "{% endfor %}");

  do_test("{% set obj = {} %}" \
          "{% for i in xrange(100) %}" \
          "{% do obj.set('HelloWorld'+i,i) %}" \
          "{% endfor %}" \
          "{% set temp = 0 %}"
          "{% for i,v in obj %}" \
          "{% set idx = temp + 1 %}"
          "{% do assert_expr( obj['HelloWorld'+(idx-1)] == (idx-1) ) %}" \
          "{% do assert_expr( obj['HelloWorld'+(idx-1)] == (idx-1) ) %}" \
          "{% move temp=idx %}" \
          "{% endfor %}");

  /* mixed control flow test */
  do_test("{% set arr = [] %}" \
          "{% for x in xrange(6) %}" \
          "{% if x < 4 and x % 3 != 0 or x == 0 %}" \
          "{% continue %}" \
          "{% endif %}" \
          "{% if x >= 5 %}" \
          "{% break %}" \
          "{% endif %}" \
          "{% do arr.append(x) %}" \
          "{% endfor %}" \
          "{% do assert_expr( #arr == 2 ) %}" \
          "{% do assert_expr( arr[0] == 3 ) %}"\
          "{% do assert_expr( arr[1] == 4 ) %}");

  /* iterate through string */
  do_test("{% set str = 'abcdef' %}" \
          "{% set char = [] %}" \
          "{% for c in str %}" \
          "{% do char.append(c) %}" \
          "{% endfor %}" \
          "{% do assert_expr(char[0] == 'a') %}" \
          "{% do assert_expr(char[1] == 'b') %}" \
          "{% do assert_expr(char[2] == 'c') %}" \
          "{% do assert_expr(char[3] == 'd') %}" \
          "{% do assert_expr(char[4] == 'e') %}" \
          "{% do assert_expr(char[5] == 'f') %}");

  /* sum of an array */
  do_test("{% set output = 0 %}" \
          "{% for x in [1,2,3,4,5,6,7,8,9,10] %}" \
          "{% set V = output + x %}" \
          "{% move output = V %}"\
          "{% endfor %}" \
          "{% do assert_expr( output == 1+2+3+4+5+6+7+8+9+10 ) %}");

  /* stack consistence check */
  do_test("{% set result = 0 %}" \
          "{% for i in xrange(100) %}" \
          "{% set sum = result + i %}" \
          "{% move result = sum %}" \
          "{% do assert_expr(sum==result) %}" \
          "{% endfor %}" \
          "{% do assert_expr(result==4950) %}");

  do_test("{% set result = 0 %}" \
          "{% for i in xrange(100) if i % 5 == 0 %}" \
          "{% set num = result + i %}" \
          "{% if i >= 20 %}" \
          "{% set new_num = num + 10 %}" \
          "{% do assert_expr(new_num == num+10) %}" \
          "{% endif %}" \
          "{% move result=num %}"
          "{% do assert_expr(result == num) %}" \
          "{% endfor %}");

  do_test("{% for i in xrange(100) if i % 5 == 0 %}" \
          "{# Set up a local variable #}" \
          "{% set val1 = i %}" \
          "% do assert_expr(val1 == i) %}" \
          "{# Set up a jump branch #}" \
          "{% if i >= 20 %} {% break %} {% endif %}" \
          "{# Set up another local variable which is right after the jump #}" \
          "{% set val2 = i*i %}" \
          "{% do assert_expr(val2 == i*i) %}" \
          "{% endfor %}");

  do_test("{% for i in xrange(100) %}" \
          "{% set val1 = i %}" \
          "{% do assert_expr(val1 == i) %}" \
          "{% if i % 5 != 0 %}" \
          "{% with another_val = 2 %}" \
          "{% continue %}" \
          "{% endwith %}" \
          "{% endif %}" \
          "{% set val2 = i*i %}" \
          "{% do assert_expr(i*i == val2) %}" \
          "{% if i >= 60 %} {% break %} {% endif %}" \
          "{% set val3 = i*i+i %}" \
          "{% do assert_expr(i*i+i == val3) %}"
          "{% do assert_expr(i*i == val2) %}"
          "{% endfor %}");

  /* Complicated nested loops */
  do_test("{% for i in xrange(10) %}" \
          "{% set loop1_v1 = i %}" \
          "{% for j in xrange(10) %}" \
          "{% set loop2_v1 = i*j %}" \
          "{% for k in xrange(10) %}" \
          "{% set loop3_v3 = k*i*j %}" \
          "{% if loop3_v3 % 5 == 0 %}" \
          "{% break %}" \
          "{% endif %}" \
          "{% endfor %}" \
          "{% if loop2_v1 %3 == 1 %}" \
          "{% with A = 1 %}" \
          "{% with B = 2 %}" \
          "{% continue %}" \
          "{% endwith %}" \
          "{% endwith %}" \
          "{% endif %}" \
          "{% set U = [] %}" \
          "{% set V = { 'U'*3 : U } %}" \
          "{% endfor %}" \
          "{% endfor %}");

  /* Matrix sum */
  do_test("{% set matrix = [ [1,2,3] , [4,5,6] , [7,8,9] ] %}" \
          "{% set result = 0 %}" \
          "{% for x in matrix %}" \
          "{% set sum = 0 %}" \
          "{% for y in x %}" \
          "{% set sum1 = sum + y %}" \
          "{% move sum = sum1 %}" \
          "{% endfor %}" \
          "{% set temp = sum + result %}" \
          "{% move result = temp %}" \
          "{% endfor %}" \
          "{# Now we get our matrix sum #}" \
          "{% do assert_expr(result==45) %}");
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
  test_simple();
  test_expr();
  test_loop();
  test_with();
  test_move();
  test_branch();
  test_macro();
  test_random_1();
}
