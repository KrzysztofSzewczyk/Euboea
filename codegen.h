
/* EUBOEA
 * Copyright (C) Kamila Palaiologos Szewczyk, 2019.
 * License: MIT
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _CODEGEN_H
#define _CODEGEN_H

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

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
    #define MAP_ANONYMOUS MAP_ANON
#endif
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

#endif
