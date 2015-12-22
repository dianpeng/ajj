GCC_PROFILE_FLAGS := -fprofile-arcs -ftest-coverage -o2 -g
LINK := -lm


vm-test:
	cd src; \
	gcc $(GCC_PROFILE_FLAGS) ajj.c opt.c util.c gc.c bc.c lex.c parse.c object.c upvalue.c vm.c builtin.c vm-test.c $(LINK) -o vm-test

opt-test2:
	cd src; \
	gcc $(GCC_PROFILE_FLAGS) ajj.c opt.c util.c bc.c lex.c parse.c object.c upvalue.c opt-test2.c $(LINK) -o opt-test2; \
	./opt-test2; \
	gcov ajj.c opt.c util.c bc.c lex.c parse.c object.c opt-test.c

parser-test:
	cd src; \
	gcc $(GCC_PROFILE_FLAGS) ajj.c builtin.c util.c bc.c lex.c parse.c object.c vm.c upvalue.c gc.c parser-test.c $(LINK) -o parser-test; \
	./parser-test; \
	gcov ajj.c util.c bc.c lex.c parse.c object.c parser-test.c

opt-test:
	cd src; \
	gcc $(GCC_PROFILE_FLAGS) ajj.c opt.c util.c bc.c lex.c parse.c object.c  opt-test.c $(LINK) -o opt-test; \
	./opt-test; \
	gcov ajj.c opt.c util.c bc.c lex.c parse.c object.c opt-test.c

clean:
	rm src/*.gcno
	rm src/*.gcda
	rm src/*.gcov
	rm src/opt-test
	rm src/opt-test2

.PHONY: opt-test clean
