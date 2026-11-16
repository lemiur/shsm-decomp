/* tools/test_dolphin.py TestSession: the session file, process identity checks (pid, executable, creation time) and
 * quit, with a harmless sleeper process (ping) standing in for Dolphin. */
#include "dolphin_fixture.h"

#include <windows.h>

typedef struct {
    HANDLE h;
    i64 pid;
    const char *exe;
    i64 created;
} Sleeper;

static Sleeper sleeper(void) {
    Sleeper s = {0};
    wchar_t cmd[] = L"ping.exe -n 60 127.0.0.1";
    STARTUPINFOW si = {sizeof si};
    PROCESS_INFORMATION pi = {0};
    AVP_ASSERT(CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi));
    CloseHandle(pi.hThread);
    s.h = pi.hProcess;
    s.pid = (i64)pi.dwProcessId;
    for (int i = 0; i < 50; i++) {
        if (dolphin_proc_info(avp_arena(), s.pid, &s.exe, &s.created)) return s;
        Sleep(50);
    }
    AVP_ASSERT(!"no process info");
    return s;
}

static void sleeper_kill(Sleeper *s) {
    TerminateProcess(s->h, 1);
    WaitForSingleObject(s->h, 5000);
    CloseHandle(s->h);
}

static bool sleeper_running(const Sleeper *s) { return WaitForSingleObject(s->h, 0) == WAIT_TIMEOUT; }

static void write_session(DolphinEnv *env, i64 pid, const char *exe, i64 created, bool gdb_port) {
    AvpArena *a = avp_arena();
    avp_makedirs(env->dir, true);
    AvpJson *s = avp_json_obj(a);
    avp_json_set(a, s, AVP_S("dolphin_pid"), avp_json_int(a, pid));
    avp_json_set(a, s, AVP_S("dolphin_exe"), avp_json_strz(a, exe));
    avp_json_set(a, s, AVP_S("dolphin_created"), avp_json_int(a, created));
    if (gdb_port) avp_json_set(a, s, AVP_S("gdb_port"), avp_json_int(a, 1));
    dolphin_write_json_atomic(env->session_path, s);
}

AVP_TEST(launch_refuses_a_second_instance) {
    DolphinEnv *env = dolphin_env_new(fx_root("launch2"));
    Sleeper p = sleeper();
    write_session(env, p.pid, p.exe, p.created, true);
    CHECK(dolphin_session_live(env) != NULL);
    FX_RAISES(fx_call(env, "launch", "{\"dolphin\": \"C:\\\\Windows\\\\System32\\\\ping.exe\"}"), SHSM_EXC_USAGE,
              "already live");
    CHECK(sleeper_running(&p));
    sleeper_kill(&p);
}

AVP_TEST(quit_refuses_a_process_it_did_not_start) {
    const char *root = fx_root("quit-refuse");
    DolphinEnv *env = dolphin_env_new(root);
    Sleeper p = sleeper();
    write_session(env, p.pid, avp_path_join(avp_arena(), root, "Dolphin.exe"), p.created, false);
    FX_RAISES(fx_call(env, "quit", NULL), SHSM_EXC_USAGE, "not the Dolphin this tool started");
    Sleep(200);
    CHECK(sleeper_running(&p));
    CHECK(dolphin_session(env) == NULL);
    write_session(env, p.pid, p.exe, p.created + 1, false);
    FX_RAISES(fx_call(env, "quit", NULL), SHSM_EXC_USAGE, NULL);
    CHECK(sleeper_running(&p));
    sleeper_kill(&p);
}

AVP_TEST(quit_terminates_its_own_process) {
    DolphinEnv *env = dolphin_env_new(fx_root("quit-own"));
    Sleeper p = sleeper();
    write_session(env, p.pid, p.exe, p.created, false);
    DolphinResult *r = fx_call(env, "quit", NULL);
    AvpJson *t = avp_json_getz(r->data, "terminated");
    REQUIRE(avp_json_len(t) == 1);
    CHECK_EQ_I64(avp_json_i64(avp_json_at(t, 0), 0), p.pid);
    CHECK(WaitForSingleObject(p.h, 5000) == WAIT_OBJECT_0);
    CHECK(dolphin_session(env) == NULL);
    sleeper_kill(&p);
}

AVP_TEST(stale_session) {
    DolphinEnv *env = dolphin_env_new(fx_root("stale"));
    write_session(env, 0, "x", 0, false);
    CHECK(dolphin_session_live(env) == NULL);
    FX_RAISES(dolphin_target(env), SHSM_EXC_USAGE, "session is gone");
    DolphinResult *r = fx_call(env, "quit", NULL);
    CHECK(strstr(dolphin_result_text(avp_arena(), r, 200).p, "not running") != NULL);
    CHECK(dolphin_session(env) == NULL);
}

AVP_TEST(cli_status_without_session) {
    AvpArena *a = avp_arena();
    const char *root = fx_root("cli-status");
    const char *exe = avp_join(a, avp_exe_dir(), "..", "dolphin.exe");
    const char *argv[] = {exe, "status", "--root", root, "--json"};
    ShsmRun r = shsm_run(a, argv, 5, NULL, 60);
    CHECK_EQ_I64(r.returncode, 0);
    AvpJsonError err;
    AvpJson *doc = avp_json_parse(a, r.out, &err);
    REQUIRE(doc != NULL);
    CHECK(avp_json_has(doc, AVP_S("session")) && avp_json_is_null(avp_json_getz(doc, "session")));
}

AVP_TEST_MAIN("dolphin")
