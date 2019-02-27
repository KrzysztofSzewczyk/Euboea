
/* EUBOEA
 * Copyright (C) Kamila Palaiologos Szewczyk, 2019.
 * License: MIT
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _LIBE_H
#define _LIBE_H

#include <stdint.h>

typedef struct {
    char * name;
    int args, addr;
} stdfn;

static stdfn stdfuncts[] = {
    {"array", 1, 12},
    {"rand", 0, 16}, {"printf", -1, 20}, {"usleep", 1, 28},
    {"fprintf", -1, 36}, {"fgets", 3, 44},
    {"free", 1, 48}, {"freeLocal", 0, 52}, {"malloc", 1, 12}, {"exit", 1, 56},
    {"abort", 0, 60}, {"read", 3, 32}, {"write", 3, 40}, {"close", 1, 64}
};

extern void * funcTable[];

extern int buildstd(char *);
extern void put_i32(int32_t n);
extern void put_str(int32_t * n);
extern void put_ln();
extern int xor128();

#endif
