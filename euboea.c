
/* EUBOEA programming language interpreter */
/* Author: Kamila Szewczyk, License: MIT */

#define _BSD_SOURCE
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
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
    #define MAP_ANONYMOUS MAP_ANON
#endif
#include <stddef.h>
#include <stdarg.h>
#ifndef Dst_DECL
#define Dst_DECL	dasm_State **Dst
#endif
#ifndef Dst_REF
#define Dst_REF		(*Dst)
#endif
#ifndef DASM_FDEF
#define DASM_FDEF	extern
#endif
#ifndef DASM_M_GROW
#define DASM_M_GROW(ctx, t, p, sz, need) \
  do { \
    size_t _sz = (sz), _need = (need); \
    if (_sz < _need) { \
      if (_sz < 16) _sz = 16; \
      while (_sz < _need) _sz += _sz; \
      (p) = (t *)realloc((p), _sz); \
      if ((p) == NULL) exit(1); \
      (sz) = _sz; \
    } \
  } while(0)
#endif
#ifndef DASM_M_FREE
#define DASM_M_FREE(ctx, p, sz)	free(p)
#endif
/* Maximum number of section buffer positions for a single dasm_put() call. */
#define DASM_MAXSECPOS		25

/* DynASM encoder status codes. Action list offset or number are or'ed in. */
#define DASM_S_OK		0x00000000
#define DASM_S_NOMEM		0x01000000
#define DASM_S_PHASE		0x02000000
#define DASM_S_MATCH_SEC	0x03000000
#define DASM_S_RANGE_I		0x11000000
#define DASM_S_RANGE_SEC	0x12000000
#define DASM_S_RANGE_LG		0x13000000
#define DASM_S_RANGE_PC		0x14000000
#define DASM_S_RANGE_VREG	0x15000000
#define DASM_S_UNDEF_L		0x21000000
#define DASM_S_UNDEF_PC		0x22000000

/* Macros to convert positions (8 bit section + 24 bit index). */
#define DASM_POS2IDX(pos)	((pos)&0x00ffffff)
#define DASM_POS2BIAS(pos)	((pos)&0xff000000)
#define DASM_SEC2POS(sec)	((sec)<<24)
#define DASM_POS2SEC(pos)	((pos)>>24)
#define DASM_POS2PTR(D, pos)	(D->sections[DASM_POS2SEC(pos)].rbuf + (pos))
typedef struct dasm_State dasm_State;
DASM_FDEF void dasm_init(Dst_DECL, int maxsection);
DASM_FDEF void dasm_free(Dst_DECL);
DASM_FDEF void dasm_setupglobal(Dst_DECL, void **gl, unsigned int maxgl);
DASM_FDEF void dasm_growpc(Dst_DECL, unsigned int maxpc);
DASM_FDEF void dasm_setup(Dst_DECL, const void *actionlist);
DASM_FDEF void dasm_put(Dst_DECL, int start, ...);
DASM_FDEF int dasm_link(Dst_DECL, size_t *szp);
DASM_FDEF int dasm_encode(Dst_DECL, void *buffer);
DASM_FDEF int dasm_getpclabel(Dst_DECL, unsigned int pc);
#ifdef DASM_CHECKS
DASM_FDEF int dasm_checkstep(Dst_DECL, int secmatch);
#else
#define dasm_checkstep(a, b)	0
#endif


#define DASM_ARCH		"x86"

#ifndef DASM_EXTERN
    #define DASM_EXTERN(a,b,c,d)	0
#endif

/* Action definitions. DASM_STOP must be 255. */
enum {
    DASM_DISP = 233,
    DASM_IMM_S, DASM_IMM_B, DASM_IMM_W, DASM_IMM_D, DASM_IMM_WB, DASM_IMM_DB,
    DASM_VREG, DASM_SPACE, DASM_SETLABEL, DASM_REL_A, DASM_REL_LG, DASM_REL_PC,
    DASM_IMM_LG, DASM_IMM_PC, DASM_LABEL_LG, DASM_LABEL_PC, DASM_ALIGN,
    DASM_EXTERN, DASM_ESC, DASM_MARK, DASM_SECTION, DASM_STOP
};

/* Action list type. */
typedef const unsigned char * dasm_ActList;

/* Per-section structure. */
typedef struct dasm_Section {
    int * rbuf;		/* Biased buffer pointer (negative section bias). */
    int * buf;		/* True buffer pointer. */
    size_t bsize;		/* Buffer size in bytes. */
    int pos;		/* Biased buffer position. */
    int epos;		/* End of biased buffer position - max single put. */
    int ofs;		/* Byte offset into section. */
} dasm_Section;

