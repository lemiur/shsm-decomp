/* avpcore base: fixed-width types, common macros, process start-up, fatal errors and the small try/throw used where
 * the Python caught a library's exception. Every other avpcore header includes this one. */
#pragma once

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define AVP_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define AVP_MIN(a, b) ((a) < (b) ? (a) : (b))
#define AVP_MAX(a, b) ((a) > (b) ? (a) : (b))
#define AVP_UNUSED(x) ((void)(x))
#define AVP_THREAD_LOCAL __declspec(thread)
#define AVP_NORETURN __declspec(noreturn)
#define AVP_PRINTF_FMT _Printf_format_string_

/* Always on (also in Release): a broken invariant is an internal error (D3), never undefined behaviour. */
#define AVP_ASSERT(cond) ((cond) ? (void)0 : avp_panic(__FILE__, __LINE__, "assertion failed: %s", #cond))

/* ---- process start-up ------------------------------------------------------------------------------------------
 * Each tool's main file is
 *     static int run(int argc, char **argv) { ... }      (argv in UTF-8, argv[0] the program)
 *     AVP_MAIN(run, "gate")
 * AVP_MAIN defines wmain: it converts the arguments to UTF-8, records the tool name (for messages), sets the
 * console output code page to UTF-8, gives stdout a large buffer when it is not a console (Python block-buffers a
 * redirected stdout too), keeps stdout/stderr in text mode ("\n" -> "\r\n", as Python's print on Windows) and
 * flushes both at exit. The return value of run is the exit code. */
int avp_start(int argc, wchar_t **wargv, const char *tool, int (*run)(int argc, char **argv));
/* Crash and hang policy. Nothing the tools run may ever wait on a UI (a crash, assert, abort or Windows Error
 * Reporting box), on any machine: the fix is in code, not in registry settings. avp_start calls this as its very
 * first statement, so every executable (tools, eqh, every test binary through AVP_TEST_MAIN) has it; the CTest test
 * entry_points fails when an executable has an entry point that does not go through AVP_MAIN. It is idempotent and:
 *   - SetErrorMode(FAILCRITICALERRORS | NOGPFAULTERRORBOX | NOOPENFILEERRORBOX) (children inherit it: never pass
 *     CREATE_DEFAULT_ERROR_MODE) and WerSetFlags(WER_FAULT_REPORTING_NO_UI);
 *   - abort() / SIGABRT: "<tool>: aborted" on stderr, exit code 3; a pure call is the same;
 *   - an invalid CRT parameter: "<tool>: invalid parameter passed to a C runtime function", exit code 0xC000000D;
 *   - an unhandled exception (access violation, stack overflow, ...): "<tool>: unhandled exception 0x%08lX at %p" on
 *     stderr, exit code = the exception code (distinct from the tools' own 0/1/2);
 *   - the debug CRT's assert/error/warning reports go to stderr (an _ASSERTE prints and carries on).
 * Every process a tool starts is killed with its tree when it overruns (proc.h: timeouts and the ceiling). */
void avp_crash_quiet_init(void);
#define AVP_MAIN(run_fn, tool_name) \
    int wmain(int argc, wchar_t **wargv) { return avp_start(argc, wargv, (tool_name), (run_fn)); }

const char *avp_tool_name(void);
/* The repository root: --root / AVP3_ROOT when given (avp_set_root), else the nearest folder at or above the current
 * directory holding matching/ and config/retail.json (a checkout or worktree; one line on stderr when it is not the checkout
 * of the executable), else two folders above the executable (<repo>/build/tools/x.exe), else the current
 * directory's git top level. A computed root is exported as AVP3_ROOT for the tools this one starts. Forward
 * slashes, no trailing slash. */
const char *avp_root(void);
void avp_set_root(const char *root);
/* The folder of the running executable (forward slashes, no trailing slash): sibling tools are started from it. */
const char *avp_exe_dir(void);
/* The build stamp of the tools: sha256 hex over the names and contents of core/ and lib/ (generated at build time by
 * cmake/tools_stamp.cmake). It versions the on-disk memos and the LTCG pin digest where the Python used the hash of
 * its own source: every executable built from the same sources shares them, a changed source invalidates them. */
const char *avp_tools_stamp(void);

/* ---- errors ----------------------------------------------------------------------------------------------------
 * avp_fatal is Python's sys.exit("message"): the message and a newline on stderr, exit code 1.
 * avp_exit flushes stdout and stderr, then exits with code.
 * avp_warn writes one line on stderr (no prefix added: the caller spells the line as the Python did).
 * avp_panic is an internal error (the Python's traceback, deviation D3): "<tool>: internal error: <file>:<line>:
 * <message>" on stderr, exit code 1. */
AVP_NORETURN void avp_fatal(AVP_PRINTF_FMT const char *fmt, ...);
AVP_NORETURN void avp_exit(int code);
void avp_warn(AVP_PRINTF_FMT const char *fmt, ...);
AVP_NORETURN void avp_panic(const char *file, int line, AVP_PRINTF_FMT const char *fmt, ...);
/* Out of memory is always fatal. */
void *avp_xmalloc(size_t n);
void *avp_xcalloc(size_t count, size_t n);
void *avp_xrealloc(void *p, size_t n);
void avp_xfree(void *p);

/* ---- try / throw -----------------------------------------------------------------------------------------------
 * Only where the Python caught an exception raised by a library (a malformed PE, a JSON decode error, a regex
 * that does not compile) and went on. Library code throws with avp_throw; the catcher reads code and msg.
 *     AvpTry t;
 *     if (AVP_TRY(&t)) { ... code that may throw ...; avp_try_end(&t); }
 *     else { ... t.code, t.msg ... }
 * A throw with no AVP_TRY active is fatal (avp_fatal with the message). Throws do not unwind resources: code
 * between AVP_TRY and the throw allocates from arenas only. */
enum {
    AVP_ERR_IO = 1,     /* file or OS error (Python OSError) */
    AVP_ERR_FORMAT,     /* malformed input: PE, COFF, JSON, cache, tar (ValueError / PEFormatError / JSONDecodeError) */
    AVP_ERR_REGEX,      /* re.error */
    AVP_ERR_PROC,       /* a subprocess could not start or timed out */
    AVP_ERR_OTHER
};
typedef struct AvpTry {
    jmp_buf jb;
    struct AvpTry *prev;
    int code;
    char msg[1024];
} AvpTry;
void avp_try_push(AvpTry *t);
void avp_try_end(AvpTry *t);
#define AVP_TRY(t) (avp_try_push(t), setjmp((t)->jb) == 0)
AVP_NORETURN void avp_throw(int code, AVP_PRINTF_FMT const char *fmt, ...);
