
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdint.h>

enum { EAX = 0, ECX, EDX, EBX, ESP, EBP, ESI, EDI };
enum { IN_GLOBAL = 0, IN_FUNC };
enum { BLOCK_LOOP = 1, BLOCK_FUNC };
enum { V_LOCAL, V_GLOBAL };
enum { T_INT, T_STRING, T_DOUBLE };

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

struct {
    uint32_t addr[0xff];
    int count;
} mem_t;

typedef struct {
    int address, args;
    char name[0xFF];
} func_t;

typedef struct {
    char name[32];
    unsigned int id;
    int type;
    int loctype;
} var_t;

struct {
    var_t var[0xFF];
    int count;
} gvar_t;

typedef struct {
    char * name;
    int args, addr;
} stdfn_t;

struct {
    char * text[0xff];
    int * addr;
    int count;
} str_t;

struct {
    func_t func[0xff];
    int count, inside;
} fn_t;

int isassign();
int assignment();
int expression(int, int);
int parser();
int getstr();
func_t * getfn(char *);
int error(char *, ...);
void asmexpr();
int32_t skip(char * s);
var_t * getvar(char *);
extern int makestd(char *);

unsigned char * ntvCode;
int ntvCount;
static var_t locVar[0xFF][0xFF];
static int varSize[0xFF], varCounter;
static int nowFunc;
static unsigned int w;

static void emit(unsigned char val) {
    ntvCode[ntvCount++] = (val);
}

static void emit32(unsigned int val) {
    emit(val << 24 >> 24);
    emit(val << 16 >> 24);
    emit(val <<  8 >> 24);
    emit(val <<  0 >> 24);
}

static void emit32ins(unsigned int val, int pos) {
    ntvCode[pos + 0] = (val << 24 >> 24);
    ntvCode[pos + 1] = (val << 16 >> 24);
    ntvCode[pos + 2] = (val <<  8 >> 24);
    ntvCode[pos + 3] = (val <<  0 >> 24);
}

static int32_t isindex() {
    return !strcmp(tok_t.tok_t[tok_t.pos].val, "[");
}

static void primaryexp() {
    if (isdigit(tok_t.tok_t[tok_t.pos].val[0])) {
        emit(0xb8 + EAX); emit32(atoi(tok_t.tok_t[tok_t.pos++].val));
    } else if (skip("'")) {
        emit(0xb8 + EAX);
        emit32(tok_t.tok_t[tok_t.pos++].val[0]);
        skip("'");
    } else if (skip("\"")) {
        emit(0xb8); getstr(); emit32(0x00);
    } else if (isalpha(tok_t.tok_t[tok_t.pos].val[0])) {
        char * name = tok_t.tok_t[tok_t.pos].val;
        var_t * v;
        if (isassign()) assignment();
        else {
            tok_t.pos++;
            if (skip("[")) {
                if ((v = getvar(name)) == NULL)
                    error("%d: '%s' was not declared", tok_t.tok_t[tok_t.pos].nline, name);
                asmexpr();
                emit(0x89); emit(0xc0 + EAX * 8 + ECX);
                if (v->loctype == V_LOCAL) {
                    emit(0x8b); emit(0x55);
                    emit(256 - sizeof(int32_t) * v->id);
                } else if (v->loctype == V_GLOBAL) {
                    emit(0x8b); emit(0x15); emit32(v->id);
                }
                if (v->type == T_INT) {
                    emit(0x8b); emit(0x04); emit(0x8a);
                } else {
                    emit(0x0f); emit(0xb6); emit(0x04); emit(0x0a);
                }
                if (!skip("]"))
                    error("%d: expected expression ']'", tok_t.tok_t[tok_t.pos].nline);
            } else if (skip("(")) {
                if (!makestd(name)) {
                    func_t * function = getfn(name);
                    char * val = tok_t.tok_t[tok_t.pos].val;
                    int i = 0;
                    if (isalpha(val[0]) || isdigit(val[0]) ||
                        !strcmp(val, "\"") || !strcmp(val, "(")) {
                        for (; i < function->args; i++) {
                            asmexpr();
                            emit(0x50 + EAX);
                            skip(",");
                        }
                    }
                    emit(0xe8); emit32(0xFFFFFFFF - (ntvCount - function->address) - 3);
                    emit(0x81); emit(0xc0 + ESP);
                    emit32(function->args * sizeof(int32_t));
                }
                if (!skip(")"))
                    error("func: %d: expected expression ')'", tok_t.tok_t[tok_t.pos].nline);
            } else {
                if ((v = getvar(name)) == NULL)
                    error("var: %d: '%s' was not declared", tok_t.tok_t[tok_t.pos].nline, name);
                if (v->loctype == V_LOCAL) {
                    emit(0x8b); emit(0x45); emit(256 - sizeof(int32_t) * v->id);
                } else if (v->loctype == V_GLOBAL) {
                    emit(0xa1); emit32(v->id);
                }
            }
        }
    } else if (skip("(")) {
        if (isassign()) assignment();
        else asmexpr();
        if (!skip(")")) error("%d: expected expression ')'", tok_t.tok_t[tok_t.pos].nline);
    }
    while (isindex()) {
        emit(0x89); emit(0xc0 + EAX * 8 + ECX);
        skip("[");
        asmexpr();
        skip("]");
        emit(0x8b); emit(0x04); emit(0x81);
    }
}

