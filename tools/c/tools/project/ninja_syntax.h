/* tools/ninja_syntax.py in C: the ninja file writer project.py uses (Writer, escape_path, escape, serialize_paths).
 *
 * Paths are passed already in their serialized form (str(Path) on Windows: backslashes). A NinjaPaths value stands
 * for one NinjaPathOrPaths argument: `given` is the truthiness of the Python argument (None, "" and [] are false; a
 * Path or a non-empty list is true even when every item is filtered out), `v` the items serialize_paths keeps (no
 * None, no ""). */
#pragma once
#include <stdbool.h>
#include <stddef.h>

#include "avp/arena.h"
#include "avp/str.h"

typedef struct {
    const char **v;
    size_t n;
    bool given;
} NinjaPaths;

/* None */
NinjaPaths ninja_none(void);
/* one str or Path value (NULL: None; "": a falsy str) */
NinjaPaths ninja_one(AvpArena *a, const char *s);
/* a list of n items (NULL items are None and dropped, "" items dropped) */
NinjaPaths ninja_list(AvpArena *a, size_t n, const char *const *items);
#define NINJA_LIST(a, ...) \
    ninja_list((a), sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *), (const char *[]){__VA_ARGS__})
/* " ".join(serialize_paths(p)) */
const char *ninja_join(AvpArena *a, NinjaPaths p);

typedef struct {
    const char *key;
    const char *val; /* the joined value ("" for None) */
} NinjaVar;

typedef struct {
    const char *name, *command, *description, *depfile;
    bool generator;
    const char *pool;
    bool restat;
    const char *rspfile, *rspfile_content, *deps; /* NULL or "": not written, as the Python's falsy checks */
} NinjaRule;

typedef struct {
    NinjaPaths outputs;
    const char *rule;
    NinjaPaths inputs, implicit, order_only, implicit_outputs;
    const NinjaVar *vars;
    size_t nvars;
    const char *pool;   /* NULL: None */
    const char *dyndep; /* NULL: None */
} NinjaBuild;

typedef struct {
    AvpArena *a;
    AvpBuf *out;
    int width;
} NinjaWriter;

void ninja_writer_init(NinjaWriter *w, AvpArena *a, AvpBuf *out); /* width 78 */
void ninja_newline(NinjaWriter *w);
void ninja_comment(NinjaWriter *w, const char *text);
void ninja_variable(NinjaWriter *w, const char *key, const char *value, int indent);
void ninja_pool(NinjaWriter *w, const char *name, int depth);
void ninja_rule(NinjaWriter *w, const NinjaRule *r);
void ninja_build(NinjaWriter *w, const NinjaBuild *b);
void ninja_include(NinjaWriter *w, const char *path);
void ninja_subninja(NinjaWriter *w, const char *path);
void ninja_default(NinjaWriter *w, NinjaPaths paths);

/* escape_path(word): "$ " -> "$$ ", " " -> "$ ", ":" -> "$:" */
const char *ninja_escape_path(AvpArena *a, const char *word);
/* escape(string): "$" -> "$$" */
const char *ninja_escape(AvpArena *a, const char *s);
/* textwrap.wrap(text, width, break_long_words=False, break_on_hyphens=False) */
AvpStrVec ninja_textwrap(AvpArena *a, const char *text, size_t width);
