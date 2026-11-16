/* dolphin.py operations: op_launch .. op_trace, the journal, and stop/breakpoint text. */
#include "dolphin_priv.h"
#include "shsm_asmindex.h"
#include "shsm_mangle.h"
#include "shsm_net.h"

#include <windows.h>
#include <bcrypt.h>

#define LINE(...) dolphin_push(&r->lines, a, avp_sprintf(a, __VA_ARGS__))
#define X32(v) ((unsigned long long)(u32)(v))

static DolphinResult *result(AvpArena *a) {
    DolphinResult *r = AVP_NEW(a, DolphinResult);
    r->data = avp_json_obj(a);
    return r;
}

static AvpJson *session_without_token(AvpArena *a, const AvpJson *s) {
    AvpJson *o = avp_json_obj(a);
    for (size_t i = 0; i < avp_json_len(s); i++) {
        const AvpJsonMember *m = avp_json_member(s, i);
        if (!avp_str_eqz(m->key, "token")) avp_json_set(a, o, m->key, m->val);
    }
    return o;
}

static const char *stop_text(DolphinEnv *env, const ShsmGdbStop *st) {
    AvpArena *a = env->a;
    if (!st) return "no stop recorded";
    if (st->kind == 'W' || st->kind == 'X') return avp_sprintf(a, "the program exited (%c%02x)", st->kind, st->signal);
    const char *where = st->has_pc ? avp_sprintf(a, "pc 0x%08llX %s", X32(st->pc), dolphin_describe(env, st->pc)) : "pc ?";
    return avp_sprintf(a, "stopped (signal %d) at %s", st->signal, where);
}

static AvpJson *stop_json(AvpArena *a, const ShsmGdbStop *st) { return shsm_gdb_stop_to_json(a, st); }

static const char *py_str_json(AvpArena *a, const AvpJson *v) { /* "%s" % value of a JSON scalar */
    if (!v || avp_json_is_null(v)) return "None";
    if (avp_json_is_str(v)) return avp_cz(a, avp_json_str_or(v, AVP_S("")));
    AvpStr t = avp_json_dumps(a, v, NULL);
    if (avp_str_eqz(t, "true")) return "True";
    if (avp_str_eqz(t, "false")) return "False";
    return avp_cz(a, t);
}

/* ---- launch ---- */

static const char *find_disc(DolphinEnv *env) {
    AvpArena *a = env->a;
    const char *root = env->repo->root;
    AvpStr inv;
    if (avp_read_text(a, avp_join(a, root, "orig", "INVENTORY.md"), AVP_TM_UTF8, &inv)) {
        i64 at = avp_str_find(inv, AVP_S("Source:"), 0, AVP_END);
        while (at >= 0) { /* re.search(r"Source:\s*`([^`]+)`") */
            size_t i = (size_t)at + 7;
            for (size_t w; i < inv.n && (w = shsm_ws_len(inv, i)) != 0;) i += w;
            if (i < inv.n && inv.p[i] == '`') {
                i64 end = avp_str_find(inv, AVP_S("`"), (i64)i + 1, AVP_END);
                if (end > (i64)i + 1) {
                    const char *p = avp_join(a, root, avp_cz(a, avp_str_slice(inv, (i64)i + 1, end)));
                    if (avp_isfile(p)) return p;
                    break;
                }
            }
            at = avp_str_find(inv, AVP_S("Source:"), at + 1, AVP_END);
        }
    }
    static const char *exts[] = {".rvz", ".iso", ".wbfs", ".gcz", ".ciso", ".wia"};
    AvpStrVec names = {0}, found = {0};
    if (!avp_listdir(a, root, &names)) shsm_raise_io();
    for (size_t i = 0; i < names.n; i++) {
        AvpStr low = avp_str_lower(a, names.v[i]);
        for (int e = 0; e < 6; e++)
            if (avp_str_endswith(low, avp_str(exts[e]))) {
                avp_vec_push(&found, names.v[i]);
                break;
            }
    }
    if (found.n != 1)
        shsm_raise(SHSM_EXC_USAGE,
                   "expected one disc image (.rvz/.iso/.wbfs/.gcz/.ciso/.wia) in the repository root, found %zu; pass --disc",
                   found.n);
    return avp_join(a, root, avp_cz(a, found.v[0]));
}

static bool path_under(AvpArena *a, const char *user, const char *base) {
    const char *u = avp_path_normcase(a, user);
    const char *b = avp_path_normcase(a, avp_path_abspath(a, base));
    size_t n = strlen(b);
    return strcmp(u, b) == 0 || (strncmp(u, b, n) == 0 && u[n] == '\\');
}

void dolphin_guard_user_dir(DolphinEnv *env, const char *user) {
    AvpArena *a = env->a;
    const char *u = avp_path_normcase(a, user);
    const char *d = avp_sprintf(a, "%s\\", avp_path_normcase(a, avp_path_abspath(a, env->dir)));
    if (strncmp(u, d, strlen(d)) != 0)
        shsm_raise(SHSM_EXC_USAGE, "refusing a Dolphin user directory outside build/dolphin: %s", user);
    const char *own[2] = {avp_join(a, avp_getenv_or(a, "APPDATA", ""), "Dolphin Emulator"),
                          avp_join(a, avp_getenv_or(a, "USERPROFILE", ""), "Documents", "Dolphin Emulator")};
    for (int i = 0; i < 2; i++)
        if (path_under(a, user, own[i]))
            shsm_raise(SHSM_EXC_USAGE, "refusing the user's own Dolphin configuration: %s", user);
}

static const char PRIVATE_INI[] =
    "[General]\nGDBPort = %d\n[Core]\nGFXBackend = Null\nCPUCore = 1\nDSPHLE = True\nSkipIPL = True\n[DSP]\n"
    "Backend = No Audio Output\n[Interface]\nDebugModeEnabled = True\nConfirmStop = False\nUsePanicHandlers = False\n"
    "OnScreenDisplayMessages = False\nPauseOnFocusLost = False\n[Analytics]\nEnabled = False\nPermissionAsked = True\n"
    "[AutoUpdate]\nUpdateTrack =\n";
static const char LOGGER_INI[] = "[Logs]\nGDB_STUB = True\nBOOT = True\n[Options]\nVerbosity = 4\nWriteToFile = True\n"
                                 "WriteToConsole = False\nWriteToWindow = False\n";

static void write_private_config(AvpArena *a, const char *user, int port) {
    const char *cfg = avp_join(a, user, "Config");
    if (!avp_makedirs(cfg, true)) shsm_raise_io();
    if (!avp_write_text(avp_join(a, cfg, "Dolphin.ini"), avp_str(avp_sprintf(a, PRIVATE_INI, port)), AVP_TM_UTF8) ||
        !avp_write_text(avp_join(a, cfg, "Logger.ini"), avp_str(LOGGER_INI), AVP_TM_UTF8))
        shsm_raise_io();
}

