CC?=gcc
PWD := $(shell pwd)
OPT := -g -O2
FLAGS := $(OPT) -Wpedantic -Wall -I$(PWD)/src $(JJOPT)
TFLAGS:= -DNDEBUG $(OPT) -I$(PWD)/src
COVFLAGS:= -DNDEBUG -DDO_COVERAGE -DDISABLE_OPTIMIZATION -I$(PWD)/src -g -fprofile-arcs -ftest-coverage
COVLINK := -lm
PROFILE_FLAGS := -fprofile-arcs -ftest-coverage
LINK := -L$(PWD)/. -lajj -lm

all: libajj

ajjobj: $(SRC)
	$(CC) $(FLAGS) -c src/all-in-one.c $(LINK)

libajj: ajjobj
	ar rcs libajj.a all-in-one.o

opt-test: test/opt-test.c $(SRC)
	cd test; $(CC) $(TFLAGS) -DDISABLE_OPTIMIZATION ../src/all-in-one.c opt-test.c -lm -o opt-test; cd -

unit-test: libajj test/unit-test.c
	cd test; $(CC) $(TFLAGS) unit-test.c $(LINK) -o unit-test; cd -

jinja-test: libajj test/jinja-test.c
	cd test; $(CC) $(TFLAGS) jinja-test.c -o jinja-test $(LINK) ; cd -

test: unit-test opt-test jinja-test
	cd test; ./unit-test ; ./jinja-test ; ./opt-test ; cd -

coverage: test/unit-test.c test/jinja-test.c test/opt-test.c
	cd test; $(CC) $(COVFLAGS) unit-test.c jinja-test.c opt-test.c cmain.c ../src/all-in-one.c $(COVLINK) -o cmain; cd -

clean:
	rm -f *.o
	rm -f *.a
	find . -maxdepth 1 -executable -type f -delete
	find test/ -maxdepth 1 -executable -type f -delete
	find test/ -name '*.gcda' -type f -delete
	find test/ -name '*.gcno' -type f -delete
	find test/ -name '*.gcov' -type f -delete

.PHONY: clean
