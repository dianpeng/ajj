OPT := -O0 -g
FLAGS := $(OPT) -Wpedantic -Wall -I$(PWD)/src
PROFILE_FLAGS := -fprofile-arcs -ftest-coverage
LINK := -L$(PWD)/. -lajj -lm
SRC := $(wildcard src/*.c)

all: libajj

ajjobj: $(SRC)
	gcc $(FLAGS) -c src/all-in-one.c $(LINK)

libajj: ajjobj
	ar rcs libajj.a all-in-one.o

test: lex-test

lex-test: libajj test/lex-test.c
	cd test; gcc $(FLAGS) lex-test.c $(LINK) -o lex-test; cd -

vm-test: libajj test/vm-test.c
	cd test; gcc $(FLAGS) vm-test.c $(LINK) -o vm-test; cd -

clean:
	rm *.o
	rm *.a
	find . -maxdepth 1 -executable -type f -delete
	find test/ -maxdepth 1 -executable -type f -delete

.PHONY: clean