static const char *token_hex(AvpArena *a) {
    u8 b[16];
    BCryptGenRandom(NULL, b, sizeof b, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return dolphin_hex(a, b, sizeof b);
}

static const char *self_exe(AvpArena *a) {
    wchar_t buf[32768];
    DWORD n = GetModuleFileNameW(NULL, buf, 32768);
    return avp_cz(a, avp_from_w(a, buf, (i64)n));
}

static const char *float_text(AvpArena *a, double v) { /* str(float) */
    char buf[32];
    avp_float_repr(v, buf);
    return avp_strdup(a, buf);
}

static DolphinResult *op_launch(DolphinEnv *env, double wait_boot, const char *dolphin, const char *disc) {
    AvpArena *a = env->a;
    AvpJson *live = dolphin_session_live(env);
    if (live)
        shsm_raise(SHSM_EXC_USAGE, "a Dolphin session is already live (pid %lld, port %lld); `quit` it first",
                   (long long)avp_json_get_i64(live, "dolphin_pid", 0), (long long)avp_json_get_i64(live, "gdb_port", 0));
    if (dolphin_session(env)) avp_remove(env->session_path);
    avp_makedirs(env->dir, true);
    AvpJson *settings = dolphin_read_json(a, env->settings_path);
    if (!avp_json_truthy(settings)) settings = avp_json_obj(a);
    const char *exe = dolphin;
    if (!exe || !*exe) exe = avp_getenv(a, "SHSM_DOLPHIN");
    if (!exe || !*exe) {
        AvpJson *e = avp_json_getz(settings, "exe");
        exe = avp_json_truthy(e) ? py_str_json(a, e) : NULL;
    }
    if (!exe || !*exe)
        shsm_raise(SHSM_EXC_USAGE, "no Dolphin executable: pass --dolphin PATH\\Dolphin.exe once (remembered in %s)",
                   shsm_repo_rel(a, env->repo, env->settings_path));
    exe = avp_path_abspath(a, exe);
    if (!avp_isfile(exe)) shsm_raise(SHSM_EXC_USAGE, "no Dolphin executable at %s", exe);
    if (dolphin && *dolphin) {
        DJ_SET(a, settings, "exe", DJ_STR(a, exe));
        dolphin_write_json_atomic(env->settings_path, settings);
    }
    disc = disc && *disc ? avp_path_abspath(a, disc) : find_disc(env);
    const char *user = avp_path_abspath(a, env->user_dir);
    dolphin_guard_user_dir(env, user);
    int gdb_port = shsm_net_free_port();
    write_private_config(a, user, gdb_port);
    double t0 = avp_monotonic();
    const char *dargv[] = {exe, "--user", user, "--batch", "--exec", disc};
    void *proc_h = NULL;
    i64 pid = dolphin_spawn_detached(dargv, 6, NULL, avp_cz(a, avp_path_dirname(exe)), &proc_h);
    const char *pexe = NULL;
    i64 created = 0;
    bool info = false;
    for (int i = 0; i < 50 && !info; i++) {
        info = dolphin_proc_info(a, pid, &pexe, &created);
        if (!info) avp_sleep(0.1);
    }
    if (!info) {
        DWORD code = STILL_ACTIVE;
        GetExitCodeProcess((HANDLE)proc_h, &code);
        CloseHandle((HANDLE)proc_h);
        shsm_raise(SHSM_EXC_USAGE, "Dolphin exited at once (exit code %s)",
                   code == STILL_ACTIVE ? "None" : avp_sprintf(a, "%lu", (unsigned long)code));
    }
    CloseHandle((HANDLE)proc_h);
    AvpJson *session = avp_json_obj(a);
    DJ_SET(a, session, "dolphin_pid", DJ_INT(a, pid));
    DJ_SET(a, session, "dolphin_exe", DJ_STR(a, pexe));
    DJ_SET(a, session, "dolphin_created", DJ_INT(a, created));
    DJ_SET(a, session, "gdb_port", DJ_INT(a, gdb_port));
    DJ_SET(a, session, "broker_pid", avp_json_null());
    DJ_SET(a, session, "broker_port", avp_json_null());
    DJ_SET(a, session, "token", DJ_STR(a, token_hex(a)));
    DJ_SET(a, session, "disc", DJ_STR(a, disc));
    DJ_SET(a, session, "user_dir", DJ_STR(a, user));
    DJ_SET(a, session, "started", DJ_STR(a, dolphin_now(a)));
    dolphin_write_json_atomic(env->session_path, session);
    const char *ready_path = avp_join(a, env->dir, "broker.json");
    if (avp_exists(ready_path)) avp_remove(ready_path);
    const char *bargv[] = {self_exe(a), "_broker", "--root", env->repo->root, "--gdb-port",
                           avp_sprintf(a, "%d", gdb_port), "--wait", float_text(a, wait_boot)};
    void *broker_h = NULL;
    i64 bpid = dolphin_spawn_detached(bargv, 8, avp_join(a, env->dir, "broker.log"), env->repo->root, &broker_h);
    double end = avp_monotonic() + wait_boot + 10;
    AvpJson *ready = NULL;
    while (avp_monotonic() < end) {
        ready = dolphin_read_json(a, ready_path);
        if (ready && (avp_json_truthy(avp_json_getz(ready, "ready")) || avp_json_truthy(avp_json_getz(ready, "error"))))
            break;
        if (WaitForSingleObject((HANDLE)broker_h, 0) == WAIT_OBJECT_0 || !dolphin_proc_info(a, pid, NULL, NULL)) break;
        avp_sleep(0.25);
    }
    CloseHandle((HANDLE)broker_h);
    if (!ready || !avp_json_truthy(avp_json_getz(ready, "ready"))) {
        AvpJson *err = ready ? avp_json_getz(ready, "error") : NULL;
        const char *why = avp_json_truthy(err)
                              ? py_str_json(a, err)
                              : avp_sprintf(a, "the GDB stub did not accept a connection within %ss", float_text(a, wait_boot));
        dolphin_terminate(bpid, 3);
        dolphin_terminate(pid, 10);
        avp_remove(env->session_path);
        shsm_raise(SHSM_EXC_USAGE, "launch failed: %s (Dolphin terminated; see %s)", why,
                   shsm_repo_rel(a, env->repo, avp_join(a, env->dir, "broker.log")));
    }
    const char *bexe = NULL;
    i64 bcreated = 0;
    bool binfo = dolphin_proc_info(a, bpid, &bexe, &bcreated);
    DJ_SET(a, session, "broker_pid", DJ_INT(a, bpid));
    DJ_SET(a, session, "broker_port", avp_json_getz(ready, "port"));
    DJ_SET(a, session, "broker_exe", binfo ? DJ_STR(a, bexe) : avp_json_null());
    DJ_SET(a, session, "broker_created", binfo ? DJ_INT(a, bcreated) : avp_json_null());
    DJ_SET(a, session, "token", avp_json_getz(ready, "token"));
    dolphin_write_json_atomic(env->session_path, session);
    double secs = avp_monotonic() - t0;
    env->target = NULL;
    ShsmGdbState st = shsm_target_state(a, dolphin_target(env));
    DolphinResult *r = result(a);
    LINE("launched Dolphin pid %lld (%s)", (long long)pid, exe);
    LINE("disc %s", avp_cz(a, avp_path_basename(disc)));
    LINE("GDB stub on port %d connected after %s s; broker pid %lld", gdb_port, avp_pyfmt(a, "%.1f", secs).p,
         (long long)bpid);
    dolphin_push(&r->lines, a, stop_text(env, st.stop));
    dolphin_push(&r->lines, a, "the CPU waits; `cont` starts it");
    DJ_SET(a, r->data, "session", session_without_token(a, session));
    DJ_SET(a, r->data, "stop", stop_json(a, st.stop));
    DJ_SET(a, r->data, "seconds", avp_json_float(a, avp_py_round(secs, 1)));
    return r;
}

/* ---- quit, status ---- */

static DolphinResult *op_quit(DolphinEnv *env) {
    AvpArena *a = env->a;
    DolphinResult *r = result(a);
    AvpJson *s = dolphin_session(env);
    AvpJson *done = avp_json_arr(a);
    DJ_SET(a, r->data, "terminated", done);
    if (!s) {
        dolphin_push(&r->lines, a, "no session recorded; nothing to do");
        return r;
    }
    i64 dpid = avp_json_get_i64(s, "dolphin_pid", 0);
    const char *dpid_text = py_str_json(a, avp_json_getz(s, "dolphin_pid"));
    if (avp_json_truthy(avp_json_getz(s, "broker_port")) &&
        dolphin_proc_info(a, avp_json_get_i64(s, "broker_pid", 0), NULL, NULL)) {
        AvpTry t;
        if (AVP_TRY(&t)) {
            AvpJson *tok = avp_json_getz(s, "token");
            ShsmBrokerClient *c = shsm_broker_client_new((int)avp_json_get_i64(s, "broker_port", 0),
                                                         py_str_json(a, tok), NULL, 60.0);
            shsm_broker_client_shutdown(a, c);
            shsm_broker_client_free(c);
            avp_try_end(&t);
        } else if (!shsm_exc_is_gdb(t.code)) {
            avp_throw(t.code, "%s", t.msg);
        }
    }
    bool running = false;
    const char *exe = NULL;
    bool same = false;
    {
        i64 created = 0;
        running = dolphin_proc_info(a, dpid, &exe, &created);
        AvpJson *c = avp_json_getz(s, "dolphin_created");
        AvpJson *e = avp_json_getz(s, "dolphin_exe");
        const char *se = avp_json_is_str(e) ? avp_cz(a, avp_json_str_or(e, AVP_S(""))) : "";
        same = running && !strcmp(avp_path_normcase(a, avp_path_abspath(a, exe)), avp_path_normcase(a, avp_path_abspath(a, se))) &&
               avp_json_is_int(c) && avp_json_i64(c, 0) == created;
    }
    if (!running) {
        LINE("Dolphin pid %s is not running", dpid_text);
    } else if (!same) {
        avp_remove(env->session_path);
        shsm_raise(SHSM_EXC_USAGE,
                   "pid %s is now %s, not the Dolphin this tool started; nothing terminated (stale session cleared)",
                   dpid_text, exe);
    } else {
        bool ok = dolphin_terminate(dpid, 10);
        if (ok) LINE("terminated Dolphin pid %lld", (long long)dpid);
        else LINE("Dolphin pid %lld did not exit", (long long)dpid);
        if (ok) avp_json_arr_push(a, done, DJ_INT(a, dpid));
    }
    i64 bpid = avp_json_get_i64(s, "broker_pid", 0);
    if (bpid) {
        double end = avp_monotonic() + 5;
        while (avp_monotonic() < end && dolphin_proc_info(a, bpid, NULL, NULL)) avp_sleep(0.1);
        const char *bexe = NULL;
        i64 bcr = 0;
        if (dolphin_proc_info(a, bpid, &bexe, &bcr)) {
            AvpJson *c = avp_json_getz(s, "broker_created");
            AvpJson *e = avp_json_getz(s, "broker_exe");
            const char *se = avp_json_is_str(e) ? avp_cz(a, avp_json_str_or(e, AVP_S(""))) : "";
            if (!strcmp(avp_path_normcase(a, avp_path_abspath(a, bexe)), avp_path_normcase(a, avp_path_abspath(a, se))) &&
                avp_json_is_int(c) && avp_json_i64(c, 0) == bcr)
                dolphin_terminate(bpid, 3);
        }
        if (!dolphin_proc_info(a, bpid, NULL, NULL)) {
            LINE("broker pid %lld ended", (long long)bpid);
            avp_json_arr_push(a, done, DJ_INT(a, bpid));
        }
    }
    if (!dolphin_proc_info(a, dpid, NULL, NULL)) {
        avp_remove(env->session_path);
        dolphin_push(&r->lines, a, "session closed");
        return r;
    }
    r->code = SHSM_EXIT_FINDING;
    return r;
}

static const char *bp_text(DolphinEnv *env, const ShsmGdbBreakpoint *b) {
    static const char *names[] = {"", "hw ", "watch-w ", "watch-r ", "watch-a "};
    const char *t = b->type >= 0 && b->type <= 4 ? names[b->type] : "?";
    return avp_sprintf(env->a, "%s%s", t, dolphin_describe(env, (u64)b->address));
}

AvpJson *dolphin_journal_entries(DolphinEnv *env) {
    AvpArena *a = env->a;
    AvpJson *writes = avp_json_arr(a);
    AvpStr text;
    if (!avp_read_text(a, env->journal_path, AVP_TM_UTF8, &text)) return writes;
    AvpJson *out = avp_json_arr(a);
    AvpStrVec lines = avp_str_splitlines(text, false);
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr l = avp_str_strip(lines.v[i]);
        if (!l.n) continue;
        AvpJsonError err;
        AvpJson *e = avp_json_parse(a, l, &err);
        if (e) avp_json_arr_push(a, out, e);
    }
    avp_vec_free(&lines);
    AvpI64Vec undone = {0};
    for (size_t i = 0; i < avp_json_len(out); i++) {
        AvpJson *e = avp_json_at(out, i);
        if (avp_json_has(e, AVP_S("undo"))) avp_vec_push(&undone, avp_json_get_i64(e, "undo", 0));
    }
    for (size_t i = 0; i < avp_json_len(out); i++) {
        AvpJson *e = avp_json_at(out, i);
        if (avp_json_has(e, AVP_S("undo"))) continue;
        i64 id = avp_json_get_i64(e, "id", 0);
        bool u = false;
        for (size_t j = 0; j < undone.n && !u; j++) u = undone.v[j] == id;
        DJ_SET(a, e, "undone", avp_json_bool(u));
        avp_json_arr_push(a, writes, e);
    }
    avp_vec_free(&undone);
    return writes;
}

static DolphinResult *op_status(DolphinEnv *env) {
    AvpArena *a = env->a;
    DolphinResult *r = result(a);
    AvpJson *s = dolphin_session(env);
    if (!s) {
        dolphin_push(&r->lines, a, "no session (launch starts one)");
        DJ_SET(a, r->data, "session", avp_json_null());
        return r;
    }
    bool live = dolphin_session_live(env) != NULL;
    DJ_SET(a, r->data, "session", session_without_token(a, s));
    DJ_SET(a, r->data, "live", avp_json_bool(live));
    LINE("session: Dolphin pid %s, GDB port %s, broker pid %s port %s, started %s",
         py_str_json(a, avp_json_getz(s, "dolphin_pid")), py_str_json(a, avp_json_getz(s, "gdb_port")),
         py_str_json(a, avp_json_getz(s, "broker_pid")), py_str_json(a, avp_json_getz(s, "broker_port")),
         py_str_json(a, avp_json_getz(s, "started")));
    LINE("Dolphin %s", live ? "running" : "NOT running (run quit)");
    if (live) {
        ShsmGdbState st = shsm_target_state(a, dolphin_target(env));
        DJ_SET(a, r->data, "state", shsm_gdb_state_to_json(a, &st));
        if (st.running) dolphin_push(&r->lines, a, "CPU running");
        else LINE("CPU %s", stop_text(env, st.stop));
        if (st.nbps) {
            AvpBuf b;
            avp_buf_init(&b, a);
            for (size_t i = 0; i < st.nbps; i++) avp_buf_printf(&b, "%s%s", i ? ", " : "", bp_text(env, &st.bps[i]));
            LINE("breakpoints: %s", avp_cz(a, avp_buf_str(&b)));
        }
    }
    AvpJson *j = dolphin_journal_entries(env);
    size_t pending = 0;
    for (size_t i = 0; i < avp_json_len(j); i++)
        if (!avp_json_get_bool(avp_json_at(j, i), "undone", false)) pending++;
    LINE("journal: %zu writes, %zu not undone", avp_json_len(j), pending);
    return r;
}

