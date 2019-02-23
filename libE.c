
#include "libE.h"
#include "codegen.h"
#include "euboea.h"

#ifndef usleep
    void usleep(int s) {
        struct timespec reqtime;
        reqtime.tv_nsec = s * 1000;
        nanosleep(&reqtime, NULL);
    }
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
    xor128,  /* 16 */ printf,  /* 20 */ add_mem, /* 24 */ usleep, /* 28 */
    read,    /* 32 */ fprintf, /* 36 */ write,   /* 40 */ fgets,  /* 44 */
    free,    /* 48 */ freeadd, /* 52 */ exit,    /* 56 */ abort,  /* 60 */
    close    /* 72 */
};

