#include "../src/parse.h"
#include "../src/bc.h"
#include "../src/object.h"
#include "../src/ajj.h"

static
void test0() {
  {
    const char* src = "UUVVXX{{ a.b }}XX";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,src,"HelloWorld");
    const struct program* prg;

    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    dump_program(prg,stdout);
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
    struct ajj_object* obj = parse(a,src,"HelloWorld");
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    dump_program(prg,stdout);
  }

  { 
    /* FOR LOOP */
    const char* src = "UVWXYZ{% for a,_ in my_dict if a != True %}\n" \
                       "<dt>{{ KeyValue | filter1 }}</dt>\n" \
                       "<dt>{{ a | lower_case }}</dt>\n" \
                       "{% endfor %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,src,"HelloWorld");
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    dump_program(prg,stdout);
  }

  {
    /* FOR LOOP */
    const char* src = "UVWXYZ{% for _,a in my_dict if a != True %}\n" \
                       "<dt>{{ KeyValue | filter1 }}</dt>\n" \
                       "<dt>{{ a | lower_case }}</dt>\n" \
                       "{% endfor %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,src,"HelloWorld");
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    dump_program(prg,stdout);
  }

  {
    /* FOR LOOP */
    const char* src = "UVWXYZ{% for _,_ in my_dict if a != True %}\n" \
                       "<dt>{{ KeyValue | filter1 }}</dt>\n" \
                       "<dt>{{ a | lower_case }}</dt>\n" \
                       "{% endfor %}";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,src,"HelloWorld");
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    dump_program(prg,stdout);
  }
}

static
void test_branch() {
#if 0
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
    struct ajj_object* obj = parse(a,src,"HelloWorld");
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    dump_program(prg,stdout);
  }
#endif

  {
    /* BRANCH */
    const char* src = "UUVVXX\n" \
      "{% if users %}\n" \
      "</ul>\n" \
      "{% elif users == 3 % upv + 2.2 * 4 %}\n" \
      "{{UUU}}\n" \
      "{% endif %}\n";
    struct ajj* a = ajj_create();
    struct ajj_object* obj = parse(a,src,"HelloWorld");
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    dump_program(prg,stdout);
  }
}



int main() {
  test_branch();
  return 0;
}
