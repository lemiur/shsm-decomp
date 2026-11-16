/* dolphin.py: processes, the session files, Env, Dol, SymbolLayer, ClassLayer and value formatting. */
#include "dolphin_priv.h"
#include "shsm_asmindex.h"
#include "shsm_mangle.h"

#include <windows.h>

/* ---- processes ---- */

bool dolphin_proc_info(AvpArena *a, i64 pid, const char **exe, i64 *created) {
    if (!pid || pid < 0 || pid > 0xFFFFFFFFll) return false;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h) return false;
    bool ok = false;
    DWORD code = 0;
    GetExitCodeProcess(h, &code);
    if (code == STILL_ACTIVE) {
        wchar_t *buf = (wchar_t *)avp_alloc(a, 32768 * sizeof(wchar_t));
        DWORD size = 32768;
        if (QueryFullProcessImageNameW(h, 0, buf, &size)) {
            FILETIME c = {0}, e, k, u;
            GetProcessTimes(h, &c, &e, &k, &u);
            if (exe) *exe = avp_cz(a, avp_from_w(a, buf, (i64)size));
            if (created) *created = (i64)(((u64)c.dwHighDateTime << 32) | c.dwLowDateTime);
            ok = true;
        }
    }
    CloseHandle(h);
    return ok;
}

bool dolphin_terminate(i64 pid, double wait) {
    HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (h) {
        TerminateProcess(h, 1);
        WaitForSingleObject(h, (DWORD)(wait * 1000));
        CloseHandle(h);
    }
    return !dolphin_proc_info(avp_scratch(), pid, NULL, NULL);
}

i64 dolphin_spawn_detached(const char *const *argv, size_t argc, const char *log_path, const char *cwd, void **handle) {
    AvpArena *a = avp_scratch();
    const char *cmd = avp_list2cmdline_n(a, argv, argc);
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    HANDLE out = CreateFileW(avp_to_wz(a, log_path ? log_path : "nul"), FILE_APPEND_DATA,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &sa, OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE in = CreateFileW(L"nul", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    STARTUPINFOW si = {sizeof si};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in;
    si.hStdOutput = out;
    si.hStdError = out;
    PROCESS_INFORMATION pi = {0};
    wchar_t *wcmd = avp_to_wz(a, cmd);
    const wchar_t *wcwd = cwd ? avp_to_wz(a, cwd) : NULL;
    DWORD base = DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_UNICODE_ENVIRONMENT;
    BOOL ok = CreateProcessW(NULL, wcmd, NULL, NULL, TRUE, base | CREATE_BREAKAWAY_FROM_JOB, NULL, wcwd, &si, &pi);
    if (!ok) ok = CreateProcessW(NULL, wcmd, NULL, NULL, TRUE, base, NULL, wcwd, &si, &pi);
    DWORD err = ok ? 0 : GetLastError();
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
    if (!ok) shsm_raise(SHSM_EXC_OS, "[WinError %lu] %s", (unsigned long)err, avp_win_error_str(a, err).p);
    CloseHandle(pi.hThread);
    if (handle) *handle = pi.hProcess;
    else CloseHandle(pi.hProcess);
    return (i64)pi.dwProcessId;
}

/* ---- JSON files ---- */

void dolphin_write_json_atomic(const char *path, const AvpJson *data) {
    AvpArena *a = avp_scratch();
    const char *tmp = avp_sprintf(a, "%s.tmp%lu", path, (unsigned long)GetCurrentProcessId());
    AvpJsonDumpOpts o = AVP_JSON_INDENT(1);
    AvpStr text = avp_json_dumps(a, data, &o);
    if (!avp_write_text(tmp, text, AVP_TM_UTF8) || !avp_replace(tmp, path)) shsm_raise_io();
}

AvpJson *dolphin_read_json(AvpArena *a, const char *path) {
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8, &text)) return NULL;
    AvpJsonError err;
    return avp_json_parse(a, text, &err);
}

/* ---- Result ---- */

AvpStr dolphin_result_text(AvpArena *a, const DolphinResult *r, i64 limit) {
    AvpStrVec lines = limit ? shsm_cap_lines(a, r->lines.v, r->lines.n, limit, false, "use --all")
                            : r->lines;
    return avp_str_join(a, AVP_S("\n"), lines.v, lines.n);
}

void dolphin_push(AvpStrVec *lines, AvpArena *a, const char *text) { avp_vec_push(lines, avp_str(avp_strdup(a, text))); }

/* ---- Env ---- */