static void binexp() {
    int32_t mul = 0, div = 0;
    primaryexp();
    while ((mul = skip("*")) || (div = skip("/")) || skip("%")) {
        emit(0x50 + EAX);
        primaryexp();
        emit(0x89); emit(0xc0 + EAX * 8 + EBX);
        emit(0x58 + EAX);
        if (mul) {
            emit(0xf7); emit(0xe8 + EBX);
        } else if (div) {
            emit(0xb8 + EDX); emit32(0);
            emit(0xf7); emit(0xf8 + EBX);
        } else {
            emit(0xb8 + EDX); emit32(0);
            emit(0xf7); emit(0xf8 + EBX);
            emit(0x89); emit(0xc0 + EDX * 8 + EAX);
        }
    }
}

static void algexp() {
    int32_t add;
    binexp();
    while ((add = skip("+")) || skip("-")) {
        emit(0x50 + EAX);
        binexp();
        emit(0x89); emit(0xc0 + EAX * 8 + EBX);
        emit(0x58 + EAX);
        if (add) {
            emit(0x03); emit(EAX * 8 + 0xc0 + EBX);
        } else {
            emit(0x2b); emit(EAX * 8 + 0xc0 + EBX);
        }
    }
}

static void conexp() {
    int32_t lt = 0, gt = 0, ne = 0, eql = 0, fle = 0;
    algexp();
    if ((lt = skip("<")) || (gt = skip(">")) || (ne = skip("!=")) || (eql = skip("==")) || (fle = skip("<=")) || skip(">=")) {
        emit(0x50 + EAX);
        algexp();
        emit(0x89); emit(0xc0 + EAX * 8 + EBX);
        emit(0x58 + EAX);
        emit(0x39); emit(0xd8);
        emit(0x0f);
        emit(lt ? 0x9c:
             gt ? 0x9f:
             ne ? 0x95:
             eql ? 0x94:
             fle ? 0x9e:
             0x9d);
        emit(0xc0);
        emit(0x0f); emit(0xb6); emit(0xc0);
    }
}

void asmexpr() {
    int and = 0, or = 0;
    conexp();
    while ((and = skip("and") || skip("&")) ||
           (or = skip("or") || skip("|")) || (skip("xor") || skip("^"))) {
        emit(0x50 + EAX);
        conexp();
        emit(0x89); emit(0xc0 + EAX * 8 + EBX);
        emit(0x58 + EAX);
        emit(and ? 0x21 : or ? 0x09 : 0x31); emit(0xd8);
    }
}

