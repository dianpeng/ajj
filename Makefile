CC?=gcc
OPT := -Os
FLAGS := $(OPT) -Wpedantic -Wall -I$(PWD)/src -DDISABLE_OPTIMIZATION
PROFILE_FLAGS := -fprofile-arcs -ftest-coverage
LINK := -L$(PWD)/. -lajj -lm
SRC := $(wildcard src/*.c)

all: libajj

ajjobj: $(SRC)
	$(CC) $(FLAGS) -c src/all-in-one.c $(LINK)

libajj: ajjobj
	ar rcs libajj.a all-in-one.o

test: lex-test

lex-test: libajj test/lex-test.c
	cd test; gcc $(FLAGS) lex-test.c $(LINK) -o lex-test; cd -

parser-test: libajj test/parser-test.c
	cd test; gcc $(FLAGS) parser-test.c $(LINK) -o parser-test; cd -

vm-test: libajj test/vm-test.c
	cd test; gcc $(FLAGS) vm-test.c $(LINK) -o vm-test; cd -

opt-test: libajj test/opt-test.c
	cd test; gcc $(FLAGS) opt-test.c $(LINK) -o opt-test; cd -

util-test: libajj test/util-test.c
	cd test; gcc $(FLAGS) util-test.c $(LINK) -o util-test; cd -

clean:
	rm *.o
	rm *.a
	find . -maxdepth 1 -executable -type f -delete
	find test/ -maxdepth 1 -executable -type f -delete

.PHONY: clean
