GCC_PROFILE_FLAGS := -fprofile-arcs -ftest-coverage
parser-test:
	cd src; \
	gcc $(GCC_PROFILE_FLAGS) -g ajj.c util.c bc.c lex.c parse.c object.c parser-test.c -o parser-test; \
	./parser-test; \
	gcov ajj.c util.c bc.c lex.c parse.c object.c parser-test.c

clean:
	rm src/parser-test
	rm src/*.gcno
	rm src/*.gcda
	rm src/*.gcov

.PHONY: test clean