/* ---- registers, memory ---- */

typedef struct {
    u64 pc, msr, cr, lr, ctr, xer;
} Special;

static Special read_special(DolphinEnv *env) {
    static const i64 nums[6] = {SHSM_GDB_REG_PC, SHSM_GDB_REG_MSR, SHSM_GDB_REG_CR,
                                SHSM_GDB_REG_LR, SHSM_GDB_REG_CTR, SHSM_GDB_REG_XER};
    u64 v[6];
    shsm_target_read_registers(dolphin_target(env), nums, 6, v);
    return (Special){v[0], v[1], v[2], v[3], v[4], v[5]};
}

static AvpJson *special_json(AvpArena *a, const Special *s) {
    AvpJson *o = avp_json_obj(a);
    DJ_SET(a, o, "pc", DJ_INT(a, s->pc));
    DJ_SET(a, o, "msr", DJ_INT(a, s->msr));
    DJ_SET(a, o, "cr", DJ_INT(a, s->cr));
    DJ_SET(a, o, "lr", DJ_INT(a, s->lr));
    DJ_SET(a, o, "ctr", DJ_INT(a, s->ctr));
    DJ_SET(a, o, "xer", DJ_INT(a, s->xer));
    return o;
}

static ShsmGdbState require_stopped(DolphinEnv *env) {
    ShsmGdbState st = shsm_target_state(env->a, dolphin_target(env));
    if (st.running) shsm_raise(SHSM_EXC_USAGE, "the CPU is running; `pause` it (or wait at a breakpoint) first");
    return st;
}

static DolphinResult *op_regs(DolphinEnv *env, bool fpr) {
    AvpArena *a = env->a;
    require_stopped(env);
    ShsmGdbTarget *t = dolphin_target(env);
    u64 g[32];
    size_t ng = shsm_target_read_gprs(t, g);
    Special sp = read_special(env);
    DolphinResult *r = result(a);
    LINE("pc  0x%08llX %s", X32(sp.pc), dolphin_describe(env, sp.pc));
    LINE("lr  0x%08llX %s", X32(sp.lr), dolphin_describe(env, sp.lr));
    LINE("ctr 0x%08llX  msr 0x%08llX  cr 0x%08llX  xer 0x%08llX", X32(sp.ctr), X32(sp.msr), X32(sp.cr), X32(sp.xer));
    for (size_t i = 0; i < 32; i += 4) {
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t j = 0; j < 4 && i + j < ng; j++)
            avp_buf_printf(&b, "%sr%-2zu %08llX", j ? "  " : "", i + j, (unsigned long long)g[i + j]);
        dolphin_push(&r->lines, a, avp_cz(a, avp_buf_str(&b)));
    }
    AvpJson *gj = avp_json_arr(a);
    for (size_t i = 0; i < ng; i++) avp_json_arr_push(a, gj, DJ_INT(a, g[i]));
    DJ_SET(a, r->data, "gpr", gj);
    DJ_SET(a, r->data, "special", special_json(a, &sp));
    if (fpr) {
        i64 nums[32];
        u64 f[32];
        for (int i = 0; i < 32; i++) nums[i] = 0x20 + i;
        shsm_target_read_registers(t, nums, 32, f);
        AvpJson *fj = avp_json_arr(a);
        for (int i = 0; i < 32; i++) avp_json_arr_push(a, fj, DJ_STR(a, dolphin_f64(a, f[i])));
        DJ_SET(a, r->data, "fpr", fj);
        for (int i = 0; i < 32; i += 4) {
            AvpBuf b;
            avp_buf_init(&b, a);
            for (int j = 0; j < 4; j++) avp_buf_printf(&b, "%sf%-2d %-12s", j ? "  " : "", i + j, dolphin_f64(a, f[i + j]));
            dolphin_push(&r->lines, a, avp_cz(a, avp_buf_str(&b)));
        }
    }
    return r;
}

static DolphinResult *op_mem(DolphinEnv *env, AvpStr address, i64 length, const char *view, bool compare_dol) {
    AvpArena *a = env->a;
    u64 addr = dolphin_resolve(env, address, NULL, NULL);
    if (!strcmp(view, "str") && length == 64) length = 256;
    ShsmBytes data = shsm_target_read_memory(a, dolphin_target(env), (i64)addr, length);
    DolphinResult *r = result(a);
    LINE("0x%08llX %s, 0x%llX bytes", X32(addr), dolphin_describe(env, addr), (unsigned long long)length);
    AvpJson *doc = r->data;
    DJ_SET(a, doc, "address", DJ_INT(a, addr));
    DJ_SET(a, doc, "length", DJ_INT(a, length));
    DJ_SET(a, doc, "hex", DJ_STR(a, dolphin_hex(a, data.p, data.n)));
    DJ_SET(a, doc, "view", DJ_STR(a, view));
    if (!strcmp(view, "str")) {
        size_t n = 0;
        while (n < data.n && data.p[n]) n++;
        AvpStr txt = AVP_S("");
        if (n) avp_decode_text(a, (AvpStr){(const char *)data.p, n}, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_REPLACE, AVP_NL_EMPTY), &txt);
        dolphin_push(&r->lines, a, dolphin_json_text(a, txt));
        DJ_SET(a, doc, "text", avp_json_str(a, txt));
    } else if (!strcmp(view, "u8")) {
        for (size_t i = 0; i < data.n; i += 16) {
            char hex[64] = {0}, asc[17] = {0};
            size_t k = 0;
            for (size_t j = 0; j < 16 && i + j < data.n; j++) {
                u8 c = data.p[i + j];
                k += (size_t)snprintf(hex + k, sizeof hex - k, "%s%02X", j ? " " : "", c);
                asc[j] = c >= 32 && c < 127 ? (char)c : '.';
            }
            LINE("%08llX  %-47s  %s", X32(addr + i), hex, asc);
        }
    } else {
        size_t w = !strcmp(view, "u16") ? 2 : 4, n = data.n / w, per = w == 2 ? 8 : 4;
        AvpJson *vals = avp_json_arr(a);
        for (size_t i = 0; i < n; i++)
            avp_json_arr_push(a, vals, DJ_INT(a, w == 2 ? dolphin_be16(data.p + 2 * i) : dolphin_be32(data.p + 4 * i)));
        for (size_t i = 0; i < n; i += per) {
            if (!strcmp(view, "ptr")) {
                for (size_t j = 0; j < per && i + j < n; j++)
                    LINE("%08llX  %s", X32(addr + (i + j) * 4), dolphin_fmt_value(env, dolphin_be32(data.p + 4 * (i + j))));
                continue;
            }
            AvpBuf b;
            avp_buf_init(&b, a);
            for (size_t j = 0; j < per && i + j < n; j++) {
                if (j) avp_buf_put(&b, AVP_S("  "));
                if (!strcmp(view, "float")) avp_buf_printf(&b, "%-12s", dolphin_f32(a, dolphin_be32(data.p + 4 * (i + j))));
                else if (w == 2) avp_buf_printf(&b, "%04X", dolphin_be16(data.p + 2 * (i + j)));
                else avp_buf_printf(&b, "%08X", dolphin_be32(data.p + 4 * (i + j)));
            }
            LINE("%08llX  %s", X32(addr + i * w), avp_cz(a, avp_buf_str(&b)));
        }
        DJ_SET(a, doc, "values", vals);
    }
    if (compare_dol) {
        const u8 *ref = dolphin_dol_read(env, addr, (u64)length);
        if (!ref) {
            LINE("the retail DOL has no bytes at this range (or %s is missing)",
                 shsm_repo_rel(a, env->repo, env->repo->retail_dol));
            DJ_SET(a, doc, "dol_equal", avp_json_null());
            r->code = SHSM_EXIT_FINDING;
        } else {
            size_t ndiff = 0, first = 0;
            for (size_t i = 0; i < (size_t)length && i < data.n; i++)
                if (ref[i] != data.p[i]) {
                    if (!ndiff) first = i;
                    ndiff++;
                }
            DJ_SET(a, doc, "dol_equal", avp_json_bool(!ndiff));
            if (ndiff) {
                LINE("differs from the retail DOL at %zu bytes, first +0x%zX (live %02X, DOL %02X)", ndiff, first,
                     data.p[first], ref[first]);
                r->code = SHSM_EXIT_FINDING;
            } else {
                LINE("equal to the retail DOL bytes at 0x%08llX..0x%08llX", X32(addr),
                     (unsigned long long)(addr + (u64)length));
            }
        }
    }
    return r;
}

/* ---- read ---- */

typedef struct {
    DolphinEnv *env;
    DolphinResult *r;
    AvpJson *fields;
    const u8 *live, *ref;
} ReadCtx;

static void read_row(ReadCtx *c, u64 off, u64 width, const char *kind, const char *text) {
    AvpArena *a = c->env->a;
    DolphinResult *r = c->r;
    bool changed = c->ref && memcmp(c->live + off, c->ref + off, (size_t)width) != 0;
    LINE("%s +0x%-4llX %-7s %s", changed ? "*" : " ", (unsigned long long)off, kind, text);
    AvpJson *f = avp_json_obj(a);
    DJ_SET(a, f, "offset", DJ_INT(a, off));
    DJ_SET(a, f, "width", DJ_INT(a, width));
    DJ_SET(a, f, "kind", DJ_STR(a, kind));
    DJ_SET(a, f, "value", DJ_STR(a, text));
    DJ_SET(a, f, "changed", avp_json_bool(changed));
    DJ_SET(a, f, "hex", DJ_STR(a, dolphin_hex(a, c->live + off, (size_t)width)));
    avp_json_arr_push(a, c->fields, f);
}

