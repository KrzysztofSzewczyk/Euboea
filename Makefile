
# Euboea
#
# Copyright (C) Kamila Szewczyk, 2019.
# License: MIT
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
# files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,
# modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
# Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

CFLAGS = -Wall -m32 -mstackrealign -std=c89 -O0 -Wno-char-subscripts -D_POSIX_C_SOURCE=199309L
C = $(CC) $(CFLAGS)

OBJ = $(patsubst %.c, %.o, $(wildcard *.c))

euboea: $(OBJ)
	$(C) -o $@ $^

coverage: CFLAGS = -coverage -Wall -m32 -mstackrealign -std=c89 -O3 -Wno-char-subscripts -D_POSIX_C_SOURCE=199309L -DNVARARG
coverage: $(OBJ)
	$(C) -o $@ $^
	/bin/sh test-coverage.sh
	exit $(.SHELLSTATUS)
	gcov -a -b -c -r -f -u *.c

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean
clean:
	rm -f euboea *.o

.PHONY: test
test: euboea
	/bin/sh test.sh
	exit $(.SHELLSTATUS)

