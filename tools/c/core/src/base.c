/* avpcore base: process start-up, the repository root, fatal errors, the checked allocators and try/throw
 * (design.md section 6). Everything here is thread-safe: the only mutable globals are set once at start-up or
 * guarded by an SRW lock, and the try stack is thread-local. */
#include "avp/avp.h"

#include <fcntl.h>
#include <io.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <werapi.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif

/* ---- small helpers ---------------------------------------------------------------------------------------------- */

/* UTF-16 to UTF-8 into an arena string (unpaired surrogates become U+FFFD, as the Win32 conversion does). */
static char *w_to_u8(AvpArena *a, const wchar_t *w, int n) {
    int len = WideCharToMultiByte(CP_UTF8, 0, w, n, NULL, 0, NULL, NULL);
    char *out = (char *)avp_alloc_noz(a, (size_t)len + 1);
    if (len > 0) WideCharToMultiByte(CP_UTF8, 0, w, n, out, len, NULL, NULL);
    out[len] = 0;
    if (n < 0 && len > 0) out[len - 1] = 0; /* n == -1 counted the terminator */
    return out;
}

static wchar_t *u8_to_w(AvpArena *a, const char *s) {
    int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *out = (wchar_t *)avp_alloc_noz(a, ((size_t)len + 1) * sizeof(wchar_t));
    if (len > 0) MultiByteToWideChar(CP_UTF8, 0, s, -1, out, len);
    else out[0] = 0;
    return out;
}

static void slashes(char *s) {
    for (; *s; s++)
        if (*s == '\\') *s = '/';
}

/* Removes a trailing slash unless the path is a drive root such as "C:/". */
static void trim_slash(char *s) {
    size_t n = strlen(s);
    while (n > 1 && s[n - 1] == '/' && !(n == 3 && s[1] == ':')) s[--n] = 0;
}

static bool path_exists(const char *path) {
    AvpArena *sc = avp_scratch();
    AvpArenaMark m = avp_arena_mark(sc);
    DWORD attr = GetFileAttributesW(u8_to_w(sc, path));
    avp_arena_reset(sc, m);
    return attr != INVALID_FILE_ATTRIBUTES;
}

/* Cuts s at its last slash (the parent folder); false when there is no parent left. */
static bool parent_dir(char *s) {
    size_t n = strlen(s);
    char *slash = NULL;
    for (char *c = s; *c; c++)
        if (*c == '/') slash = c;
    if (!slash) return false;
    size_t keep = (size_t)(slash - s);
    if (keep == 0) return false;
    if (keep == 2 && s[1] == ':') {
        if (n == 3) return false; /* already "C:/" */
        s[3] = 0;
        return true;
    }
    s[keep] = 0;
    return true;
}

/* ---- tool state ------------------------------------------------------------------------------------------------- */

static const char *g_tool = "avp";
static SRWLOCK g_lock = SRWLOCK_INIT;
static char *g_root;    /* guarded by g_lock */
static char *g_exe_dir; /* guarded by g_lock */

const char *avp_tool_name(void) { return g_tool; }

static char *compute_exe_dir(void) {
    wchar_t stack[MAX_PATH];
    wchar_t *buf = stack;
    DWORD cap = MAX_PATH, n;
    for (;;) {
        n = GetModuleFileNameW(NULL, buf, cap);
        if (n < cap) break;
        cap *= 2;
        wchar_t *nb = (wchar_t *)malloc(cap * sizeof(wchar_t));
        if (!nb) avp_fatal("out of memory");
        if (buf != stack) free(buf);
        buf = nb;
    }
    const wchar_t *w = buf;
    if (n >= 4 && w[0] == L'\\' && w[1] == L'\\' && w[2] == L'?' && w[3] == L'\\') w += 4;
    char *path = w_to_u8(avp_arena(), w, -1);
    if (buf != stack) free(buf);
    slashes(path);
    parent_dir(path);
    trim_slash(path);
    return path;
}

