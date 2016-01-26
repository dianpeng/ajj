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
  struct ajj_io* output = ajj_io_create_file(a,stdout);
  const struct string NAME = { "Input" , 5 };
  jinja = parse(a,"vm-test-jinja",src,0);
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
          "{% do assert_expr(val1 == i) %}" \
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
  do_test("{% if True is True %}" \
          "{% do assert_expr(1+3*4==13) %}" \
          "{% else %}" \
          "{% do assert_expr(False) %}" \
          "{% endif %}");
  do_test("{% set variable = 3 %}" \
          "{% if variable > 4 %}" \
          "{% do assert_expr(False) %}" \
          "{% elif variable == 3 %}" \
          "{% do assert_expr(True) %}" \
          "{% else %}" \
          "{% do assert_expr(False)%}" \
          "{% endif %}" );
  do_test("{% set variable = 3 %}" \
          "{% if variable >= 3 %}" \
          "  {% if variable % 3 == 0 %}" \
          "     {% do assert_expr(True) %}" \
          "  {% else %}" \
          "  {% do assert_expr(False) %}" \
          "  {% endif %}" \
          "{% elif variable <= 1 %}" \
          "{% do assert_expr(False) %}" \
          "{% else %}" \
          "{% do assert_expr(False) %}" \
          "{% endif %}");
}

/* MOVE operations */
void test_move() {
  /* Move to outer scope with primitive , should nothing changed */
  do_test("{% set Outer = 0 %}" \
          "{% with %}" \
          "{% set Inner = 100 %}" \
          "{% do assert_expr(Outer==0 )  %}" \
          "{% move Outer=Inner %}" \
          "{% do assert_expr(Inner==100) %}" \
          "{% endwith %}" \
          "{% do assert_expr(Outer==100) %}");
  do_test("{% set Outer = True %}" \
          "{% with %}" \
          "{% set Inner = False %}" \
          "{% do assert_expr( Outer == True  ) %}" \
          "{% move Outer = Inner %}" \
          "{% do assert_expr( Inner == False ) %}" \
          "{% endwith %}" \
          "{% do assert_expr( Outer == False ) %}");
  do_test("{% set Outer = False %}" \
          "{% with %}" \
          "{% set Inner = True %}" \
          "{% do assert_expr( Outer == False) %}" \
          "{% move Outer=Inner %}" \
          "{% do assert_expr( Inner == True ) %}" \
          "{% endwith %}" \
          "{% do assert_expr( Outer == True ) %}");
  /* Move object out of the scope.
   * The correctness of this move semantic is also related
   * to the correctness of implementation for this object */
  do_test("{% set Outer = [1,2,3,4] %}" \
          "{% with %}" \
          "{% with %}" \
          "{% with %}" \
          "{% with Inner = [100,101,102] %}" \
          "{% do assert_expr( Outer[0] == 1 ) %}" \
          "{% do assert_expr( Outer[1] == 2 ) %}" \
          "{% do assert_expr( Outer[2] == 3 ) %}" \
          "{% do assert_expr( Outer[3] == 4 ) %}" \
          "{% move Outer=Inner %}" \
          "{% do assert_expr( Inner[0] == 100 ) %}" \
          "{% do assert_expr( Inner[1] == 101 ) %}" \
          "{% do assert_expr( Inner[2] == 102 ) %}" \
          "{% endwith %}" \
          "{% endwith %}" \
          "{% endwith %}" \
          "{% endwith %}" \
          "{% do assert_expr( Outer[0] == 100 ) %}" \
          "{% do assert_expr( Outer[1] == 101 ) %}" \
          "{% do assert_expr( Outer[2] == 102 ) %}");
  do_test("{% set Outer = { 'U'*3 : [ 1 , 2 ] } %}" \
          "{% with %}" \
          "{% with Inner = { 'V'*3 : [ 3, 4 ] } %}" \
          "{% do assert_expr( Outer.UUU[0] == 1 ) %}" \
          "{% do assert_expr( Outer['UUU'][1] == 2 ) %}" \
          "{% move Outer=Inner %}" \
          "{% do assert_expr( Inner.VVV[0] == 3 ) %}" \
          "{% do assert_expr( Inner['VVV'][1] == 4 ) %}" \
          "{% endwith %}" \
          "{% endwith %}" \
          "{% do assert_expr( Outer.VVV[0] == 3 ) %}" \
          "{% do assert_expr( Outer.VVV[1] == 4 ) %}");
}

/* WITH */
void test_with() {
  do_test("{% do assert_expr( local_variable == None ) %}" \
          "{% with local_variable = True %}" \
          "{% do assert_expr( local_variable ) %}" \
          "{% endwith %}" \
          "{% do assert_expr( local_variable is None ) %}");
}

/* ============================
 * Function/Macro
 * ==========================*/

