GCC_PROFILE_FLAGS := -fprofile-arcs -ftest-coverage
LINK := -lm

opt-test2:
	cd src; \
	gcc $(GCC_PROFILE_FLAGS) -g ajj.c opt.c util.c bc.c lex.c parse.c object.c  opt-test2.c $(LINK) -o opt-test2; \
	./opt-test2; \
	gcov ajj.c opt.c util.c bc.c lex.c parse.c object.c opt-test.c

opt-test:
	cd src; \
	gcc $(GCC_PROFILE_FLAGS) -g ajj.c opt.c util.c bc.c lex.c parse.c object.c  opt-test.c $(LINK) -o opt-test; \
	./opt-test; \
	gcov ajj.c opt.c util.c bc.c lex.c parse.c object.c opt-test.c

parser-test:
	cd src; \
	gcc $(GCC_PROFILE_FLAGS) -g ajj.c util.c bc.c lex.c parse.c object.c parser-test.c -o parser-test; \
	./parser-test; \
	gcov ajj.c util.c bc.c lex.c parse.c object.c parser-test.c


clean:
	rm src/opt-test2
	rm src/*.gcno
	rm src/*.gcda
	rm src/*.gcov

.PHONY: opt-test clean
