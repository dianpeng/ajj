CC?=gcc
PWD := $(shell pwd)
JJOPT:=
OPT := -g
FLAGS := $(OPT) -Wpedantic -Wall -I$(PWD)/src $(JJOPT)
TFLAGS:= -DNDEBUG $(OPT) -I$(PWD)/src
COVFLAGS:= -DNDEBUG -DDO_COVERAGE -DDISABLE_OPTIMIZATION -I$(PWD)/src -g -fprofile-arcs -ftest-coverage
COVLINK := -lm
PROFILE_FLAGS := -fprofile-arcs -ftest-coverage
LINK := -L$(PWD)/. -lajj -lm
SRC := $(wildcard src/*.c)
TEST:= $(wildcard test/*.c)
TESTF:= $(TEST:test/%=%)

all: libajj

ajjobj: $(SRC)
	$(CC) $(FLAGS) -c src/all-in-one.c $(LINK)

libajj: ajjobj
	ar rcs libajj.a all-in-one.o

lex-test: libajj test/lex-test.c
	cd test; $(CC) $(TFLAGS) lex-test.c $(LINK) -o lex-test; cd -

parser-test: libajj test/parser-test.c
	cd test; $(CC) $(TFLAGS) parser-test.c $(LINK) -o parser-test; cd -

vm-test: libajj test/vm-test.c
	cd test; $(CC) $(TFLAGS) vm-test.c $(LINK) -o vm-test; cd -

opt-test: test/opt-test.c $(SRC)
	cd test; $(CC) $(TFLAGS) -DDISABLE_OPTIMIZATION ../src/all-in-one.c opt-test.c -lm -o opt-test; cd -

util-test: libajj test/util-test.c
	cd test; $(CC) $(TFLAGS) util-test.c $(LINK) -o util-test; cd -

bc-test: libajj test/bc-test.c
	cd test; $(CC) $(TFLAGS) bc-test.c $(LINK) -o bc-test; cd -

jinja-test: libajj test/jinja-test.c
	cd test; $(CC) $(TFLAGS) jinja-test.c $(LINK) -o jinja-test; cd -

test: lex-test parser-test vm-test opt-test util-test bc-test jinja-test
	cd test; ./lex-test ; ./parser-test ; ./vm-test ; ./opt-test ; ./util-test ; ./bc-test ; ./jinja-test; cd -

test-coverage: test/lex-test.c test/parser-test.c test/vm-test.c test/util-test.c test/bc-test.c test/jinja-test.c test/opt-test.c $(SRC)
	cd test; $(CC) $(COVFLAGS) ../src/all-in-one.c $(TESTF) $(COVLINK) -o cmain; cd -

clean:
	rm *.o
	rm *.a
	find . -maxdepth 1 -executable -type f -delete
	find test/ -maxdepth 1 -executable -type f -delete

.PHONY: clean
