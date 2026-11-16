/* Bump arenas. The tools are short-lived processes: most memory comes from the calling thread's long-lived arena
 * (avp_arena()) and is never freed; loops that allocate per item use a scratch arena with a mark and a reset.
 * Memory from any arena stays valid for the process's life unless that arena is reset or freed, so pointers may be
 * handed to other threads. An arena itself is used by one thread at a time. */
#pragma once
#include "base.h"

typedef struct AvpArena AvpArena;

AvpArena *avp_arena_new(size_t block_size); /* 0: the default block size (1 MiB); larger requests get their own block */
void avp_arena_free(AvpArena *a);

void *avp_alloc(AvpArena *a, size_t n);     /* zeroed, 16-byte aligned; n == 0 gives a valid unique pointer */
void *avp_alloc_noz(AvpArena *a, size_t n); /* not zeroed */
/* Grows (or shrinks) the allocation p of size old_n; in place when p is the arena's last allocation. */
void *avp_arena_realloc(AvpArena *a, void *p, size_t old_n, size_t new_n);
char *avp_strdup(AvpArena *a, const char *s);
void *avp_memdup(AvpArena *a, const void *p, size_t n);
char *avp_sprintf(AvpArena *a, AVP_PRINTF_FMT const char *fmt, ...); /* C printf rules (not Python's: see fmt.h) */
char *avp_vsprintf(AvpArena *a, const char *fmt, va_list ap);

typedef struct {
    void *block;
    size_t used;
} AvpArenaMark;
AvpArenaMark avp_arena_mark(AvpArena *a);
void avp_arena_reset(AvpArena *a, AvpArenaMark mark); /* frees everything allocated after the mark */
/* Between a mark and its reset, do not grow what was allocated before the mark (avp_arena_realloc, an AvpBuf or AvpVec on
 * that arena): the grown part counts as allocated after the mark and the reset frees it. */

/* The calling thread's long-lived arena (created on first use, never freed). */
AvpArena *avp_arena(void);
/* The calling thread's scratch arena: take a mark, use it, reset to the mark before returning. Never return
 * scratch memory to a caller. */
AvpArena *avp_scratch(void);

#define AVP_NEW(a, T) ((T *)avp_alloc((a), sizeof(T)))
#define AVP_NEWN(a, T, count) ((T *)avp_alloc((a), sizeof(T) * (size_t)(count)))
