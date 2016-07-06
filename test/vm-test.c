#include <vm.h>
#include <parse.h>
#include <ajj-priv.h>
#include <object.h>
#include <ajj-priv.h>
#include <bc.h>
#include <opt.h>
#include <stdlib.h>
#include <sys/time.h>
#include <inttypes.h>
#include <builtin.h>

#include <dirent.h>

#include "test-check.h"

static void do_test( const char* src ) {
  struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
  struct ajj_object* jinja;
  struct ajj_io* output = ajj_io_create_file(a,stdout);
  jinja = parse(a,"vm-test-jinja",src,0,0);
  if(!jinja) {
    fprintf(stderr,"%s",a->err);
    abort();
  }
  if(vm_run_jinja(a,jinja,output,NULL)) {
    fprintf(stderr,"%s",a->err);
    abort();
  }
  ajj_io_destroy(a,output);
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

static
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

  /* Loop object maniuplation */
  do_test("{% for i in xrange(10) %}" \
      "{% do assert_expr( loop.index == i + 1 ) %}" \
      "{% do assert_expr( loop.index0== i ) %}" \
      "{% do assert_expr( loop.revindex == 10-i ) %}" \
      "{% do assert_expr( loop.revindex0 == 9-i ) %}" \
      "{% do assert_expr( loop.length == 10 ) %}" \
      "{% if i == 9 %}" \
      "{% do assert_expr( loop.last ) %}" \
      "{% do assert_expr( loop.first == False ) %}" \
      "{% elif i == 0 %}" \
      "{% do assert_expr( loop.last == False ) %}" \
      "{% do assert_expr( loop.first ) %}" \
      "{% else %}" \
      "{% do assert_expr( not loop.last ) %}" \
      "{% do assert_expr( not loop.first) %}" \
      "{% endif %}" \
      "{% endfor %}");
}

static
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
static
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
static
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
static
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
  /* combine the call with other complicated structure */
  /* filter odd number */
  do_test("{% macro filter_odd(arr) %}" \
      "{% set result = [] %}" \
      "{% for i in arr if i % 2 == 0 %}" \
      "{% do result.append(i) %}" \
      "{% endfor %}" \
      "{% return result %}" \
      "{% endmacro %}" \
      "{% do assert_expr( ([1,2,3,4] | filter_odd) == [2,4] ) %}");
  /* fib with iteration */
  do_test("{% macro fib(num) %}" \
      "{% if num == 0 %}" \
      "{% return 0 %}" \
      "{% elif num == 1 %}" \
      "{% return 1 %}" \
      "{% else %}" \
      "{% set a = 0 %}" \
      "{% set b = 1 %}" \
      "{% for i in xrange(num-1) %}" \
      "{% set sum = a + b %}" \
      "{% set l = b %}" \
      "{% move a= l %}" \
      "{% move b= sum %}" \
      "{% endfor %}" \
      "{% return b %}"\
      "{% endif %}" \
      "{% endmacro %}"\
      "{% do assert_expr(fib(20) == 3*5*11*41) %}");

  /* binary search */
  do_test("{% macro bsearch(arr,low,high,val) %}" \
      "{% if low > high %}" \
      "{% return -1 %}" \
      "{% else %}" \
      "{% if low == high %}" \
      "{% if val == arr[low] %}" \
      "{% return low %}"
      "{% else %}" \
      "{% return -1 %}" \
      "{% endif %}" \
      "{% endif %}" \
      "{% endif %}" \
      "{% set mid = (low+high)/2 | floor %}" \
      "{% if arr[mid] == val %}" \
      "{% return mid %}" \
      "{% elif arr[mid] > val %}" \
      "{% if mid-1 < low %}" \
      "{% return -1 %}" \
      "{% else %}" \
      "{% return bsearch(arr,low,mid-1,val) %}" \
      "{% endif %}" \
      "{% else %}" \
      "{% if mid + 1 > high %} "\
      "{% return -1 %}" \
      "{% else %}" \
      "{% return bsearch(arr,mid+1,high,val) %}" \
      "{% endif %}" \
      "{% endif %}" \
      "{% endmacro %}" \
      "{% set arr = [1,2,3,4,5,6,7,8,9] %}" \
      "{% set capture = bsearch(arr,0,#arr-1,7) %}" \
      "{% do assert_expr(arr[capture] == 7) %}");

  /* slice */
  do_test("{% macro slice(arr,low,up) %}" \
      "{% set result = [] %}" \
      "{% if low >= up %}" \
      "{% return result %}" \
      "{% endif %}" \
      "{% for i in xrange(up-low) %}" \
      "{% do result.append(arr[low+i]) %}" \
      "{% endfor %}" \
      "{% return result %}" \
      "{% endmacro %}" \
      "{% do assert_expr( ( [ 1,2,3,4,5 ] | slice(2,3) ) == [3] ) %}");
}

