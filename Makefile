test:
	gcc -g src/util.c src/lex.c test/lex-test.c -o lex-test
	gcc -g src/util.c test/util-test.c -o util-test
	gcc -g src/ajj.c src/util.c src/lex.c src/bc.c src/parse.c src/object.c test/parser-test.c -o parser-test
	gcc -g src/bc.c src/util.c test/bc-test.c -o bc-test

clean:
	rm lex-test util-test parser-test bc-test

.PHONY: test clean