int32_t getstr() {
    str_t.text[ str_t.count ] = calloc(sizeof(char), strlen(tok_t.tok_t[tok_t.pos].val) + 1);
    strcpy(str_t.text[str_t.count], tok_t.tok_t[tok_t.pos++].val);
    *str_t.addr++ = ntvCount;
    return str_t.count++;
}

var_t * getvar(char * name) {
    int i = 0;
    for (; i < varCounter; i++) {
        if (!strcmp(name, locVar[nowFunc][i].name))
            return &locVar[nowFunc][i];
    }
    for (i = 0; i < gvar_t.count; i++) {
        if (!strcmp(name, gvar_t.var[i].name))
            return &gvar_t.var[i];
    }
    return NULL;
}

static var_t * appvar(char * name, int type) {
    if (fn_t.inside == IN_FUNC) {
        int32_t sz = 1 + ++varSize[nowFunc];
        strcpy(locVar[nowFunc][varCounter].name, name);
        locVar[nowFunc][varCounter].type = type;
        locVar[nowFunc][varCounter].id = sz;
        locVar[nowFunc][varCounter].loctype = V_LOCAL;
        return &locVar[nowFunc][varCounter++];
    } else if (fn_t.inside == IN_GLOBAL) {
        strcpy(gvar_t.var[gvar_t.count].name, name);
        gvar_t.var[gvar_t.count].type = type;
        gvar_t.var[gvar_t.count].loctype = V_GLOBAL;
        gvar_t.var[gvar_t.count].id = (uint32_t) &ntvCode[ntvCount];
        ntvCount += sizeof(int32_t);
        return &gvar_t.var[gvar_t.count++];
    }
    return NULL;
}

func_t * getfn(char * name) {
    int i = 0;
    for (; i < fn_t.count; i++) {
        if (!strcmp(fn_t.func[i].name, name))
            return &fn_t.func[i];
    }
    return NULL;
}

static func_t * appfn(char * name, int address, int args) {
    fn_t.func[fn_t.count].address = address;
    fn_t.func[fn_t.count].args = args;
    strcpy(fn_t.func[fn_t.count].name, name);
    return &fn_t.func[fn_t.count++];
}

static int32_t appbrk() {
    emit(0xe9);
    brks_t.addr = realloc(brks_t.addr, 4 * (brks_t.count + 1));
    brks_t.addr[brks_t.count] = ntvCount;
    emit32(0);
    return brks_t.count++;
}

