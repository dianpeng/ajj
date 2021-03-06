CC?=gcc
PWD := $(shell pwd)
OPT := -g -O3
FLAGS := $(OPT) -Werror -Wpedantic -Wall -I$(PWD)/src $(OPT)
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

opt-test: test/opt-test.c $(SRC) create_test_bin
	cd test/bin; $(CC) $(TFLAGS) -DDISABLE_OPTIMIZATION ../../src/all-in-one.c ../opt-test.c -lm -o opt-test; cd -

unit-test: libajj test/unit-test.c create_test_bin
	cd test/bin; $(CC) $(TFLAGS) ../unit-test.c $(LINK) -o unit-test; cd -

jinja-test: libajj test/jinja-test.c create_test_bin
	cd test/bin; $(CC) $(TFLAGS) ../jinja-test.c -o jinja-test $(LINK) ; cd -

test: unit-test opt-test jinja-test
	cd test; ./run_test.sh ; cd -

test-detail: unit-test opt-test jinja-test
	cd test; ./run_test.sh output; cd -

coverage: test/unit-test.c test/jinja-test.c test/opt-test.c create_test_bin
	cd test/bin; $(CC) $(COVFLAGS) \
		../unit-test.c \
		../jinja-test.c \
		../opt-test.c \
		../cmain.c \
		../../src/all-in-one.c \
		$(COVLINK) -o cmain; cd -

clean:
	rm -f *.o
	rm -f *.a
	rm -rf test/bin

create_test_bin:
	mkdir -p test/bin

.PHONY: clean create_test_bin