static
void test_call() {
  do_test("{% macro child(arg) %}" \
      "{% do assert_expr(arg == 10) %}" \
      "{% do assert_expr(caller() == 'HelloWorld') %}" \
      "{% endmacro %}" \
      "{% call child(10) %}" \
      "{% return 'HelloWorld' %}" \
      "{% endcall %}");

  do_test("{% macro child(arg1,arg2=True,arg3=False,arg4=[1,2,3,4]) %}" \
      "{% do assert_expr(arg1 == 10) %}" \
      "{% do assert_expr(arg2 == True ) %}" \
      "{% do assert_expr(arg3 == False) %}" \
      "{% do assert_expr(arg4 == [1,2,3,4] ) %}" \
      "{% do assert_expr(caller('HelloWorld') == 'Correct') %}" \
      "{% endmacro %}" \
      "{% call(arg) child(10) %}" \
      "{% do assert_expr(arg == 'HelloWorld') %}" \
      "{% return 'Correct' %}" \
      "{% endcall %}");

  do_test("{% macro child(arg) %}" \
      "{% if __caller_stub__ is not None %}" \
      "{% do assert_expr( caller() == 'HelloWorld' ) %}" \
      "{% endif %}" \
      "{% endmacro %}" \
      "{% call child(10) %}" \
      "{% return 'HelloWorld' %}" \
      "{% endcall %}" \
      "{% do child(10) %}");
}

static void do_import( const char* source , const char* lib ) {
  struct ajj* a ;
  struct ajj_object* jinja;
  struct ajj_object* m;
  struct ajj_io* output;
  a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
  jinja = parse(a,"External",lib,0,0);
  if(!jinja) {
    fprintf(stderr,"%s",a->err);
    abort();
  }
  m = parse(a,"Main",source,0,0);
  if(!m) {
    fprintf(stderr,"%s",a->err);
    abort();
  }
  output = ajj_io_create_file(a,stdout);
  if(vm_run_jinja(a,m,output,NULL)) {
    fprintf(stderr,"%s",a->err);
    abort();
  }
  ajj_io_destroy(a,output);
  ajj_destroy(a);
}

static
void test_import() {
  do_import("{% import 'External' as Lib %}" \
      "{% do assert_expr( Lib.HelloWorld() == 'HelloWorld' ) %}",
      "{% macro HelloWorld() %}" \
      "{% return 'HelloWorld' %}" \
      "{% endmacro %}");
}

static
struct ajj_object*
load_template( struct ajj* a , const char* key , const char* src ) {
  struct ajj_object* jinja;
  jinja = parse(a,key,src,0,0);
  if(!jinja) {
    fprintf(stderr,"%s",a->err);
    abort();
  }
  return jinja;
}

static
void test_extends() {
  {
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    load_template(a,"Base",
        "{% block head %}" \
        "This line should never show up!\n" \
        "{% endblock %}");

    jinja = load_template(a,"Child",
        "{% extends 'Base' %}" \
        "{% block head %}" \
        "This line should show up!\n"
        "{% endblock %}");
    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }

  { /* Multi-level inheritance */
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    load_template(a,"Base",
        "{% block head %}" \
        "This line should never show up!\n" \
        "{% endblock %}");

    load_template(a,"Child",
        "{% block tail %}" \
        "This is a block will never showed up when this "\
        "template is inherited or it will be showed when "\
        "this template is used as a leaf node !" \
        "{% endblock %}" \
        "{% extends 'Base' %}" \
        "{% block head %}" \
        "This line should show up!\n"
        "{% endblock %}");

    jinja = load_template(a,"Leaf",
        "{% extends 'Child' %}" \
        "{% block tail %}" \
        "This is a tail overload ~\n"\
        "{% endblock %}" \
        "{% block head %}" \
        "This is from leaf~\n"\
        "{% endblock %}");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }

  { /* Multiple inheritance */
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Base1",
        "{% block b1_head %}" \
        "Hello from Base1::b1_head!" \
        "{% endblock %}");

    load_template(a,"Base2",
        "{% block b2_head %}" \
        "Hello from Base2::b2_head!" \
        "{% endblock %}");

    jinja = load_template(a,"Child",
        "{% extends 'Base1' %}" \
        "{% extends 'Base2' %}" \
        "{% block b1_head %}" \
        "Child for b1_head\n" \
        "{% endblock %}" \
        "{% block b2_head %}" \
        "Child for b2_head\n" \
        "{% endblock %}");
    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }

  { /* Calling blocks as normal function */
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Base",
        "{% block b1 %}" \
        "Won't show up through extends\n" \
        "{% endblock %}" \
        "{% block b2 %}" \
        "{% do self.b1() %}" \
        "{% endblock %}");

    jinja = load_template(a,"Child",
        "{% extends 'Base' %}" \
        "{% block b1 %}" \
        "Show me \n" \
        "{% endblock %}");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }

  { /* Super */
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Base",
        "{% block b1 %}" \
        "Won't show up through extends\n" \
        "{% endblock %}" \
        "{% block b2 %}" \
        "{% do self.b1() %}" \
        "{% endblock %}");

    jinja = load_template(a,"Child",
        "{% extends 'Base' %}" \
        "{% block b1 %}" \
        "{% do super() %}" \
        "Show me \n" \
        "{% endblock %}");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }
}

