
CFLAGS = -Wall -m32 -mstackrealign -std=c89 -O3 -Wno-char-subscripts
C = $(CC) $(CFLAGS)

euboea: lexer.o euboea.o codegen.o
	$(C) -o $@ $^

minilua: dasm/minilua.c
	$(CC) -Wall -std=c89 -O3 -o $@ $< -lm

euboea.o: euboea.c euboea.h
	$(C) -o $@ -c euboea.c

lexer.o: lexer.c
	$(C) -o $@ -c lexer.c

codegen.o: codegen.dasc minilua
	cat codegen.dasc | ./minilua dasm/dynasm.lua -o codegen.c -
	$(C) -o $@ -c codegen.c

clean:
	$(RM) a.out euboea minilua *.o *~ text codegen.c codegen.o lexer.o euboea.o
