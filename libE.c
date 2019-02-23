
#include "libE.h"
#include "codegen.h"

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