static DolphinResult *op_read(DolphinEnv *env, AvpStr symbol) {
    AvpArena *a = env->a;
    const ShsmSymbol *sym = NULL;
    u64 addr = dolphin_resolve(env, symbol, "object", &sym);
    if (!sym || !sym->has_size || !sym->size)
        shsm_raise(SHSM_EXC_USAGE, "%.*s has no size in symbols.txt; use `mem`", AVP_SF(symbol));
    u64 size = sym->size;
    ShsmBytes live = shsm_target_read_memory(a, dolphin_target(env), (i64)addr, (i64)size);
    const u8 *ref = dolphin_dol_read(env, addr, size);
    const ShsmAsmItem **items = NULL; /* the object's items at or after addr */
    size_t nitems = 0;
    ShsmAsmIndex *ix = dolphin_ix(env);
    if (ix) {
        AvpTry t;
        if (AVP_TRY(&t)) {
            ShsmAsmFile *file = NULL;
            ShsmTarget tg = sym->address == addr ? shsm_target_name(avp_str(sym->name)) : shsm_target_addr((i64)addr);
            ShsmAsmBlock *b = shsm_asmindex_object(a, ix, tg, &file);
            if (b && b->items.n) {
                items = AVP_NEWN(a, const ShsmAsmItem *, b->items.n);
                for (size_t i = 0; i < b->items.n; i++)
                    if ((u64)b->items.v[i].address >= addr) items[nitems++] = &b->items.v[i];
            }
            avp_try_end(&t);
        } else {
            nitems = 0;
        }
    }
    DolphinResult *r = result(a);
    LINE("%s  %s 0x%08llX size 0x%llX%s", sym->name, sym->section, X32(addr), (unsigned long long)size,
         ref ? "  (* = differs from the retail DOL)" : "");
    ReadCtx c = {env, r, avp_json_arr(a), live.p, ref};
    typedef struct {
        const char *kind;
        u64 off, width;
    } Triple;
    AVP_VEC(Triple) tr = {0};
    if (!nitems) {
        const char *kind = sym->data && *sym->data ? sym->data : "4byte";
        if (!strcmp(kind, "string")) {
            avp_vec_push(&tr, ((Triple){"string", 0, size}));
        } else {
            u64 w = !strcmp(kind, "float") ? 4 : !strcmp(kind, "double") ? 8 : !strcmp(kind, "byte") ? 1
                    : !strcmp(kind, "2byte") ? 2 : 4;
            bool known = !strcmp(kind, "float") || !strcmp(kind, "double") || !strcmp(kind, "byte") || !strcmp(kind, "2byte");
            for (u64 o = 0; o + w <= size - size % w; o += w) avp_vec_push(&tr, ((Triple){known ? kind : "4byte", o, w}));
            if (size % w) avp_vec_push(&tr, ((Triple){"byte", size - size % w, size % w}));
        }
    } else {
        for (size_t i = 0; i < nitems; i++)
            avp_vec_push(&tr, ((Triple){items[i]->kind, (u64)items[i]->address - addr,
                                        items[i]->size > 0 ? (u64)items[i]->size : 0})); /* it.size or 0 */
    }
    for (size_t i = 0; i < tr.n; i++) {
        const char *kind = tr.v[i].kind;
        u64 off = tr.v[i].off, width = tr.v[i].width;
        if (off >= size) continue;
        width = width ? (width < size - off ? width : size - off) : 4;
        const u8 *chunk = live.p + off;
        size_t avail = off < live.n ? live.n - (size_t)off : 0; /* a chunk past the read is short, as a slice */
        if (width > avail) width = avail;
        if (!strcmp(kind, "skip") && width % 4) {
            read_row(&c, off, width, "u8", dolphin_hex(a, chunk, (size_t)width));
        } else if (!strcmp(kind, "string") || !strcmp(kind, "string16")) {
            size_t n = 0;
            while (n < width && chunk[n]) n++;
            read_row(&c, off, width, "string", dolphin_json_text(a, avp_str(dolphin_latin1_to_utf8(a, chunk, n))));
        } else if (!strcmp(kind, "float") && width >= 4) {
            for (u64 o = 0; o + 3 < width; o += 4) read_row(&c, off + o, 4, "float", dolphin_f32(a, dolphin_be32(chunk + o)));
        } else if (!strcmp(kind, "double") && width >= 8) {
            u64 bits = (u64)dolphin_be32(chunk) << 32 | dolphin_be32(chunk + 4);
            read_row(&c, off, 8, "double", dolphin_f64(a, bits));
        } else if (!strcmp(kind, "2byte")) {
            for (u64 o = 0; o + 1 < width; o += 2)
                read_row(&c, off + o, 2, "u16", avp_sprintf(a, "0x%04X", dolphin_be16(chunk + o)));
        } else if (!strcmp(kind, "byte")) {
            AvpBuf b;
            avp_buf_init(&b, a);
            for (u64 o = 0; o < width; o++) avp_buf_printf(&b, "%s%02X", o ? " " : "", chunk[o]);
            read_row(&c, off, width, "u8", avp_cz(a, avp_buf_str(&b)));
        } else {
            bool p = !strcmp(kind, "ptr") || !strcmp(kind, "rel");
            for (u64 o = 0; o + 3 < width; o += 4)
                read_row(&c, off + o, 4, p ? "ptr" : "u32", dolphin_fmt_value(env, dolphin_be32(chunk + o)));
            if (width % 4)
                read_row(&c, off + width - width % 4, width % 4, "u8",
                         dolphin_hex(a, chunk + width - width % 4, (size_t)(width % 4)));
        }
    }
    avp_vec_free(&tr);
    DJ_SET(a, r->data, "symbol", DJ_STR(a, sym->name));
    DJ_SET(a, r->data, "address", DJ_INT(a, addr));
    DJ_SET(a, r->data, "size", DJ_INT(a, size));
    DJ_SET(a, r->data, "hex", DJ_STR(a, dolphin_hex(a, live.p, live.n)));
    DJ_SET(a, r->data, "fields", c.fields);
    return r;
}

/* ---- writes and the journal ---- */

static void journal_append(DolphinEnv *env, const AvpJson *entry) {
    AvpArena *a = env->a;
    avp_makedirs(env->dir, true);
    AvpJsonDumpOpts o = {0};
    AvpStr line = avp_json_dumps(a, entry, &o);
    FILE *f = _wfopen(avp_to_wz(a, env->journal_path), L"ab");
    if (!f) shsm_raise(SHSM_EXC_OS, "cannot open %s", env->journal_path);
    fwrite(line.p, 1, line.n, f);
    fputc('\n', f);
    fclose(f);
}

static bool fromhex(AvpArena *a, AvpStr s, ShsmBytes *out) {
    u8 *b = (u8 *)avp_alloc(a, s.n / 2 + 1);
    size_t n = 0, i = 0;
    while (i < s.n) {
        if (isspace((unsigned char)s.p[i])) {
            i++;
            continue;
        }
        if (i + 1 >= s.n || !isxdigit((unsigned char)s.p[i]) || !isxdigit((unsigned char)s.p[i + 1])) {
            size_t pos = (i + 1 >= s.n || isxdigit((unsigned char)s.p[i])) ? i + 1 : i;
            shsm_raise(SHSM_EXC_VALUE, "non-hexadecimal number found in fromhex() arg at position %zu", pos);
        }
        char h[3] = {s.p[i], s.p[i + 1], 0};
        b[n++] = (u8)strtoul(h, NULL, 16);
        i += 2;
    }
    out->p = b;
    out->n = n;
    return true;
}

static ShsmBytes encode_value(AvpArena *a, AvpStr value, const char *view) {
    AvpStr v = avp_str_strip(value);
    ShsmBytes out = {0};
    if (!strcmp(view, "bytes")) {
        AvpStr t = avp_str_replace(a, avp_str_replace(a, v, AVP_S(" "), AVP_S(""), -1), AVP_S("0x"), AVP_S(""), -1);
        fromhex(a, t, &out);
        if (!out.n) shsm_raise(SHSM_EXC_USAGE, "no bytes in %s", avp_repr_str(a, value).p);
        return out;
    }
    u8 *b = (u8 *)avp_alloc(a, 8);
    if (!strcmp(view, "float")) {
        double d;
        if (!avp_parse_float(v, &d))
            shsm_raise(SHSM_EXC_VALUE, "could not convert string to float: %s", avp_repr_str(a, v).p);
        float f = (float)d;
        if (isinf(f) && !isinf(d)) shsm_raise(SHSM_EXC_VALUE, "float too large to pack with f format");
        u32 bits;
        memcpy(&bits, &f, 4);
        for (int i = 0; i < 4; i++) b[i] = (u8)(bits >> (24 - 8 * i));
        out.p = b, out.n = 4;
        return out;
    }
    i64 n = 0;
    bool overflow = false;
    if (!avp_parse_int(v, 0, &n, &overflow)) {
        if (overflow) shsm_raise(SHSM_EXC_USAGE, "%.*s does not fit in %s", AVP_SF(value), view);
        shsm_raise(SHSM_EXC_VALUE, "invalid literal for int() with base 0: %s", avp_repr_str(a, v).p);
    }
    int w = !strcmp(view, "u8") ? 1 : !strcmp(view, "u16") ? 2 : 4;
    u64 lim = (u64)1 << (8 * w);
    if (n < 0) n = (i64)((u64)n & (lim - 1));
    if ((u64)n >= lim) shsm_raise(SHSM_EXC_USAGE, "%.*s does not fit in %s", AVP_SF(value), view);
    for (int i = 0; i < w; i++) b[i] = (u8)((u64)n >> (8 * (w - 1 - i)));
    out.p = b, out.n = (size_t)w;
    return out;
}

static DolphinResult *op_write(DolphinEnv *env, AvpStr address, AvpStr value, const char *view, bool confirm) {
    AvpArena *a = env->a;
    u64 addr = dolphin_resolve(env, address, NULL, NULL);
    ShsmBytes nw = encode_value(a, value, view);
    if (!confirm)
        shsm_raise(SHSM_EXC_USAGE, "refused: writing %zu bytes at 0x%08llX changes the running game; add --confirm", nw.n,
                   X32(addr));
    ShsmGdbTarget *t = dolphin_target(env);
    ShsmBytes old = shsm_target_read_memory(a, t, (i64)addr, (i64)nw.n);
    AvpJson *ids = dolphin_journal_entries(env);
    i64 id = 1;
    for (size_t i = 0; i < avp_json_len(ids); i++) {
        i64 x = avp_json_get_i64(avp_json_at(ids, i), "id", 0);
        if (i == 0 || x + 1 > id) id = x + 1;
    }
    AvpJson *s = dolphin_session(env);
    AvpJson *entry = avp_json_obj(a);
    DJ_SET(a, entry, "id", DJ_INT(a, id));
    DJ_SET(a, entry, "time", DJ_STR(a, dolphin_now(a)));
    DJ_SET(a, entry, "address", DJ_INT(a, addr));
    DJ_SET(a, entry, "old", DJ_STR(a, dolphin_hex(a, old.p, old.n)));
    DJ_SET(a, entry, "new", DJ_STR(a, dolphin_hex(a, nw.p, nw.n)));
    DJ_SET(a, entry, "symbol", DJ_STR(a, dolphin_describe(env, addr)));
    AvpJson *started = s ? avp_json_getz(s, "started") : NULL;
    DJ_SET(a, entry, "session", started ? started : avp_json_null());
    journal_append(env, entry);
    shsm_target_write_memory(t, (i64)addr, nw.p, nw.n);
    ShsmBytes back = shsm_target_read_memory(a, t, (i64)addr, (i64)nw.n);
    DolphinResult *r = result(a);
    LINE("wrote %s at 0x%08llX %s (was %s); journal #%lld", dolphin_hex(a, nw.p, nw.n), X32(addr),
         dolphin_describe(env, addr), dolphin_hex(a, old.p, old.n), (long long)id);
    for (size_t i = 0; i < avp_json_len(entry); i++) {
        const AvpJsonMember *m = avp_json_member(entry, i);
        avp_json_set(a, r->data, m->key, m->val);
    }
    if (back.n != nw.n || memcmp(back.p, nw.p, nw.n) != 0) {
        LINE("read back %s: the write did not stick", dolphin_hex(a, back.p, back.n));
        DJ_SET(a, r->data, "readback", DJ_STR(a, dolphin_hex(a, back.p, back.n)));
        r->code = SHSM_EXIT_FINDING;
    }
    return r;
}

