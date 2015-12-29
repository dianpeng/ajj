OPT := -O0
FLAGS := $(OPT) -pg -Wpedantic -Wall -I./src
PROFILE_FLAGS := -fprofile-arcs -ftest-coverage
LINK := -L./. -lajj -lm
SRC := $(wildcard src/*.c)

all: libajj

ajjobj: $(SRC)
	gcc $(FLAGS) -c src/all-in-one.c $(LINK)

libajj: ajjobj
	ar rcs libajj.a all-in-one.o

test: vm-test

vm-test: libajj test/vm-test.c
	gcc $(FLAGS) test/vm-test.c $(LINK) -o vm-test

clean:
	rm *.o
	rm *.a
	find . -maxdepth 1 -executable -type f -delete

.PHONY: clean