DolphinEnv *dolphin_env_new(const char *root) {
    AvpArena *a = avp_arena();
    DolphinEnv *env = AVP_NEW(a, DolphinEnv);
    env->a = a;
    env->repo = shsm_repo_find(root);
    env->dir = avp_join(a, env->repo->root, "build", "dolphin");
    env->session_path = avp_join(a, env->dir, "session.json");
    env->journal_path = avp_join(a, env->dir, "journal.jsonl");
    env->settings_path = avp_join(a, env->dir, "dolphin.json");
    env->user_dir = avp_join(a, env->dir, "user");
    return env;
}

void dolphin_env_close(DolphinEnv *env) {
    if (env->bc) {
        shsm_broker_client_free(env->bc);
        env->bc = NULL;
    }
    if (env->ix) {
        shsm_asmindex_close(env->ix);
        env->ix = NULL;
        env->ix_state = 0;
    }
}

ShsmAsmIndex *dolphin_ix(DolphinEnv *env) {
    if (!env->ix_state) {
        env->ix_state = -1;
        AvpTry t;
        if (AVP_TRY(&t)) {
            bool fresh = !avp_exists(avp_join(env->a, env->repo->index_dir, "asm.sqlite"));
            ShsmAsmIndexOpts o = {0};
            o.repo = env->repo;
            o.readonly = true;
            o.no_refresh = !fresh;
            env->ix = shsm_asmindex_open(&o);
            if (env->ix) env->ix_state = 1;
            avp_try_end(&t);
        }
    }
    return env->ix;
}

AvpJson *dolphin_session(DolphinEnv *env) {
    AvpJson *s = dolphin_read_json(env->a, env->session_path);
    return avp_json_truthy(s) ? s : NULL;
}

static bool same_path(AvpArena *a, const char *x, const char *y) {
    return strcmp(avp_path_normcase(a, avp_path_abspath(a, x ? x : "")),
                  avp_path_normcase(a, avp_path_abspath(a, y ? y : ""))) == 0;
}

static const char *json_cz(AvpArena *a, const AvpJson *obj, const char *key) {
    AvpJson *v = avp_json_getz(obj, key);
    return avp_json_is_str(v) ? avp_cz(a, avp_json_str_or(v, AVP_S(""))) : NULL;
}

/* the session's (pid, exe, created) still name the running process */
static bool proc_matches(AvpArena *a, const AvpJson *s, const char *pid_key, const char *exe_key, const char *cr_key,
                         bool *running, const char **exe_out) {
    const char *exe = NULL;
    i64 created = 0;
    bool run = dolphin_proc_info(a, avp_json_get_i64(s, pid_key, 0), &exe, &created);
    if (running) *running = run;
    if (exe_out) *exe_out = exe;
    if (!run) return false;
    AvpJson *c = avp_json_getz(s, cr_key);
    return same_path(a, exe, json_cz(a, s, exe_key)) && avp_json_is_int(c) && avp_json_i64(c, 0) == created;
}

AvpJson *dolphin_session_live(DolphinEnv *env) {
    AvpJson *s = dolphin_session(env);
    if (!s) return NULL;
    return proc_matches(env->a, s, "dolphin_pid", "dolphin_exe", "dolphin_created", NULL, NULL) ? s : NULL;
}

ShsmGdbTarget *dolphin_target(DolphinEnv *env) {
    if (env->target) return env->target;
    if (!env->bc) {
        AvpJson *s = dolphin_session(env);
        if (!s) shsm_raise(SHSM_EXC_USAGE, "no Dolphin session; run `build/tools/dolphin.exe launch` first");
        if (!dolphin_session_live(env))
            shsm_raise(SHSM_EXC_USAGE, "the recorded Dolphin session is gone; run `quit` to clear it, then `launch`");
        const char *token = json_cz(env->a, s, "token");
        env->bc = shsm_broker_client_new((int)avp_json_get_i64(s, "broker_port", 0), token ? token : "", NULL, 60.0);
        env->own = shsm_gdb_target_broker(env->bc);
    }
    return &env->own;
}

/* ---- SymbolLayer ---- */

DolphinSyms *dolphin_syms(DolphinEnv *env) {
    if (!env->syms) {
        DolphinSyms *s = AVP_NEW(env->a, DolphinSyms);
        s->st = shsm_load_symbols(env->repo, true);
        for (size_t i = 0; i < s->st->n; i++) {
            const ShsmSymbol *y = &s->st->symbols[i];
            if (y->type && !strcmp(y->type, "label")) continue;
            u64 end = y->address + (y->has_size ? y->size : 0);
            if (end > s->image_end) s->image_end = end;
        }
        env->syms = s;
    }
    return env->syms;
}

