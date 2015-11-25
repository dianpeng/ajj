test:
	gcc -g src/util.c src/lex.c test/lex-test.c -o lex-test
	gcc -g src/util.c test/util-test.c -o util-test

clean:
	rm lex-test util-test

.PHONY: test clean
