
#include "dasm/dasm_proto.h"
#include "dasm/dasm_x86.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
    #define MAP_ANONYMOUS MAP_ANON
#endif

extern void * jit_buf;
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
    token_t * tok_t;
    int size, pos;
} tok_t;

struct {
    unsigned int * addr;
    int count;
} brks_t, rets_t;

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
static unsigned int w;

struct {
    uint32_t addr[0xff];
    int count;
} memory;

static void setxor() {
    w = 1234 + (getpid() ^ 0xFFBA9285);
}

void init() {
    tok_t.pos = 0;
    tok_t.size = 0xfff;
    setxor();
    tok_t.tok_t = calloc(sizeof(token_t), tok_t.size);
    brks_t.addr = calloc(sizeof(uint32_t), 1);
    rets_t.addr = calloc(sizeof(uint32_t), 1);
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
    free(brks_t.addr);
    free(rets_t.addr);
    free(tok_t.tok_t);
    freeadd();
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
    memory.addr[memory.count++] = addr;
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

int32_t lex(char * code) {
    int32_t codeSize = strlen(code), line = 1, i = 0;
    int is_crlf = 0;
    for (; i < codeSize; i++) {
        if (tok_t.size <= i)
            tok_t.tok_t = realloc(tok_t.tok_t, (tok_t.size += 512 * sizeof(token_t)));
        if (isdigit(code[i])) {
            for (; isdigit(code[i]); i++)
                strncat(tok_t.tok_t[tok_t.pos].val, &(code[i]), 1);
            tok_t.tok_t[tok_t.pos].nline = line;
            i--;
            tok_t.pos++;
        } else if (isalpha(code[i])) {
            char * str = tok_t.tok_t[tok_t.pos].val;
            for (; isalpha(code[i]) || isdigit(code[i]) || code[i] == '_'; i++)
                *str++ = code[i];
            tok_t.tok_t[tok_t.pos].nline = line;
            i--;
            tok_t.pos++;
        } else if (code[i] == ' ' || code[i] == '\t') {
        } else if (code[i] == '#') {
            for (i++; code[i] != '\n'; i++)    line++;
        } else if (code[i] == '"') {
            strcpy(tok_t.tok_t[tok_t.pos].val, "\"");
            tok_t.tok_t[tok_t.pos++].nline = line;
            for (i++; code[i] != '"' && code[i] != '\0'; i++)
                strncat(tok_t.tok_t[tok_t.pos].val, &(code[i]), 1);
            tok_t.tok_t[tok_t.pos].nline = line;
            if (code[i] == '\0')
                error("%d: expected expression '\"'", tok_t.tok_t[tok_t.pos].nline);
            tok_t.pos++;
        } else if (code[i] == '\n' || (is_crlf = (code[i] == '\r' && code[i + 1] == '\n'))) {
            i += is_crlf;
            strcpy(tok_t.tok_t[tok_t.pos].val, ";");
            tok_t.tok_t[tok_t.pos].nline = line++;
            tok_t.pos++;
        } else {
            strncat(tok_t.tok_t[tok_t.pos].val, &(code[i]), 1);
            if (code[i + 1] == '=' || (code[i] == '+' && code[i + 1] == '+') || (code[i] == '-' && code[i + 1] == '-'))
                strncat(tok_t.tok_t[tok_t.pos].val, &(code[++i]), 1);
            tok_t.tok_t[tok_t.pos].nline = line;
            tok_t.pos++;
        }
    }
    tok_t.tok_t[tok_t.pos].nline = line;
    tok_t.size = tok_t.pos - 1;
    return 0;
}

static int execute(char * source) {
    int (*jit_main)(int *, void **);
    init();
    lex(source);
    jit_main = parser();
    jit_main(0, funcTable);
    dispose();
    return 0;
}

enum {
    L_START,
    L__MAX
};

static const unsigned char euboeaactions[269] = {
    252,233,245,255,133,192,15,133,244,247,252,233,245,248,1,255,249,255,252,
    233,245,249,255,249,85,137,229,129,252,236,0,0,0,128,249,255,139,133,233,
    137,133,233,255,201,195,255,249,85,137,229,129,252,236,0,0,0,128,249,139,
    181,233,255,184,237,255,80,255,252,255,150,233,255,252,255,22,255,131,196,
    4,255,184,237,137,4,36,255,137,132,253,36,233,255,248,10,252,233,245,255,
    139,141,233,90,255,137,4,145,255,137,4,17,255,139,133,233,80,255,64,255,72,
    255,88,255,139,13,237,90,255,137,4,18,255,163,237,255,161,237,80,255,137,
    193,255,139,149,233,255,139,21,237,255,139,4,138,255,15,182,4,10,255,232,
    245,129,196,239,255,139,133,233,255,161,237,255,139,4,129,255,137,195,88,
    255,252,247,252,235,255,49,210,252,247,252,251,255,49,210,252,247,252,251,
    137,208,255,1,216,255,41,216,255,137,195,88,57,216,255,15,156,208,255,15,
    159,208,255,15,149,208,255,15,148,208,255,15,158,208,255,15,157,208,255,15,
    182,192,255,33,216,255,9,216,255,49,216,255,193,224,2,137,4,36,252,255,150,
    233,80,137,4,36,252,255,150,233,88,255
};

dasm_State * d;

static dasm_State ** Dst = &d;

void * euboealabels[L__MAX];
void * jit_buf;

size_t jit_sz;
int npc;
static int main_address, mainFunc;

struct {
    var_t var[0xFF];
    int count;
    int data[0xFF];
} gvar_t;

struct {
    var_t var[0xFF][0xFF];
    int count, size[0xFF];
} lvar_t;

struct {
    char * text[0xff];
    int * addr;
    int count;
} str_t;

struct {
    func_t func[0xff];
    int count, inside, now;
} fnc_t;

void initjit() {
    dasm_init(&d, 1);
    dasm_setupglobal(&d, euboealabels, L__MAX);
    dasm_setup(&d, euboeaactions);
}

void * deinitjit() {
    dasm_link(&d, &jit_sz);
    jit_buf = mmap(0, jit_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    dasm_encode(&d, jit_buf);
    mprotect(jit_buf, jit_sz, PROT_READ | PROT_WRITE | PROT_EXEC);
    return jit_buf;
}

char * getstr() {
    str_t.text[str_t.count] = calloc(sizeof(char), strlen(tok_t.tok_t[tok_t.pos].val) + 1);
    strcpy(str_t.text[str_t.count], tok_t.tok_t[tok_t.pos++].val);
    return str_t.text[str_t.count++];
}

var_t * getvar(char * name) {
    int i = 0;
    for (; i < lvar_t.count; i++) {
        if (!strcmp(name, lvar_t.var[fnc_t.now][i].name))
            return &lvar_t.var[fnc_t.now][i];
    }
    for (i = 0; i < gvar_t.count; i++) {
        if (!strcmp(name, gvar_t.var[i].name))
            return &gvar_t.var[i];
    }
    return NULL;
}

static var_t * appvar(char * name, int type) {
    if (fnc_t.inside == IN_FUNC) {
        int32_t sz = 1 + ++lvar_t.size[fnc_t.now];
        strcpy(lvar_t.var[fnc_t.now][lvar_t.count].name, name);
        lvar_t.var[fnc_t.now][lvar_t.count].type = type;
        lvar_t.var[fnc_t.now][lvar_t.count].id = sz;
        lvar_t.var[fnc_t.now][lvar_t.count].loctype = V_LOCAL;
        return &lvar_t.var[fnc_t.now][lvar_t.count++];
    } else if (fnc_t.inside == IN_GLOBAL) {
        strcpy(gvar_t.var[gvar_t.count].name, name);
        gvar_t.var[gvar_t.count].type = type;
        gvar_t.var[gvar_t.count].loctype = V_GLOBAL;
        gvar_t.var[gvar_t.count].id = (int)&gvar_t.data[gvar_t.count];
        return &gvar_t.var[gvar_t.count++];
    }
    return NULL;
}

func_t * getfn(char * name) {
    int i = 0;
    for (; i < fnc_t.count; i++) {
        if (!strcmp(fnc_t.func[i].name, name))
            return &fnc_t.func[i];
    }
    return NULL;
}

static func_t * appfn(char * name, int address, int espBgn, int args) {
    fnc_t.func[fnc_t.count].address = address;
    fnc_t.func[fnc_t.count].espBgn = espBgn;
    fnc_t.func[fnc_t.count].args = args;
    strcpy(fnc_t.func[fnc_t.count].name, name);
    return &fnc_t.func[fnc_t.count++];
}

static int32_t mkbrk() {
    uint32_t lbl = npc++;
    dasm_growpc(&d, npc);
    dasm_put(Dst, 0, lbl);
    brks_t.addr = realloc(brks_t.addr, 4 * (brks_t.count + 1));
    brks_t.addr[brks_t.count] = lbl;
    return brks_t.count++;
}

static int32_t mkret() {
    cmpexpr();
    int lbl = npc++;
    dasm_growpc(&d, npc);
    dasm_put(Dst, 0, lbl);
    rets_t.addr = realloc(rets_t.addr, 4 * (rets_t.count + 1));
    if (rets_t.addr == NULL) error("no enough memory");
    rets_t.addr[rets_t.count] = lbl;
    return rets_t.count++;
}

int32_t skip(char * s) {
    if (!strcmp(s, tok_t.tok_t[tok_t.pos].val)) {
        tok_t.pos++;
        return 1;
    }
    return 0;
}

int32_t error(char * errs, ...) {
    va_list args;
    va_start(args, errs);
    printf("error: ");
    vprintf(errs, args);
    puts("");
    va_end(args);
    exit(0);
    return 0;
}

static int eval(int pos, int status) {
    while (tok_t.pos < tok_t.size)
        if (expression(pos, status)) return 1;
    return 0;
}

static var_t * mkvar() {
    int32_t npos = tok_t.pos;
    if (isalpha(tok_t.tok_t[tok_t.pos].val[0])) {
        tok_t.pos++;
        if (skip(":")) {
            if (skip("int")) {
                --tok_t.pos;
                return appvar(tok_t.tok_t[npos].val, T_INT);
            }
            if (skip("string")) {
                --tok_t.pos;
                return appvar(tok_t.tok_t[npos].val, T_STRING);
            }
            if (skip("double")) {
                --tok_t.pos;
                return appvar(tok_t.tok_t[npos].val, T_DOUBLE);
            }
        } else {
            --tok_t.pos;
            return appvar(tok_t.tok_t[npos].val, T_INT);
        }
    } else error("%d: can't declare variable", tok_t.tok_t[tok_t.pos].nline);
    return NULL;
}

static int chkstmt() {
    cmpexpr();
    uint32_t end = npc++;
    dasm_growpc(&d, npc);
    dasm_put(Dst, 4, end);
    return eval(end, 0);
}

static int whilestmt() {
    uint32_t loopBgn = npc++;
    dasm_growpc(&d, npc);
    dasm_put(Dst, 16, loopBgn);
    cmpexpr();
    uint32_t stepBgn[2], stepOn = 0;
    if (skip(",")) {
        stepOn = 1;
        stepBgn[0] = tok_t.pos;
        for (; tok_t.tok_t[tok_t.pos].val[0] != ';'; tok_t.pos++);
    }
    uint32_t end = npc++;
    dasm_growpc(&d, npc);
    dasm_put(Dst, 4, end);
    if (skip(":")) expression(0, BLOCK_LOOP);
    else eval(0, BLOCK_LOOP);
    if (stepOn) {
        stepBgn[1] = tok_t.pos;
        tok_t.pos = stepBgn[0];
        if (isassign()) assignment();
        tok_t.pos = stepBgn[1];
    }
    dasm_put(Dst, 18, loopBgn, end);
    for (--brks_t.count; brks_t.count >= 0; brks_t.count--)
        dasm_put(Dst, 16, brks_t.addr[brks_t.count]);
    brks_t.count = 0;
    return 0;
}

static int32_t fnstmt() {
    int32_t argsc = 0;
    int i = 0;
    char * funcName = tok_t.tok_t[tok_t.pos++].val;
    fnc_t.now++; fnc_t.inside = IN_FUNC;
    if (skip("(")) {
        do {
            mkvar();
            tok_t.pos++;
            argsc++;
        } while (skip(","));
        if (!skip(")"))
            error("%d: expecting ')'", tok_t.tok_t[tok_t.pos].nline);
    }
    int func_addr = npc++;
    dasm_growpc(&d, npc);
    int func_esp = npc++;
    dasm_growpc(&d, npc);
    appfn(funcName, func_addr, func_esp, argsc);
    dasm_put(Dst, 23, func_addr, func_esp);
    for (; i < argsc; i++)
        dasm_put(Dst, 36, ((argsc - i - 1) * sizeof(int32_t) + 8), - (i + 2)*4);
    eval(0, BLOCK_FUNC);
    for (--rets_t.count; rets_t.count >= 0; --rets_t.count)
        dasm_put(Dst, 16, rets_t.addr[rets_t.count]);
    rets_t.count = 0;
    dasm_put(Dst, 43);
    return 0;
}

int expression(int pos, int status) {
    int isputs = 0;
    if (skip("$")) {
        if (isassign()) assignment();
    } else if (skip("def"))
        fnstmt();
    else if (fnc_t.inside == IN_GLOBAL &&
             strcmp("def", tok_t.tok_t[tok_t.pos+1].val) &&
             strcmp("$", tok_t.tok_t[tok_t.pos+1].val) &&
             (tok_t.pos+1 == tok_t.size || strcmp(";", tok_t.tok_t[tok_t.pos+1].val))) {
        fnc_t.inside = IN_FUNC;
        mainFunc = ++fnc_t.now;
        int main_esp = npc++;
        dasm_growpc(&d, npc);
        appfn("main", main_address, main_esp, 0);
        dasm_put(Dst, 46, main_address, main_esp, 12);
        eval(0, 0);
        dasm_put(Dst, 43);
        fnc_t.inside = IN_GLOBAL;
    } else if (isassign())
        assignment();
    else if ((isputs = skip("puts")) || skip("output")) {
        do {
            int isstring = 0;
            if (skip("\"")) {
                dasm_put(Dst, 62, getstr());
                isstring = 1;
            } else
                cmpexpr();
            dasm_put(Dst, 65);
            if (isstring)
                dasm_put(Dst, 67, 4);

            else
                dasm_put(Dst, 72);
            dasm_put(Dst, 76);
        } while (skip(","));
        if (isputs)
            dasm_put(Dst, 67, 0x8);
    } else if (skip("printf")) {
        if (skip("\""))
            dasm_put(Dst, 80, getstr());
        if (skip(",")) {
            uint32_t a = 4;
            do {
                cmpexpr();
                dasm_put(Dst, 86, a);
                a += 4;
            } while (skip(","));
        }
        dasm_put(Dst, 67, 0x14);
    } else if (skip("for")) {
        assignment();
        if (!skip(","))
            error("%d: expecting ','", tok_t.tok_t[tok_t.pos].nline);
        whilestmt();
    } else if (skip("while"))
        whilestmt();
    else if (skip("return"))
        mkret();
    else if (skip("if"))
        chkstmt();
    else if (skip("else")) {
        int32_t end = npc++;
        dasm_growpc(&d, npc);
        dasm_put(Dst, 18, end, pos);
        eval(end, 0);
        return 1;
    } else if (skip("elsif")) {
        int32_t endif = npc++;
        dasm_growpc(&d, npc);
        dasm_put(Dst, 18, endif, pos);
        cmpexpr();
        uint32_t end = npc++;
        dasm_growpc(&d, npc);
        dasm_put(Dst, 4, end);
        eval(end, 0);
        dasm_put(Dst, 16, endif);
        return 1;
    } else if (skip("break"))
        mkbrk();
    else if (skip("end")) {
        if (status == 0)
            dasm_put(Dst, 16, pos);

        else if (status == BLOCK_FUNC) fnc_t.inside = IN_GLOBAL;
        return 1;
    } else if (!skip(";"))  cmpexpr();
    return 0;
}

static char * repescape(char * str) {
    char escape[14][3] = {
        "\\a", "\a", "\\r", "\r", "\\f", "\f",
        "\\n", "\n", "\\t", "\t", "\\b", "\b",
        "\\q", "\""
    };
    int32_t i = 0;
    for (; i < 12; i += 2) {
        char * pos;
        while ((pos = strstr(str, escape[i])) != NULL) {
            *pos = escape[i + 1][0];
            memmove(pos + 1, pos + 2, strlen(pos + 2) + 1);
        }
    }
    return str;
}

int (*parser())(int *, void **) {
    int i;
    uint8_t * buf;
    initjit();
    tok_t.pos = 0;
    str_t.addr = calloc(0xFF, sizeof(int32_t));
    main_address = npc++;
    dasm_growpc(&d, npc);
    dasm_put(Dst, 92, main_address);
    eval(0, 0);
    for (i = 0; i < str_t.count; ++i)
        repescape(str_t.text[i]);
    buf = (uint8_t *)deinitjit();
    for (i = 0; i < fnc_t.count; i++)
        *(int *)(buf + dasm_getpclabel(&d, fnc_t.func[i].espBgn) - 4) = (lvar_t.size[i+1] + 6)*4;
    dasm_free(&d);
    return ((int (*)(int *, void **))euboealabels[L_START]);
}

int32_t isassign() {
    char * val = tok_t.tok_t[tok_t.pos + 1].val;
    if (!strcmp(val, "=") || !strcmp(val, "++") || !strcmp(val, "--")) return 1;
    if (!strcmp(val, "[")) {
        int32_t i = tok_t.pos + 2, t = 1;
        while (t) {
            val = tok_t.tok_t[i].val;
            if (!strcmp(val, "[")) t++;
            if (!strcmp(val, "]")) t--;
            if (!strcmp(val, ";"))
                error("%d: invalid expression", tok_t.tok_t[tok_t.pos].nline);
            i++;
        }
        if (!strcmp(tok_t.tok_t[i].val, "=")) return 1;
    } else if (!strcmp(val, ":") && !strcmp(tok_t.tok_t[tok_t.pos + 3].val, "="))
        return 1;
    return 0;
}

int32_t assignment() {
    var_t * v = getvar(tok_t.tok_t[tok_t.pos].val);
    int32_t inc = 0, dec = 0, declare = 0;
    if (v == NULL) {
        declare++;
        v = mkvar();
    }
    tok_t.pos++;
    int siz = (v->type == T_INT ? sizeof(int32_t) : v->type == T_STRING ? sizeof(int32_t *) : v->type == T_DOUBLE ? sizeof(double) : 4);
    if (v->loctype == V_LOCAL) {
        if (skip("[")) {
            cmpexpr();
            dasm_put(Dst, 65);
            if (skip("]") && skip("=")) {
                cmpexpr();
                dasm_put(Dst, 98, - siz*v->id);
                if (v->type == T_INT)
                    dasm_put(Dst, 103);

                else
                    dasm_put(Dst, 107);
            } else if ((inc = skip("++")) || (dec = skip("--"))) {
            } else
                error("%d: invalid assignment", tok_t.tok_t[tok_t.pos].nline);
        } else {
            if (skip("="))
                cmpexpr();
            else if ((inc = skip("++")) || (dec = skip("--"))) {
                dasm_put(Dst, 111, - siz*v->id);
                if (inc)
                    dasm_put(Dst, 116);
                else if (dec)
                    dasm_put(Dst, 118);
            }
            dasm_put(Dst, 39, - siz*v->id);
            if (inc || dec)
                dasm_put(Dst, 120);
        }
    } else if (v->loctype == V_GLOBAL) {
        if (declare) {
            if (skip("=")) {
                unsigned * m = (unsigned *) v->id;
                *m = atoi(tok_t.tok_t[tok_t.pos++].val);
            }
        } else {
            if (skip("[")) {
                cmpexpr();
                dasm_put(Dst, 65);
                if (skip("]") && skip("=")) {
                    cmpexpr();
                    dasm_put(Dst, 122, v->id);
                    if (v->type == T_INT)
                        dasm_put(Dst, 103);

                    else
                        dasm_put(Dst, 127);
                } else error("%d: invalid assignment",
                                 tok_t.tok_t[tok_t.pos].nline);
            } else if (skip("=")) {
                cmpexpr();
                dasm_put(Dst, 131, v->id);
            } else if ((inc = skip("++")) || (dec = skip("--"))) {
                dasm_put(Dst, 134, v->id);
                if (inc)
                    dasm_put(Dst, 116);
                else if (dec)
                    dasm_put(Dst, 118);
                dasm_put(Dst, 131, v->id);
            }
            if (inc || dec)
                dasm_put(Dst, 120);
        }
    }
    return 0;
}
extern int buildstd(char *);

static int32_t isidx() {
    return !strcmp(tok_t.tok_t[tok_t.pos].val, "[");
}

static void priexp() {
    if (isdigit(tok_t.tok_t[tok_t.pos].val[0]))
        dasm_put(Dst, 62, atoi(tok_t.tok_t[tok_t.pos++].val));

    else if (skip("'")) {
        dasm_put(Dst, 62, tok_t.tok_t[tok_t.pos++].val[0]);
        skip("'");
    } else if (skip("\""))
        dasm_put(Dst, 62, getstr());

    else if (isalpha(tok_t.tok_t[tok_t.pos].val[0])) {
        char * name = tok_t.tok_t[tok_t.pos].val;
        var_t * v;
        if (isassign()) assignment();
        else {
            tok_t.pos++;
            if (skip("[")) {
                if ((v = getvar(name)) == NULL)
                    error("%d: '%s' was not declared",
                          tok_t.tok_t[tok_t.pos].nline, name);
                cmpexpr();
                dasm_put(Dst, 138);
                if (v->loctype == V_LOCAL)
                    dasm_put(Dst, 141, - v->id*4);

                else if (v->loctype == V_GLOBAL)
                    dasm_put(Dst, 145, v->id);
                if (v->type == T_INT)
                    dasm_put(Dst, 149);

                else
                    dasm_put(Dst, 153);
                if (!skip("]"))
                    error("%d: expected expression ']'",
                          tok_t.tok_t[tok_t.pos].nline);
            } else if (skip("(")) {
                if (!buildstd(name)) {
                    func_t * function = getfn(name);
                    char * val = tok_t.tok_t[tok_t.pos].val;
                    int i = 0;
                    if (isalpha(val[0]) || isdigit(val[0]) ||
                        !strcmp(val, "\"") || !strcmp(val, "(")) {
                        for (; i < function->args; i++) {
                            cmpexpr();
                            dasm_put(Dst, 65);
                            skip(",");
                        }
                    }
                    dasm_put(Dst, 158, function->address, function->args * sizeof(int32_t));
                }
                if (!skip(")"))
                    error("func: %d: expected expression ')'",
                          tok_t.tok_t[tok_t.pos].nline);
            } else {
                if ((v = getvar(name)) == NULL)
                    error("var: %d: '%s' was not declared",
                          tok_t.tok_t[tok_t.pos].nline, name);
                if (v->loctype == V_LOCAL)
                    dasm_put(Dst, 164, - v->id*4);

                else if (v->loctype == V_GLOBAL)
                    dasm_put(Dst, 168, v->id);
            }
        }
    } else if (skip("(")) {
        if (isassign()) assignment();
        else cmpexpr();
        if (!skip(")")) error("%d: expected expression ')'", tok_t.tok_t[tok_t.pos].nline);
    }
    while (isidx()) {
        dasm_put(Dst, 138);
        skip("[");
        cmpexpr();
        skip("]");
        dasm_put(Dst, 171);
    }
}

static void muldivexp() {
    int32_t mul = 0, div = 0;
    priexp();
    while ((mul = skip("*")) || (div = skip("/")) || skip("%")) {
        dasm_put(Dst, 65);
        priexp();
        dasm_put(Dst, 175);
        if (mul)
            dasm_put(Dst, 179);

        else if (div)
            dasm_put(Dst, 184);

        else
            dasm_put(Dst, 191);
    }
}

static void addSubExpr() {
    int32_t add;
    muldivexp();
    while ((add = skip("+")) || skip("-")) {
        dasm_put(Dst, 65);
        muldivexp();
        dasm_put(Dst, 175);
        if (add)
            dasm_put(Dst, 200);

        else
            dasm_put(Dst, 203);
    }
}

static void logicexp() {
    int32_t lt = 0, gt = 0, ne = 0, eql = 0, fle = 0;
    addSubExpr();
    if ((lt = skip("<")) || (gt = skip(">")) || (ne = skip("!=")) ||
        (eql = skip("==")) || (fle = skip("<=")) || skip(">=")) {
        dasm_put(Dst, 65);
        addSubExpr();
        dasm_put(Dst, 206);
        if (lt)
            dasm_put(Dst, 212);
        else if (gt)
            dasm_put(Dst, 216);
        else if (ne)
            dasm_put(Dst, 220);
        else if (eql)
            dasm_put(Dst, 224);
        else if (fle)
            dasm_put(Dst, 228);
        else
            dasm_put(Dst, 232);
        dasm_put(Dst, 236);
    }
}

void cmpexpr() {
    int and = 0, or = 0;
    logicexp();
    while ((and = skip("and") || skip("&")) ||
           (or = skip("or") || skip("|")) || (skip("xor") || skip("^"))) {
        dasm_put(Dst, 65);
        logicexp();
        dasm_put(Dst, 175);
        if (and)
            dasm_put(Dst, 240);
        else if (or)
            dasm_put(Dst, 243);
        else
            dasm_put(Dst, 246);
    }
}

typedef struct {
    char * name;
    int args, addr;
} stdfn;

static stdfn stdfuncts[] = {
    {"Array", 1, 12},
    {"rand", 0, 16}, {"printf", -1, 20}, {"usleep", 1, 28},
    {"fprintf", -1, 36}, {"fgets", 3, 44},
    {"free", 1, 48}, {"freeLocal", 0, 52}, {"malloc", 1, 12}, {"exit", 1, 56},
    {"abort", 0, 60}, {"read", 3, 32}, {"write", 3, 40}, {"close", 1, 64}
};

int buildstd(char * name) {
    size_t i = 0;
    for (; i < sizeof(stdfuncts) / sizeof(stdfuncts[0]); i++) {
        if (!strcmp(stdfuncts[i].name, name)) {
            if (!strcmp(name, "Array")) {
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
