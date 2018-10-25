
CFLAGS = -Wall -mstackrealign -std=c89 -O3 -Wno-char-subscripts
C = gcc $(CFLAGS)

euboea: euboea.o
	$(C) -o $@ $^

euboea.o: euboea.c
	$(C) -o $@ -c euboea.c