/* Core structure holding the DynASM encoding state. */
struct dasm_State {
    size_t psize;			/* Allocated size of this structure. */
    dasm_ActList actionlist;	/* Current actionlist pointer. */
    int * lglabels;		/* Local/global chain/pos ptrs. */
    size_t lgsize;
    int * pclabels;		/* PC label chains/pos ptrs. */
    size_t pcsize;
    void ** globals;		/* Array of globals (bias -10). */
    dasm_Section * section;	/* Pointer to active section. */
    size_t codesize;		/* Total size of all code sections. */
    int maxsection;		/* 0 <= sectionidx < maxsection. */
    int status;			/* Status code. */
    dasm_Section sections[1];	/* All sections. Alloc-extended. */
};

/* The size of the core structure depends on the max. number of sections. */
#define DASM_PSZ(ms)	(sizeof(dasm_State)+(ms-1)*sizeof(dasm_Section))


/* Initialize DynASM state. */
void dasm_init(Dst_DECL, int maxsection) {
    dasm_State * D;
    size_t psz = 0;
    int i;
    Dst_REF = NULL;
    DASM_M_GROW(Dst, struct dasm_State, Dst_REF, psz, DASM_PSZ(maxsection));
    D = Dst_REF;
    D->psize = psz;
    D->lglabels = NULL;
    D->lgsize = 0;
    D->pclabels = NULL;
    D->pcsize = 0;
    D->globals = NULL;
    D->maxsection = maxsection;
    for (i = 0; i < maxsection; i++) {
        D->sections[i].buf = NULL;  /* Need this for pass3. */
        D->sections[i].rbuf = D->sections[i].buf - DASM_SEC2POS(i);
        D->sections[i].bsize = 0;
        D->sections[i].epos = 0;  /* Wrong, but is recalculated after resize. */
    }
}

/* Free DynASM state. */
void dasm_free(Dst_DECL) {
    dasm_State * D = Dst_REF;
    int i;
    for (i = 0; i < D->maxsection; i++)
        if (D->sections[i].buf)
            DASM_M_FREE(Dst, D->sections[i].buf, D->sections[i].bsize);
    if (D->pclabels) DASM_M_FREE(Dst, D->pclabels, D->pcsize);
    if (D->lglabels) DASM_M_FREE(Dst, D->lglabels, D->lgsize);
    DASM_M_FREE(Dst, D, D->psize);
}

/* Setup global label array. Must be called before dasm_setup(). */
void dasm_setupglobal(Dst_DECL, void ** gl, unsigned int maxgl) {
    dasm_State * D = Dst_REF;
    D->globals = gl - 10;  /* Negative bias to compensate for locals. */
    DASM_M_GROW(Dst, int, D->lglabels, D->lgsize, (10+maxgl)*sizeof(int));
}

/* Grow PC label array. Can be called after dasm_setup(), too. */
void dasm_growpc(Dst_DECL, unsigned int maxpc) {
    dasm_State * D = Dst_REF;
    size_t osz = D->pcsize;
    DASM_M_GROW(Dst, int, D->pclabels, D->pcsize, maxpc*sizeof(int));
    memset((void *)(((unsigned char *)D->pclabels)+osz), 0, D->pcsize-osz);
}

/* Setup encoder. */
void dasm_setup(Dst_DECL, const void * actionlist) {
    dasm_State * D = Dst_REF;
    int i;
    D->actionlist = (dasm_ActList)actionlist;
    D->status = DASM_S_OK;
    D->section = &D->sections[0];
    memset((void *)D->lglabels, 0, D->lgsize);
    if (D->pclabels) memset((void *)D->pclabels, 0, D->pcsize);
    for (i = 0; i < D->maxsection; i++) {
        D->sections[i].pos = DASM_SEC2POS(i);
        D->sections[i].ofs = 0;
    }
}


