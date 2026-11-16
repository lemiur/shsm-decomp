/* Base tests: try/throw, root and exe dir, start-up conventions. */
#include "avp/avp.h"
#include "avp/test.h"

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif

/* The throws are conditional on a variable the compiler cannot see through, as real code is (avp_throw never
 * returns, and the code after an unconditional one would be a C4702 warning). */
static volatile int g_go = 1;

AVP_TEST(try_catches_a_throw) {
    AvpTry t;
    bool reached_end = false;
    if (AVP_TRY(&t)) {
        if (g_go) avp_throw(AVP_ERR_FORMAT, "bad %s at %d", "header", 12);
        reached_end = true;
        avp_try_end(&t);
    } else {
        CHECK_EQ_I64(t.code, AVP_ERR_FORMAT);
        CHECK_CZ(t.msg, "bad header at 12");
    }
    CHECK(!reached_end);
}

AVP_TEST(try_without_throw_ends_cleanly) {
    AvpTry t;
    int ran = 0;
    if (AVP_TRY(&t)) {
        ran = 1;
        avp_try_end(&t);
    } else {
        ran = -1;
    }
    CHECK_EQ_I64(ran, 1);
}

static void throw_deep(int depth) {
    if (depth == 0 && g_go) avp_throw(AVP_ERR_IO, "deep");
    if (depth > 0) throw_deep(depth - 1);
}

AVP_TEST(try_nested_and_rethrow) {
    AvpTry outer, inner;
    volatile int path = 0;
    if (AVP_TRY(&outer)) {
        if (AVP_TRY(&inner)) {
            throw_deep(5);
            avp_try_end(&inner);
        } else {
            path |= 1;
            CHECK_EQ_I64(inner.code, AVP_ERR_IO);
            if (g_go) avp_throw(AVP_ERR_REGEX, "again: %s", inner.msg); /* goes to the outer try */
        }
        avp_try_end(&outer);
    } else {
        path |= 2;
        CHECK_EQ_I64(outer.code, AVP_ERR_REGEX);
        CHECK_CZ(outer.msg, "again: deep");
    }
    CHECK_EQ_I64(path, 3);
}

static DWORD WINAPI throw_thread(void *arg) {
    int *flag = (int *)arg;
    AvpTry t;
    if (AVP_TRY(&t)) {
        if (g_go) avp_throw(AVP_ERR_OTHER, "in thread");
        avp_try_end(&t);
    } else {
        *flag = (t.code == AVP_ERR_OTHER) ? 1 : -1;
    }
    return 0;
}

AVP_TEST(try_stack_is_thread_local) {
    AvpTry t;
    int flag = 0;
    if (AVP_TRY(&t)) {
        HANDLE h = CreateThread(NULL, 0, throw_thread, &flag, 0, NULL);
        REQUIRE(h != NULL);
        WaitForSingleObject(h, 10000);
        CloseHandle(h);
        avp_try_end(&t);
    }
    CHECK_EQ_I64(flag, 1);
}

AVP_TEST(root_and_exe_dir_shape) {
    const char *r = avp_root();
    const char *e = avp_exe_dir();
    CHECK(r != NULL && e != NULL);
    CHECK(strchr(r, '\\') == NULL && strchr(e, '\\') == NULL);
    size_t rn = strlen(r), en = strlen(e);
    CHECK(rn > 3 && r[rn - 1] != '/');
    CHECK(en > 3 && e[en - 1] != '/');
    /* The tests run from the repository root with the executables under build/tools/tests. */
    CHECK(strstr(e, "build/tools") != NULL);
    char *marker = avp_sprintf(avp_scratch(), "%s/tools/c/CMakeLists.txt", r);
    FILE *f = fopen(marker, "rb");
    CHECK_MSG(f != NULL, "no %s", marker);
    if (f) fclose(f);
}

AVP_TEST(set_root_normalizes) {
    const char *old = avp_root();
    avp_set_root("C:\\some\\dir\\");
    CHECK_CZ(avp_root(), "C:/some/dir");
    avp_set_root("D:\\");
    CHECK_CZ(avp_root(), "D:/");
    avp_set_root(old);
    CHECK_CZ(avp_root(), old);
}

AVP_TEST(tool_name_recorded) {
    CHECK_CZ(avp_tool_name(), "test_base");
}

