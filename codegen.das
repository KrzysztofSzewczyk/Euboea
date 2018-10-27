
#include "dasm/dasm_proto.h"
#include "dasm/dasm_x86.h"
#include <sys/mman.h>
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "euboea.h"

|.arch x86
|.globals L_
|.actionlist euboeaactions

dasm_State * d;
static dasm_State ** Dst = &d;
void * rubilabels[L__MAX];
void * jit_buf;
size_t jit_sz;

int npc;
static int main_address, mainFunc;

extern int make_stdfunc(char *);

struct {
    var_t var[0xFF];
    int count;
    int data[0xFF];
} gblVar;

struct {
    var_t var[0xFF][0xFF];
    int count, size[0xFF];
} locVar;

struct {
    char * text[0xff];
    int * addr;
    int count;
} strings;

struct {
    func_t func[0xff];
    int count, inside, now;
} functions;

typedef struct {
    char *name;
    int args, addr;
} std_function;

static std_function stdfunc[] = {
    {"Array", 1, 12},
    {"rand", 0, 16}, {"printf", -1, 20}, {"usleep", 1, 28},
    {"fprintf", -1, 36}, {"fgets", 3, 44},
    {"free", 1, 48}, {"freeLocal", 0, 52}, {"malloc", 1, 12}, {"exit", 1, 56},
    {"abort", 0, 60}, {"read", 3, 32}, {"write", 3, 40}, {"close", 1, 64}
};

static int32_t isIndex() {
    return !strcmp(tok_t.tok_t[tok_t.pos].val, "[");
}

void jit_init() {
    dasm_init(&d, 1);
    dasm_setupglobal(&d, rubilabels, L__MAX);
    dasm_setup(&d, euboeaactions);
}

