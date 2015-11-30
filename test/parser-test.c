#include "../src/parse.h"
#include "../src/bc.h"
#include "../src/object.h"
#include "../src/ajj.h"

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
    dump_program(src,prg,stdout);
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
    dump_program(src,prg,stdout);
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
    dump_program(src,prg,stdout);
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
    dump_program(src,prg,stdout);
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
    dump_program(src,prg,stdout);
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
    dump_program(src,prg,stdout);
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
    dump_program(src,prg,stdout);
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
    dump_program(src,prg,stdout);
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
    dump_program(src,prg,stdout);
  }
}

static
void test_expr() {
#if 0
  { /* simple expression 1 */
    const char* src = " {% do users+3+4+foo(1,2,3)|user_end + 78-2 %} ";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj); prg = ajj_object_jinja_main(obj);
    assert(prg); dump_program(src,prg,stdout);
  }
#endif

#if 0
  { /* simple expression 2 */
    const char* src = " {% do users.append( some_string + \'Hello\' ) // another.one ** 3 %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj); prg = ajj_object_jinja_main(obj);
    assert(prg); dump_program(src,prg,stdout);
  }
#endif

#if 0
  { /* simple expression 3 */
    const char* src = "{% do users.append(some_thing+\'UUV\') >= users.name %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj); prg = ajj_object_jinja_main(obj);
    assert(prg); 
    printf("%s\n",src);
    dump_program(src,prg,stdout);
  }
#endif

#if 0
  { /* logic */
    const char* src = "{% do append >= only and user >= shoot or user == None or not True %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj); prg = ajj_object_jinja_main(obj);
    assert(prg);
    printf("%s\n",src);
    dump_program(src,prg,stdout);
  }
#endif

#if 0
  { /* logic */
    const char* src = "{% if append >= only and user >= more or user < 100 or None != False %}\n" \
                      "{% endif %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    printf("%s\n",src);
    dump_program(src,prg,stdout);
  }
#endif

#if 0

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
    dump_program(src,prg,stdout);
  }

#endif
  { /* tenary */
    const char* src = "{% do 1+3*2//5 if None == True else 3+5 %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,"Hello World",src,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    printf("%s\n",src);
    dump_program(src,prg,stdout);
  }
}

static
void test_macro() {
#if 0
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
    //dump_program(src,prg,stdout);
    prg = ajj_object_get_jinja_macro(obj,&input);
    assert(prg);
    dump_program(src,prg,stdout);
  }
#endif

#if 0
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
    //dump_program(src,prg,stdout);
    prg = ajj_object_get_jinja_macro(obj,&input);
    assert(prg);
    dump_program(src,prg,stdout);
  }
#endif
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
    //dump_program(src,prg,stdout);
    prg = ajj_object_get_jinja_macro(obj,&input);
    assert(prg);
    dump_program(src,prg,stdout);
  }
}

static
void test_block() {
#if 0
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
    dump_program(src,prg,stdout);
  }
#endif
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
    dump_program(src,prg,stdout);
  }
}

int main() {
  test_block();
  return 0;
}
