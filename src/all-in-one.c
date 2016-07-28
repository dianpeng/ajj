#include "ajj.c"
#include "object.c"
#include "lex.c"
#include "parse.c"
#include "vm.c"
#include "gc.c"
#include "upvalue.c"
#include "opt.c"
#include "bc.c"
#include "utf.c"
#include "util.c"
#include "builtin.c"

#if defined __APPLE__ || defined __linux__
#include "unix-vfs.c"
#else
#error "Doesn't support this platform ???"
#endif /* __linux__  || __OSX__ */
