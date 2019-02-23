
#ifndef _EUBOEA_H
#define _EUBOEA_H

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stddef.h>
#include <stdarg.h>

#include "codegen.h"

extern void * jit_buf;
extern size_t jit_sz;
extern int npc;
extern dasm_State * d;
extern dasm_State ** Dst = &d;

enum { IN_GLOBAL = 0, IN_FUNC };
enum { BLOCK_LOOP = 1, BLOCK_FUNC };
enum { V_LOCAL, V_GLOBAL };
enum { T_INT, T_STRING, T_DOUBLE };

typedef struct {
    int address, args, espBgn;
    char name[0xFF];
} func_t;

typedef struct {
    char name[32];
    unsigned int id;
    int type;
    int loctype;
} var_t;

typedef struct {
    char val[128];
    int nline;
} token_t;

struct {
    token_t * tok_t;
    int size, pos;
} tok_t;

struct {
    unsigned int * addr;
    int count;
} brks_t, rets_t;

struct {
    uint32_t addr[0xff];
    int count;
} memory;

int error(char *, ...);
int lex(char *);
int32_t skip(char * s);
void asmexpr();
int isassign();
int assignment();
int expression(int, int);
int (*parser())(int *, void **);
char * getstr();
func_t * getfn(char *);
var_t * getvar(char *);
void cmpexpr();
int execute(char * source);
void add_mem(int32_t addr);
void freeadd();

extern unsigned int w;

#endif