static
void test_include() {
  {
    /* Simple version */
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Inc1","{{ 'Hello World' }}\n");
    jinja = load_template(a,"Main","{% include 'Inc1' %}From Main\n");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }
  {
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Inc1","{{ 'Hello World' }}\n" \
        "{% macro MyTestMacro1() %}" \
        "{{ 'Hello World\n' }}" \
        "{% return 'Hello World2\n' %}" \
        "{% endmacro %}" \
        "{{ MyTestMacro1() }}");

    jinja = load_template(a,"Main","{% include 'Inc1' %}");
    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }
}

static
void test_json() {
  DIR *d;
  struct dirent* dir;
  struct ajj* a;
  a = ajj_create(&AJJ_DEFAULT_VFS,NULL);

  d = opendir("json-test/");
  if(d) {
    while((dir = readdir(d)) != NULL) {
      char filename[1024];
      struct ajj_object* json;
      int valid;

      if(dir->d_type == DT_REG) {
        if(strcmp(dir->d_name,"LICENSE") ==0)
          continue;
        sprintf(filename,"json-test/%s",dir->d_name);

        json = json_parse(a,&(a->gc_root),filename,"test");
        if(strstr(filename,"invalid") || strstr(filename,"fail"))
          valid = 0;
        else
          valid = 1;

        if(valid) {
          if(!json) {
            fprintf(stderr,"%s:%s\n",dir->d_name,a->err);
          }
        } else {
          if(json) {
            fprintf(stderr,"This is a invalid example but gets parsed:%s!\n",
                dir->d_name);
          }
        }
      }
    }
    closedir(d);
  }
}

/* Test include with json as its MODEL */
static
void test_include_with_json() {
  {
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Inc1","{{ Hello_World }}");

    jinja = load_template(a,
        "Main",
        "{% include 'Inc1' json 'hello_world.json' %}" \
        "{% endinclude %}");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }

  {
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Inc1",
        "{% macro sum(a) %}"  \
          "{% set s = 0 %}" \
          "{% for x in a %}" \
            "{% set v = x + s %}" \
            "{% move s = v %}" \
          "{% endfor %}" \
          "{% return s %}" \
        "{% endmacro %}" \
        "{{ sum(Arg1) }}");

    jinja = load_template(a,
        "Main",
        "{% include 'Inc1' json 'hello_world.json' %}" \
        "{% endinclude %}");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }

  {
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Inc1","{% do assert_expr( Arg3 + Arg4 == 10 ) %}");

    jinja = load_template(a,
        "Main",
        "{% include 'Inc1' json 'hello_world.json' %}" \
        "{% endinclude %}");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }

  {
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Inc1",
        "\n\n{% for X,V in Arg2 %}" \
          "Key:{{X}};Value:{{V}}\n\n" \
        "{% endfor %}");

    jinja = load_template(a,
        "Main",
        "{% include 'Inc1' json 'hello_world.json' %}" \
        "{% endinclude %}");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }
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

#ifndef DO_COVERAGE
int main() {
#else
  int vm_test_main() {
#endif

    test_expr();
    test_loop();
    test_move();
    test_with();
    test_branch();
    test_macro();
    test_call();
    test_import();
    test_extends();
    test_json();
    test_include_with_json();
    return 0;
  }