static DolphinResult *op_undo(DolphinEnv *env, bool confirm, bool force) {
    AvpArena *a = env->a;
    AvpJson *all = dolphin_journal_entries(env);
    AvpJson *e = NULL;
    for (size_t i = 0; i < avp_json_len(all); i++)
        if (!avp_json_get_bool(avp_json_at(all, i), "undone", false)) e = avp_json_at(all, i);
    DolphinResult *r = result(a);
    if (!e) {
        dolphin_push(&r->lines, a, "nothing to undo");
        DJ_SET(a, r->data, "undone", avp_json_null());
        r->code = SHSM_EXIT_FINDING;
        return r;
    }
    AvpStr old_hex = avp_json_get_str(e, "old", AVP_S("")), new_hex = avp_json_get_str(e, "new", AVP_S(""));
    ShsmBytes old, nw;
    fromhex(a, old_hex, &old);
    fromhex(a, new_hex, &nw);
    u64 addr = (u64)avp_json_get_i64(e, "address", 0);
    i64 id = avp_json_get_i64(e, "id", 0);
    if (!confirm)
        shsm_raise(SHSM_EXC_USAGE, "refused: undo writes %.*s back at 0x%08llX (journal #%lld); add --confirm",
                   AVP_SF(old_hex), X32(addr), (long long)id);
    ShsmGdbTarget *t = dolphin_target(env);
    ShsmBytes cur = shsm_target_read_memory(a, t, (i64)addr, (i64)nw.n);
    if ((cur.n != nw.n || memcmp(cur.p, nw.p, nw.n) != 0) && !force)
        shsm_raise(SHSM_EXC_USAGE, "memory at 0x%08llX holds %s, not the %.*s written by journal #%lld; --force to restore anyway",
                   X32(addr), dolphin_hex(a, cur.p, cur.n), AVP_SF(new_hex), (long long)id);
    shsm_target_write_memory(t, (i64)addr, old.p, old.n);
    AvpJson *u = avp_json_obj(a);
    DJ_SET(a, u, "undo", avp_json_getz(e, "id"));
    DJ_SET(a, u, "time", DJ_STR(a, dolphin_now(a)));
    journal_append(env, u);
    LINE("restored %.*s at 0x%08llX (journal #%lld)", AVP_SF(old_hex), X32(addr), (long long)id);
    DJ_SET(a, r->data, "undone", avp_json_getz(e, "id"));
    DJ_SET(a, r->data, "address", avp_json_getz(e, "address"));
    DJ_SET(a, r->data, "restored", avp_json_getz(e, "old"));
    return r;
}

/* ---- execution control ---- */

static AvpJson *bps_json(DolphinEnv *env) {
    ShsmGdbBreakpoint *bps = NULL;
    size_t n = shsm_target_breakpoints(env->a, dolphin_target(env), &bps);
    return shsm_gdb_breakpoints_to_json(env->a, bps, n);
}

static DolphinResult *op_break(DolphinEnv *env, const AvpStr *functions, size_t nfn, const char *watch, i64 length) {
    AvpArena *a = env->a;
    ShsmGdbTarget *t = dolphin_target(env);
    static const char *types[] = {"exec", "hw", "w", "r", "a"};
    i64 kind = 0;
    if (watch) {
        kind = -1;
        for (int i = 0; i < 5; i++)
            if (!strcmp(watch, types[i])) kind = i;
        if (kind < 0) shsm_raise(SHSM_EXC_KEY, "%s", watch);
    }
    AvpJson *added = avp_json_arr(a);
    DolphinResult *r = result(a);
    for (size_t i = 0; i < nfn; i++) {
        const ShsmSymbol *sym = NULL;
        u64 addr = dolphin_resolve(env, functions[i], NULL, &sym);
        i64 n = watch ? length : 4;
        if (watch && sym && sym->has_size && sym->size && length == 4 && sym->address == addr)
            n = sym->size < 8 ? (i64)sym->size : 8;
        shsm_target_insert_breakpoint(t, (i64)addr, kind, n);
        AvpJson *o = avp_json_obj(a);
        DJ_SET(a, o, "type", DJ_INT(a, kind));
        DJ_SET(a, o, "address", DJ_INT(a, addr));
        DJ_SET(a, o, "length", DJ_INT(a, n));
        DJ_SET(a, o, "symbol", DJ_STR(a, dolphin_describe(env, addr)));
        avp_json_arr_push(a, added, o);
        if (watch) LINE("watch-%s %s 0x%08llX len %lld", watch, dolphin_describe(env, addr), X32(addr), (long long)n);
        else LINE("break %s 0x%08llX", dolphin_describe(env, addr), X32(addr));
    }
    DJ_SET(a, r->data, "added", added);
    DJ_SET(a, r->data, "breakpoints", bps_json(env));
    return r;
}

static DolphinResult *op_clear(DolphinEnv *env) {
    AvpArena *a = env->a;
    ShsmGdbTarget *t = dolphin_target(env);
    ShsmGdbBreakpoint *bps = NULL;
    size_t n = shsm_target_breakpoints(a, t, &bps);
    shsm_target_clear_breakpoints(t);
    DolphinResult *r = result(a);
    LINE("cleared %zu breakpoints", n);
    DJ_SET(a, r->data, "cleared", DJ_INT(a, n));
    return r;
}

static DolphinResult *op_cont(DolphinEnv *env, double timeout) {
    AvpArena *a = env->a;
    ShsmGdbTarget *t = dolphin_target(env);
    shsm_target_resume(t, SHSM_GDB_NONE);
    ShsmGdbStop *st = shsm_target_wait_stop(a, t, timeout);
    DolphinResult *r = result(a);
    if (!st) {
        LINE("still running after %s s (`pause` stops it)", avp_pyfmt(a, "%.1f", timeout).p);
        DJ_SET(a, r->data, "running", avp_json_bool(true));
        r->code = SHSM_EXIT_FINDING;
        return r;
    }
    dolphin_push(&r->lines, a, stop_text(env, st));
    DJ_SET(a, r->data, "running", avp_json_bool(false));
    DJ_SET(a, r->data, "stop", stop_json(a, st));
    return r;
}

static DolphinResult *op_pause(DolphinEnv *env) {
    AvpArena *a = env->a;
    ShsmGdbStop *st = shsm_target_interrupt(a, dolphin_target(env), SHSM_GDB_NONE);
    DolphinResult *r = result(a);
    dolphin_push(&r->lines, a, stop_text(env, st));
    DJ_SET(a, r->data, "stop", stop_json(a, st));
    return r;
}

static DolphinResult *op_step(DolphinEnv *env, i64 count) {
    AvpArena *a = env->a;
    require_stopped(env);
    ShsmGdbStop *st = shsm_target_step(a, dolphin_target(env), count, SHSM_GDB_NONE);
    DolphinResult *r = result(a);
    dolphin_push(&r->lines, a, stop_text(env, st));
    DJ_SET(a, r->data, "stop", stop_json(a, st));
    return r;
}

static DolphinResult *op_where(DolphinEnv *env) {
    AvpArena *a = env->a;
    ShsmGdbState st = shsm_target_state(a, dolphin_target(env));
    DolphinResult *r = result(a);
    if (st.running) {
        dolphin_push(&r->lines, a, "CPU running");
        DJ_SET(a, r->data, "running", avp_json_bool(true));
        return r;
    }
    Special sp = read_special(env);
    const ShsmSymbol *f = dolphin_function_at(env, sp.pc);
    const char *unit = f ? shsm_sym_unit_of(dolphin_syms(env)->st, sp.pc) : NULL;
    LINE("pc 0x%08llX %s%s", X32(sp.pc), dolphin_describe(env, sp.pc), unit ? avp_sprintf(a, "  [%s]", unit) : "");
    LINE("lr 0x%08llX %s", X32(sp.lr), dolphin_describe(env, sp.lr));
    if (f) {
        const char *dm = shsm_demangle(a, f->name);
        if (dm && *dm) LINE("   %s", dm);
    }
    DJ_SET(a, r->data, "running", avp_json_bool(false));
    DJ_SET(a, r->data, "pc", DJ_INT(a, sp.pc));
    DJ_SET(a, r->data, "lr", DJ_INT(a, sp.lr));
    DJ_SET(a, r->data, "function", f ? DJ_STR(a, f->name) : avp_json_null());
    DJ_SET(a, r->data, "unit", unit ? DJ_STR(a, unit) : avp_json_null());
    DJ_SET(a, r->data, "stop", stop_json(a, st.stop));
    return r;
}

static bool read_word(DolphinEnv *env, u64 addr, u32 *out) { /* struct.unpack(">I", read_memory(addr, 4)), GdbError: false */
    AvpTry t;
    if (AVP_TRY(&t)) {
        ShsmBytes b = shsm_target_read_memory(env->a, dolphin_target(env), (i64)addr, 4);
        avp_try_end(&t);
        if (b.n < 4) shsm_raise(SHSM_EXC_VALUE, "unpack requires a buffer of 4 bytes");
        *out = dolphin_be32(b.p);
        return true;
    }
    if (!shsm_exc_is_gdb(t.code)) avp_throw(t.code, "%s", t.msg);
    return false;
}

static DolphinResult *op_bt(DolphinEnv *env, i64 depth) {
    AvpArena *a = env->a;
    require_stopped(env);
    ShsmGdbTarget *t = dolphin_target(env);
    Special sp = read_special(env);
    u64 r1 = shsm_target_read_register(t, 1);
    typedef struct {
        u64 pc;
        const char *via;
    } Frame;
    AVP_VEC(Frame) fr = {0};
    avp_vec_push(&fr, ((Frame){sp.pc, "pc"}));
    const ShsmSymbol *f = dolphin_function_at(env, sp.pc);
    if (!f || sp.pc == f->address) avp_vec_push(&fr, ((Frame){sp.lr, "lr"}));
    u64 chain = r1;
    AvpU64Vec seen = {0};
    while ((i64)fr.n < depth) {
        bool dup = false;
        for (size_t i = 0; i < seen.n && !dup; i++) dup = seen.v[i] == chain;
        if (!dolphin_in_ram(chain) || dup) break;
        avp_vec_push(&seen, chain);
        u32 back, ret;
        if (!read_word(env, chain, &back)) break;
        if (!back || !dolphin_in_ram(back) || back <= chain) break;
        if (!read_word(env, back + 4, &ret)) break;
        if (!ret || !dolphin_in_ram(ret)) break;
        Frame *last = &fr.v[fr.n - 1];
        if (!(!strcmp(last->via, "lr") && last->pc == ret && fr.n == 2))
            avp_vec_push(&fr, ((Frame){ret, avp_sprintf(a, "0x%08llX", X32(back + 4))}));
        chain = back;
    }
    avp_vec_free(&seen);
    DolphinResult *r = result(a);
    AvpJson *frames = avp_json_arr(a);
    for (size_t i = 0; i < fr.n; i++) {
        u64 pc = fr.v[i].pc - (i ? 4 : 0);
        LINE("#%-2zu 0x%08llX %s%s", i, X32(fr.v[i].pc), dolphin_describe(env, i ? pc : fr.v[i].pc),
             i == 0 ? "" : avp_sprintf(a, "  (from %s)", fr.v[i].via));
        AvpJson *o = avp_json_obj(a);
        DJ_SET(a, o, "pc", DJ_INT(a, fr.v[i].pc));
        DJ_SET(a, o, "via", DJ_STR(a, fr.v[i].via));
        DJ_SET(a, o, "symbol", DJ_STR(a, dolphin_describe(env, pc)));
        avp_json_arr_push(a, frames, o);
    }
    avp_vec_free(&fr);
    DJ_SET(a, r->data, "frames", frames);
    DJ_SET(a, r->data, "sp", DJ_INT(a, r1));
    return r;
}

