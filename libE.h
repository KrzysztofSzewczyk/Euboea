
#ifndef _LIBE_H
#define _LIBE_H

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

static void * funcTable[] = {
    put_i32, /*  0 */ put_str, /*  4 */ put_ln,  /*  8 */ malloc, /* 12 */
    xor128,  /* 16 */ printf,  /* 20 */ add_mem, /* 24 */ usleep, /* 28 */
    read,    /* 32 */ fprintf, /* 36 */ write,   /* 40 */ fgets,  /* 44 */
    free,    /* 48 */ freeadd, /* 52 */ exit,    /* 56 */ abort,  /* 60 */
    close    /* 72 */
};

extern int buildstd(char *);
extern void put_i32(int32_t n);
extern void put_str(int32_t * n);
extern void put_ln();
extern int xor128();

#endif
