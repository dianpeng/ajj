extern int bc_test_main();
extern int util_test_main();
extern int lex_test_main();
extern int jinja_test_main();
extern int opt_test_main();
extern int parser_test_main();
extern int vm_test_main();

int main() {
  bc_test_main();
  util_test_main();
  lex_test_main();
  opt_test_main();
  parser_test_main();
  vm_test_main();
  jinja_test_main();
  return 0;
}