static int32_t appret() {
    asmexpr();
    emit(0xe9);
    rets_t.addr = realloc(rets_t.addr, 4 * (rets_t.count + 1));
    if (rets_t.addr == NULL) error("no enough memory");
    rets_t.addr[rets_t.count] = ntvCount;
    emit32(0);
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

static var_t * declvar() {
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

static int condstmt() {
    uint32_t end;
    asmexpr();
    emit(0x83); emit(0xf8); emit(0x00);
    emit(0x75); emit(0x05);
    emit(0xe9); end = ntvCount; emit32(0);
    return eval(end, 0);
}

static int loopstmt() {
    uint32_t loopBgn = ntvCount, end, stepBgn[2], stepOn = 0;
    asmexpr();
    if (skip(",")) {
        stepOn = 1;
        stepBgn[0] = tok_t.pos;
        for (; tok_t.tok_t[tok_t.pos].val[0] != ';'; tok_t.pos++);
    }
    emit(0x83); emit(0xf8); emit(0x00);
    emit(0x75); emit(0x05);
    emit(0xe9); end = ntvCount; emit32(0);
    if (skip(":")) expression(0, BLOCK_LOOP);
    else eval(0, BLOCK_LOOP);
    if (stepOn) {
        stepBgn[1] = tok_t.pos;
        tok_t.pos = stepBgn[0];
        if (isassign()) assignment();
        tok_t.pos = stepBgn[1];
    }
    uint32_t n = 0xFFFFFFFF - ntvCount + loopBgn - 4;
    emit(0xe9); emit32(n);
    emit32ins(ntvCount - end - 4, end);
    for (--brks_t.count; brks_t.count >= 0; brks_t.count--)
        emit32ins(ntvCount - brks_t.addr[brks_t.count] - 4, brks_t.addr[brks_t.count]);
    brks_t.count = 0;
    return 0;
}

static int32_t fnstmt() {
    int32_t espBgn, argsc = 0;
    char * funcName = tok_t.tok_t[tok_t.pos++].val;
    nowFunc++; fn_t.inside = IN_FUNC;
    if (skip("(")) {
        do {
            declvar();
            tok_t.pos++;
            argsc++;
        } while (skip(","));
        if (!skip(")"))
            error("%d: expecting ')'", tok_t.tok_t[tok_t.pos].nline);
    }
    appfn(funcName, ntvCount, argsc);
    emit(0x50 + EBP);
    emit(0x89); emit(0xc0 + ESP * 8 + EBP);
    espBgn = ntvCount + 2;
    emit(0x81); emit(0xe8 + ESP); emit32(0);
    int32_t argpos[128], i;
    for (i = 0; i < argsc; i++) {
        emit(0x8b); emit(0x45); emit(0x08 + (argsc - i - 1) * sizeof(int32_t));
        emit(0x89); emit(0x44); emit(0x24);
        argpos[i] = ntvCount; emit(0x00);
    }
    eval(0, BLOCK_FUNC);
    for (--rets_t.count; rets_t.count >= 0; --rets_t.count) {
        emit32ins(ntvCount - rets_t.addr[rets_t.count] - 4, rets_t.addr[rets_t.count]);
    }
    rets_t.count = 0;
    emit(0x81); emit(0xc0 + ESP);
    emit32(sizeof(int32_t) * (varSize[nowFunc] + 6));
    emit(0xc9);
    emit(0xc3);
    emit32ins(sizeof(int32_t) * (varSize[nowFunc] + 6), espBgn);
    for (i = 1; i <= argsc; i++)
        ntvCode[argpos[i - 1]] = 256 - sizeof(int32_t) * i + (((varSize[nowFunc] + 6) * sizeof(int32_t)) - 4);
    return 0;
}

int expression(int pos, int status) {
    int isputs = 0;
    if (skip("$")) {
        if (isassign()) assignment();
    } else if (skip("def"))
        fnstmt();

    else if (fn_t.inside == IN_GLOBAL &&
             strcmp("def", tok_t.tok_t[tok_t.pos+1].val) &&
             strcmp("$", tok_t.tok_t[tok_t.pos+1].val) &&
             strcmp(";", tok_t.tok_t[tok_t.pos+1].val)) {
        fn_t.inside = IN_FUNC;
        nowFunc++;
        appfn("main", ntvCount, 0);
        emit(0x50 + EBP);
        emit(0x89); emit(0xc0 + ESP * 8 + EBP);
        uint32_t espBgn = ntvCount + 2;
        emit(0x81); emit(0xe8 + ESP); emit32(0);
        emit(0x8b); emit(0x75); emit(0x0c);
        eval(0, 0);
        emit(0x81); emit(0xc4);
        emit32(sizeof(int32_t) * (varSize[nowFunc] + 6));
        emit(0xc9);
        emit(0xc3);
        emit32ins(sizeof(int32_t) * (varSize[nowFunc] + 6), espBgn);
        fn_t.inside = IN_GLOBAL;
    } else if (isassign())
        assignment();
    else if ((isputs = skip("puts")) || skip("output")) {
        do {
            int isstring = 0;
            if (skip("\"")) {
                emit(0xb8); getstr(); emit32(0x00);
                isstring = 1;
            } else
                asmexpr();
            emit(0x50 + EAX);
            if (isstring) {
                emit(0xff); emit(0x56); emit(4);
            } else {
                emit(0xff); emit(0x16);
            }
            emit(0x81); emit(0xc0 + ESP); emit32(4);
        } while (skip(","));
        if (isputs) {
            emit(0xff); emit(0x56); emit(8);
        }
    } else if (skip("printf")) {
        if (skip("\"")) {
            emit(0xb8); getstr(); emit32(0x00);
            emit(0x89); emit(0x44); emit(0x24); emit(0x00);
        }
        if (skip(",")) {
            uint32_t a = 4;
            do {
                asmexpr();
                emit(0x89); emit(0x44); emit(0x24); emit(a);
                a += 4;
            } while (skip(","));
        }
        emit(0xff); emit(0x56); emit(12 + 8);
    } else if (skip("for")) {
        assignment();
        if (!skip(","))
            error("%d: expecting ','", tok_t.tok_t[tok_t.pos].nline);
        loopstmt();
    } else if (skip("while"))
        loopstmt();
    else if (skip("return"))
        appret();
    else if (skip("if"))
        condstmt();
    else if (skip("else")) {
        int32_t end;
        emit(0xe9); end = ntvCount; emit32(0);
        emit32ins(ntvCount - pos - 4, pos);
        eval(end, 0);
        return 1;
    } else if (skip("elif")) {
        int32_t endif, end;
        emit(0xe9); endif = ntvCount; emit32(0);
        emit32ins(ntvCount - pos - 4, pos);
        asmexpr();
        emit(0x83); emit(0xf8); emit(0x00);
        emit(0x75); emit(0x05);
        emit(0xe9); end = ntvCount; emit32(0);
        eval(end, 0);
        emit32ins(ntvCount - endif - 4, endif);
        return 1;
    } else if (skip("break"))
        appbrk();
    else if (skip("end")) {
        if (status == 0)
            emit32ins(ntvCount - pos - 4, pos);

        else if (status == BLOCK_FUNC) fn_t.inside = IN_GLOBAL;
        return 1;
    } else if (!skip(";"))  asmexpr();
    return 0;
}

static char * procesc(char * str) {
    char escape[12][3] = {
        "\\a", "\a", "\\r", "\r", "\\f", "\f",
        "\\n", "\n", "\\t", "\t", "\\b", "\b"
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

int32_t parser() {
    tok_t.pos = ntvCount = 0;
    str_t.addr = calloc(0xFF, sizeof(int32_t));
    uint32_t main_address;
    emit(0xe9); main_address = ntvCount; emit32(0);
    eval(0, 0);
    uint32_t addr = getfn("main")->address;
    emit32ins(addr - 5, main_address);
    for (str_t.addr--; str_t.count; str_t.addr--) {
        int32_t i = 0;
        emit32ins((uint32_t) &ntvCode[ntvCount], *str_t.addr);
        procesc(str_t.text[--str_t.count]);
        for (; str_t.text[str_t.count][i]; i++)
            emit(str_t.text[str_t.count][i]);
        emit(0);
    }
    return 1;
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
        v = declvar();
    }
    tok_t.pos++;
    if (v->loctype == V_LOCAL) {
        if (skip("[")) {
            asmexpr();
            emit(0x50 + EAX);
            if (skip("]") && skip("=")) {
                asmexpr();
                emit(0x8b); emit(0x4d);
                emit(256 -
                     (v->type == T_INT ? sizeof(int32_t) :
                      v->type == T_STRING ? sizeof(int32_t *) :
                      v->type == T_DOUBLE ? sizeof(double) : 4) * v->id);
                emit(0x58 + EDX);
                if (v->type == T_INT) {
                    emit(0x89); emit(0x04); emit(0x91);
                } else {
                    emit(0x89); emit(0x04); emit(0x11);
                }
            } else if ((inc = skip("++")) || (dec = skip("--"))) {
            } else
                error("%d: invalid assignment", tok_t.tok_t[tok_t.pos].nline);
        } else {
            if (skip("="))
                asmexpr();

            else if ((inc = skip("++")) || (dec = skip("--"))) {
                emit(0x8b); emit(0x45);
                emit(256 -
                     (v->type == T_INT ? sizeof(int32_t) :
                      v->type == T_STRING ? sizeof(int32_t *) :
                      v->type == T_DOUBLE ? sizeof(double) : 4) * v->id);
                emit(0x50 + EAX);
                if (inc) emit(0x40);
                else if (dec) emit(0x48);
            }
            emit(0x89); emit(0x45);
            emit(256 -
                 (v->type == T_INT ? sizeof(int32_t) :
                  v->type == T_STRING ? sizeof(int32_t *) :
                  v->type == T_DOUBLE ? sizeof(double) : 4) * v->id);
            if (inc || dec) emit(0x58 + EAX);
        }
    } else if (v->loctype == V_GLOBAL) {
        if (declare) {
            if (skip("=")) {
                unsigned * m = (unsigned *) v->id;
                *m = atoi(tok_t.tok_t[tok_t.pos++].val);
            }
        } else {
            if (skip("[")) {
                asmexpr();
                emit(0x50 + EAX);
                if (skip("]") && skip("=")) {
                    asmexpr();
                    emit(0x8b); emit(0x0d); emit32(v->id);
                    emit(0x58 + EDX);
                    if (v->type == T_INT) {
                        emit(0x89); emit(0x04); emit(0x91);
                    } else {
                        emit(0x89); emit(0x04); emit(0x11);
                    }
                } else error("%d: invalid assignment", tok_t.tok_t[tok_t.pos].nline);
            } else if (skip("=")) {
                asmexpr();
                emit(0xa3); emit32(v->id);
            } else if ((inc = skip("++")) || (dec = skip("--"))) {
                emit(0xa1); emit32(v->id);
                emit(0x50 + EAX);
                if (inc) emit(0x40);
                else if (dec) emit(0x48);
                emit(0xa3); emit32(v->id);
            }
            if (inc || dec) emit(0x58 + EAX);
        }
    }
    return 0;
}

static void setxor() {
    w = 1234 + (getpid() ^ 0xFFBA9285);
}

void init() {
    long memsz = 0xFFFF + 1;
    if (posix_memalign((void **) &ntvCode, memsz, memsz))
        perror("posix_memalign");
    if (mprotect(ntvCode, memsz, PROT_READ | PROT_WRITE | PROT_EXEC))
        perror("mprotect");
    tok_t.pos = ntvCount = 0;
    tok_t.size = 0xfff;
    setxor();
    tok_t.tok_t = calloc(sizeof(token_t), tok_t.size);
    brks_t.addr = calloc(sizeof(uint32_t), 1);
    rets_t.addr = calloc(sizeof(uint32_t), 1);
}

static void freeadd() {
    if (mem_t.count > 0) {
        for (--mem_t.count; mem_t.count >= 0; --mem_t.count)
            free((void *)mem_t.addr[mem_t.count]);
        mem_t.count = 0;
    }
}

void dispose() {
    free(ntvCode);
    free(brks_t.addr);
    free(rets_t.addr);
    free(tok_t.tok_t);
    freeadd();
}

int32_t lex(char * code) {
    int32_t codeSize = strlen(code), line = 1;
    int32_t is_crlf = 0;
    int32_t i = 0;
    
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
                error("%d: expected expression '\"'",
                      tok_t.tok_t[tok_t.pos].nline);
            tok_t.pos++;
        } else if (code[i] == '\n' ||
                   (is_crlf = (code[i] == '\r' && code[i + 1] == '\n'))) {
            i += is_crlf;
            strcpy(tok_t.tok_t[tok_t.pos].val, ";");
            tok_t.tok_t[tok_t.pos].nline = line++;
            tok_t.pos++;
        } else {
            strncat(tok_t.tok_t[tok_t.pos].val, &(code[i]), 1);
            if (code[i + 1] == '=' || (code[i] == '+' && code[i + 1] == '+') ||
                (code[i] == '-' && code[i + 1] == '-'))
                strncat(tok_t.tok_t[tok_t.pos].val, &(code[++i]), 1);
            tok_t.tok_t[tok_t.pos].nline = line;
            tok_t.pos++;
        }
    }
    tok_t.tok_t[tok_t.pos].nline = line;
    tok_t.size = tok_t.pos - 1;
    return 0;
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
    mem_t.addr[mem_t.count++] = addr;
}

static int xor128() {
    static uint32_t x = 123456789, y = 362436069, z = 521288629;
    uint32_t t;
    t = x ^ (x << 11);
    x = y; y = z; z = w;
    w = (w ^ (w >> 19)) ^ (t ^ (t >> 8));
    return ((int32_t) w < 0) ? -(int32_t) w : (int32_t) w;
}

static stdfn_t stdfns[] = {
    {"Array", 1, 12},
    {"rand", 0, 16}, {"printf", -1, 20}, {"usleep", 1, 28},
    {"fopen", 2, 32}, {"fprintf", -1, 36}, {"fclose", 1, 40}, {"fgets", 3, 44},
    {"free", 1, 48}, {"freeLocal", 0, 52}, {"malloc", 1, 12}, {"exit", 1, 56},
    {"abort", 0, 60}, {"read", 3, 64}, {"write", 3, 68}, {"close", 1, 72}
};
static void * funcTable[] = {
    put_i32, /*  0 */ put_str, /*  4 */ put_ln, /*   8 */ malloc, /* 12 */
    xor128,  /* 16 */ printf,  /* 20 */ add_mem, /* 24 */ usleep, /* 28 */
    fopen,   /* 32 */ fprintf, /* 36 */ fclose,  /* 40 */ fgets,   /* 44 */
    free,    /* 48 */ freeadd,  /* 52 */exit,    /* 56 */ abort,   /* 60 */
    read,    /* 64 */ write,    /* 68 */close,   /* 72 */
};

static int execute(char * source) {
    init();
    lex(source);
    parser();
    ((int (*)(int *, void **)) ntvCode)(0, funcTable);
    dispose();
    return 0;
}

int main(int argc, char ** argv) {
    char * src;
    if (argc < 2) error("no given filename");
    FILE * fp = fopen(argv[1], "rb");
    size_t ssz = 0;
    if (!fp) {
        perror("fopen");
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

int makestd(char * name) {
    size_t i = 0;
    for (; i < sizeof(stdfns) / sizeof(stdfns[0]); i++) {
        if (!strcmp(stdfns[i].name, name)) {
            if (!strcmp(name, "Array")) {
                asmexpr();
                emit(0xc1); emit(0xe0 + EAX); emit(2);
                emit(0x89); emit(0x04); emit(0x24);
                emit(0xff); emit(0x56); emit(12);
                emit(0x50 + EAX);
                emit(0x89); emit(0x04); emit(0x24);
                emit(0xff); emit(0x56); emit(24);
                emit(0x58 + EAX);
            } else {
                if (stdfns[i].args == -1) {
                    uint32_t a = 0;
                    do {
                        asmexpr();
                        emit(0x89); emit(0x44); emit(0x24); emit(a);
                        a += 4;
                    } while (skip(","));
                } else {
                    int arg = 0;
                    for (; arg < stdfns[i].args; arg++) {
                        asmexpr();
                        emit(0x89); emit(0x44); emit(0x24); emit(arg * 4);
                        skip(",");
                    }
                }
                emit(0xff); emit(0x56); emit(stdfns[i].addr);
            }
            return 1;
        }
    }
    return 0;
}