void test_macro() {
  /* arguments passing is correct or not */
  do_test("{% macro func(arg1,arg2,arg3,arg4,arg5,arg6,arg7) %}" \
          "{% do assert_expr( arg1 == True ) %}" \
          "{% do assert_expr( arg2 == False) %}" \
          "{% do assert_expr( arg3 == 10 ) %}" \
          "{% do assert_expr( arg4 == None ) %}" \
          "{% do assert_expr( arg5 == [1,2,3,4] ) %}" \
          "{% do assert_expr( arg6 == {'uu':3 } ) %}" \
          "{% do assert_expr( arg7 == 'HelloWorld') %}" \
          "{# Test builtin value of macro/functions. #}" \
          "{% do assert_expr( __argnum__ == 7 ) %}" \
          "{% do assert_expr( __func__ == 'func' ) %}" \
          "{% do assert_expr( caller == '__main__' ) %}" \
          "{% do assert_expr( vargs == None ) %}" \
          "{% do assert_expr( self != None ) %}" \
          "{% endmacro %}" \
          "{% do func(True,False,10,None,[1,2,3,4],{'uu':3},'HelloWorld') %}");
  /* default arguments */
  do_test("{% macro func(arg1,arg2=True,arg3=False,arg4='HelloWorld',arg5=[1,2,3,4],arg6={'UU':'Hello'}) %}" \
          "{% do assert_expr( arg1 == 10 ) %}" \
          "{% do assert_expr( arg2 == True ) %}" \
          "{% do assert_expr( arg3 == False ) %}" \
          "{% do assert_expr( arg4 == 'HelloWorld') %}" \
          "{% do assert_expr( arg5 == [1,2,3,4] ) %}" \
          "{% do assert_expr( arg6 == { 'UU' : 'Hello' } ) %}" \
          "{% do assert_expr( __argnum__ == 1 ) %}" \
          "{% do assert_expr( __func__ == 'func' )%}" \
          "{% do assert_expr( caller == '__main__' ) %}" \
          "{% do assert_expr( vargs == None ) %}" \
          "{% do assert_expr( self != None ) %}" \
          "{% endmacro %}" \
          "{% do func(10) %}");
  /* varags */
  do_test("{% macro func(arg1) %}" \
          "{% do assert_expr(arg1 == 10) %}" \
          "{% do assert_expr(vargs != None) %}" \
          "{% do assert_expr(vargs[0] == True) %}" \
          "{% do assert_expr(vargs[1] == False) %}" \
          "{% do assert_expr(vargs[2] == 10) %}" \
          "{% do assert_expr(vargs[3] == 'HelloWorld') %}" \
          "{% do assert_expr(vargs[4] == [1,2,3,4] ) %}" \
          "{% do assert_expr(vargs[5] == {'UU':'Hello' } ) %}" \
          "{% do assert_expr(#vargs == 6) %}" \
          "{% do assert_expr(__argnum__ == 1+6) %}" \
          "{% endmacro %}" \
          "{% do func(10,True,False,10,'HelloWorld',[1,2,3,4],{'UU':'Hello'}) %}");
  /* return */
  do_test("{% macro Sum(arg) %}" \
          "{% set result = 0 %}" \
          "{% for i in arg %}" \
          "{% set sum = i + result %}" \
          "{% move result = sum %}" \
          "{% endfor %}" \
          "{% return result %}" \
          "{% endmacro %}" \
          "{% do assert_expr(Sum([1,2,3,4,5]) == 1+2+3+4+5) %}" );
  /* return in different branch */
  do_test("{% macro ComplicatedReturn(arg) %}" \
          "{% set result1 = [] %}" \
          "{% set result2 = [] %}" \
          "{% set result3 = [] %}" \
          "{% if #arg >= 6  %}" \
          "{% for i in arg %}" \
          "{% if i % 3 == 0 %}" \
          "{% do result1.append(i) %}" \
          "{% endif %}" \
          "{% endfor %}" \
          "{% return result1 %}" \
          "{% elif #arg <= 3 %}" \
          "{% set local_var = 0 %}" \
          "{% for i in arg %}" \
          "{% set sum = local_var + i %}" \
          "{% move local_var = sum %}" \
          "{% do result2.append(sum) %}" \
          "{% endfor %}" \
          "{% return result2 %}" \
          "{% else %}" \
          "{% return arg %}"\
          "{% endif %}" \
          "{% endmacro %}" \
          "{% set LocalVar1 = 1 %}" \
          "{% set LocalVar2 = True %}" \
          "{% set LocalVar3 = [1,2,3,4] %}" \
          "{% set LocalVar4 = {'uu':'vv','hh':'pp'} %}" \
          "{% do assert_expr(ComplicatedReturn([1,2,3,4]) == [1,2,3,4]) %}" \
          "{% do assert_expr(ComplicatedReturn([1,2,3]) == [1,3,6]) %}" \
          "{% do assert_expr(ComplicatedReturn([1,2,3]) == [1,3,6]) %}" \
          "{% do assert_expr(ComplicatedReturn([1,2,3,4,5,6]) == [3,6]) %}" \
          "{% do assert_expr(LocalVar1 == 1 ) %}" \
          "{% do assert_expr(LocalVar2 == True) %}" \
          "{% do assert_expr(LocalVar3 == [1,2,3,4] ) %}" \
          "{% do assert_expr(LocalVar4 == {'uu':'vv','hh':'pp'} ) %}"
          );
  /* recursive call */
  do_test("{% macro fib(num) %}" \
          "{% if num == 0 %}" \
          "{% return 0 %}" \
          "{% elif num == 1 %}" \
          "{% return 1 %}" \
          "{% else %}" \
          "{% return fib(num-1) + fib(num-2) %}" \
          "{% endif %}" \
          "{% endmacro %}" \
          "{% do assert_expr(fib(20) == 3*5*11*41)  %}");
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
  test_expr();
  test_loop();
  test_move();
  test_with();
  test_branch();
  test_macro();
  test_random_1();
}