const char *avp_exe_dir(void) {
    AcquireSRWLockExclusive(&g_lock);
    if (!g_exe_dir) g_exe_dir = compute_exe_dir();
    const char *r = g_exe_dir;
    ReleaseSRWLockExclusive(&g_lock);
    return r;
}

static bool has_marker(const char *dir, const char *rel) {
    AvpArena *sc = avp_scratch();
    AvpArenaMark m = avp_arena_mark(sc);
    bool ok = path_exists(avp_sprintf(sc, "%s/%s", dir, rel));
    avp_arena_reset(sc, m);
    return ok;
}

/* The checkout of the current directory: the nearest folder at or above it holding matching/ and config/retail.json
 * (a repository or worktree root; AGENTS.md is not versioned, so a worktree has none), else NULL. */
static char *cwd_checkout(char **cwd_out) {
    DWORD n = GetCurrentDirectoryW(0, NULL);
    wchar_t *cwd = (wchar_t *)avp_alloc_noz(avp_arena(), ((size_t)n + 1) * sizeof(wchar_t));
    GetCurrentDirectoryW(n + 1, cwd);
    char *start = w_to_u8(avp_arena(), cwd, -1);
    slashes(start);
    trim_slash(start);
    *cwd_out = start;
    char *walk = avp_strdup(avp_arena(), start);
    for (;;) {
        if (has_marker(walk, "matching") && has_marker(walk, "config/retail.json")) return walk;
        if (!parent_dir(walk)) break;
    }
    return NULL;
}

static char *compute_root(void) {
    const wchar_t *env = _wgetenv(L"AVP3_ROOT");
    if (env && *env) {
        char *r = w_to_u8(avp_arena(), env, -1);
        slashes(r);
        trim_slash(r);
        return r;
    }
    /* <repo>/build/tools/x.exe; a test in build/tools/tests/ falls to the walks below. */
    char *exe_root = avp_strdup(avp_arena(), avp_exe_dir());
    if (!(parent_dir(exe_root) && parent_dir(exe_root) && has_marker(exe_root, "tools/c/CMakeLists.txt")))
        exe_root = NULL;
    /* The checkout the tool is run in wins over the one it was built in: a lane worktree's headers, not main's. */
    char *start = NULL;
    char *r = cwd_checkout(&start);
    if (r) {
        if (exe_root && _stricmp(r, exe_root) != 0) {
            fprintf(stderr, "%s: root %s (the current checkout)\n", avp_tool_name(), r);
            fflush(stderr);
        }
    } else if (exe_root) {
        return exe_root;
    } else {
        /* Otherwise the first folder at or above the current directory that has .git (a folder or, in a worktree, a
         * file). */
        char *walk = avp_strdup(avp_arena(), start);
        for (;;) {
            if (has_marker(walk, ".git")) { r = walk; break; }
            if (!parent_dir(walk)) break;
        }
        if (!r) r = start;
    }
    /* The tools this one starts (with their own working directories) work on the same checkout. */
    avp_setenv("AVP3_ROOT", r);
    return r;
}

const char *avp_root(void) {
    AcquireSRWLockExclusive(&g_lock);
    char *r = g_root;
    ReleaseSRWLockExclusive(&g_lock);
    if (r) return r;
    /* Computed outside the lock: compute_root calls avp_exe_dir, which takes it. A race computes twice; the first
     * result stays. */
    char *c = compute_root();
    AcquireSRWLockExclusive(&g_lock);
    if (!g_root) g_root = c;
    r = g_root;
    ReleaseSRWLockExclusive(&g_lock);
    return r;
}

void avp_set_root(const char *root) {
    char *r = avp_strdup(avp_arena(), root);
    slashes(r);
    trim_slash(r);
    AcquireSRWLockExclusive(&g_lock);
    g_root = r;
    ReleaseSRWLockExclusive(&g_lock);
}

/* ---- start-up and exit ------------------------------------------------------------------------------------------ */

