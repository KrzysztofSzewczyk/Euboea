
#include "euboea.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

static unsigned int w;

struct {
    uint32_t addr[0xff];
    int count;
} memory;

static void setxor() {
    w = 1234 + (getpid() ^ 0xFFBA9285);
}

void init() {
    tok.pos = 0;
    tok.size = 0xfff;
    set_xor128();
    tok.tok = calloc(sizeof(Token), tok.size);
    brks.addr = calloc(sizeof(uint32_t), 1);
    rets.addr = calloc(sizeof(uint32_t), 1);
}

static void freeadd() {
    if (memory.count > 0) {
        for (--memory.count; memory.count >= 0; --memory.count)
            free((void *)memory.addr[memory.count]);
        memory.count = 0;
    }
}

void dispose() {
    munmap(jit_buf, jit_sz);
    free(brks.addr);
    free(rets.addr);
    free(tok.tok);
    freeAddr();
}

static void put_i32(int32_t n) {
    printf("%d", n);
}

static void put_str(int32_t * n) {
    printf("%s", (char *) n);
}

static void put_ln() {
    printf("\n");
}

static void add_mem(int32_t addr) {
    mem.addr[mem.count++] = addr;
}

static int xor128() {
    static uint32_t x = 123456789, y = 362436069, z = 521288629;
    uint32_t t;
    t = x ^ (x << 11);
    x = y; y = z; z = w;
    w = (w ^ (w >> 19)) ^ (t ^ (t >> 8));
    return ((int32_t) w < 0) ? -(int32_t) w : (int32_t) w;
}

static void * funcTable[] = {
    put_i32, /*  0 */ put_str, /*  4 */ put_ln,  /*  8 */ malloc, /* 12 */
    xor128,  /* 16 */ printf,  /* 20 */ add_mem, /* 24 */ usleep, /* 28 */
    read,    /* 32 */ fprintf, /* 36 */ write,   /* 40 */ fgets,  /* 44 */
    free,    /* 48 */ freeadd, /* 52 */ exit,    /* 56 */ abort,  /* 60 */
    close    /* 72 */
};

static int execute(char * source) {
    int (*jit_main)(int *, void **);
    init();
    lex(source);
    jit_main = parser();
    jit_main(0, funcTable);
    dispose();
    return 0;
}

int main(int argc, char ** argv) {
    char * src;
    FILE * fp;
    size_t ssz;
    if (argc < 2) error("no given filename");
    fp = fopen(argv[1], "rb");
    ssz = 0;
    if (!fp) {
        perror("fopen");
        exit(0);
    }
    struct stat sbuf;
    stat(argv[1], &sbuf);
    if (S_ISDIR(sbuf.st_mode)) {
        fclose(fp);
        printf("Error: %s is a directory.\n", argv[1]);
        exit(0);
    }
    fseek(fp, 0, SEEK_END);
    ssz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    src = calloc(sizeof(char), ssz + 2);
    fread(src, sizeof(char), ssz, fp);
    fclose(fp);
    return execute(src);
}