static AvpMap *dm_table(DolphinEnv *env) {
    DolphinSyms *s = dolphin_syms(env);
    if (s->dm) return s->dm;
    s->dm = avp_map_new();
    AvpArena *a = env->a;
    for (size_t i = 0; i < s->st->n; i++) {
        ShsmSymbol *y = &s->st->symbols[i];
        if (y->type && !strcmp(y->type, "label")) continue;
        const char *d = shsm_demangle(a, y->name);
        if (!d || !*d) continue;
        AvpStr keys[2] = {avp_str(d), avp_str(d)};
        const char *paren = strchr(d, '(');
        if (paren) keys[1] = avp_strn(d, (size_t)(paren - d));
        for (int k = 0; k < 2; k++) {
            if (k == 1 && avp_str_eq(keys[0], keys[1])) break;
            bool created = false;
            AvpVal *v = avp_map_put(s->dm, keys[k], &created);
            if (created) v->p = AVP_NEW(a, AvpPtrVec);
            avp_vec_push((AvpPtrVec *)v->p, y);
        }
    }
    return s->dm;
}

static bool is_type(const ShsmSymbol *s, const char *t) { return s->type && !strcmp(s->type, t); }

/* ADDR_RE: ^(?:0x)?([0-9A-Fa-f]{8})$|^0x([0-9A-Fa-f]{1,8})$ */
static bool parse_address(AvpStr t, u64 *out) {
    t = avp_str_strip(t);
    AvpStr digits = t;
    bool pre = t.n >= 2 && t.p[0] == '0' && t.p[1] == 'x';
    if (pre) digits = avp_str_slice(t, 2, (i64)t.n);
    for (size_t i = 0; i < digits.n; i++)
        if (!isxdigit((unsigned char)digits.p[i])) return false;
    if (!(digits.n == 8 || (pre && digits.n >= 1 && digits.n <= 8))) return false;
    u64 v = 0;
    for (size_t i = 0; i < digits.n; i++) {
        char c = digits.p[i];
        v = v * 16 + (u64)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
    }
    *out = v;
    return true;
}

u64 dolphin_resolve(DolphinEnv *env, AvpStr text, const char *kind, const ShsmSymbol **sym) {
    AvpArena *a = env->a;
    DolphinSyms *sy = dolphin_syms(env);
    text = avp_str_strip(text);
    u64 addr = 0;
    if (parse_address(text, &addr) && (dolphin_in_ram(addr) || avp_str_startswith(avp_str_lower(a, text), AVP_S("0x")))) {
        const ShsmSymbol *s = shsm_sym_first_at(sy->st, addr, NULL);
        if (!s) s = shsm_sym_containing(sy->st, addr);
        if (sym) *sym = s;
        return addr;
    }
    AvpPtrVec all = {0}, cands = {0};
    shsm_sym_all_by_name(sy->st, text, &all);
    for (size_t i = 0; i < all.n; i++)
        if (!is_type(all.v[i], "label")) avp_vec_push(&cands, all.v[i]);
    if (!cands.n) avp_vec_extend(&cands, all.v, all.n);
    if (!cands.n) {
        AvpVal *v = avp_map_get(dm_table(env), text);
        if (v) avp_vec_extend(&cands, ((AvpPtrVec *)v->p)->v, ((AvpPtrVec *)v->p)->n);
    }
    AvpStr rep = avp_repr_str(a, text);
    if (kind) {
        AvpPtrVec typed = {0};
        for (size_t i = 0; i < cands.n; i++)
            if (is_type(cands.v[i], kind)) avp_vec_push(&typed, cands.v[i]);
        if (cands.n && !typed.n) {
            const ShsmSymbol *c0 = cands.v[0];
            shsm_raise(SHSM_EXC_USAGE, "%.*s is a %s, not a %s", AVP_SF(text), c0->type ? c0->type : "None", kind);
        }
        avp_vec_free(&cands);
        cands = typed;
    }
    AvpPtrVec uniq = {0};
    for (size_t i = 0; i < cands.n; i++) {
        bool seen = false;
        for (size_t j = 0; j < uniq.n && !seen; j++)
            seen = ((const ShsmSymbol *)uniq.v[j])->address == ((const ShsmSymbol *)cands.v[i])->address;
        if (!seen) avp_vec_push(&uniq, cands.v[i]);
    }
    avp_vec_free(&all);
    avp_vec_free(&cands);
    if (!uniq.n)
        shsm_raise(SHSM_EXC_USAGE, "no symbol matches %.*s (a symbols.txt name, a demangled name or an address)",
                   AVP_SF(rep));
    if (uniq.n > 1) {
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t i = 0; i < uniq.n && i < 8; i++) {
            const ShsmSymbol *s = uniq.v[i];
            avp_buf_printf(&b, "%s%s@0x%08llX", i ? ", " : "", s->name, (unsigned long long)s->address);
        }
        shsm_raise(SHSM_EXC_USAGE, "%.*s is ambiguous: %.*s", AVP_SF(rep), AVP_SF(avp_buf_str(&b)));
    }
    const ShsmSymbol *s = uniq.v[0];
    avp_vec_free(&uniq);
    if (sym) *sym = s;
    return s->address;
}