AVP_TEST(crt_text_mode_writes_crlf) {
    /* A text-mode file gets "\r\n" for "\n", which is what stdout and stderr are set to by avp_start. */
    char path[MAX_PATH + 16];
    GetTempPathA(MAX_PATH, path);
    strcat(path, "avp_base_test.txt");
    FILE *f = fopen(path, "w");
    REQUIRE(f != NULL);
    fputs("a\nb", f);
    fclose(f);
    f = fopen(path, "rb");
    REQUIRE(f != NULL);
    char buf[16] = {0};
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    remove(path);
    CHECK_EQ_U64(n, 4);
    CHECK_MEM(buf, "a\r\nb", 4);
}

/* ---- fatal / panic / exit run in a child process: the parent starts this executable again with an environment
 * variable that makes exactly one child_* probe act. */

static bool child_selected(const char *name) {
    const char *v = getenv("AVP_TEST_CHILD");
    return v && strcmp(v, name) == 0;
}

AVP_TEST(child_fatal_probe) {
    if (!child_selected("fatal")) return;
    printf("before\n");
    avp_fatal("boom %d: %s", 7, "caf\xc3\xa9");
}

AVP_TEST(child_panic_probe) {
    if (!child_selected("panic")) return;
    AVP_ASSERT(1 + 1 == 3);
}

AVP_TEST(child_exit_probe) {
    if (!child_selected("exit")) return;
    printf("hello\n");
    fprintf(stderr, "warn-side\n");
    avp_warn("warned %s", "line");
    avp_exit(3);
}

AVP_TEST(child_throw_probe) {
    if (!child_selected("throw")) return;
    avp_throw(AVP_ERR_REGEX, "uncaught %s", "regex");
}

/* Crash and hang policy probes (avp_crash_quiet_init): each one ends the child in a way that raises a dialog without
 * the policy; the parent kills a child that has not ended within the time limit, so a dialog fails the test. */
static volatile int *g_null_ptr;
static volatile int g_zero;
static const char *volatile g_null_fmt;

static int recurse(volatile int depth) {
    volatile char pad[1024];
    pad[0] = (char)depth;
    if (g_zero != 0) return pad[0]; /* never taken: the compiler may not see the recursion as endless */
    return recurse(depth + 1) + pad[0];
}

AVP_TEST(child_nullptr_probe) {
    if (!child_selected("nullptr")) return;
    *g_null_ptr = 1;
}

AVP_TEST(child_abort_probe) {
    if (!child_selected("abort")) return;
    abort();
}

AVP_TEST(child_invalid_param_probe) {
    if (!child_selected("invalid_param")) return;
    (void)_close(g_zero - 5); /* no such file descriptor: the CRT's invalid parameter handler */
}

AVP_TEST(child_printf_null_probe) {
    if (!child_selected("printf_null")) return;
    printf(g_null_fmt);
}

AVP_TEST(child_stack_overflow_probe) {
    if (!child_selected("stack_overflow")) return;
    g_zero = recurse(0);
}

AVP_TEST(child_assert_probe) {
    if (!child_selected("assert")) return;
#ifdef _DEBUG
    _ASSERTE(g_zero != 0);
#endif
    printf("after assert\n");
}

#define CHILD_LIMIT_MS 30000

/* Runs the probe in a child; returns its exit code (the DWORD, unsigned) and fills the files' contents
 * (NUL-terminated). A child that outlives CHILD_LIMIT_MS is killed with its job and gives -2. */
