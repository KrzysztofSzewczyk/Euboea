
CFLAGS = -Wall -m32 -mstackrealign -std=c89 -O3 -Wno-char-subscripts
C = $(CC) $(CFLAGS)

OBJ = $(patsubst %.c, %.o, $(wildcard *.c))

euboea: $(OBJ)
	$(C) -o $@ $^

clicoverage: coverage %.c.gcov

coverage: CFLAGS = -coverage -Wall -m32 -mstackrealign -std=c89 -O3 -Wno-char-subscripts
coverage: $(OBJ)
	$(C) -o $@ $^
	/bin/sh test-coverage.sh
	exit $(.SHELLSTATUS)
	gcov -a -b -c -r -f -u euboea.c

%.c.gcov: %.c
	gcov -a -b -c -r -f -u $^

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean
clean:
	rm -f euboea *.o

.PHONY: test
test: euboea
	/bin/sh test.sh
	exit $(.SHELLSTATUS)