#ifdef DASM_CHECKS
#define CK(x, st) \
  do { if (!(x)) { \
    D->status = DASM_S_##st|(int)(p-D->actionlist-1); return; } } while (0)
#define CKPL(kind, st) \
  do { if ((size_t)((char *)pl-(char *)D->kind##labels) >= D->kind##size) { \
    D->status=DASM_S_RANGE_##st|(int)(p-D->actionlist-1); return; } } while (0)
#else
#define CK(x, st)	((void)0)
#define CKPL(kind, st)	((void)0)
#endif

/* Pass 1: Store actions and args, link branches/labels, estimate offsets. */
void dasm_put(Dst_DECL, int start, ...) {
    va_list ap;
    dasm_State * D = Dst_REF;
    dasm_ActList p = D->actionlist + start;
    dasm_Section * sec = D->section;
    int pos = sec->pos, ofs = sec->ofs, mrm = 4;
    int * b;
    if (pos >= sec->epos) {
        DASM_M_GROW(Dst, int, sec->buf, sec->bsize,
                    sec->bsize + 2*DASM_MAXSECPOS*sizeof(int));
        sec->rbuf = sec->buf - DASM_POS2BIAS(pos);
        sec->epos = (int)sec->bsize/sizeof(int) - DASM_MAXSECPOS+DASM_POS2BIAS(pos);
    }
    b = sec->rbuf;
    b[pos++] = start;
    va_start(ap, start);
    while (1) {
        int action = *p++;
        if (action < DASM_DISP)
            ofs++;

        else if (action <= DASM_REL_A) {
            int n = va_arg(ap, int);
            b[pos++] = n;
            switch (action) {
                case DASM_DISP:
                    if (n == 0) { if ((mrm&7) == 4) mrm = p[-2]; if ((mrm&7) != 5) break; }
                case DASM_IMM_DB: if (((n+128)&-256) == 0) goto ob;
                case DASM_REL_A: /* Assumes ptrdiff_t is int. !x64 */
                case DASM_IMM_D: ofs += 4; break;
                case DASM_IMM_S: CK(((n+128)&-256) == 0, RANGE_I); goto ob;
            case DASM_IMM_B: CK((n&-256) == 0, RANGE_I); ob: ofs++; break;
                case DASM_VREG: CK((n&-8) == 0 && (n != 4 || (*p&1) == 0), RANGE_VREG);
            }
            mrm = 4;
        } else {
            int * pl, n;
            switch (action) {
                case DASM_REL_LG:
                case DASM_IMM_LG:
                    n = *p++; pl = D->lglabels + n;
                    /* Bkwd rel or global. */
                    if (n <= 246) { CK(n>=10||*pl<0, RANGE_LG); CKPL(lg, LG); goto putrel; }
                    pl -= 246; n = *pl;
                    if (n < 0) n = 0;  /* Start new chain for fwd rel if label exists. */
                    goto linkrel;
                case DASM_REL_PC:
                case DASM_IMM_PC: pl = D->pclabels + va_arg(ap, int); CKPL(pc, PC);
                putrel:
                    n = *pl;
                    if (n < 0)    /* Label exists. Get label pos and store it. */
                        b[pos] = -n;

                    else {
                    linkrel:
                        b[pos] = n;  /* Else link to rel chain, anchored at label. */
                        *pl = pos;
                    }
                    pos++;
                    ofs += 4;  /* Maximum offset needed. */
                    if (action == DASM_REL_LG || action == DASM_REL_PC)
                        b[pos++] = ofs;  /* Store pass1 offset estimate. */
                    break;
                case DASM_LABEL_LG: pl = D->lglabels + *p++; CKPL(lg, LG); goto putlabel;
                case DASM_LABEL_PC: pl = D->pclabels + va_arg(ap, int); CKPL(pc, PC);
                putlabel:
                    n = *pl;  /* n > 0: Collapse rel chain and replace with label pos. */
                    while (n > 0) { int * pb = DASM_POS2PTR(D, n); n = *pb; *pb = pos; }
                    *pl = -pos;  /* Label exists now. */
                    b[pos++] = ofs;  /* Store pass1 offset estimate. */
                    break;
                case DASM_ALIGN:
                case DASM_ESC: p++; ofs++; break;
                case DASM_MARK: mrm = p[-2]; break;
                case DASM_SECTION:
                case DASM_STOP: goto stop;
            }
        }
    }
stop:
    va_end(ap);
    sec->pos = pos;
    sec->ofs = ofs;
}
#undef CK

/* Pass 2: Link sections, shrink branches/aligns, fix label offsets. */
int dasm_link(Dst_DECL, size_t * szp) {
    dasm_State * D = Dst_REF;
    int secnum;
    int ofs = 0;
    #ifdef DASM_CHECKS
    *szp = 0;
    if (D->status != DASM_S_OK) return D->status;
    {
        int pc;
        for (pc = 0; pc*sizeof(int) < D->pcsize; pc++)
            if (D->pclabels[pc] > 0) return DASM_S_UNDEF_PC|pc;
    }
    #endif
    {
        /* Handle globals not defined in this translation unit. */
        int idx;
        for (idx = 10; idx*sizeof(int) < D->lgsize; idx++) {
            int n = D->lglabels[idx];
            /* Undefined label: Collapse rel chain and replace with marker (< 0). */
            while (n > 0) { int * pb = DASM_POS2PTR(D, n); n = *pb; *pb = -idx; }
        }
    }
    /* Combine all code sections. No support for data sections (yet). */
    for (secnum = 0; secnum < D->maxsection; secnum++) {
        dasm_Section * sec = D->sections + secnum;
        int * b = sec->rbuf;
        int pos = DASM_SEC2POS(secnum);
        int lastpos = sec->pos;
        while (pos != lastpos) {
            dasm_ActList p = D->actionlist + b[pos++];
            while (1) {
                int op, action = *p++;
                switch (action) {
                    case DASM_REL_LG: p++; op = p[-3]; goto rel_pc;
                case DASM_REL_PC: op = p[-2]; rel_pc: {
                            int shrink = op == 0xe9 ? 3 : ((op&0xf0) == 0x80 ? 4 : 0);
                            if (shrink) {  /* Shrinkable branch opcode? */
                                int lofs, lpos = b[pos];
                                if (lpos < 0) goto noshrink;  /* Ext global? */
                                lofs = *DASM_POS2PTR(D, lpos);
                                if (lpos > pos) {  /* Fwd label: add cumulative section offsets. */
                                    int i;
                                    for (i = secnum; i < DASM_POS2SEC(lpos); i++)
                                        lofs += D->sections[i].ofs;
                                } else {
                                    lofs -= ofs;  /* Bkwd label: unfix offset. */
                                }
                                lofs -= b[pos+1];  /* Short branch ok? */
                                if (lofs >= -128-shrink && lofs <= 127) ofs -= shrink;  /* Yes. */
                            else  noshrink: shrink = 0;    /* No, cannot shrink op. */
                            }
                            b[pos+1] = shrink;
                            pos += 2;
                            break;
                        }
                    case DASM_SPACE: case DASM_IMM_LG: case DASM_VREG: p++;
                    case DASM_DISP: case DASM_IMM_S: case DASM_IMM_B: case DASM_IMM_W:
                    case DASM_IMM_D: case DASM_IMM_WB: case DASM_IMM_DB:
                    case DASM_SETLABEL: case DASM_REL_A: case DASM_IMM_PC: pos++; break;
                    case DASM_LABEL_LG: p++;
                    case DASM_LABEL_PC: b[pos++] += ofs; break; /* Fix label offset. */
                    case DASM_ALIGN: ofs -= (b[pos++]+ofs)&*p++; break; /* Adjust ofs. */
                    case DASM_EXTERN: p += 2; break;
                    case DASM_ESC: p++; break;
                    case DASM_MARK: break;
                    case DASM_SECTION: case DASM_STOP: goto stop;
                }
            }
        stop: (void)0;
        }
        ofs += sec->ofs;  /* Next section starts right after current section. */
    }
    D->codesize = ofs;  /* Total size of all code sections */
    *szp = ofs;
    return DASM_S_OK;
}

#define dasmb(x)	*cp++ = (unsigned char)(x)
#ifndef DASM_ALIGNED_WRITES
#define dasmw(x) \
  do { *((unsigned short *)cp) = (unsigned short)(x); cp+=2; } while (0)
#define dasmd(x) \
  do { *((unsigned int *)cp) = (unsigned int)(x); cp+=4; } while (0)
#else
#define dasmw(x)	do { dasmb(x); dasmb((x)>>8); } while (0)
#define dasmd(x)	do { dasmw(x); dasmw((x)>>16); } while (0)
#endif

/* Pass 3: Encode sections. */
int dasm_encode(Dst_DECL, void * buffer) {
    dasm_State * D = Dst_REF;
    unsigned char * base = (unsigned char *)buffer;
    unsigned char * cp = base;
    int secnum;
    /* Encode all code sections. No support for data sections (yet). */
    for (secnum = 0; secnum < D->maxsection; secnum++) {
        dasm_Section * sec = D->sections + secnum;
        int * b = sec->buf;
        int * endb = sec->rbuf + sec->pos;
        while (b != endb) {
            dasm_ActList p = D->actionlist + *b++;
            unsigned char * mark = NULL;
            while (1) {
                int action = *p++;
                int n = (action >= DASM_DISP && action <= DASM_ALIGN) ? *b++ : 0;
                switch (action) {
                    case DASM_DISP: if (!mark) mark = cp; {
                            unsigned char * mm = mark;
                            if (*p != DASM_IMM_DB && *p != DASM_IMM_WB) mark = NULL;
                            if (n == 0) {
                                int mrm = mm[-1]&7;
                                if (mrm == 4) mrm = mm[0]&7;
                                if (mrm != 5) { mm[-1] -= 0x80; break; }
                            }
                            if (((n+128) & -256) != 0) goto wd;
                            else mm[-1] -= 0x40;
                        }
                case DASM_IMM_S: case DASM_IMM_B: wb: dasmb(n); break;
                    case DASM_IMM_DB: if (((n+128)&-256) == 0) {
                        db:
                            if (!mark) mark = cp; mark[-2] += 2; mark = NULL; goto wb;
                        } else mark = NULL;
                case DASM_IMM_D: wd: dasmd(n); break;
                    case DASM_IMM_WB: if (((n+128)&-256) == 0) goto db;
                        else mark = NULL;
                    case DASM_IMM_W: dasmw(n); break;
                    case DASM_VREG: { int t = *p++; if (t >= 2) n<<=3; cp[-1] |= n; break; }
                    case DASM_REL_LG: p++;
                        if (n >= 0) goto rel_pc;
                        b++; n = (int)(ptrdiff_t)D->globals[-n];
                case DASM_REL_A: rel_a: n -= (int)(ptrdiff_t)(cp+4); goto wd; /* !x64 */
                case DASM_REL_PC: rel_pc: {
                            int shrink = *b++;
                            int * pb = DASM_POS2PTR(D, n);
                            if (*pb < 0) { n = pb[1]; goto rel_a; }
                            n = *pb - ((int)(cp-base) + 4-shrink);
                            if (shrink == 0) goto wd;
                            if (shrink == 4) { cp--; cp[-1] = *cp-0x10; } else cp[-1] = 0xeb;
                            goto wb;
                        }
                    case DASM_IMM_LG:
                        p++;
                        if (n < 0) { n = (int)(ptrdiff_t)D->globals[-n]; goto wd; }
                    case DASM_IMM_PC: {
                            int * pb = DASM_POS2PTR(D, n);
                            n = *pb < 0 ? pb[1] : (*pb + (int)(ptrdiff_t)base);
                            goto wd;
                        }
                    case DASM_LABEL_LG: {
                            int idx = *p++;
                            if (idx >= 10)
                                D->globals[idx] = (void *)(base + (*p == DASM_SETLABEL ? *b : n));
                            break;
                        }
                    case DASM_LABEL_PC: case DASM_SETLABEL: break;
                    case DASM_SPACE: { int fill = *p++; while (n--) *cp++ = fill; break; }
                    case DASM_ALIGN:
                        n = *p++;
                        while (((cp-base) & n)) *cp++ = 0x90; /* nop */
                        break;
                    case DASM_EXTERN: n = DASM_EXTERN(Dst, cp, p[1], *p); p += 2; goto wd;
                    case DASM_MARK: mark = cp; break;
                    case DASM_ESC: action = *p++;
                    default: *cp++ = action; break;
                    case DASM_SECTION: case DASM_STOP: goto stop;
                }
            }
        stop: (void)0;
        }
    }
    if (base + D->codesize != cp)  /* Check for phase errors. */
        return DASM_S_PHASE;
    return DASM_S_OK;
}

int dasm_getpclabel(Dst_DECL, unsigned int pc) {
    dasm_State * D = Dst_REF;
    if (pc*sizeof(int) < D->pcsize) {
        int pos = D->pclabels[pc];
        if (pos < 0) return *DASM_POS2PTR(D, -pos);
        if (pos > 0) return -1;  /* Undefined. */
    }
    return -2;  /* Unused or out of range. */
}

#ifdef DASM_CHECKS
/* Optional sanity checker to call between isolated encoding steps. */
int dasm_checkstep(Dst_DECL, int secmatch) {
    dasm_State * D = Dst_REF;
    if (D->status == DASM_S_OK) {
        int i;
        for (i = 1; i <= 9; i++) {
            if (D->lglabels[i] > 0) { D->status = DASM_S_UNDEF_L|i; break; }
            D->lglabels[i] = 0;
        }
    }
    if (D->status == DASM_S_OK && secmatch >= 0 &&
        D->section != &D->sections[secmatch])
        D->status = DASM_S_MATCH_SEC|(int)(D->section-D->sections);
    return D->status;
}
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