static bool is_console(DWORD std_handle) {
    HANDLE h = GetStdHandle(std_handle);
    DWORD mode;
    return h != NULL && h != INVALID_HANDLE_VALUE && GetFileType(h) == FILE_TYPE_CHAR && GetConsoleMode(h, &mode);
}

/* ---- crash-quiet start-up --------------------------------------------------------------------------------------
 * Nothing the tools run may ever wait on a dialog: a crash, an abort, an invalid CRT argument or a failed debug
 * assertion ends the process at once with a one-line message on stderr and a distinct exit code. Each handler
 * writes with WriteFile from a stack buffer (the CRT's streams may be the very thing that broke). */

#define AVP_EXIT_ABORT 3                      /* abort() / raise(SIGABRT) / a pure call */
#define AVP_EXIT_INVALID_PARAMETER 0xC000000DU /* STATUS_INVALID_PARAMETER */

static void crash_say(const char *line) {
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    if (!h || h == INVALID_HANDLE_VALUE) return;
    DWORD n;
    WriteFile(h, line, (DWORD)strlen(line), &n, NULL);
}

/* "<tool>: <what>\r\n" (stderr is a text-mode stream elsewhere, so the same line ending). */
static void crash_report(AVP_PRINTF_FMT const char *fmt, ...) {
    char buf[1024];
    int k = snprintf(buf, sizeof buf - 3, "%s: ", g_tool);
    if (k < 0) k = 0;
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(buf + k, sizeof buf - 3 - (size_t)k, fmt, ap);
    va_end(ap);
    size_t len = m < 0 ? (size_t)k : (size_t)k + (size_t)m;
    if (len > sizeof buf - 3) len = sizeof buf - 3;
    buf[len++] = '\r';
    buf[len++] = '\n';
    buf[len] = 0;
    crash_say(buf);
}

static AVP_NORETURN void crash_die(UINT code) {
    /* The buffered stdout is worth keeping; a second fault while flushing lands in the filter again and ends it. */
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), code);
    for (;;) Sleep(1000); /* not reached */
}

static LONG WINAPI crash_exception_filter(EXCEPTION_POINTERS *ep) {
    static volatile LONG entered;
    if (InterlockedExchange(&entered, 1)) TerminateProcess(GetCurrentProcess(), ep->ExceptionRecord->ExceptionCode);
    crash_report("unhandled exception 0x%08lX at %p", ep->ExceptionRecord->ExceptionCode,
                 ep->ExceptionRecord->ExceptionAddress);
    crash_die(ep->ExceptionRecord->ExceptionCode);
}

static void crash_invalid_parameter(const wchar_t *expr, const wchar_t *func, const wchar_t *file, unsigned line,
                                    uintptr_t reserved) {
    AVP_UNUSED(reserved);
    if (expr || func || file)
        crash_report("invalid parameter passed to a C runtime function (%ls in %ls, %ls:%u)", expr ? expr : L"?",
                     func ? func : L"?", file ? file : L"?", line);
    else
        crash_report("invalid parameter passed to a C runtime function");
    crash_die(AVP_EXIT_INVALID_PARAMETER);
}

static void crash_sigabrt(int sig) {
    AVP_UNUSED(sig);
    crash_report("aborted");
    fflush(stdout);
    _exit(AVP_EXIT_ABORT);
}

static void crash_purecall(void) {
    crash_report("pure virtual function call");
    fflush(stdout);
    _exit(AVP_EXIT_ABORT);
}

