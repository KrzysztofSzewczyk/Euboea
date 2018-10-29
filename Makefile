
CFLAGS = -Wall -m32 -mstackrealign -std=c89 -O3 -Wno-char-subscripts
C = $(CC) $(CFLAGS)

euboea: euboea.o
	$(C) -o $@ $^
euboea.o: euboea.c
	$(C) -o $@ -c $^
clean:
	$(RM) a.out euboea *.o *~ text euboea.o
coverage.o: euboea.c
	$(C) -coverage -o $@ -c $^
coverage: coverage.o
	$(C) -coverage -o $@ $^
	/bin/sh test-coverage.sh
	exit $(.SHELLSTATUS)
	cp euboea.c coverage.c
	gcov -a -b -c -r -f -u coverage.c
	cat euboea.c.gcov
test: euboea
	/bin/sh test.sh
	exit $(.SHELLSTATUS)