const char *dolphin_describe(DolphinEnv *env, u64 addr) { return shsm_sym_describe(env->a, dolphin_syms(env)->st, addr); }

const ShsmSymbol *dolphin_function_at(DolphinEnv *env, u64 addr) {
    const ShsmSymbol *s = shsm_sym_containing(dolphin_syms(env)->st, addr);
    return s && is_type(s, "function") ? s : NULL;
}

/* ---- ClassLayer ---- */

static bool hex_int(AvpStr s, i64 *out) { return avp_parse_int(s, 16, out, NULL); }

DolphinClasses *dolphin_classes(DolphinEnv *env) {
    if (env->classes) return env->classes;
    AvpArena *a = env->a;
    DolphinClasses *c = AVP_NEW(a, DolphinClasses);
    c->path = avp_join(a, env->repo->version_build_dir, "classes.json");
    c->doc = dolphin_read_json(a, c->path);
    if (!avp_json_truthy(c->doc)) {
        c->doc = avp_json_obj(a);
        DJ_SET(a, c->doc, "classes", avp_json_arr(a));
    }
    c->by_vt = avp_imap_new();
    AvpJson *classes = avp_json_getz(c->doc, "classes");
    for (size_t i = 0; i < avp_json_len(classes); i++) {
        AvpJson *k = avp_json_at(classes, i);
        AvpJson *vt = avp_json_getz(k, "vtable");
        AvpJson *addr = vt ? avp_json_getz(vt, "address") : NULL;
        i64 base = 0;
        if (!avp_json_is_str(addr) || !hex_int(avp_json_str_or(addr, AVP_S("")), &base)) continue;
        u64 keys[64];
        u64 adds[64];
        size_t nk = 0;
        keys[nk] = (u64)base, adds[nk++] = 0;
        AvpJson *groups = avp_json_getz(vt, "groups");
        for (size_t g = 0; g < avp_json_len(groups) && nk < 64; g++) {
            AvpJson *ad = avp_json_getz(avp_json_at(groups, g), "addend");
            AvpStr t = avp_json_truthy(ad) ? avp_json_str_or(ad, AVP_S("0")) : AVP_S("0");
            i64 add = 0;
            if (!hex_int(t, &add)) continue;
            if (add) keys[nk] = (u64)(base + add), adds[nk++] = (u64)add;
        }
        for (size_t j = 0; j < nk; j++) {
            bool created = false;
            AvpVal *v = avp_imap_put(c->by_vt, keys[j], &created);
            if (!created) continue;
            v->u = c->cls.n;
            avp_vec_push(&c->cls, k);
            avp_vec_push(&c->add, adds[j]);
        }
    }
    env->classes = c;
    return c;
}

bool dolphin_classes_available(DolphinEnv *env) {
    return avp_json_truthy(avp_json_getz(dolphin_classes(env)->doc, "classes"));
}

AvpJson *dolphin_class_by_vtable(DolphinEnv *env, u64 addr, u64 *addend) {
    DolphinClasses *c = dolphin_classes(env);
    AvpVal *v = avp_imap_get(c->by_vt, addr);
    if (!v) return NULL;
    if (addend) *addend = c->add.v[v->u];
    return c->cls.v[v->u];
}

static bool json_eq_str(const AvpJson *v, AvpStr s) { return avp_json_is_str(v) && avp_str_eq(avp_json_str_or(v, AVP_S("")), s); }

AvpJson *dolphin_class_by_name(DolphinEnv *env, AvpStr name) {
    AvpJson *classes = avp_json_getz(dolphin_classes(env)->doc, "classes");
    for (size_t i = 0; i < avp_json_len(classes); i++) {
        AvpJson *k = avp_json_at(classes, i);
        if (json_eq_str(avp_json_getz(k, "name"), name) || json_eq_str(avp_json_getz(k, "vtable_symbol"), name) ||
            json_eq_str(avp_json_getz(k, "key"), name))
            return k;
    }
    return NULL;
}