void avp_crash_quiet_init(void) {
    static volatile LONG done;
    if (InterlockedExchange(&done, 1)) return;
    /* Inherited by every child (never pass CREATE_DEFAULT_ERROR_MODE when starting one). */
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    WerSetFlags(WER_FAULT_REPORTING_NO_UI);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    signal(SIGABRT, crash_sigabrt);
#ifdef _DEBUG
    /* The debug CRT's assert and error boxes go to stderr instead; _ASSERTE then continues, as a message does. */
    int types[3] = {_CRT_ASSERT, _CRT_ERROR, _CRT_WARN};
    for (int i = 0; i < 3; i++) {
        _CrtSetReportMode(types[i], _CRTDBG_MODE_FILE);
        _CrtSetReportFile(types[i], _CRTDBG_FILE_STDERR);
    }
#endif
    _set_invalid_parameter_handler(crash_invalid_parameter);
    _set_purecall_handler(crash_purecall);
    SetUnhandledExceptionFilter(crash_exception_filter);
    ULONG guarantee = 64 * 1024; /* room for the filter to print after a stack overflow on this thread */
    SetThreadStackGuarantee(&guarantee);
}

int avp_start(int argc, wchar_t **wargv, const char *tool, int (*run)(int argc, char **argv)) {
    avp_crash_quiet_init();
    g_tool = tool ? avp_strdup(avp_arena(), tool) : "avp";
    char **argv = (char **)avp_alloc(avp_arena(), ((size_t)argc + 1) * sizeof(char *));
    for (int i = 0; i < argc; i++) argv[i] = w_to_u8(avp_arena(), wargv[i], -1);
    argv[argc] = NULL;

    if (is_console(STD_OUTPUT_HANDLE) || is_console(STD_ERROR_HANDLE)) SetConsoleOutputCP(CP_UTF8);
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    bool out_char = out != NULL && out != INVALID_HANDLE_VALUE && GetFileType(out) == FILE_TYPE_CHAR;
    /* Python block-buffers a redirected stdout; a console stays line-buffered by the CRT's own rules. */
    if (!out_char) setvbuf(stdout, NULL, _IOFBF, 64 * 1024);
    /* Text mode: "\n" becomes "\r\n" as Python's print does on Windows. The bytes are UTF-8 and pass untouched. */
    _setmode(_fileno(stdout), _O_TEXT);
    _setmode(_fileno(stderr), _O_TEXT);

    int code = run(argc, argv);
    fflush(stdout);
    fflush(stderr);
    return code;
}

void avp_exit(int code) {
    fflush(stdout);
    fflush(stderr);
    exit(code);
}

/* ---- errors ----------------------------------------------------------------------------------------------------- */

void avp_fatal(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    avp_exit(1);
}

void avp_warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

void avp_panic(const char *file, int line, const char *fmt, ...) {
    /* __FILE__ is an absolute path: show it from the tools/c folder on. */
    const char *rel = strstr(file, "tools\\c\\");
    if (!rel) rel = strstr(file, "tools/c/");
    if (rel) file = rel + 8;
    fprintf(stderr, "%s: internal error: %s:%d: ", g_tool, file, line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    avp_exit(1);
}

/* ---- allocators -------------------------------------------------------------------------------------------------- */

void *avp_xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) avp_fatal("out of memory (%zu bytes)", n);
    return p;
}

void *avp_xcalloc(size_t count, size_t n) {
    void *p = calloc(count ? count : 1, n ? n : 1);
    if (!p) avp_fatal("out of memory (%zu x %zu bytes)", count, n);
    return p;
}

void *avp_xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) avp_fatal("out of memory (%zu bytes)", n);
    return q;
}

void avp_xfree(void *p) { free(p); }

/* ---- try / throw ------------------------------------------------------------------------------------------------ */

static AVP_THREAD_LOCAL AvpTry *t_try_top;

void avp_try_push(AvpTry *t) {
    t->prev = t_try_top;
    t->code = 0;
    t->msg[0] = 0;
    t_try_top = t;
}

void avp_try_end(AvpTry *t) {
    AVP_ASSERT(t_try_top == t);
    t_try_top = t->prev;
}

void avp_throw(int code, const char *fmt, ...) {
    AvpTry *t = t_try_top;
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    if (!t) avp_fatal("%s", msg);
    /* Pops the try first, so a throw from its else branch goes to the next one out. */
    t_try_top = t->prev;
    t->code = code;
    memcpy(t->msg, msg, sizeof msg);
    longjmp(t->jb, 1);
}
