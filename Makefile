
CFLAGS = -Wall -m32 -mstackrealign -std=c89 -O3 -Wno-char-subscripts
C = $(CC) $(CFLAGS)

euboea: lexer.o euboea.o codegen.o
	$(C) -o $@ $^

minilua: dasm/minilua.c
	$(CC) -Wall -std=c89 -O3 -o $@ $< -lm

euboea.o: euboea.c
	$(C) -o $@ -c $^

lexer.o: lexer.c
	$(C) -o $@ -c $^

codegen.o: codegen.c
	$(C) -o $@ -c $^

clean:
	$(RM) a.out euboea minilua *.o *~ text codegen.c codegen.o lexer.o euboea.o
