#include "parse.h"
#include "bc.h"
#include "object.h"
#include "ajj.h"

/* ALL these tests requires human interaction to look into the code
 * generated and verify they are correct or not. We may record the
 * correct code and assert on them in future for regression test */

static
void test0() {
  {
    const char* src = "UUVVXX{{ a.b }}XX";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"HelloWorld",src,0);
    const struct program* prg;

    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }
}

static
void test_for() {
  {
    /* FOR LOOP */
    const char* src = "UUXX{% for key,value in my_dictionary if key == None %}\n" \
                       "<dt>{{ keyvalue | filter1 }}</dt>\n" \
                       "<dd>{{ keyvalue | filter2 }}</dt>\n" \
                       "<dd>{{ key }} {{ value }}</dt>\n"\
                      "{% else %}\n" \
                      "{{ key | filter3 | filter4 }}{{ val }}\n"\
                      "{% endfor %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"HelloWorld",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }

  {
    /* FOR LOOP */
    const char* src = "UVWXYZ{% for a,_ in my_dict if a != True %}\n" \
                       "<dt>{{ KeyValue | filter1 }}</dt>\n" \
                       "<dt>{{ a | lower_case }}</dt>\n" \
                       "{% endfor %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"HelloWorld",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }

  {
    /* FOR LOOP */
    const char* src = "UVWXYZ{% for _,a in my_dict if a != True %}\n" \
                       "<dt>{{ KeyValue | filter1 }}</dt>\n" \
                       "<dt>{{ a | lower_case }}</dt>\n" \
                       "{% endfor %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"HelloWorld",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }

  {
    /* FOR LOOP */
    const char* src = "UVWXYZ{% for _,_ in my_dict if a != True %}\n" \
                       "<dt>{{ KeyValue | filter1 }}</dt>\n" \
                       "<dt>{{ a | lower_case }}</dt>\n" \
                       "{% endfor %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"HelloWorld",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }



  {
    /* FOR LOOP WITH CONTROLS */
    const char* src = "{% for a in my_dict %}\n" \
                      "{% if b == 3 %}{% continue %}{% endif %}\n" \
                      "{% endfor %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg =ajj_object_jinja_main(obj);
    assert(prg);

  }

  {
    /* FOR LOOP WITH CONTROLS */
    const char* src = "{% for a in my_dict %}\n" \
                      "{% if b == 3 %}{% break %}{% endif %}\n" \
                      "{% if b == 2 %}{% continue %}{% endif %}\n" \
                      "{% endfor %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg =ajj_object_jinja_main(obj);
    assert(prg);

  }

  {
    /* FOR LOOP WITH CONTROLS */
    const char* src = "{% for a,b in my_dict %}\n" \
                      "{% if b == 3 %}{% break %}{% endif %}\n" \
                      "{{ UU }}\n" \
                      "{% if b == 2 %}{% continue %}{% endif %}\n" \
                      "{{ VV }}\n" \
                      "{% endfor %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg =ajj_object_jinja_main(obj);
    assert(prg);

  }
  {
    /* FOR LOOP WITH CONTROLS */
    const char* src = "{% for _,_ in my_dict %}\n" \
                      "{% if b == 3 %}{% break %}{% endif %}\n" \
                      "{{ UU }}\n" \
                      "{% if b == 2 %}{% continue %}{% endif %}\n" \
                      "{{ VV }}\n" \
                      "{% endfor %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg =ajj_object_jinja_main(obj);
    assert(prg);

  }


  {
    /* FOR LOOP WITH CONTROLS */
    const char* src = "{% for _,_ in my_dict %}\n" \
                      "{% if b == 3 %}{% break %}{% endif %}\n" \
                      "{% if b == 2 %}{% continue %}{% endif %}\n" \
                      "{% if b == 4 %}{% break %}{% endif %}\n" \
                      "{% if b == 5 %}{% continue %}{% endif %}\n" \
                      "{% if b == 6 %}{% break %}{% endif %}\n" \
                      "{% if b == 7 %}{% continue %}{% endif %}\n" \
                      "{% endfor %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg =ajj_object_jinja_main(obj);
    assert(prg);

  }

}

static
void test_branch() {
  {
    /* BRANCH */
    const char* src = "UUVVXX\n" \
      "{% if users %}\n" \
      "<ul>HelloWorld</ul>\n" \
      "{% for use in users %}\n" \
      "{{use}}\n" \
      "{% endfor %}\n" \
      "</ul>\n" \
      "{% endif %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"HelloWorld",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }
  {
    /* BRANCH */
    const char* src = "UUVVXX\n" \
      "{% if users %}\n" \
      "</ul>\n" \
      "{% elif users == 3 % upv + 2.2 * 4 %}\n" \
      "{{UUU}}\n" \
      "{% endif %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"HelloWorld",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }
  {
    /* BRANCH */
    const char* src = "UUVVXX\n" \
      "{% if users %}\n" \
      "</ul>\n" \
      "{% else %}\n" \
      "{{UUU}}\n" \
      "{% endif %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"HelloWorld",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }
  {
    /* BRANCH */
    const char* src = \
      "UUVVXX\n" \
      "{% if users == 3+4*7%5//uv %}\n" \
      "{{ users }}\n" \
      "{% elif users % 3 == 0 %}\n" \
      "{{ users }} {{ users }}\n" \
      "{% endif %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }
}

static
void test_expr() {
 
  { /* simple expression 1 */
    const char* src = " {% do users+3+4+foo(1,2,3)|user_end + 78-2 %} ";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj); prg = ajj_object_jinja_main(obj);
    assert(prg);
  }



  { /* simple expression 2 */
    const char* src = " {% do users.append( some_string + \'Hello\' ) // another.one ** 3 %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj); prg = ajj_object_jinja_main(obj);
    assert(prg);
  }



  { /* simple expression 3 */
    const char* src = "{% do users.append(some_thing+\'UUV\') >= -users.name %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj); prg = ajj_object_jinja_main(obj);
    assert(prg);
    printf("%s\n",src);

  }

  { /* logic */
    const char* src = "{% do append >= only and user >= shoot or user == None or not True %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj); prg = ajj_object_jinja_main(obj);
    assert(prg);
    printf("%s\n",src);

  }



  { /* logic */
    const char* src = "{% if append >= only and user >= more or not(user < 100) or None != False %}\n" \
                      "{% endif %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    printf("%s\n",src);

  }

  {/* logic */
    const char* src = "{% if append > a %}\n" \
                      "{{ append }} {{ uuvv }}\n" \
                      "{{ hello  }}\n" \
                      "{% elif append == a %}\n" \
                      "{{ append }}\n" \
                      "{% else %}\n" \
                      "{{ yo+are+my+sunshine }}\n" \
                      "{% endif %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    printf("%s\n",src);

  }


  { /* tenary */
    const char* src = "{% do 1+3*2//5 if me is you and i in some_map else 3+5 %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    printf("%s\n",src);

  }

  { /* list */
    const char* src = "{% do [1,2,'three',True,False,None,1*MyName,[]] %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
  }

  { /* dict */
    const char* src = "{% do { 'You' : 'What' ," \
                      "        'Me'  : []," \
                      "        'Go'  : { 'Yet':1 , 'Another' : MyName+3333/2 }," \
                      "        HUHU  : {} } %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
  }

  { /* tuple */
    const char* src = "{% do ([],{},(),(())) %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
  }

  { /* tuple */
    const char* src = "{% do (1+1) + (1+1,) %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
  }
 

  { /* component */
    const char* src = "{% do a.p.u[\'v\'][myname+1+'string'].push(U) %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
  }
}

static
void test_macro() {

  { /* MACRO */
    const char* src = "{% macro input(name,value=' ',type='text',size=20) %}\n" \
                      "type=\"{{type}}\" name=\"{{name}}\" value=\"{{value|e}}\" \
                      size=\"{{size}}\"\n" \
                      "{% endmacro %}\n" \
                      "{{ input('UserName') }}\n" \
                      "{{ input('You','Val','TT',30) }}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    struct string input = CONST_STRING("input");
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    printf("%s\n",src);
    //
    prg = ajj_object_get_jinja_macro(obj,&input);
    assert(prg);

  }



  { /* MACRO */
    const char* src = "{% macro input() %}\n" \
                      "type={{U}} {{V}} {{W}} {{X}} {{Y}} {{Z}}\n" \
                      "{% endmacro %}\n" \
                      "{{ input() }}\n" \
                      "{{ input(1)}}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    struct string input = CONST_STRING("input");
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    //
    prg = ajj_object_get_jinja_macro(obj,&input);
    assert(prg);

  }

  { /* MACRO */
    const char* src = "{% macro input(my_dict) %}\n" \
                      "{% for a in my_dict if a == None %}\n" \
                      "{{ a }} UVW {{ a }}\n" \
                      "{% endfor %}\n" \
                      "{% endmacro %}\n" \
                      "{{ input(1)}}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    struct string input = CONST_STRING("input");
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    //
    prg = ajj_object_get_jinja_macro(obj,&input);
    assert(prg);

  }
}

static
void test_block() {

  {
    const char* src = "================\n" \
                      "{% block head %}\n" \
                      "<><><><><><><><><>\n" \
                      "{% endblock %}\n"\
                      "================\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    const struct string head = CONST_STRING("head");
    assert(obj);
    prg =ajj_object_jinja_main(obj);
    assert(prg);
    prg =ajj_object_get_jinja_block(obj,&head);

  }

  {
    const char* src = "==================\n" \
                      "{% block head %}\n" \
                      "{% for a in my_dict if a == my_upvalue * 2 %}\n" \
                      "{% do print(a) %}\n" \
                      "{% endfor %}\n" \
                      "{% endblock %}\n" \
                      "{{ UUVV }} Hello World\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    const struct string head = CONST_STRING("head");
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }
}

static
void test_do() {
  {
    const char* src = "<><>><><>{{}}{% do my_list.append(1) %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }
}

static
void test_filter() {

  {
    const char* src="A{% filter upper %} MyLargeBlock {% endfilter %}N\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }


  {
    const char* src="A{% filter upper(1,None,True,'Hello') %} MyLargeBlock {% endfilter %}N\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }
}

static
void test_set() {

  {
    const char* src = "A{% set hulala= append.append.append(1,2,3,'V') %}UU\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }

  {
    const char* src = "A{% set hulala %}\n" \
                      "UUVVWWXXYYZZ\n" \
                      "{% endset %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }
}

static
void test_with() {
  {
    const char* src = "A{% with %}\n" \
                      "{% set foo = 42 %}\n" \
                      "{{ foo }}\n" \
                      "{% endwith %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }
}

static
void test_call() {

  {
    const char* src  = "{% call(user) dump_users(list_of_user) %}\n" \
                       "<dl> {{ user }} </dl>\n" \
                       "{% endcall %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    struct string call_name = CONST_STRING("@c0");
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    prg = ajj_object_get_jinja_macro(obj,&call_name);

  }

  {
    const char* src  = "{% call dump_users() %}\n" \
                       "<dl> {{ user }} </dl>\n" \
                       "{% endcall %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    struct string call_name = CONST_STRING("@c0");
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    //prg = ajj_object_get_jinja_macro(obj,&call_name);

  }
}

static
void test_include() {

  { /* basic include */
    const char* src = "{% include 'file.html' %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg =ajj_object_jinja_main(obj);
    assert(prg);

  }
  { /* upvalue */
    const char* src = "{% include 'file.html' upvalue %}\n" \
                      "{% key1 my_key+2 %}\n" \
                      "{% key2 my_key+3 fix %}\n" \
                      "{% key3 my_key+4 override %}\n" \
                      "{% endinclude %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }
  { /* upvalue */
    const char* src = "{% include 'file.html' upvalue %}\n" \
                      "{% endinclude %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }
  { /* json */
    const char* src = "{% include 'file.html' json 'json.file'+json_path %}\n" \
                      "{% key1 my_key2 %} \n" \
                      "{% key2 my_key3 override %}\n" \
                      "{% key3 my_key4 fix %}\n" \
                      "{% endinclude %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Whoha",src,0);
    const struct program* prg;
    assert(obj);
    prg =ajj_object_jinja_main(obj);
    assert(prg);

  }

  { /* json */
    const char* src = "{% include 'file.html' json 'json.file'+json_path %}\n" \
                      "{% endinclude %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Whoha",src,0);
    const struct program* prg;
    assert(obj);
    prg =ajj_object_jinja_main(obj);
    assert(prg);

  }
}

static
void test_import() {

  { /* basic import */
    const char* src = "{% import 'file.name'+1 as uuvv %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Whoha",src,0);
    const struct program* prg;
    assert(obj);
    prg =ajj_object_jinja_main(obj);
    assert(prg);

  }
  { /* basic import */
    const char* src = "{% import 'file.name'+1 as uuvv upvalue %}{% endimport %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Whoha",src,0);
    const struct program* prg;
    assert(obj);
    prg =ajj_object_jinja_main(obj);
    assert(prg);

  }

  { /* basic import */
    const char* src = "{% import 'file.name'+1 as uuvv upvalue %}\n" \
                      "{% key1 my_key1 %}\n" \
                      "{% key2 my_key2 fix %}\n" \
                      "{% key3 my_key3 override %}\n" \
                      "{% endimport %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Whoha",src,0);
    const struct program* prg;
    assert(obj);
    prg =ajj_object_jinja_main(obj);
    assert(prg);

  }
}

static
void test_extends() {

  { /* basic extends */
    const char* src = "{% extends 'some-file.html' %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Whoha",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }

  {
    const char* src = "{% block you %} {{UUVV}} Haha {% endblock %}\n" \
                      "{% extends 'some-file.html' %}\n" \
                      "{% block me %} {{ UUBB }} asad {% endblock %}\n";

    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Whoha",src,0);
    const struct program* prg;
    struct string me = CONST_STRING("me");
    struct string you= CONST_STRING("you");
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
  }
}

static
void test_move() {
  { /* same scope move , should fail */
    const char* src = "{% set U = 1 %}\n" \
                      "{% set V = 2 %}\n" \
                      "{% move U V %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Whoha",src,0);
    const struct program* prg;
    struct string me = CONST_STRING("me");
    struct string you= CONST_STRING("you");
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
  }
  { /* different scope */
    const char* src = "{% set U = 1 %}\n" \
                      "{% with V = 2 %} \n" \
                      "{% move U V %}\n" \
                      "{% endwith %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Whoha",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
  }
  { /* different scope */
    const char* src = "{% set U = 1 %}\n" \
                      "{% with V = 2 %} \n" \
                      "{% move V U %}\n" \
                      "{% endwith %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Whoha",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
  }
}

static
void test_upvalue() {
  {
    const char* src = "{% upvalue my_key my_value.item(1)+2 %}" \
                      "{{ my_key }}" \
                      "{% endupvalue %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Whoha",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
  }
}

static
void test_constexpr() {
 
  {
    const char* src = "{% macro input1(arg1=[],arg2={},arg3='UV',arg4=44,arg5=True,arg6=False,arg7=None) %}" \
                      "{% endmacro %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Whoha",src,0);
    struct string input1 = CONST_STRING("input1");
    const struct program* prg;
    assert(obj);
    prg = ajj_object_get_jinja_macro(obj,&input1);
    assert(prg);
  }
 
  {
    const char* src = "{% macro input1(arg1=[1,-2,3],arg2={'U':'V','Q':123,'P':()},arg3='UV',arg4=44,arg5=True,arg6=False,arg7=None) %}" \
                      "{% endmacro %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Whoha",src,0);
    struct string input1 = CONST_STRING("input1");
    const struct program* prg;
    assert(obj);
    prg = ajj_object_get_jinja_macro(obj,&input1);
    assert(prg);
  }
}

int main() {
  test0();
  test_set();
  test_for();
  test_do();
  test_branch();
  test_expr();
  test_call();
  test_filter();
  test_macro();
  test_block();
  test_with();
  test_include();
  test_import();
  test_extends();
  test_upvalue();
  test_move();
  test_constexpr();
  return 0;
}