/* ---- Dol ---- */

u32 dolphin_be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
u16 dolphin_be16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }

bool dolphin_have_dol(DolphinEnv *env) {
    if (!env->dol_state) {
        env->dol_state = -1;
        if (env->repo->retail_dol && avp_read_all(env->a, env->repo->retail_dol, &env->dol) && env->dol.n >= 0xE4)
            env->dol_state = 1;
    }
    return env->dol_state == 1;
}

const u8 *dolphin_dol_read(DolphinEnv *env, u64 address, u64 size) {
    if (!dolphin_have_dol(env)) return NULL;
    const u8 *d = env->dol.v;
    for (int i = 0; i < 18; i++) {
        u64 off = dolphin_be32(d + 4 * i), a = dolphin_be32(d + 0x48 + 4 * i), sz = dolphin_be32(d + 0x90 + 4 * i);
        if (!sz) continue;
        if (a <= address && address + size <= a + sz) {
            u64 s = off + address - a, e = s + size;
            if (e > env->dol.n) return NULL; /* a slice past the end is short; the comparison then fails anyway */
            return d + s;
        }
    }
    u64 b = dolphin_be32(d + 0xD8), bsz = dolphin_be32(d + 0xDC);
    if (b <= address && address + size <= b + bsz) return (const u8 *)avp_alloc(env->a, size ? size : 1);
    return NULL;
}

/* ---- values ---- */

bool dolphin_in_ram(u64 v) { return (v >= 0x80000000u && v < 0x81800000u) || (v >= 0x90000000u && v < 0x94000000u); }

const char *dolphin_fmt_value(DolphinEnv *env, u64 v) {
    if (v && dolphin_in_ram(v)) {
        const char *d = dolphin_describe(env, v);
        if (strncmp(d, "0x", 2) == 0) return avp_sprintf(env->a, "0x%08llX", (unsigned long long)v);
        return avp_sprintf(env->a, "0x%08llX <%s>", (unsigned long long)v, d);
    }
    return avp_sprintf(env->a, "0x%08llX", (unsigned long long)v);
}

static const char *repr_f(AvpArena *a, double f) {
    char buf[32];
    avp_float_repr(f, buf);
    return avp_strdup(a, buf);
}

const char *dolphin_fmt_float(AvpArena *a, double f) {
    if (f != f || f == INFINITY || f == -INFINITY) return repr_f(a, f);
    float want = (float)f;
    for (int prec = 1; prec < 10; prec++) {
        char spec[8];
        snprintf(spec, sizeof spec, "%%.%dg", prec);
        AvpStr t = avp_pyfmt(a, spec, f);
        double back = 0;
        avp_parse_float(t, &back);
        float got = (float)back;
        if (memcmp(&got, &want, 4) == 0) return t.p;
    }
    return repr_f(a, f);
}

const char *dolphin_f32(AvpArena *a, u64 bits) {
    u32 b = (u32)bits;
    float f;
    memcpy(&f, &b, 4);
    return dolphin_fmt_float(a, (double)f);
}

const char *dolphin_f64(AvpArena *a, u64 bits) {
    double d;
    memcpy(&d, &bits, 8);
    return repr_f(a, d);
}

const char *dolphin_region(DolphinEnv *env, u64 ptr) {
    if (!dolphin_in_ram(ptr)) return "not RAM";
    if (ptr >= 0x80000000u && ptr < dolphin_syms(env)->image_end) return "static";
    return "heap";
}

const char *dolphin_hex(AvpArena *a, const u8 *p, size_t n) {
    char *out = (char *)avp_alloc(a, 2 * n + 1);
    for (size_t i = 0; i < n; i++) snprintf(out + 2 * i, 3, "%02x", p[i]);
    return out;
}

const char *dolphin_json_text(AvpArena *a, AvpStr utf8) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_json_put_string(&b, utf8, false);
    return avp_cz(a, avp_buf_str(&b));
}

const char *dolphin_latin1_to_utf8(AvpArena *a, const u8 *p, size_t n) {
    char *out = (char *)avp_alloc(a, 2 * n + 1), *o = out;
    for (size_t i = 0; i < n; i++) {
        if (p[i] < 0x80) *o++ = (char)p[i];
        else {
            *o++ = (char)(0xC0 | (p[i] >> 6));
            *o++ = (char)(0x80 | (p[i] & 0x3F));
        }
    }
    *o = 0;
    return out;
}

const char *dolphin_now(AvpArena *a) {
    AvpStr s;
    avp_strftime_now(a, "%Y-%m-%d %H:%M:%S", &s);
    return avp_cz(a, s);
}