static i64 run_child(const char *probe, char *out, size_t out_cap, char *err, size_t err_cap) {
    wchar_t exe[MAX_PATH], tmp[MAX_PATH], fo[MAX_PATH + 32], fe[MAX_PATH + 32], cmd[MAX_PATH + 128];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    GetTempPathW(MAX_PATH, tmp);
    swprintf(fo, MAX_PATH + 32, L"%savp_child_out_%lu.txt", tmp, GetCurrentProcessId());
    swprintf(fe, MAX_PATH + 32, L"%savp_child_err_%lu.txt", tmp, GetCurrentProcessId());
    swprintf(cmd, MAX_PATH + 128, L"\"%s\" --filter child_%S_probe", exe, probe);
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    HANDLE ho = CreateFileW(fo, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE he = CreateFileW(fe, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    STARTUPINFOW si = {sizeof si};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = ho;
    si.hStdError = he;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    SetEnvironmentVariableW(L"AVP_TEST_CHILD", L"");
    wchar_t name[32];
    swprintf(name, 32, L"%S", probe);
    SetEnvironmentVariableW(L"AVP_TEST_CHILD", name);
    PROCESS_INFORMATION pi;
    BOOL ok = CreateProcessW(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    SetEnvironmentVariableW(L"AVP_TEST_CHILD", NULL);
    CloseHandle(ho);
    CloseHandle(he);
    i64 code = -1;
    if (ok) {
        if (WaitForSingleObject(pi.hProcess, CHILD_LIMIT_MS) == WAIT_TIMEOUT) {
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 10000);
            code = -2;
        } else {
            DWORD c = 0;
            GetExitCodeProcess(pi.hProcess, &c);
            code = (i64)c;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    wchar_t *files[2] = {fo, fe};
    char *bufs[2] = {out, err};
    size_t caps[2] = {out_cap, err_cap};
    for (int i = 0; i < 2; i++) {
        bufs[i][0] = 0;
        FILE *f = _wfopen(files[i], L"rb");
        if (f) {
            size_t n = fread(bufs[i], 1, caps[i] - 1, f);
            bufs[i][n] = 0;
            fclose(f);
        }
        DeleteFileW(files[i]);
    }
    return code;
}

AVP_TEST(fatal_prints_message_and_exits_1) {
    char out[512], err[512];
    i64 code = run_child("fatal", out, sizeof out, err, sizeof err);
    CHECK_EQ_I64(code, 1);
    CHECK_CZ(err, "boom 7: caf\xc3\xa9\r\n");
    CHECK_CZ(out, "before\r\n"); /* stdout is flushed at exit, with CRLF */
}

AVP_TEST(panic_prints_internal_error_line) {
    char out[512], err[512];
    i64 code = run_child("panic", out, sizeof out, err, sizeof err);
    CHECK_EQ_I64(code, 1);
    CHECK(strncmp(err, "test_base: internal error: ", 27) == 0);
    CHECK(strstr(err, "base_test.c:") != NULL);
    CHECK(strstr(err, "assertion failed: 1 + 1 == 3\r\n") != NULL);
}

AVP_TEST(exit_flushes_both_streams_and_keeps_code) {
    char out[512], err[512];
    i64 code = run_child("exit", out, sizeof out, err, sizeof err);
    CHECK_EQ_I64(code, 3);
    CHECK_CZ(out, "hello\r\n");
    CHECK_CZ(err, "warn-side\r\nwarned line\r\n");
}

AVP_TEST(uncaught_throw_is_fatal) {
    char out[512], err[512];
    i64 code = run_child("throw", out, sizeof out, err, sizeof err);
    CHECK_EQ_I64(code, 1);
    CHECK_CZ(err, "uncaught regex\r\n");
}

AVP_TEST(null_dereference_ends_the_process_with_the_exception_code) {
    char out[512], err[512];
    i64 code = run_child("nullptr", out, sizeof out, err, sizeof err);
    CHECK_EQ_I64(code, 0xC0000005);
    CHECK(strncmp(err, "test_base: unhandled exception 0xC0000005 at ", 45) == 0);
}

AVP_TEST(stack_overflow_ends_the_process_with_the_exception_code) {
    char out[512], err[512];
    i64 code = run_child("stack_overflow", out, sizeof out, err, sizeof err);
    CHECK_EQ_I64(code, 0xC00000FD);
    CHECK(strncmp(err, "test_base: unhandled exception 0xC00000FD at ", 45) == 0);
}

AVP_TEST(abort_prints_aborted_and_exits_3) {
    char out[512], err[512];
    i64 code = run_child("abort", out, sizeof out, err, sizeof err);
    CHECK_EQ_I64(code, 3);
    CHECK_CZ(err, "test_base: aborted\r\n");
}

AVP_TEST(invalid_crt_parameter_ends_the_process_with_status_invalid_parameter) {
    char out[2048], err[2048];
    i64 code = run_child("invalid_param", out, sizeof out, err, sizeof err);
    CHECK_EQ_I64(code, 0xC000000D);
    CHECK(strstr(err, "test_base: invalid parameter passed to a C runtime function") != NULL);
}

AVP_TEST(printf_with_a_null_format_is_an_invalid_parameter) {
    char out[2048], err[2048];
    i64 code = run_child("printf_null", out, sizeof out, err, sizeof err);
    CHECK_EQ_I64(code, 0xC000000D);
    CHECK(strstr(err, "test_base: invalid parameter passed to a C runtime function") != NULL);
}

AVP_TEST(failed_debug_assertion_goes_to_stderr_and_does_not_block) {
    char out[2048], err[2048];
    i64 code = run_child("assert", out, sizeof out, err, sizeof err);
    CHECK_EQ_I64(code, 0);
    CHECK(strncmp(out, "after assert\r\n", 14) == 0); /* the probe went on past the assertion */
#ifdef _DEBUG
    CHECK(strstr(err, "Assertion failed") != NULL);
    CHECK(strstr(err, "g_zero != 0") != NULL);
#endif
}

AVP_TEST_MAIN("base")
