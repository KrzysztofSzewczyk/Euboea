
CFLAGS = -Wall -m32 -mstackrealign -std=c89 -O3 -Wno-char-subscripts
C = $(CC) $(CFLAGS)

euboea: euboea.o
	$(C) -o $@ $^
euboea.o: euboea.c
	$(C) -o $@ -c $^
clean:
	$(RM) a.out euboea *.o *~ text euboea.o
test: euboea
	/bin/sh test.sh
	exit $(.SHELLSTATUS)