/* ---- calls, object, trace ---- */

static const char *PARAM_TYPES[] = {"ptr", "int", "u32", "s32", "hex", "bool", "u8", "u16", "s16", "char", "float",
                                    "double", "str", "i64", "obj"};

DolphinSig dolphin_default_signature(AvpArena *a, const char *name) {
    DolphinSig sig = {0};
    DolphinParsed p;
    if (!name || !dolphin_mangle_parse(a, name, &p)) {
        sig.none = true;
        return sig;
    }
    sig.v = AVP_NEWN(a, DolphinSigArg, p.nparams + 1);
    if (p.nscope && p.function) sig.v[sig.n++] = (DolphinSigArg){"this", "obj"};
    for (size_t i = 0; i < p.nparams; i++) {
        AvpStr tt = avp_str_strip(avp_str_replace(a, p.params[i], AVP_S("const "), AVP_S(""), -1));
        if (avp_str_eqz(tt, "...") || avp_str_eqz(tt, "void")) break;
        const char *ty;
        if (avp_str_eqz(tt, "float")) ty = "float";
        else if (avp_str_eqz(tt, "double")) ty = "double";
        else if (avp_str_eqz(tt, "char*") || avp_str_eqz(tt, "unsigned char*") || avp_str_eqz(tt, "signed char*")) ty = "str";
        else if (avp_str_find(tt, AVP_S("*"), 0, -1) >= 0 || avp_str_find(tt, AVP_S("&"), 0, AVP_END) >= 0) ty = "ptr";
        else if (avp_str_eqz(tt, "long long") || avp_str_eqz(tt, "unsigned long long")) ty = "i64";
        else if (avp_str_eqz(tt, "bool")) ty = "bool";
        else {
            static const char *ints[] = {"char", "unsigned char", "short", "unsigned short", "int", "unsigned int",
                                         "long", "unsigned long", "signed char"};
            ty = "ptr"; /* a struct by value travels by reference */
            for (int k = 0; k < 9; k++)
                if (avp_str_eqz(tt, ints[k])) ty = "int";
        }
        sig.v[sig.n++] = (DolphinSigArg){avp_sprintf(a, "p%zu", i + 1), ty};
    }
    return sig;
}

DolphinSig dolphin_parse_signature(AvpArena *a, AvpStr text) {
    DolphinSig sig = {0};
    AvpStrVec parts = avp_str_split(text, AVP_S(","), -1);
    sig.v = AVP_NEWN(a, DolphinSigArg, parts.n + 1);
    for (size_t i = 0; i < parts.n; i++) {
        AvpStr part = avp_str_strip(parts.v[i]);
        if (!part.n) continue;
        AvpStr n = part, ty = AVP_S("hex"), rest;
        if (avp_str_partition(part, AVP_S(":"), &n, &rest)) n = avp_str_strip(n), ty = avp_str_strip(rest);
        bool ok = false;
        for (int k = 0; k < 15 && !ok; k++) ok = avp_str_eqz(ty, PARAM_TYPES[k]);
        if (!ok)
            shsm_raise(SHSM_EXC_USAGE, "unknown argument type %s (one of ptr, int, u32, s32, hex, bool, u8, u16, s16, "
                                       "char, float, double, str, i64, obj)", avp_repr_str(a, ty).p);
        sig.v[sig.n++] = (DolphinSigArg){avp_cz(a, n), avp_cz(a, ty)};
    }
    avp_vec_free(&parts);
    return sig;
}

/* class_of_pointer: the class whose vtable the object's first word is (NULL: None) */
static AvpJson *class_of_pointer(DolphinEnv *env, u64 ptr, u64 *add) {
    if (!dolphin_in_ram(ptr) || (ptr & 3) || !dolphin_classes_available(env)) return NULL;
    u32 vptr;
    if (!read_word(env, ptr, &vptr)) return NULL;
    return dolphin_class_by_vtable(env, vptr, add);
}

static const char *class_name(DolphinEnv *env, const AvpJson *c) { return py_str_json(env->a, avp_json_getz(c, "name")); }

static AvpJson *describe_args(DolphinEnv *env, const DolphinSig *sig, const u64 *gprs, size_t ngprs, const u64 *fprs) {
    AvpArena *a = env->a;
    AvpJson *out = avp_json_arr(a);
    size_t gi = 3, fi = 1;
    for (size_t k = 0; k < sig->n; k++) {
        const char *name = sig->v[k].name, *ty = sig->v[k].type, *text;
        if (!strcmp(ty, "float") || !strcmp(ty, "double")) {
            if (fi > 8) text = "(on the stack)";
            else {
                u64 bits = fprs[fi - 1];
                fi++;
                double val;
                memcpy(&val, &bits, 8);
                text = !strcmp(ty, "float") ? dolphin_fmt_float(a, val) : dolphin_f64(a, bits);
            }
        } else if (!strcmp(ty, "i64")) {
            if (gi % 2 == 0) gi++;
            if (gi + 1 > 10) text = "(on the stack)";
            else {
                u64 v = (gprs[gi] << 32) | gprs[gi + 1];
                gi += 2;
                text = avp_sprintf(a, "%lld", (long long)v);
            }
        } else if (gi > 10) {
            text = "(on the stack)";
        } else {
            u64 v = gi < ngprs ? gprs[gi] : 0;
            gi++;
            if (!strcmp(ty, "int") || !strcmp(ty, "s32")) text = avp_sprintf(a, "%lld", (long long)(v >> 31 ? (i64)v - ((i64)1 << 32) : (i64)v));
            else if (!strcmp(ty, "u32")) text = avp_sprintf(a, "%llu", (unsigned long long)v);
            else if (!strcmp(ty, "u8") || !strcmp(ty, "char") || !strcmp(ty, "bool")) text = avp_sprintf(a, "%llu", (unsigned long long)(v & 0xFF));
            else if (!strcmp(ty, "u16")) text = avp_sprintf(a, "%llu", (unsigned long long)(v & 0xFFFF));
            else if (!strcmp(ty, "s16")) text = avp_sprintf(a, "%lld", (long long)((v & 0x8000) ? (i64)(v & 0xFFFF) - 0x10000 : (i64)(v & 0xFFFF)));
            else if (!strcmp(ty, "str") && dolphin_in_ram(v)) {
                AvpTry t;
                if (AVP_TRY(&t)) {
                    ShsmBytes b = shsm_target_read_memory(a, dolphin_target(env), (i64)v, 96);
                    avp_try_end(&t);
                    size_t n = 0;
                    while (n < b.n && b.p[n]) n++;
                    text = avp_sprintf(a, "0x%08llX %s", X32(v), dolphin_json_text(a, avp_str(dolphin_latin1_to_utf8(a, b.p, n))));
                } else {
                    if (!shsm_exc_is_gdb(t.code)) avp_throw(t.code, "%s", t.msg);
                    text = avp_sprintf(a, "0x%08llX", X32(v));
                }
            } else if (!strcmp(ty, "ptr") || !strcmp(ty, "obj")) {
                text = dolphin_fmt_value(env, v);
                u64 add = 0;
                AvpJson *c = class_of_pointer(env, v, &add);
                if (c)
                    text = avp_sprintf(a, "%s <%s%s> (%s)", text, class_name(env, c),
                                       add ? avp_sprintf(a, " +0x%llX sub-vtable", (unsigned long long)add) : "",
                                       dolphin_region(env, v));
            } else {
                text = avp_sprintf(a, "0x%08llX", X32(v));
            }
        }
        AvpJson *o = avp_json_obj(a);
        DJ_SET(a, o, "name", DJ_STR(a, name));
        DJ_SET(a, o, "type", DJ_STR(a, ty));
        DJ_SET(a, o, "value", DJ_STR(a, text));
        avp_json_arr_push(a, out, o);
    }
    return out;
}

static double remaining(double deadline) {
    double r = deadline - avp_monotonic();
    return r > 1e-6 ? r : 1e-6; /* max(0.0, ...); a zero timeout would mean "the default" in the C client */
}

