
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

#include "libE.h"
#include "codegen.h"
#include "euboea.h"

#include <time.h>

#ifndef esleep
    void esleep(int s) {
        struct timespec reqtime;
        reqtime.tv_nsec = s * 1000;
        nanosleep(&reqtime, NULL);
    }
#else
    #define esleep usleep
#endif

void put_i32(int32_t n) {
    printf("%d", n);
}

void put_str(int32_t * n) {
    printf("%s", (char *) n);
}

void put_ln() {
    printf("\n");
}

int xor128() {
    static uint32_t x = 123456789, y = 362436069, z = 521288629;
    uint32_t t;
    t = x ^ (x << 11);
    x = y; y = z; z = w;
    w = (w ^ (w >> 19)) ^ (t ^ (t >> 8));
    return ((int32_t) w < 0) ? -(int32_t) w : (int32_t) w;
}

int buildstd(char * name) {
    size_t i = 0;
    for (; i < sizeof(stdfuncts) / sizeof(stdfuncts[0]); i++) {
        if (!strcmp(stdfuncts[i].name, name)) {
            if (!strcmp(name, "array")) {
                cmpexpr();
                dasm_put(Dst, 249, 12, 24);
            } else {
                if (stdfuncts[i].args == -1) {
                    uint32_t a = 0;
                    do {
                        cmpexpr();
                        dasm_put(Dst, 86, a);
                        a += 4;
                    } while (skip(","));
                } else {
                    int arg = 0;
                    for (; arg < stdfuncts[i].args; arg++) {
                        cmpexpr();
                        dasm_put(Dst, 86, arg*4);
                        skip(",");
                    }
                }
                dasm_put(Dst, 67, stdfuncts[i].addr);
            }
            return 1;
        }
    }
    return 0;
}

void * funcTable[] = {
    put_i32, /*  0 */ put_str, /*  4 */ put_ln,  /*  8 */ malloc, /* 12 */
    xor128,  /* 16 */ printf,  /* 20 */ add_mem, /* 24 */ esleep, /* 28 */
    read,    /* 32 */ fprintf, /* 36 */ write,   /* 40 */ fgets,  /* 44 */
    free,    /* 48 */ freeadd, /* 52 */ exit,    /* 56 */ abort,  /* 60 */
    close    /* 72 */
};

