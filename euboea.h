
#ifndef _EUBOEA_H
#define _EUBOEA_H

#include <stdint.h>
#include <stddef.h>

extern void* jit_buf;
extern size_t jit_sz;
extern int npc;

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
    char val[32];
    int nline;
} token_t;

struct {
    token_t *tok;
    int size, pos;
} tok_t;

struct {
    unsigned int *addr;
    int count;
} brks_t, rets_t;

int error(char *, ...);
int lex(char *);
int32_t skip(char *s);
void asmexpr();
int isassign();
int assignment();
int expression(int, int);
int (*parser())(int *, void **);
char* getstr();
func_t *getfn(char *);
var_t *getvar(char *);

#endif
