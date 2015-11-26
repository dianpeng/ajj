test:
	gcc -g src/util.c src/lex.c test/lex-test.c -o lex-test
	gcc -g src/util.c test/util-test.c -o util-test
	gcc -g src/ajj.c src/util.c src/lex.c src/bc.c src/parse.c src/object.c test/parser-test.c -o parser-test

clean:
	rm lex-test util-test parser-test

.PHONY: test clean