static DolphinResult *op_calls(DolphinEnv *env, AvpStr function, i64 count, double timeout, const char *args) {
    AvpArena *a = env->a;
    const ShsmSymbol *sym = NULL;
    u64 addr = dolphin_resolve(env, function, "function", &sym);
    DolphinSig sig = args && *args ? dolphin_parse_signature(a, avp_str(args))
                                   : dolphin_default_signature(a, sym ? sym->name : NULL);
    ShsmGdbTarget *t = dolphin_target(env);
    if (shsm_target_state(a, t).running) shsm_target_interrupt(a, t, SHSM_GDB_NONE);
    shsm_target_insert_breakpoint(t, (i64)addr, 0, 4);
    AvpJson *hits = avp_json_arr(a);
    size_t nreal = 0;
    double deadline = avp_monotonic() + timeout;
    AvpTry tr;
    if (AVP_TRY(&tr)) {
        while ((i64)avp_json_len(hits) < count) {
            shsm_target_resume(t, SHSM_GDB_NONE);
            ShsmGdbStop *st = shsm_target_wait_stop(a, t, remaining(deadline));
            if (!st) break;
            if (!st->has_pc || st->pc != addr) {
                AvpJson *h = avp_json_obj(a);
                DJ_SET(a, h, "pc", st->has_pc ? DJ_INT(a, st->pc) : avp_json_null());
                DJ_SET(a, h, "other_stop", avp_json_bool(true));
                DJ_SET(a, h, "stop", stop_json(a, st));
                avp_json_arr_push(a, hits, h);
                break;
            }
            u64 g[32] = {0};
            size_t ng = shsm_target_read_gprs(t, g);
            u64 lr = shsm_target_read_register(t, SHSM_GDB_REG_LR);
            i64 nums[8];
            u64 f[8];
            for (int i = 0; i < 8; i++) nums[i] = 0x21 + i;
            shsm_target_read_registers(t, nums, 8, f);
            AvpJson *h = avp_json_obj(a);
            DJ_SET(a, h, "n", DJ_INT(a, avp_json_len(hits) + 1));
            DJ_SET(a, h, "lr", DJ_INT(a, lr));
            DJ_SET(a, h, "caller", DJ_STR(a, dolphin_describe(env, lr - 4)));
            AvpJson *gj = avp_json_arr(a);
            for (size_t i = 3; i < 11 && i < ng; i++) avp_json_arr_push(a, gj, DJ_INT(a, g[i]));
            DJ_SET(a, h, "gpr", gj);
            AvpJson *fj = avp_json_arr(a);
            for (int i = 0; i < 8; i++) avp_json_arr_push(a, fj, DJ_STR(a, dolphin_f64(a, f[i])));
            DJ_SET(a, h, "fpr", fj);
            if (!sig.none) {
                DJ_SET(a, h, "args", describe_args(env, &sig, g, ng, f));
            } else {
                AvpJson *aj = avp_json_arr(a);
                for (size_t i = 3; i < 8; i++) {
                    AvpJson *o = avp_json_obj(a);
                    DJ_SET(a, o, "name", DJ_STR(a, avp_sprintf(a, "r%zu", i)));
                    DJ_SET(a, o, "type", DJ_STR(a, "hex"));
                    const char *v = dolphin_fmt_value(env, g[i]);
                    if (i == 3) {
                        AvpJson *c = class_of_pointer(env, g[3], NULL);
                        if (c) v = avp_sprintf(a, "%s <%s>", v, class_name(env, c));
                    }
                    DJ_SET(a, o, "value", DJ_STR(a, v));
                    avp_json_arr_push(a, aj, o);
                }
                DJ_SET(a, h, "args", aj);
            }
            avp_json_arr_push(a, hits, h);
            nreal++;
        }
        avp_try_end(&tr);
    }
    {
        AvpTry t2;
        if (AVP_TRY(&t2)) {
            shsm_target_remove_breakpoint(t, (i64)addr, 0, 4);
            if (!shsm_target_state(a, t).running) shsm_target_resume(t, SHSM_GDB_NONE);
            avp_try_end(&t2);
        } else if (!shsm_exc_is_gdb(t2.code)) {
            avp_throw(t2.code, "%s", t2.msg);
        }
    }
    if (tr.code) avp_throw(tr.code, "%s", tr.msg);
    const char *name = dolphin_describe(env, addr);
    size_t nh = avp_json_len(hits);
    DolphinResult *r = result(a);
    LINE("%s 0x%08llX: %zu hit%s%s", name, X32(addr), nreal, nh == 1 ? "" : "s",
         (i64)nh >= count ? "" : avp_sprintf(a, " within %s s", avp_pyfmt(a, "%.0f", timeout).p));
    for (size_t i = 0; i < nh; i++) {
        AvpJson *h = avp_json_at(hits, i);
        if (avp_json_get_bool(h, "other_stop", false)) {
            ShsmGdbStop *st = shsm_gdb_stop_from_json(a, avp_json_getz(h, "stop"));
            LINE("  stopped elsewhere: %s", stop_text(env, st));
            continue;
        }
        AvpJson *args_j = avp_json_getz(h, "args");
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t k = 0; k < avp_json_len(args_j); k++) {
            AvpJson *o = avp_json_at(args_j, k);
            AvpStr n = avp_json_get_str(o, "name", AVP_S("")), v = avp_json_get_str(o, "value", AVP_S(""));
            avp_buf_printf(&b, "%s%.*s=%.*s", k ? ", " : "", AVP_SF(n), AVP_SF(v));
        }
        LINE("  #%lld from %s: %s", (long long)avp_json_get_i64(h, "n", 0),
             avp_cz(a, avp_json_get_str(h, "caller", AVP_S(""))), avp_cz(a, avp_buf_str(&b)));
    }
    DJ_SET(a, r->data, "function", DJ_STR(a, name));
    DJ_SET(a, r->data, "address", DJ_INT(a, addr));
    if (sig.none) DJ_SET(a, r->data, "signature", avp_json_null());
    else {
        AvpJson *sj = avp_json_arr(a);
        for (size_t i = 0; i < sig.n; i++) {
            AvpJson *pair = avp_json_arr(a);
            avp_json_arr_push(a, pair, DJ_STR(a, sig.v[i].name));
            avp_json_arr_push(a, pair, DJ_STR(a, sig.v[i].type));
            avp_json_arr_push(a, sj, pair);
        }
        DJ_SET(a, r->data, "signature", sj);
    }
    DJ_SET(a, r->data, "hits", hits);
    r->code = nh ? SHSM_EXIT_OK : SHSM_EXIT_FINDING;
    return r;
}

static i64 field_int(const AvpJson *fd, const char *key, i64 dflt, bool hex) {
    AvpJson *v = avp_json_getz(fd, key);
    if (!avp_json_truthy(v)) return dflt;
    if (avp_json_is_int(v)) return avp_json_i64(v, dflt);
    i64 x = dflt;
    if (avp_json_is_str(v) && !avp_parse_int(avp_json_str_or(v, AVP_S("")), hex ? 16 : 10, &x, NULL))
        shsm_raise(SHSM_EXC_VALUE, "invalid literal for int() with base %d", hex ? 16 : 10);
    return x;
}

static DolphinResult *op_object(DolphinEnv *env, AvpStr address, const char *cls, i64 size) {
    AvpArena *a = env->a;
    u64 addr = dolphin_resolve(env, address, NULL, NULL);
    ShsmGdbTarget *t = dolphin_target(env);
    u64 add = 0;
    ShsmBytes vb = shsm_target_read_memory(a, t, (i64)addr, 4);
    u32 vptr = dolphin_be32(vb.p);
    AvpJson *c = NULL;
    if (cls && *cls) {
        c = dolphin_class_by_name(env, avp_str(cls));
        if (!c)
            shsm_raise(SHSM_EXC_USAGE, "no class %s in %s", avp_repr_str(a, avp_str(cls)).p,
                       shsm_repo_rel(a, env->repo, dolphin_classes(env)->path));
    } else {
        c = dolphin_class_by_vtable(env, vptr, &add);
    }
    AvpJson *fields = c ? avp_json_getz(c, "fields") : NULL;
    if (!avp_json_truthy(fields)) fields = NULL;
    i64 n = size;
    if (!n && c) n = field_int(c, "size_estimate", 0, false);
    if (!n) {
        n = 0x40;
        for (size_t i = 0; i < avp_json_len(fields); i++) {
            AvpJson *fd = avp_json_at(fields, i);
            i64 e = field_int(fd, "offset", 0, true) + field_int(fd, "width", 4, false);
            if (i == 0 || e > n) n = e;
        }
    }
    n = n < 4 ? 4 : n > 0x1000 ? 0x1000 : n;
    ShsmBytes data = shsm_target_read_memory(a, t, (i64)addr, n);
    const char *head = avp_sprintf(a, "0x%08llX (%s) vptr %s", X32(addr), dolphin_region(env, addr), dolphin_fmt_value(env, vptr));
    DolphinResult *r = result(a);
    dolphin_push(&r->lines, a, head);
    AvpJson *rows = avp_json_arr(a);
    DJ_SET(a, r->data, "address", DJ_INT(a, addr));
    if (!c) {
        dolphin_push(&r->lines, a, "no known vtable at +0x0; words follow (pass --class to use a layout)");
        for (i64 o = 0; o + 4 <= n - n % 4; o += 4) {
            u32 v = dolphin_be32(data.p + o);
            LINE("  +0x%-4llX %s", (unsigned long long)o, dolphin_fmt_value(env, v));
            AvpJson *row = avp_json_obj(a);
            DJ_SET(a, row, "offset", DJ_INT(a, o));
            DJ_SET(a, row, "value", DJ_INT(a, v));
            avp_json_arr_push(a, rows, row);
        }
        DJ_SET(a, r->data, "class", avp_json_null());
        DJ_SET(a, r->data, "words", rows);
        r->code = SHSM_EXIT_FINDING;
        return r;
    }
    AvpJson *ns = avp_json_getz(c, "name_source");
    LINE("class %s%s (%s; layout is a draft from classes.json)", class_name(env, c),
         add ? avp_sprintf(a, " via sub-vtable +0x%llX", (unsigned long long)add) : "",
         avp_json_truthy(ns) ? py_str_json(a, ns) : "");
    for (size_t i = 0; i < avp_json_len(fields); i++) {
        AvpJson *fd = avp_json_at(fields, i);
        i64 o = field_int(fd, "offset", 0, true), w = field_int(fd, "width", 4, false);
        if (o + w > n) continue;
        const u8 *chunk = data.p + o;
        AvpJson *tyj = avp_json_getz(fd, "type");
        if (!avp_json_truthy(tyj)) tyj = avp_json_getz(fd, "kind");
        const char *ty = py_str_json(a, tyj);
        bool fl = avp_str_eqz(avp_json_get_str(fd, "kind", AVP_S("")), "float");
        const char *text;
        if (fl && w == 4) text = dolphin_f32(a, dolphin_be32(chunk));
        else if (fl && w == 8) text = dolphin_f64(a, (u64)dolphin_be32(chunk) << 32 | dolphin_be32(chunk + 4));
        else if (w == 4) text = dolphin_fmt_value(env, dolphin_be32(chunk));
        else if (w == 2) text = avp_sprintf(a, "0x%04X", dolphin_be16(chunk));
        else if (w == 1) text = avp_sprintf(a, "0x%02X", chunk[0]);
        else text = dolphin_hex(a, chunk, (size_t)w);
        AvpJson *inh = avp_json_getz(fd, "inherited_from");
        LINE("  +0x%-4llX %-6s %s%s", (unsigned long long)o, ty, text,
             avp_json_truthy(inh) ? avp_sprintf(a, "  (%s)", py_str_json(a, inh)) : "");
        AvpJson *row = avp_json_obj(a);
        DJ_SET(a, row, "offset", DJ_INT(a, o));
        DJ_SET(a, row, "width", DJ_INT(a, w));
        DJ_SET(a, row, "type", tyj ? tyj : avp_json_null());
        DJ_SET(a, row, "value", DJ_STR(a, text));
        DJ_SET(a, row, "hex", DJ_STR(a, dolphin_hex(a, chunk, (size_t)w)));
        avp_json_arr_push(a, rows, row);
    }
    DJ_SET(a, r->data, "class", avp_json_getz(c, "name"));
    DJ_SET(a, r->data, "vptr", DJ_INT(a, vptr));
    DJ_SET(a, r->data, "fields", rows);
    DJ_SET(a, r->data, "size", DJ_INT(a, n));
    return r;
}

