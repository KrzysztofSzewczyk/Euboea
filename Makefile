
CFLAGS = -Wall -m32 -mstackrealign -std=c89 -O3 -Wno-char-subscripts
C = $(CC) $(CFLAGS)

euboea: euboea.o
	$(C) -o $@ $^
coverage: CFLAGS = -coverage -Wall -m32 -mstackrealign -std=c89 -O3 -Wno-char-subscripts
coverage: euboea.o
	$(C) -o $@ $^
	/bin/sh test-coverage.sh
	exit $(.SHELLSTATUS)
	gcov -a -b -c -r -f -u euboea.c

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	$(RM) a.out euboea *.o *~ text euboea.o
test: euboea
	/bin/sh test.sh
	exit $(.SHELLSTATUS)

