/* permute: a port of tools/permute.py (its docstring and --help are the spec). Sets up a decomp-permuter directory
 * for one function (base.c / base.cpp, target.s / target.o, compile.sh, settings.toml), prints the base's score,
 * scores a file (--score) and starts decomp-permuter (--run; a third-party Python program, launched exactly as the
 * Python tool launched it).
 *
 * Files: permute_strip.c (the source strippers and symbol names; their own header), permute_cc.c (the pieces of
 * tools/mwcc_trace/cc.py and the Python standard library these tools use: unit_flags, find_source, shlex.quote,
 * universal newlines; flagsweep links them too), permute.c (the tool). Paths are Windows paths in the Python's own
 * spelling (os.path.join with backslashes), since several are printed. Errors the Python raised as SystemExit(msg)
 * or as an uncaught exception end the process through avp_fatal (the message on stderr, exit code 1). */
#pragma once
#include "avp/avp.h"
#include "cc_codecmp.h"

/* ---- cc.py and standard-library pieces (permute_cc.c) --------------------------------------------------------------- */

/* The checkout this executable belongs to, as os.path.abspath spells it (backslashes): permute.REPO. */
const char *permute_repo(void);
/* os.path.join(first, second): ntpath.join for the relative second parts these tools use. */
const char *permute_join(AvpArena *a, const char *first, const char *second);
/* cc.unit_flags(unit) with build.ninja read from root: the compiler version ("Wii/1.0") and the cflags. avp_fatal
 * "unit not found in build.ninja: <unit>" (SystemExit) and on a read failure (the Python's traceback). */
void permute_unit_flags(AvpArena *a, const char *root, AvpStr unit, const char **version, AvpStrVec *cflags);
/* permute.find_source(unit): root\src\<unit>.c / .cp / .cpp, the first that exists; avp_fatal "no source for unit
 * <unit>". */
const char *permute_find_source(AvpArena *a, const char *root, AvpStr unit);
/* shlex.quote(s); shlex.join(words) */
AvpStr permute_shlex_quote(AvpArena *a, AvpStr s);
AvpStr permute_shlex_join(AvpArena *a, const AvpStr *words, size_t n);
/* Captured process output as Python's text=True reads it: \r\n and \r become \n (bytes pass through: every source
 * of this project is ASCII). */
AvpStr permute_universal_newlines(AvpArena *a, AvpStr bytes);
/* Run argv (cwd may be NULL) capturing both streams as text; *rc the exit code. false when the process did not
 * start (*msg the OSError text). */
bool permute_run_capture(AvpArena *a, const char *const *argv, const char *cwd, i64 *rc, AvpStr *out, AvpStr *err,
                         const char **msg);

/* ---- the tool (permute.c) ----------------------------------------------------------------------------------------- */

/* resolve_symbol(asm, name, cxx): true with *sym, or false with *error (the Python's SystemExit text). */
bool permute_resolve_symbol(AvpArena *a, AvpStr asm_text, AvpStr name, bool cxx, AvpStr *sym, AvpStr *error);
/* retail_fn(unit, fn, asm): the ".fn fn," ... ".endfn fn" block; avp_fatal when absent. */
AvpStr permute_retail_fn(AvpArena *a, AvpStr unit, AvpStr fn, AvpStr asm_text);
/* compile_flags(cflags, cxx) */
AvpStrVec permute_compile_flags(const AvpStrVec *cflags, bool cxx);
/* settings_toml(func_name, objdump) */
AvpStr permute_settings_toml(AvpArena *a, AvpStr func_name, const char *objdump);
/* posix(p): the absolute path with "/" and "/x" for a drive "X:". */
AvpStr permute_posix(AvpArena *a, const char *p);
/* score_line(res): res NULL with error: "error: " + error. */
AvpStr permute_score_line(AvpArena *a, const CcFnResult *res, AvpStr error);
/* run_requirements(root, have_binutils): the messages for what --run needs and lacks. */
AvpStrVec permute_run_requirements(AvpArena *a, const char *root, bool have_binutils);

int permute_main(int argc, char **argv);
