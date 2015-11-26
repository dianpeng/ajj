#include "../src/parse.h"
#include "../src/bc.h"
#include "../src/object.h"
#include "../src/ajj.h"

static
void test1() {
  const char* src = "UUVVXX{{ a.b }}XX";
  struct ajj* a = ajj_create();
  struct ajj_object* obj = parse(a,src,"HelloWorld");
  const struct program* prg;

  assert(obj);
  prg = ajj_object_jinja_main(obj);
  assert(prg);
  dump_program(prg,stdout);
}

int main() {
  test1();
  return 0;
}
