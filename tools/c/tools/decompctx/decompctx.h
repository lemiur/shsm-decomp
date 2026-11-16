/* decompctx: builds a decomp.me context file by inlining a source file's #include lines (ports tools/decompctx.py).
 *
 * The functions below are the Python's module-level functions and globals, collected in a DecompctxState. The text of
 * a file is read with the Python's encodings (UTF-8, then the locale's default) and universal newlines; the output
 * text uses "\n" (the writer turns it into CRLF, as the Python's text-mode open does). */
#pragma once
#include "avp/avp.h"

/* Include depth at which the C port gives up (the Python hits RecursionError at about this nesting). */
#define DECOMPCTX_MAX_DEPTH 300

typedef struct {
    AvpArena *a;
    const char *root_dir;    /* repository root, ntpath form */
    AvpStrVec include_dirs;  /* -I */
    AvpStrVec exclude_globs; /* -x */
    AvpSet *defines;         /* header guards and #pragma once file names seen */
    AvpStrVec deps;          /* files read, in order, relative to root_dir */
    int depth;
    bool failed;             /* a Python exception is propagating: err_class / err_text */
    bool fatal;              /* ...and no except clause catches it */
    const char *err_class;
    const char *err_text;
    const char *script;      /* file name shown in a traceback */
} DecompctxState;

void decompctx_init(DecompctxState *st, AvpArena *a, const char *root_dir);

/* generate_prelude: "" for no defines. */
AvpStr decompctx_prelude(AvpArena *a, const AvpStrVec *defs);
/* sanitize_path: backslashes to slashes, spaces escaped. */
AvpStr decompctx_sanitize_path(AvpArena *a, AvpStr path);

/* The three line patterns, on an already stripped line. Each returns true on a match; the group is in *out. */
bool decompctx_match_include(AvpStr line, AvpStr *out);
bool decompctx_match_guard(AvpStr line, AvpStr *out);
bool decompctx_match_once(AvpStr line);

/* process_file over decoded text; appends to *out. False when an include raised (the exception is in st). */
bool decompctx_process_text(DecompctxState *st, const char *in_file, AvpStr text, AvpBuf *out);
/* import_c_file with the Python's try/except: a file is read as UTF-8; when that or its processing raises, it is read
 * again with the locale's encoding and processed again (progress lines and the guard set are not rolled back, the
 * partly built text is dropped). Appends the expansion to *out. False when an exception propagates out of it
 * (st->err_class / st->err_text). */
bool decompctx_import_c_file(DecompctxState *st, const char *in_file, AvpBuf *out);
