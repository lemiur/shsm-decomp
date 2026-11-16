/* Private to the names tool's .c files. */
#pragma once
#include "names.h"

#include <stdlib.h>

/* WORD = A-Za-z0-9_$@ */
bool names_word_char(unsigned char c);
/* ident_rx(words): (?<![WORD])(?:w...)(?![WORD]), longest word first. Words are byte strings; the matcher works on
 * bytes (WORD is ASCII, so this is the Python's code-point match on UTF-8 text). */
typedef struct NamesIdentRx NamesIdentRx;
NamesIdentRx *names_ident_rx(AvpArena *a, const AvpStr *words, size_t n);
bool names_ident_find(const NamesIdentRx *rx, AvpStr s, size_t pos, size_t *start, size_t *len);
size_t names_ident_count(const NamesIdentRx *rx, AvpStr s);
typedef AvpStr (*NamesSubFn)(void *ctx, AvpStr match);
AvpStr names_ident_sub(AvpArena *a, const NamesIdentRx *rx, AvpStr s, NamesSubFn fn, void *ctx);

/* The planned file of rel, created when absent. */
NamesFile *names_plan_file_add(NamesPlan *p, const char *rel);

size_t names_all_by_name(const NamesCtx *cx, const char *name, AvpPtrVec *out);
bool names_scope_eq(const char *const *a, size_t na, const char *const *b, size_t nb);

/* read_text(path) as bytes (the Python's surrogateescape decoding round-trips the bytes); raises the OSError. */
AvpStr names_read_text(AvpArena *a, const char *path);
/* text.split("\n") */
AvpStrVec names_split_nl(AvpStr text);
/* SRC_EXT */
bool names_src_ext(AvpStr name);
/* sorted(fns) of an os.walk item */
void names_sort_strs(AvpStrVec *v);