void * jit_finalize() {
    dasm_link(&d, &jit_sz);
    jit_buf = mmap(0, jit_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    dasm_encode(&d, jit_buf);
    mprotect(jit_buf, jit_sz, PROT_READ | PROT_WRITE | PROT_EXEC);
    return jit_buf;
}

char * getString() {
    strings.text[ strings.count ] = calloc(sizeof(char), strlen(tok_t.tok_t[tok_t.pos].val) + 1);
    strcpy(strings.text[strings.count], tok_t.tok_t[tok_t.pos++].val);
    return strings.text[strings.count++];
}

var_t * getVar(char *name) {
    int i = 0;
    
    for (; i < locVar.count; i++) {
        if (!strcmp(name, locVar.var[functions.now][i].name))
            return &locVar.var[functions.now][i];
    }
    
    for (i = 0; i < gblVar.count; i++) {
        if (!strcmp(name, gblVar.var[i].name))
            return &gblVar.var[i];
    }
    return NULL;
}

static var_t * appendVar(char *name, int type) {
    if (functions.inside == IN_FUNC) {
        int32_t sz = 1 + ++locVar.size[functions.now];
        strcpy(locVar.var[functions.now][locVar.count].name, name);
        locVar.var[functions.now][locVar.count].type = type;
        locVar.var[functions.now][locVar.count].id = sz;
        locVar.var[functions.now][locVar.count].loctype = V_LOCAL;
        return &locVar.var[functions.now][locVar.count++];
    } else if (functions.inside == IN_GLOBAL) {
        strcpy(gblVar.var[gblVar.count].name, name);
        gblVar.var[gblVar.count].type = type;
        gblVar.var[gblVar.count].loctype = V_GLOBAL;
        gblVar.var[gblVar.count].id = (int)&gblVar.data[gblVar.count];
        return &gblVar.var[gblVar.count++];
    }
    return NULL;
}

func_t * getFunc(char * name) {
    int i = 0;
    
    for (; i < functions.count; i++) {
        if (!strcmp(functions.func[i].name, name))
            return &functions.func[i];
    }
    return NULL;
}

static func_t * appendFunc(char *name, int address, int espBgn, int args) {
    functions.func[functions.count].address = address;
    functions.func[functions.count].espBgn = espBgn;
    functions.func[functions.count].args = args;
    strcpy(functions.func[functions.count].name, name);
    return &functions.func[functions.count++];
}

static int32_t make_break() {
    uint32_t lbl = npc++;
    dasm_growpc(&d, npc);
    | jmp =>lbl
    brks_t.addr = realloc(brks_t.addr, 4 * (brks_t.count + 1));
    brks_t.addr[brks_t.count] = lbl;
    return brks_t.count++;
}

static int32_t make_return() {
    compExpr();

    int lbl = npc++;
    dasm_growpc(&d, npc);

    | jmp =>lbl

    rets_t.addr = realloc(rets_t.addr, 4 * (rets_t.count + 1));
    if (rets_t.addr == NULL) error("out of memory");
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

int32_t error(char *errs, ...) {
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

static var_t *declarevar_t() {
    int32_t npos = tok_t.pos;
    if (isalpha(tok_t.tok_t[tok_t.pos].val[0])) {
        tok_t.pos++;
        if (skip(":")) {
            if (skip("int")) {
                --tok_t.pos;
                return appendVar(tok_t.tok_t[npos].val, T_INT);
            }
            if (skip("string")) {
                --tok_t.pos;
                return appendVar(tok_t.tok_t[npos].val, T_STRING);
            }
            if (skip("double")) {
                --tok_t.pos;
                return appendVar(tok_t.tok_t[npos].val, T_DOUBLE);
            }
        } else {
            --tok_t.pos;
            return appendVar(tok_t.tok_t[npos].val, T_INT);
        }
    } else error("%d: can't declare variable", tok_t.tok_t[tok_t.pos].nline);
    return NULL;
}

static int ifStmt() {
    compExpr();
    uint32_t end = npc++;
    dasm_growpc(&d, npc);
    | test eax, eax
    | jnz >1
    | jmp =>end
    |1:
    return eval(end, 0);
}

static int whileStmt() {
    uint32_t loopBgn = npc++;
    uint32_t stepBgn[2], stepOn = 0;
    uint32_t end;
    dasm_growpc(&d, npc);
    |=>loopBgn:
    compExpr();
    if (skip(",")) {
        stepOn = 1;
        stepBgn[0] = tok_t.pos;
        for (; tok_t.tok_t[tok_t.pos].val[0] != ';'; tok_t.pos++);
    }
    end = npc++;
    dasm_growpc(&d, npc);
    | test eax, eax
    | jnz >1
    | jmp =>end
    |1:
    if (skip(":")) expression(0, BLOCK_LOOP);
    else eval(0, BLOCK_LOOP);
    if (stepOn) {
        stepBgn[1] = tok_t.pos;
        tok_t.pos = stepBgn[0];
        if (isassign()) assignment();
        tok_t.pos = stepBgn[1];
    }
    | jmp =>loopBgn
    |=>end:
    for (--brks_t.count; brks_t.count >= 0; brks_t.count--)
        |=>brks_t.addr[brks_t.count]:
    brks_t.count = 0;
    return 0;
}

static int32_t functionStmt() {
    int32_t argsc = 0;
    int func_addr, func_esp, i;
    char *funcName = tok_t.tok_t[tok_t.pos++].val;
    functions.now++; functions.inside = IN_FUNC;
    if (skip("(")) {
        do {
            declarevar_t();
            tok_t.pos++;
            argsc++;
        } while(skip(","));
        if (!skip(")"))
            error("%d: expecting ')'", tok_t.tok_t[tok_t.pos].nline);
    }
    func_addr = npc++;
    dasm_growpc(&d, npc);
    func_esp = npc++;
    dasm_growpc(&d, npc);
    appendFunc(funcName, func_addr, func_esp, argsc);
    |=>func_addr:
    | push ebp
    | mov ebp, esp
    | sub esp, 0x80000000
    |=>func_esp:
    for(i = 0; i < argsc; i++) {
        | mov eax, [ebp + ((argsc - i - 1) * sizeof(int32_t) + 8)]
        | mov [ebp - (i + 2)*4], eax
    }
    eval(0, BLOCK_FUNC);
    for (--rets_t.count; rets_t.count >= 0; --rets_t.count) {
        |=>rets_t.addr[rets_t.count]:
    }
    rets_t.count = 0;
    | leave
    | ret
    return 0;
}

int expression(int pos, int status) {
    int isputs = 0;
    int main_esp;
    if (skip("$")) {
        if (isassign()) assignment();
    } else if (skip("def")) {
        functionStmt();
    } else if (functions.inside == IN_GLOBAL && strcmp("def", tok_t.tok_t[tok_t.pos+1].val) && strcmp("$", tok_t.tok_t[tok_t.pos+1].val) && (tok_t.pos+1 == tok_t.size || strcmp(";", tok_t.tok_t[tok_t.pos+1].val))) {
        functions.inside = IN_FUNC;
        mainFunc = ++functions.now;
        main_esp = npc++;
        dasm_growpc(&d, npc);
        appendFunc("main", main_address, main_esp, 0);
        |=>main_address:
        | push ebp
        | mov ebp, esp
        | sub esp, 0x80000000
        |=>main_esp:
        | mov esi, [ebp + 12]
        eval(0, 0);
        | leave
        | ret
        functions.inside = IN_GLOBAL;
    } else if (isassign()) {
        assignment();
    } else if ((isputs = skip("puts"))) {
        do {
            int isstring = 0;
            if (skip("\"")) {
                | mov eax, getString()
                isstring = 1;
            } else {
                compExpr();
            }
            | push eax
            if (isstring) {
                | call dword [esi + 4]
            } else {
                | call dword [esi]
            }
            | add esp, 4
        } while (skip(","));
        | call dword [esi + 0x8]
    } else if(skip("printf")) {
        if (skip("\"")) {
            | mov eax, getString()
            | mov [esp], eax
        }
        if (skip(",")) {
            uint32_t a = 4;
            do {
                compExpr();
                | mov [esp + a], eax
                a += 4;
            } while(skip(","));
        }
        | call dword [esi + 0x14]
    } else if (skip("for")) {
        assignment();
        if (!skip(","))
            error("%d: expecting ','", tok_t.tok_t[tok_t.pos].nline);
        whileStmt();
    } else if (skip("while")) {
        whileStmt();
    } else if(skip("return")) {
        make_return();
    } else if(skip("if")) {
        ifStmt();
    } else if(skip("else")) {
        int32_t end = npc++;
        dasm_growpc(&d, npc);
        | jmp =>end
        |=>pos:
        eval(end, 0);
        return 1;
    } else if (skip("elif")) {
        int32_t endif = npc++;
        uint32_t end;
        dasm_growpc(&d, npc);
        | jmp =>endif
        |=>pos:
        compExpr();
        end = npc++;
        dasm_growpc(&d, npc);
        | test eax, eax
        | jnz >1
        | jmp =>end
        |1:
        eval(end, 0);
        |=>endif:
        return 1;
    } else if (skip("break")) {
        make_break();
    } else if (skip("end")) {
        if (status == 0) {
            |=>pos:
        } else if (status == BLOCK_FUNC) functions.inside = IN_GLOBAL;
        return 1;
    } else if (!skip(";")) {
        compExpr();
    }

    return 0;
}

static char * replaceEscape(char *str) {
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

int (*parser())(int *, void **) {
    int i = 0;
    uint8_t * buf;
    jit_init();
    tok_t.pos = 0;
    strings.addr = calloc(0xFF, sizeof(int32_t));
    main_address = npc++;
    dasm_growpc(&d, npc);
    |->START:
    | jmp =>main_address
    eval(0, 0);
    for (; i < strings.count; ++i)
        replaceEscape(strings.text[i]);
    buf = (uint8_t*)jit_finalize();
    for (i = 0; i < functions.count; i++)
        *(int*)(buf + dasm_getpclabel(&d, functions.func[i].espBgn) - 4) = (locVar.size[i+1] + 6)*4;
    dasm_free(&d);
    return ((int (*)(int *, void **))rubilabels[L_START]);
}

int32_t isassign() {
    char *val = tok_t.tok_t[tok_t.pos + 1].val;
    if (!strcmp(val, "=") || !strcmp(val, "++") || !strcmp(val, "--")) return 1;
    if (!strcmp(val, "[")) {
        int32_t i = tok_t.pos + 2, t = 1;
        while (t) {
            val = tok_t.tok_t[i].val;
            if (!strcmp(val, "[")) t++; if (!strcmp(val, "]")) t--;
            if (!strcmp(val, ";"))
                error("%d: invalid expression", tok_t.tok_t[tok_t.pos].nline);
            i++;
        }
        if (!strcmp(tok_t.tok_t[i].val, "=")) return 1;
    } else if (!strcmp(val, ":") && !strcmp(tok_t.tok_t[tok_t.pos + 3].val, "=")) {
        return 1;
    }
    return 0;
}

int32_t assignment() {
    var_t *v = getVar(tok_t.tok_t[tok_t.pos].val);
    int32_t inc = 0, dec = 0, declare = 0;
    int siz;
    
    if (v == NULL) {
        declare++;
        v = declarevar_t();
    }
    tok_t.pos++;

    siz = (v->type == T_INT ? sizeof(int32_t) : v->type == T_STRING ? sizeof(int32_t *) : v->type == T_DOUBLE ? sizeof(double) : 4);

    if (v->loctype == V_LOCAL) {
        if (skip("[")) {
            compExpr();
            | push eax
            if (skip("]") && skip("=")) {
                compExpr();
                | mov ecx, [ebp - siz*v->id]
                | pop edx
                if (v->type == T_INT) {
                    | mov [ecx+edx*4], eax
                } else {
                    | mov [ecx+edx], eax
                }
            } else if ((inc = skip("++")) || (dec = skip("--"))) {
            } else 
                error("%d: invalid assignment", tok_t.tok_t[tok_t.pos].nline);
        } else {
            if(skip("=")) {
                compExpr();
            } else if((inc = skip("++")) || (dec = skip("--"))) {
                | mov eax, [ebp - siz*v->id]
                | push eax
                if (inc)
                    | inc eax
                else if(dec)
                    | dec eax
            }
            | mov [ebp - siz*v->id], eax
            if (inc || dec)
                | pop eax
        }
    } else if (v->loctype == V_GLOBAL) {
        if (declare) {
            if (skip("=")) {
                unsigned *m = (unsigned *) v->id;
                *m = atoi(tok_t.tok_t[tok_t.pos++].val);
            }
        } else {
            if (skip("[")) {
                compExpr();
                | push eax
                if(skip("]") && skip("=")) {
                    compExpr();
                    | mov ecx, [v->id]
                    | pop edx
                    if (v->type == T_INT) {
                        | mov [ecx + edx*4], eax
                    } else {
                        | mov [edx+edx], eax
                    }
                } else error("%d: invalid assignment", tok_t.tok_t[tok_t.pos].nline);
            } else if (skip("=")) {
                compExpr();
                | mov [v->id], eax
            } else if ((inc = skip("++")) || (dec = skip("--"))) {
                | mov eax, [v->id]
                | push eax
                if (inc)
                    | inc eax
                else if (dec)
                    | dec eax
                | mov [v->id], eax
            }
            if (inc || dec)
                | pop eax
        }
    }
    return 0;
}

static void primExpr() {
    if (isdigit(tok_t.tok_t[tok_t.pos].val[0])) {
        | mov eax, atoi(tok_t.tok_t[tok_t.pos++].val)
    } else if (skip("'")) {
        | mov eax, tok_t.tok_t[tok_t.pos++].val[0]
        skip("'");
    } else if (skip("\"")) {
        | mov eax, getString()
    } else if (isalpha(tok_t.tok_t[tok_t.pos].val[0])) {
        char *name = tok_t.tok_t[tok_t.pos].val;
        var_t *v;
        if (isassign()) assignment();
        else {
            tok_t.pos++;
            if (skip("[")) {
                if ((v = getVar(name)) == NULL)
                    error("%d: '%s' was not declared", tok_t.tok_t[tok_t.pos].nline, name);
                compExpr();
                | mov ecx, eax
                if (v->loctype == V_LOCAL) {
                    | mov edx, [ebp - v->id*4]
                } else if (v->loctype == V_GLOBAL) {
                    | mov edx, [v->id]
                }
                if (v->type == T_INT) {
                    | mov eax, [edx + ecx * 4]
                } else {
                    | movzx eax, byte [edx + ecx]
                }
                if (!skip("]"))
                    error("%d: expected expression ']'",
                          tok_t.tok_t[tok_t.pos].nline);
            } else if (skip("(")) {
                if (!make_stdfunc(name)) {
                    func_t *function = getFunc(name);
                    char *val = tok_t.tok_t[tok_t.pos].val;
                    if (isalpha(val[0]) || isdigit(val[0]) || !strcmp(val, "\"") || !strcmp(val, "(")) {
                        int i = 0;
                        for (; i < function->args; i++) {
                            compExpr();
                            | push eax
                            skip(",");
                        }
                    }
                    | call =>function->address
                    | add esp, function->args * sizeof(int32_t)
                }
                if (!skip(")"))
                    error("func: %d: expected expression ')'", tok_t.tok_t[tok_t.pos].nline);
            } else {
                if ((v = getVar(name)) == NULL)
                    error("var: %d: '%s' was not declared",
                          tok_t.tok_t[tok_t.pos].nline, name);
                if (v->loctype == V_LOCAL) {
                    | mov eax, [ebp - v->id*4]
                } else if (v->loctype == V_GLOBAL) {
                    | mov eax, [v->id]
                }
            }
        }
    } else if (skip("(")) {
        if (isassign()) assignment(); else compExpr();
        if (!skip(")")) error("%d: expected expression ')'", tok_t.tok_t[tok_t.pos].nline);
    }
    while (isIndex()) {
        | mov ecx, eax
        skip("[");
        compExpr();
        skip("]");
        | mov eax, [ecx + eax*4]
    }
}

static void mulDivExpr() {
    int32_t mul = 0, div = 0;
    primExpr();
    while ((mul = skip("*")) || (div = skip("/")) || skip("%")) {
        | push eax
        primExpr();
        | mov ebx, eax
        | pop eax
        if (mul) {
            | imul ebx
        } else if (div) {
            | xor edx, edx
            | idiv ebx
        } else {
            | xor edx, edx
            | idiv ebx
            | mov eax, edx
        }
    }
}

static void addSubExpr() {
    int32_t add;
    mulDivExpr();
    while ((add = skip("+")) || skip("-")) {
        | push eax
        mulDivExpr();
        | mov ebx, eax
        | pop eax
        if (add) {
            | add eax, ebx
        } else {
            | sub eax, ebx
        }
    }
}

static void logicExpr() {
    int32_t lt = 0, gt = 0, ne = 0, eql = 0, fle = 0;
    addSubExpr();
    if ((lt = skip("<")) || (gt = skip(">")) || (ne = skip("!=")) ||
        (eql = skip("==")) || (fle = skip("<=")) || skip(">=")) {
        | push eax
        addSubExpr();
        | mov ebx, eax
        | pop eax
        | cmp eax, ebx
        if (lt)
            | setl al
        else if (gt)
            | setg al
        else if (ne)
            | setne al
        else if (eql)
            | sete al
        else if (fle)
            | setle al
        else
            | setge al
        | movzx eax, al
    }
}

void compExpr() {
    int and = 0, or = 0;
    logicExpr();
    while ((and = skip("and") || skip("&")) ||
           (or = skip("or") || skip("|")) || (skip("xor") || skip("^"))) {
        | push eax
        logicExpr();
        | mov ebx, eax
        | pop eax
        if (and)
            | and eax, ebx
        else if (or)
            | or eax, ebx
        else
            | xor eax, ebx
    }
}

int make_stdfunc(char *name) {
    size_t i = 0;
    
    for (; i < sizeof(stdfunc) / sizeof(stdfunc[0]); i++) {
        if (!strcmp(stdfunc[i].name, name)) {
            if(!strcmp(name, "Array")) {
                compExpr();
                | shl eax, 2
                | mov [esp], eax
                | call dword [esi + 12]
                | push eax
                | mov [esp], eax
                | call dword [esi + 24]
                | pop eax
            } else {
                if (stdfunc[i].args == -1) {
                    uint32_t a = 0;
                    do {
                        compExpr();
                        | mov [esp + a], eax
                        a += 4;
                    } while(skip(","));
                } else {
                    int arg = 0;
                    for(; arg < stdfunc[i].args; arg++) {
                        compExpr();
                        | mov [esp + arg*4], eax
                        skip(",");
                    }
                }
                | call dword [esi + stdfunc[i].addr]
            }
            return 1;
        }
    }
    return 0;
}