static DolphinResult *op_trace(DolphinEnv *env, const AvpStr *functions, size_t nfn, double seconds) {
    AvpArena *a = env->a;
    ShsmGdbTarget *t = dolphin_target(env);
    if (shsm_target_state(a, t).running) shsm_target_interrupt(a, t, SHSM_GDB_NONE);
    AvpU64Vec addrs = {0};
    AvpI64Vec counts = {0};
    for (size_t i = 0; i < nfn; i++) {
        u64 ad = dolphin_resolve(env, functions[i], "function", NULL);
        bool seen = false;
        for (size_t j = 0; j < addrs.n && !seen; j++) seen = addrs.v[j] == ad;
        if (!seen) avp_vec_push(&addrs, ad), avp_vec_push(&counts, 0);
    }
    i64 other = 0;
    for (size_t i = 0; i < addrs.n; i++) shsm_target_insert_breakpoint(t, (i64)addrs.v[i], 0, 4);
    double end = avp_monotonic() + seconds;
    AvpTry tr;
    if (AVP_TRY(&tr)) {
        while (avp_monotonic() < end) {
            shsm_target_resume(t, SHSM_GDB_NONE);
            ShsmGdbStop *st = shsm_target_wait_stop(a, t, remaining(end));
            if (!st) break;
            bool hit = false;
            for (size_t j = 0; j < addrs.n && st->has_pc; j++)
                if (addrs.v[j] == st->pc) counts.v[j]++, hit = true;
            if (!hit) {
                other++;
                break;
            }
        }
        avp_try_end(&tr);
    }
    {
        AvpTry t2;
        if (AVP_TRY(&t2)) {
            if (shsm_target_state(a, t).running) shsm_target_interrupt(a, t, SHSM_GDB_NONE);
            for (size_t i = 0; i < addrs.n; i++) shsm_target_remove_breakpoint(t, (i64)addrs.v[i], 0, 4);
            shsm_target_resume(t, SHSM_GDB_NONE);
            avp_try_end(&t2);
        } else if (!shsm_exc_is_gdb(t2.code)) {
            avp_throw(t2.code, "%s", t2.msg);
        }
    }
    if (tr.code) avp_throw(tr.code, "%s", tr.msg);
    /* sorted by -hits, stable */
    size_t *ord = AVP_NEWN(a, size_t, addrs.n + 1);
    for (size_t i = 0; i < addrs.n; i++) ord[i] = i;
    for (size_t i = 1; i < addrs.n; i++)
        for (size_t j = i; j > 0 && counts.v[ord[j]] > counts.v[ord[j - 1]]; j--) {
            size_t x = ord[j];
            ord[j] = ord[j - 1], ord[j - 1] = x;
        }
    DolphinResult *r = result(a);
    LINE("hits over %s s:", avp_pyfmt(a, "%.1f", seconds).p);
    AvpJson *cj = avp_json_arr(a);
    bool any = false;
    for (size_t k = 0; k < addrs.n; k++) {
        size_t i = ord[k];
        const char *nm = dolphin_describe(env, addrs.v[i]);
        LINE("  %6lld  %s 0x%08llX", (long long)counts.v[i], nm, X32(addrs.v[i]));
        AvpJson *o = avp_json_obj(a);
        DJ_SET(a, o, "function", DJ_STR(a, nm));
        DJ_SET(a, o, "address", DJ_INT(a, addrs.v[i]));
        DJ_SET(a, o, "hits", DJ_INT(a, counts.v[i]));
        avp_json_arr_push(a, cj, o);
        any = any || counts.v[i];
    }
    if (other) dolphin_push(&r->lines, a, "  stopped at another breakpoint; trace ended early");
    DJ_SET(a, r->data, "seconds", avp_json_float(a, seconds));
    DJ_SET(a, r->data, "counts", cj);
    r->code = any ? SHSM_EXIT_OK : SHSM_EXIT_FINDING;
    avp_vec_free(&addrs);
    avp_vec_free(&counts);
    return r;
}

/* ---- the op table and dispatch ---- */

#define P(n, t, d) {n, t, d, false}
#define PR(n, t, d) {n, t, d, true}
static const DolphinParam P_LAUNCH[] = {P("wait_boot", "number", "seconds to wait for the stub (default 120)"),
                                        P("dolphin", "string", "path to Dolphin.exe (remembered)"),
                                        P("disc", "string", "disc image (default: the one in the repository root)"),
                                        {0}};
static const DolphinParam P_NONE[] = {{0}};
static const DolphinParam P_REGS[] = {P("fpr", "boolean", "also f0..f31"), {0}};
static const DolphinParam P_MEM[] = {PR("address", "string", "address or symbol"),
                                     P("length", "integer", "bytes (default 64)"),
                                     P("view", "string", "u8|u16|u32|float|str|ptr"),
                                     P("compare_dol", "boolean", "compare with the retail DOL's bytes"),
                                     {0}};
static const DolphinParam P_READ[] = {PR("symbol", "string", "data symbol"), {0}};
static const DolphinParam P_WRITE[] = {PR("address", "string", "address or symbol"), PR("value", "string", "value"),
                                       P("view", "string", "u8|u16|u32|float|bytes (default u32)"),
                                       {0}};
static const DolphinParam P_UNDO[] = {P("force", "boolean", "restore even when memory changed since"), {0}};
static const DolphinParam P_BREAK[] = {PR("functions", "array", "symbols or addresses"),
                                       P("watch", "string", "r|w|a: a memory watchpoint instead"),
                                       P("length", "integer", "watched bytes"),
                                       {0}};
static const DolphinParam P_CONT[] = {P("timeout", "number", "seconds (default 10)"), {0}};
static const DolphinParam P_STEP[] = {P("count", "integer", "instructions"), {0}};
static const DolphinParam P_BT[] = {P("depth", "integer", "frames"), {0}};
static const DolphinParam P_CALLS[] = {PR("function", "string", "function"), P("count", "integer", "hits (default 5)"),
                                       P("timeout", "number", "seconds (default 30)"),
                                       P("args", "string", "signature such as 'this:obj, value:float'"),
                                       {0}};
static const DolphinParam P_OBJECT[] = {PR("address", "string", "address or symbol"), P("cls", "string", "class name"),
                                        P("size", "integer", "bytes to read"),
                                        {0}};
static const DolphinParam P_TRACE[] = {PR("functions", "array", "functions"),
                                       P("seconds", "number", "seconds (default 5)"),
                                       {0}};

const DolphinOp dolphin_ops[] = {
    {"launch", "start Dolphin on the game with the GDB stub and a private user directory; the CPU then waits at the "
               "entry point", false, P_LAUNCH},
    {"quit", "terminate the Dolphin this tool started and its broker", true, P_NONE},
    {"status", "session, CPU state, breakpoints, journal", false, P_NONE},
    {"regs", "registers (stopped CPU)", false, P_REGS},
    {"mem", "memory at an address or symbol", false, P_MEM},
    {"read", "a data symbol, typed from symbols.txt and the asm index, live", false, P_READ},
    {"write", "write memory (journaled, undoable)", true, P_WRITE},
    {"undo", "restore the last journaled write", true, P_UNDO},
    {"break", "set breakpoints (or watchpoints) on functions or addresses", false, P_BREAK},
    {"clear", "remove every breakpoint", false, P_NONE},
    {"cont", "continue until a breakpoint or the timeout", false, P_CONT},
    {"pause", "stop the running CPU", false, P_NONE},
    {"step", "single-step instructions", false, P_STEP},
    {"where", "where the CPU is stopped", false, P_NONE},
    {"bt", "backtrace from r1's back chain", false, P_BT},
    {"calls", "log N calls of a function: arguments, caller, the class of `this`", false, P_CALLS},
    {"object", "an object's class (from its vtable) and draft fields", false, P_OBJECT},
    {"trace", "count hits per function over a period", false, P_TRACE},
};
const size_t dolphin_nops = sizeof dolphin_ops / sizeof dolphin_ops[0];

const DolphinOp *dolphin_op(const char *name) {
    for (size_t i = 0; i < dolphin_nops; i++)
        if (!strcmp(dolphin_ops[i].name, name)) return &dolphin_ops[i];
    return NULL;
}

/* argument access: null or absent -> the Python default */
static AvpJson *arg(const AvpJson *args, const char *k) {
    AvpJson *v = args ? avp_json_getz(args, k) : NULL;
    return v && !avp_json_is_null(v) ? v : NULL;
}
static const char *arg_str(AvpArena *a, const AvpJson *args, const char *k, const char *dflt) {
    AvpJson *v = arg(args, k);
    return v ? py_str_json(a, v) : dflt;
}
static AvpStr arg_req(AvpArena *a, const char *op, const AvpJson *args, const char *k) {
    AvpJson *v = arg(args, k);
    if (!v && !(args && avp_json_getz(args, k)))
        shsm_raise(SHSM_EXC_TYPE, "op_%s() missing 1 required positional argument: '%s'", op, k);
    return avp_str(py_str_json(a, v));
}
static double arg_f(const AvpJson *args, const char *k, double d) { return avp_json_f64(arg(args, k), d); }
static i64 arg_i(const AvpJson *args, const char *k, i64 d) {
    AvpJson *v = arg(args, k);
    return v ? (avp_json_is_int(v) ? avp_json_i64(v, d) : (i64)avp_json_f64(v, (double)d)) : d;
}
static bool arg_b(const AvpJson *args, const char *k) { return avp_json_truthy(arg(args, k)); }
static AvpStr *arg_list(AvpArena *a, const char *op, const AvpJson *args, const char *k, size_t *n) {
    AvpJson *v = arg(args, k);
    if (!v) shsm_raise(SHSM_EXC_TYPE, "op_%s() missing 1 required positional argument: '%s'", op, k);
    if (avp_json_is_str(v)) { /* a string iterates over its characters in Python; take it as one name */
        AvpStr *one = AVP_NEW(a, AvpStr);
        *one = avp_json_str_or(v, AVP_S(""));
        *n = 1;
        return one;
    }
    *n = avp_json_len(v);
    AvpStr *out = AVP_NEWN(a, AvpStr, *n + 1);
    for (size_t i = 0; i < *n; i++) out[i] = avp_str(py_str_json(a, avp_json_at(v, i)));
    return out;
}

DolphinResult *dolphin_call(DolphinEnv *env, const char *name, const AvpJson *args) {
    AvpArena *a = env->a;
    size_t n = 0;
    AvpStr *list;
    if (!strcmp(name, "launch"))
        return op_launch(env, arg_f(args, "wait_boot", 120.0), arg_str(a, args, "dolphin", NULL),
                         arg_str(a, args, "disc", NULL));
    if (!strcmp(name, "quit")) return op_quit(env);
    if (!strcmp(name, "status")) return op_status(env);
    if (!strcmp(name, "regs")) return op_regs(env, arg_b(args, "fpr"));
    if (!strcmp(name, "mem"))
        return op_mem(env, arg_req(a, name, args, "address"), arg_i(args, "length", 64), arg_str(a, args, "view", "u8"),
                      arg_b(args, "compare_dol"));
    if (!strcmp(name, "read")) return op_read(env, arg_req(a, name, args, "symbol"));
    if (!strcmp(name, "write")) {
        AvpStr ad = arg_req(a, name, args, "address"), v = arg_req(a, name, args, "value");
        return op_write(env, ad, v, arg_str(a, args, "view", "u32"), arg_b(args, "confirm"));
    }
    if (!strcmp(name, "undo")) return op_undo(env, arg_b(args, "confirm"), arg_b(args, "force"));
    if (!strcmp(name, "break")) {
        list = arg_list(a, name, args, "functions", &n);
        return op_break(env, list, n, arg_str(a, args, "watch", NULL), arg_i(args, "length", 4));
    }
    if (!strcmp(name, "clear")) return op_clear(env);
    if (!strcmp(name, "cont")) return op_cont(env, arg_f(args, "timeout", 10.0));
    if (!strcmp(name, "pause")) return op_pause(env);
    if (!strcmp(name, "step")) return op_step(env, arg_i(args, "count", 1));
    if (!strcmp(name, "where")) return op_where(env);
    if (!strcmp(name, "bt")) return op_bt(env, arg_i(args, "depth", 24));
    if (!strcmp(name, "calls"))
        return op_calls(env, arg_req(a, name, args, "function"), arg_i(args, "count", 5), arg_f(args, "timeout", 30.0),
                        arg_str(a, args, "args", NULL));
    if (!strcmp(name, "object"))
        return op_object(env, arg_req(a, name, args, "address"), arg_str(a, args, "cls", NULL), arg_i(args, "size", 0));
    if (!strcmp(name, "trace")) {
        list = arg_list(a, name, args, "functions", &n);
        return op_trace(env, list, n, arg_f(args, "seconds", 5.0));
    }
    shsm_raise(SHSM_EXC_USAGE, "unknown operation %s", avp_repr_str(a, avp_str(name)).p);
}
