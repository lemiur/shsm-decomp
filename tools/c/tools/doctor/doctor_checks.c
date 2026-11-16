/* doctor: the checks (doctor.py Check, check_required, check_all and the check_* helpers). */
#include "doctor.h"
#include "shsm_asm.h"
#include "shsm_cli.h"
#include "shsm_git.h"
#include "shsm_links.h"
#include "shsm_text.h"
#include "slot.h"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define A avp_arena()
/* Python's % operator; the result is NUL-terminated. */
#define FMT(...) ((char *)avp_pyfmt(A, __VA_ARGS__).p)

/* ---- pins ------------------------------------------------------------------------------------------------------ */
static const char *const venv_packages[] = {"toml", "Levenshtein", "capstone", NULL};
const DoctorPin doctor_pins[] = {
    {.key = "dtk", .path = "tools/bin/dtk.exe", .version = "v1.8.4",
     .url = "https://github.com/encounter/decomp-toolkit/releases/download/v1.8.4/dtk-windows-x86_64.exe",
     .sha256 = "6a693e95aa4d30acccc673343302a4abdfa3146bd0fb15cb1a2bce924cbc782a"},
    {.key = "objdiff-cli", .path = "tools/bin/objdiff-cli.exe", .version = "v3.8.2",
     .url = "https://github.com/encounter/objdiff/releases/download/v3.8.2/objdiff-cli-windows-x86_64.exe",
     .sha256 = "36d229ac6ce74a26b47f42cf86ea8e808d8cd834a54aa62d7ed3f47b765d971b"},
    {.key = "ninja", .path = "tools/bin/ninja.exe", .version = "v1.13.2",
     .url = "https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip",
     .zip_sha256 = "07fc8261b42b20e71d1720b39068c2e14ffcee6396b76fb7a795fb460b78dc65"},
    {.key = "binutils", .path = "build/binutils", .version = "2.42-2", .asset = "windows-x86_64.zip",
     .zip_sha256 = "bd95bd57607efb45c437e97b77ec199073e242869027eb72196b0720611bb1c1"},
    {.key = "sjiswrap", .path = "build/tools/sjiswrap.exe", .version = "v1.2.2", .asset = "sjiswrap-windows-x86.exe",
     .sha256 = "01d50fa1148cfc3c3b16658c6c9b1c0d9f5324cde79ed6c09ef8353af9d63adf"},
    {.key = "permuter", .path = "tools/permuter", .url = "https://github.com/simonlindholm/decomp-permuter",
     .commit = "059609d4aec73eb0650726772954e1ad575825f8"},
    {.key = "venv", .path = "tools/venv", .packages = venv_packages},
    {.key = "dol", .path = "orig/R5WEA4/sys/main.dol", .sha1 = "6a3964eaf1ebe8225283363e4161a793a03683d3"},
};
const size_t doctor_npins = sizeof(doctor_pins) / sizeof(doctor_pins[0]);

const DoctorPin *doctor_pin(const char *key) {
    for (size_t i = 0; i < doctor_npins; i++)
        if (strcmp(doctor_pins[i].key, key) == 0) return &doctor_pins[i];
    return NULL;
}

const char *const doctor_compilers_required[] = {"Wii/1.0", "Wii/1.0a", "GC/3.0a5", NULL}; /* used by configure.py */
const char *const doctor_compilers_optional[] = {"Wii/1.0RC1", "Wii/1.1", "Wii/1.3", "Wii/1.5", "Wii/1.6", "Wii/1.7",
                                                 "Wii/0x4201_127", "GC/2.7", "GC/3.0a3", "GC/3.0a5.2", NULL};
/* What the tools write into the root of build/ (doctor.BUILD_ROOT_OK). */
const char *const doctor_build_root_ok[] = {
    "R5WEA4", "tools", "binutils", "gate", "neutral", "levers", "index", "tmp", "ghidra", "permute", "walls.json",
    "score_history.sqlite", "score_history.sqlite-journal", "score_history.sqlite-wal", "score_history.sqlite-shm",
    "dolphin", "draft", "eqcheck.json", NULL};
const char *const doctor_fix_components[] = {"venv", "permuter", "binutils", NULL};

/* (file, tool, the command that rebuilds it): doctor.DERIVED_DBS */
static const char *const derived_dbs[][3] = {
    {"twins.sqlite", "twins", "build/tools/twins.exe --rebuild stats"},
    {"fragments.sqlite", "fragments", "build/tools/fragments.exe --rebuild stats"},
    {"hashes.sqlite", "hashes", "build/tools/hashes.exe stats"},
    {"witness.sqlite", "witness", "build/tools/witness.exe --rebuild stats"},
    {"unit_plan.pickle", "unit_plan", "build/tools/unit_plan.exe check"},
};

/* ---- Check ----------------------------------------------------------------------------------------------------- */
static DoctorCheck chk(const char *name, const char *status, const char *detail, const char *fix, bool required,
                       bool advisory) {
    return (DoctorCheck){name, status, detail ? detail : "", fix ? fix : "", required, advisory};
}
#define REQ(name, status, detail, fix) chk((name), (status), (detail), (fix), true, false)
#define OPT(name, status, detail, fix) chk((name), (status), (detail), (fix), false, false)
#define ADV(name, status, detail, fix) chk((name), (status), (detail), (fix), false, true)

char *doctor_check_line(AvpArena *a, const DoctorCheck *c) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_pyfmt(&b, "%-8s %s: %s", c->status, c->name, c->detail);
    bool ok = strcmp(c->status, "OK") == 0;
    if (!ok && c->fix[0]) avp_buf_pyfmt(&b, "  [fix: %s]", c->fix);
    if (!ok && !c->required) avp_buf_putz(&b, c->advisory ? " (advisory)" : " (optional)");
    return avp_buf_cz(&b);
}

AvpJson *doctor_check_json(AvpArena *a, const DoctorCheck *c) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("name"), avp_json_strz(a, c->name));
    avp_json_set(a, o, AVP_S("status"), avp_json_strz(a, c->status));
    avp_json_set(a, o, AVP_S("detail"), avp_json_strz(a, c->detail));
    avp_json_set(a, o, AVP_S("fix"), avp_json_strz(a, c->fix));
    avp_json_set(a, o, AVP_S("required"), avp_json_bool(c->required));
    avp_json_set(a, o, AVP_S("advisory"), avp_json_bool(c->advisory));
    return o;
}

const DoctorCheck *doctor_find(const DoctorChecks *v, const char *name) {
    const DoctorCheck *hit = NULL; /* the dict comprehension keeps the last of a name */
    for (size_t i = 0; i < v->n; i++)
        if (strcmp(v->v[i].name, name) == 0) hit = &v->v[i];
    return hit;
}

/* ---- small helpers --------------------------------------------------------------------------------------------- */
static bool is_cont(char c) { return ((unsigned char)c & 0xC0) == 0x80; }

AvpStr doctor_cp_head(AvpStr s, size_t n) {
    size_t i = 0, k = 0;
    while (i < s.n && k < n) {
        i++;
        while (i < s.n && is_cont(s.p[i])) i++;
        k++;
    }
    return avp_strn(s.p, i);
}

AvpStr doctor_cp_tail(AvpStr s, size_t n) {
    size_t i = s.n, k = 0;
    while (i > 0 && k < n) {
        i--;
        while (i > 0 && is_cont(s.p[i])) i--;
        k++;
    }
    return avp_strn(s.p + i, s.n - i);
}

/* An arena copy of s, for %S arguments. */
static const AvpStr *SP(AvpStr s) {
    AvpStr *p = AVP_NEW(A, AvpStr);
    *p = s;
    return p;
}

static char *join2(const char *x, const char *y) { return avp_path_join(A, x, y); }

/* sha(path, algo): an unreadable file raises the OSError the Python's open() raised. */
static char *file_hex(AvpHashAlg alg, const char *path) {
    char *h = avp_file_digest_hex(A, alg, path);
    if (!h) shsm_raise_io();
    return h;
}

static double mtime_or_raise(const char *p) {
    double t = 0;
    if (!avp_getmtime(p, &t)) shsm_raise_io();
    return t;
}

/* newest(paths): the newest mtime of the paths that exist, 0.0 when none. */
static double newest(const char *const *paths, size_t n) {
    double best = 0.0;
    bool any = false;
    for (size_t i = 0; i < n; i++) {
        if (!avp_exists(paths[i])) continue;
        double t = mtime_or_raise(paths[i]);
        if (!any || t > best) best = t;
        any = true;
    }
    return any ? best : 0.0;
}

/* newest_in_tree(d): the folder's own mtime and every file's below it (os.walk, links not followed). */
static double newest_in_tree(const char *d) {
    double best = mtime_or_raise(d);
    AvpWalk *w = avp_walk_open(d, true);
    AvpWalkItem it = {0};
    while (avp_walk_next(w, &it)) {
        const char *dp = avp_cz(A, it.dirpath);
        for (size_t i = 0; i < it.files.n; i++) {
            double t = mtime_or_raise(join2(dp, avp_cz(A, it.files.v[i])));
            if (t > best) best = t;
        }
    }
    avp_walk_close(w);
    return best;
}

static AvpStr joined(const char *const *items, const char *sep) {
    AvpBuf b;
    avp_buf_init(&b, A);
    for (size_t i = 0; items[i]; i++) {
        if (i) avp_buf_putz(&b, sep);
        avp_buf_putz(&b, items[i]);
    }
    return avp_buf_str(&b);
}

static bool is_hex(AvpStr s) {
    for (size_t i = 0; i < s.n; i++) {
        char c = s.p[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

/* ---- the runners ----------------------------------------------------------------------------------------------- */
static const char **nul_terminated(const char *const *argv, size_t argc) {
    const char **v = AVP_NEWN(A, const char *, argc + 1);
    for (size_t i = 0; i < argc; i++) v[i] = argv[i];
    return v;
}

DoctorRunResult doctor_run(void *ctx, const char *const *argv, size_t argc, const char *cwd) {
    (void)ctx;
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = nul_terminated(argv, argc), .cwd = cwd, .out = AVP_IO_CAPTURE,
                                             .err = AVP_IO_CAPTURE, .text = true, .errors = AVP_ERRORS_REPLACE,
                                             .timeout = 60, .arena = A});
    if (r.status == AVP_PROC_START_FAILED) return (DoctorRunResult){-1, r.message ? r.message : ""};
    if (r.status == AVP_PROC_TIMEOUT) {
        /* str(TimeoutExpired): "Command '<str(list)>' timed out after 60 seconds" */
        AvpBuf b;
        avp_buf_init(&b, A);
        avp_buf_putz(&b, "Command '[");
        for (size_t i = 0; i < argc; i++) {
            if (i) avp_buf_putz(&b, ", ");
            avp_buf_put(&b, avp_repr_str(A, avp_str(argv[i])));
        }
        avp_buf_putz(&b, "]' timed out after 60 seconds");
        return (DoctorRunResult){-1, avp_buf_cz(&b)};
    }
    return (DoctorRunResult){r.returncode, avp_cz(A, avp_str_strip(r.out))};
}

#define RUN(...) \
    doctor_run(NULL, (const char *const[]){__VA_ARGS__}, sizeof((const char *const[]){__VA_ARGS__}) / sizeof(char *), NULL)

Doctor doctor_new(const char *root) {
    return (Doctor){.root = root, .dol_sha1 = doctor_pin("dol")->sha1, .runner = doctor_run, .runner_ctx = NULL,
                    .asmindex = avp_path_join(A, avp_exe_dir(), "asmindex.exe"),
                    .decomp = avp_path_join(A, avp_exe_dir(), "decomp.exe")};
}

/* ---- shared probes --------------------------------------------------------------------------------------------- */
char *doctor_venv_python(AvpArena *a, const char *root) {
    static const char *const rels[] = {"tools/venv/Scripts/python.exe", "tools/venv/bin/python"};
    for (size_t i = 0; i < 2; i++) {
        char *p = avp_path_join(a, root, rels[i]);
        if (avp_isfile(p)) return p;
    }
    return NULL;
}

char *doctor_configure_binutils_tag(AvpArena *a, const char *root) {
    AvpStr text;
    if (!avp_read_text(a, avp_path_join(a, root, "configure.py"), AVP_TM_UTF8, &text)) {
        if (avp_io_err()->kind == AVP_IOE_OS) return NULL; /* except OSError */
        shsm_raise_io();
    }
    /* re.search(r'binutils_tag\s*=\s*"([^"]+)"') */
    AvpStr key = AVP_S("binutils_tag");
    for (i64 at = avp_str_find(text, key, 0, AVP_END); at >= 0; at = avp_str_find(text, key, at + 1, AVP_END)) {
        size_t i = shsm_skip_ws(text, (size_t)at + key.n);
        if (i >= text.n || text.p[i] != '=') continue;
        i = shsm_skip_ws(text, i + 1);
        if (i >= text.n || text.p[i] != '"') continue;
        size_t j = i + 1;
        while (j < text.n && text.p[j] != '"') j++;
        if (j >= text.n || j == i + 1) continue;
        return avp_cz(a, avp_strn(text.p + i + 1, j - i - 1));
    }
    return NULL;
}

char *doctor_hooks_dir(AvpArena *a, const char *root) {
    DoctorRunResult r = RUN("git", "-C", root, "rev-parse", "--git-path", "hooks");
    if (r.rc != 0) return NULL;
    return avp_path_isabs(r.out) ? avp_strdup(a, r.out) : avp_path_join(a, root, r.out);
}

/* (build.sha1's hash, config.yml's `hash:`), each NULL when absent (read_contract). */
static void read_contract(const char *root, char **want, char **yml) {
    *want = *yml = NULL;
    AvpStr t;
    if (avp_read_text(A, join2(root, "config/R5WEA4/build.sha1"), AVP_TM_UTF8, &t)) {
        AvpStrVec w = avp_str_split(t, (AvpStr){0}, -1);
        if (w.n) *want = avp_cz(A, avp_str_lower(A, w.v[0]));
    } else if (avp_io_err()->kind != AVP_IOE_OS) {
        shsm_raise_io();
    }
    if (avp_read_text(A, join2(root, "config/R5WEA4/config.yml"), AVP_TM_UTF8, &t)) {
        AvpStrVec lines = avp_str_splitlines(t, true);
        for (size_t k = 0; k < lines.n; k++) {
            AvpStr ln = lines.v[k];
            if (!avp_str_startswith(ln, AVP_S("hash:"))) continue;
            size_t i = shsm_skip_ws(ln, 5);
            if (i + 40 > ln.n) continue;
            AvpStr h = avp_strn(ln.p + i, 40);
            if (!is_hex(h) || shsm_skip_ws(ln, i + 40) != ln.n) continue;
            *yml = avp_cz(A, avp_str_lower(A, h));
        }
    } else if (avp_io_err()->kind != AVP_IOE_OS) {
        shsm_raise_io();
    }
}

/* ---- check_required -------------------------------------------------------------------------------------------- */
static char *missing_compilers(const char *root, const char *const *names) {
    AvpBuf b;
    avp_buf_init(&b, A);
    bool any = false;
    for (size_t i = 0; names[i]; i++) {
        if (avp_isfile(avp_join(A, root, "tools/compilers", names[i], "mwcceppc.exe"))) continue;
        if (any) avp_buf_putz(&b, ", ");
        avp_buf_putz(&b, names[i]);
        any = true;
    }
    return any ? avp_buf_cz(&b) : NULL;
}

void doctor_check_required(Doctor *d, DoctorChecks *c) {
    const char *root = d->root;
    static const char *const keys[] = {"dtk", "objdiff-cli"};
    for (size_t k = 0; k < 2; k++) {
        const DoctorPin *pin = doctor_pin(keys[k]);
        char *p = join2(root, pin->path);
        if (!avp_isfile(p))
            avp_vec_push(c, REQ(pin->key, "MISSING", pin->path, FMT("download %s", pin->url)));
        else if (strcmp(file_hex(AVP_HASH_SHA256, p), pin->sha256) != 0)
            avp_vec_push(c, REQ(pin->key, "MISMATCH", FMT("%s sha256 differs from %s pin", pin->path, pin->version),
                                FMT("download %s", pin->url)));
        else
            avp_vec_push(c, REQ(pin->key, "OK", FMT("%s %s sha256 ok", pin->path, pin->version), NULL));
    }
    const DoctorPin *pin = doctor_pin("ninja");
    char *p = join2(root, pin->path);
    if (!avp_isfile(p)) {
        avp_vec_push(c, REQ("ninja", "MISSING", pin->path, FMT("extract %s", pin->url)));
    } else {
        DoctorRunResult r = d->runner(d->runner_ctx, (const char *const[]){p, "--version"}, 2, NULL);
        bool ok = r.rc == 0 && strcmp(FMT("v%s", r.out), pin->version) == 0;
        avp_vec_push(c, REQ("ninja", ok ? "OK" : "MISMATCH",
                            FMT("%s --version %s (pin is the zip's sha256)", pin->path, r.out[0] ? r.out : "?"),
                            FMT("extract %s", pin->url)));
    }

    char *miss = missing_compilers(root, doctor_compilers_required);
    if (miss)
        avp_vec_push(c, REQ("compilers", "MISSING", miss, "copy into tools/compilers/"));
    else
        avp_vec_push(c, REQ("compilers", "OK", avp_cz(A, joined(doctor_compilers_required, ", ")), NULL));

    const DoctorPin *dpin = doctor_pin("dol");
    char *dol = join2(root, dpin->path);
    char *want, *yml;
    read_contract(root, &want, &yml);
    if (!avp_isfile(dol)) {
        avp_vec_push(c, REQ("dol", "MISSING", dpin->path, "extract sys/main.dol from the disc"));
    } else {
        char *got = file_hex(AVP_HASH_SHA1, dol);
        bool ok = want && strcmp(got, want) == 0 && strcmp(want, d->dol_sha1) == 0;
        avp_vec_push(c, REQ("dol", ok ? "OK" : "MISMATCH",
                            FMT("%s sha1 %s%s", dpin->path, avp_cz(A, avp_strn(got, 12)),
                                ok ? "" : FMT(" (build.sha1 %s)", want)),
                            "use the retail USA disc's main.dol"));
    }
    if (!want || !yml) {
        avp_vec_push(c, REQ("contract", "MISSING", "config/R5WEA4/build.sha1 or config.yml's hash: line not found",
                            "restore config/R5WEA4 from git"));
    } else {
        bool ok = strcmp(want, yml) == 0 && strcmp(yml, d->dol_sha1) == 0;
        avp_vec_push(c, REQ("contract", ok ? "OK" : "MISMATCH",
                            FMT("build.sha1 %S, config.yml hash %S, pin %S", SP(doctor_cp_head(avp_str(want), 12)),
                                SP(doctor_cp_head(avp_str(yml), 12)),
                                SP(doctor_cp_head(avp_str(d->dol_sha1), 12))),
                            "restore config/R5WEA4 from git"));
    }
}

/* ---- ghidra (decomp.exe status, D-doctor-4) and the asm index (asmindex.exe, D-doctor-3) ------------------------ */
bool doctor_parse_ghidra(AvpStr g, AvpStr *version, AvpStr *install) {
    /* (\S+) : a run of non-whitespace from the start, which must be followed by " (" */
    size_t i = 0;
    while (i < g.n && shsm_ws_len(g, i) == 0) i++;
    if (i == 0 || i + 1 >= g.n || g.p[i] != ' ' || g.p[i + 1] != '(') return false;
    *version = avp_strn(g.p, i);
    /* \(.*?\) at (.+?) : the first ") at " (no newline before it) that leaves at least one character */
    AvpStr sep = AVP_S(") at ");
    for (i64 at = avp_str_find(g, sep, (i64)i + 2, AVP_END); at >= 0; at = avp_str_find(g, sep, at + 1, AVP_END)) {
        if (memchr(g.p + i + 2, '\n', (size_t)at - i - 2)) return false;
        size_t s = (size_t)at + sep.n;
        if (s >= g.n || g.p[s] == '\n') continue;
        /* (.+?)(?:, language .*)?$ : up to the first ", language " after at least one character, else to the end */
        size_t end = g.n;
        const char *nl = memchr(g.p + s, '\n', g.n - s);
        size_t line_end = nl ? (size_t)(nl - g.p) : g.n;
        i64 lang = avp_str_find(g, AVP_S(", language "), (i64)s + 1, (i64)line_end);
        if (lang >= 0) {
            end = (size_t)lang;
        } else {
            /* `$` matches at the end or before a final "\n"; the group cannot cross a newline */
            if (nl && !(line_end + 1 == g.n)) continue;
            end = line_end;
        }
        *install = avp_strn(g.p + s, end - s);
        return true;
    }
    return false;
}

DoctorCheck doctor_check_ghidra(Doctor *d) {
    const char *root = d->root;
    if (!avp_isfile(d->decomp))
        return OPT("ghidra", "MISSING", "decomp.exe not built", "tools/build_c.cmd");
    DoctorRunResult r = d->runner(
        d->runner_ctx, (const char *const[]){d->decomp, "--root", root, "status"}, 4, root);
    AvpStrVec lines = avp_str_splitlines(avp_str(r.out), false);
    if (r.rc != 0 && r.rc != 1) {
        AvpStr last = lines.n ? lines.v[lines.n - 1] : AVP_S("?");
        return OPT("ghidra", "WARN", FMT("decomp.exe status exited %d: %S", r.rc, SP(doctor_cp_head(last, 120))),
                   "build/tools/decomp.exe status");
    }
    /* {key.strip(): value.strip()} over the lines holding ":" (a later key wins) */
    AvpStr g = {0}, project = {0}, listing = AVP_S("?");
    bool have_g = false, have_project = false;
    for (size_t k = 0; k < lines.n; k++) {
        AvpStr key, val;
        if (!avp_str_partition(lines.v[k], AVP_S(":"), &key, &val)) continue;
        key = avp_str_strip(key);
        val = avp_str_strip(val);
        if (avp_str_eqz(key, "ghidra")) g = val, have_g = true;
        else if (avp_str_eqz(key, "project")) project = val, have_project = true;
        else if (avp_str_eqz(key, "listings")) listing = val;
    }
    if (!have_g) g = AVP_S("");
    if (!have_project) project = AVP_S("");
    AvpStr version = {0}, install = {0};
    bool m = doctor_parse_ghidra(g, &version, &install);
    bool has_install = m && install.n > 0;
    bool proj = avp_str_contains(project, AVP_S("present")) && !avp_str_contains(project, AVP_S("MISSING"));
    AvpStr first;
    AvpStr rest;
    avp_str_partition(listing, AVP_S(";"), &first, &rest);
    char *detail = FMT("install %s, project %s, listings %S", has_install ? FMT("%S %S", &version, &install) : "not found",
                       proj ? "present" : "absent", &first);
    if (has_install && proj) return OPT("ghidra", "OK", detail, "");
    return OPT("ghidra", "MISSING", detail, "build/tools/decomp.exe setup");
}

/* str() of a JSON value as Python's "%s" prints it. */
static char *py_str(const AvpJson *v) {
    if (!v) return "?";
    if (avp_json_is_str(v)) return avp_cz(A, avp_json_str_or(v, AVP_S("")));
    if (avp_json_is_null(v)) return "None";
    if (avp_json_is_int(v)) return FMT("%d", avp_json_i64(v, 0));
    if (avp_json_is_num(v)) return FMT("%v", avp_json_f64(v, 0));
    if (!avp_json_is_arr(v) && !avp_json_is_obj(v)) return avp_json_truthy(v) ? "True" : "False";
    return avp_cz(A, avp_json_dumps(A, v, &(AvpJsonDumpOpts){0}));
}

DoctorCheck doctor_check_index(Doctor *d) {
    const char *root = d->root;
    if (!avp_isdir(avp_join(A, root, "build", "R5WEA4", "asm")))
        return OPT("asm-index", "MISSING", "no build/R5WEA4/asm to index", "tools/bin/ninja.exe");
    /* `build/tools/asmindex.exe --stats --json` in the Python; asmindex.exe is its C port (same JSON) */
    DoctorRunResult r = d->runner(d->runner_ctx, (const char *const[]){d->asmindex, "--stats", "--json", "--root", root},
                                  5, root);
    AvpJson *doc = NULL;
    if (r.rc == 0) {
        const char *brace = strchr(r.out, '{');
        if (brace) {
            AvpJsonError err;
            doc = avp_json_parse(A, avp_str(brace), &err);
        }
    }
    if (!doc || !avp_json_truthy(doc) || !avp_json_is_obj(doc) || !avp_json_getz(doc, "refresh"))
        return OPT("asm-index", "WARN", FMT("index --stats exited %d without JSON", r.rc),
                   "build/tools/asmindex.exe --stats");
    AvpJson *ref = avp_json_getz(doc, "refresh");
    AvpJson *st = avp_json_getz(doc, "stats");
    char *size = FMT("%s files, %s symbols", py_str(st ? avp_json_getz(st, "files") : NULL),
                     py_str(st ? avp_json_getz(st, "symbols") : NULL));
    AvpJson *parsed = avp_json_getz(ref, "parsed"), *removed = avp_json_getz(ref, "removed");
    if ((parsed && avp_json_truthy(parsed)) || (removed && avp_json_truthy(removed)))
        return OPT("asm-index", "STALE",
                   FMT("was behind the asm: %d file(s) re-parsed, %d removed (now current: %s)",
                       parsed ? avp_json_i64(parsed, (i64)avp_json_f64(parsed, 0)) : 0,
                       removed ? avp_json_i64(removed, (i64)avp_json_f64(removed, 0)) : 0, size),
                   "build/tools/asmindex.exe");
    return OPT("asm-index", "OK", FMT("current: %s", size), "");
}

/* asm listings left in build/R5WEA4/asm by an earlier split: not named by the current build/R5WEA4/config.json. The asm
 * index ignores them (and says how many); classes.exe scan and the retail-asm tools that walk the directory do not. */
DoctorCheck doctor_check_asm_listings(Doctor *d) {
    const char *asm_dir = avp_join(A, d->root, "build", "R5WEA4", "asm");
    ShsmListings *l = shsm_asm_listings(A, asm_dir);
    if (!l->record)
        return OPT("asm-listings", "WARN", "no usable split record (build/R5WEA4/config.json): stale listings cannot be told apart",
                   "remove build/R5WEA4 and rebuild: tools/bin/ninja.exe");
    if (!l->stale.n) return OPT("asm-listings", "OK", FMT("%d listing(s), all named by config.json", (i64)l->rels.n), "");
    AvpStr shown = avp_str_join(A, AVP_S(", "), l->stale.v, l->stale.n > 8 ? 8 : l->stale.n);
    return OPT("asm-listings", "STALE",
               FMT("%d of %d listing(s) are from an earlier split, not named by config.json (the asm index ignores them; classes.exe "
                   "scan reads them): %S%s",
                   (i64)l->stale.n, (i64)(l->stale.n + l->rels.n), &shown, l->stale.n > 8 ? ", ..." : ""),
               "remove build/R5WEA4 and rebuild: tools/bin/ninja.exe");
}

/* ---- venv and permuter ----------------------------------------------------------------------------------------- */
DoctorCheck doctor_check_venv(Doctor *d) {
    char *py = doctor_venv_python(A, d->root);
    if (!py) return OPT("venv", "MISSING", "tools/venv", "build/tools/doctor.exe --fix venv");
    const char *const *pk = doctor_pin("venv")->packages;
    DoctorRunResult r = RUN(py, "-c",
                            "import importlib.util as u,sys;print(' '.join(m for m in sys.argv[1:] if u.find_spec(m) is None))",
                            pk[0], pk[1], pk[2]);
    if (r.rc != 0)
        return OPT("venv", "MISMATCH", FMT("venv python fails: %S", SP(doctor_cp_tail(avp_str(r.out), 80))),
                   "build/tools/doctor.exe --fix venv");
    if (r.out[0]) return OPT("venv", "MISSING", FMT("not importable: %s", r.out), "build/tools/doctor.exe --fix venv");
    return OPT("venv", "OK", FMT("tools/venv: %S", SP(joined(pk, ", "))), "");
}

DoctorCheck doctor_check_permuter(Doctor *d) {
    const DoctorPin *pin = doctor_pin("permuter");
    char *p = join2(d->root, pin->path);
    if (!avp_isdir(p)) return OPT("permuter", "MISSING", "tools/permuter", "build/tools/doctor.exe --fix permuter");
    DoctorRunResult r = RUN("git", "-C", p, "rev-parse", "HEAD");
    if (r.rc != 0) return OPT("permuter", "MISMATCH", "tools/permuter is not a git checkout", "move it away, then --fix");
    bool ok = strcmp(r.out, pin->commit) == 0;
    return OPT("permuter", ok ? "OK" : "MISMATCH", FMT("tools/permuter at %S", SP(doctor_cp_head(avp_str(r.out), 12))),
               "build/tools/doctor.exe --fix permuter");
}

/* ---- advisory lines -------------------------------------------------------------------------------------------- */
/* dolphin.proc_info(pid): (executable path, creation stamp) of a running process. */
static bool proc_info(i64 pid, char **exe, u64 *created) {
    if (pid <= 0 || pid > 0xFFFFFFFFll) return false;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h) return false;
    bool ok = false;
    DWORD code = 0;
    GetExitCodeProcess(h, &code);
    if (code == STILL_ACTIVE) {
        static wchar_t buf[32768];
        DWORD size = 32768;
        if (QueryFullProcessImageNameW(h, 0, buf, &size)) {
            FILETIME c = {0}, e, k, u;
            GetProcessTimes(h, &c, &e, &k, &u);
            *exe = avp_cz(A, avp_from_w(A, buf, -1));
            *created = ((u64)c.dwHighDateTime << 32) | c.dwLowDateTime;
            ok = true;
        }
    }
    CloseHandle(h);
    return ok;
}

static bool same_path(const char *x, const char *y) { /* dolphin.same_path */
    return strcmp(avp_path_normcase(A, avp_path_abspath(A, x ? x : "")),
                  avp_path_normcase(A, avp_path_abspath(A, y ? y : ""))) == 0;
}

/* dolphin.Env(root).session_live(): the recorded session when its Dolphin still runs, else NULL. */
static AvpJson *session_live(const char *root) {
    AvpStr t;
    if (!avp_read_text(A, avp_join(A, root, "build", "dolphin", "session.json"), AVP_TM_UTF8, &t)) return NULL;
    AvpJsonError err;
    AvpJson *s = avp_json_parse(A, t, &err);
    if (!s || !avp_json_is_obj(s) || !avp_json_truthy(s)) return NULL;
    AvpJson *pid = avp_json_getz(s, "dolphin_pid");
    if (!pid || !avp_json_truthy(pid) || !avp_json_is_int(pid)) return NULL;
    char *exe;
    u64 created;
    if (!proc_info(avp_json_i64(pid, 0), &exe, &created)) return NULL;
    AvpJson *dexe = avp_json_getz(s, "dolphin_exe"), *dc = avp_json_getz(s, "dolphin_created");
    if (dexe && !avp_json_is_str(dexe) && !avp_json_is_null(dexe)) return NULL;
    if (!same_path(exe, dexe && avp_json_is_str(dexe) ? avp_cz(A, avp_json_str_or(dexe, AVP_S(""))) : NULL)) return NULL;
    if (!dc || !avp_json_is_int(dc) || (u64)avp_json_i64(dc, -1) != created) return NULL;
    return s;
}

static DoctorCheck check_dolphin(const char *root) {
    AvpStr t;
    AvpJson *rec = NULL;
    AvpJsonError err;
    if (avp_read_text(A, avp_join(A, root, "build", "dolphin", "dolphin.json"), AVP_TM_UTF8, &t))
        rec = avp_json_parse(A, t, &err);
    if (!rec || !avp_json_is_obj(rec))
        return ADV("dolphin", "MISSING", "no Dolphin recorded (build/dolphin/dolphin.json)",
                   "build/tools/dolphin.exe launch --dolphin PATH");
    AvpJson *e = avp_json_getz(rec, "exe"), *v = avp_json_getz(rec, "version");
    char *exe = e && avp_json_truthy(e) ? py_str(e) : "?";
    char *detail = (v && avp_json_truthy(v)) ? FMT("%s %s", exe, py_str(v)) : exe;
    AvpJson *live = session_live(root);
    detail = FMT("%s; session %s", detail,
                 live ? FMT("live (pid %s)", py_str(avp_json_getz(live, "dolphin_pid"))) : "none");
    bool isf = avp_isfile(exe);
    return ADV("dolphin", isf ? "OK" : "MISSING", detail, isf ? "" : "build/tools/dolphin.exe launch --dolphin PATH");
}

/* PS2_EXE: ^S[LC][UE]S_\d{3}\.\d{2}$ (re.I) */
static bool ps2_exe_name(AvpStr n) {
    if (n.n != 11) return false;
    const char *s = n.p;
    char c0 = s[0] | 0x20, c1 = s[1] | 0x20, c2 = s[2] | 0x20, c3 = s[3] | 0x20;
    if (c0 != 's' || (c1 != 'l' && c1 != 'c') || (c2 != 'u' && c2 != 'e') || c3 != 's' || s[4] != '_') return false;
    for (int i = 5; i < 8; i++)
        if (s[i] < '0' || s[i] > '9') return false;
    if (s[8] != '.') return false;
    return s[9] >= '0' && s[9] <= '9' && s[10] >= '0' && s[10] <= '9';
}

static DoctorCheck check_ps2_exe(const char *root) {
    char *d = avp_join(A, root, "orig", "_iso");
    AvpStrVec names = {0};
    if (avp_isdir(d) && !avp_listdir(A, d, &names)) shsm_raise_io();
    avp_sort_strs(names.v, names.n);
    const char *exe = NULL;
    for (size_t i = 0; i < names.n && !exe; i++)
        if (ps2_exe_name(names.v[i]) && avp_isfile(join2(d, avp_cz(A, names.v[i])))) exe = avp_cz(A, names.v[i]);
    for (size_t i = 0; i < names.n && !exe; i++)
        if (avp_str_endswith(avp_str_lower(A, names.v[i]), AVP_S(".iso"))) exe = avp_cz(A, names.v[i]);
    if (exe) return ADV("ps2-exe", "OK", FMT("orig/_iso/%s", exe), "");
    return ADV("ps2-exe", "MISSING", "orig/_iso/ holds no SLUS_* ELF or ISO image (witness.exe needs one)",
               "copy the PS2 disc image into orig/_iso/, or pass witness.exe --exe PATH");
}

static void check_derived(const char *root, DoctorChecks *out) {
    char *ix = avp_join(A, root, "build", "index", "asm.sqlite");
    bool have_ix = avp_isfile(ix);
    double ix_t = have_ix ? mtime_or_raise(ix) : 0;
    for (size_t i = 0; i < sizeof(derived_dbs) / sizeof(derived_dbs[0]); i++) {
        const char *name = derived_dbs[i][0], *tool = derived_dbs[i][1], *cmd = derived_dbs[i][2];
        char *p = avp_join(A, root, "build", "index", name);
        char *cname = FMT("index %s", tool);
        if (!avp_isfile(p))
            avp_vec_push(out, ADV(cname, "MISSING", FMT("build/index/%s (built on first use)", name), cmd));
        else if (have_ix && mtime_or_raise(p) < ix_t)
            avp_vec_push(out, ADV(cname, "STALE", FMT("build/index/%s is older than the asm index", name), cmd));
        else
            avp_vec_push(out, ADV(cname, "OK", FMT("build/index/%s current", name), ""));
    }
}

void doctor_check_advisory(Doctor *d, DoctorChecks *out) {
    avp_vec_push(out, check_dolphin(d->root));
    avp_vec_push(out, check_ps2_exe(d->root));
    check_derived(d->root, out);
}

/* ---- slots (slot.Repo's worktree list here; slot.health from the slot tool's library, tool_slot) -------------- */
void doctor_check_slots(Doctor *d, DoctorChecks *out) {
    ShsmGitOpts q = {.quotepath_off = true};
    ShsmGitRun r = SHSM_GIT_OPTS(A, d->root, &q, "rev-parse", "--show-toplevel", "--git-common-dir");
    if (!shsm_git_ok(&r)) {
        avp_vec_push(out, OPT("slots", "WARN", FMT("not a git repository: %s", d->root), ""));
        return;
    }
    AvpStrVec tc = avp_str_splitlines(r.out, false);
    const char *top = tc.n ? avp_cz(A, tc.v[0]) : "";
    ShsmGitRun w = SHSM_GIT_OPTS(A, top, &q, "worktree", "list", "--porcelain");
    if (!shsm_git_ok(&w)) {
        /* ToolError("git %s: %s" % (" ".join(args), (stderr or stdout).strip().splitlines()[-1:])) */
        AvpStrVec el = avp_str_splitlines(avp_str_strip(w.err.n ? w.err : w.out), false);
        avp_vec_push(out, OPT("slots", "WARN",
                              FMT("git worktree list --porcelain: %s",
                                  el.n ? FMT("[%R]", &el.v[el.n - 1]) : "[]"),
                              ""));
        return;
    }
    /* slot.Repo.worktrees(): path = os.path.normpath(line[9:]) */
    AvpStrVec paths = {0};
    AvpStrVec lines = avp_str_splitlines(w.out, false);
    for (size_t i = 0; i < lines.n; i++)
        if (avp_str_startswith(lines.v[i], AVP_S("worktree ")))
            avp_vec_push(&paths, avp_str(avp_path_normpath(A, avp_cz(A, avp_str_slice(lines.v[i], 9, AVP_END)))));
    if (!paths.n) {
        avp_vec_push(out, OPT("slots", "WARN", "git worktree list returned nothing", ""));
        return;
    }
    const char *main_path = paths.v[0].p;
    char *main_key = shsm_git_path_key(A, main_path);
    for (size_t i = 0; i < paths.n; i++) {
        const char *p = paths.v[i].p;
        if (strcmp(shsm_git_path_key(A, p), main_key) == 0) continue;
        AvpStr base = avp_path_basename(p);
        char *name = FMT("slot %S", &base);
        if (!avp_isdir(p)) {
            avp_vec_push(out, OPT(name, "MISSING", p, "git worktree prune"));
            continue;
        }
        AvpStrVec probs = slot_health(A, p, main_path).problems;
        avp_vec_push(out, OPT(name, probs.n ? "MISMATCH" : "OK",
                              probs.n ? avp_cz(A, avp_str_join(A, AVP_S("; "), probs.v, probs.n)) : "links ok",
                              FMT("build/tools/slot.exe check %S", &base)));
    }
}

/* ---- check_all ------------------------------------------------------------------------------------------------- */
static int cmp_str(const void *x, const void *y) { return avp_str_cmp(*(const AvpStr *)x, *(const AvpStr *)y); }

void doctor_check_all(Doctor *d, DoctorChecks *c) {
    const char *root = d->root;
    doctor_check_required(d, c);
    char *miss = missing_compilers(root, doctor_compilers_optional);
    if (miss)
        avp_vec_push(c, OPT("compilers-extra", "MISSING", miss, "copy into tools/compilers/"));
    else
        avp_vec_push(c, OPT("compilers-extra", "OK", avp_cz(A, joined(doctor_compilers_optional, ", ")), ""));

    AvpPtrVec inputs = {0};
    avp_vec_push(&inputs, join2(root, "configure.py"));
    AvpStrVec txt = avp_glob(A, join2(root, "config/R5WEA4/*.txt"), false);
    for (size_t i = 0; i < txt.n; i++) avp_vec_push(&inputs, avp_cz(A, txt.v[i]));
    double t_in = newest((const char *const *)inputs.v, inputs.n);
    static const char *const gen[] = {"build.ninja", "objdiff.json"};
    for (size_t i = 0; i < 2; i++) {
        char *p = join2(root, gen[i]);
        if (!avp_isfile(p))
            avp_vec_push(c, REQ(gen[i], "MISSING", "not generated", "build/tools/configure.exe"));
        else if (mtime_or_raise(p) < t_in)
            avp_vec_push(c, REQ(gen[i], "STALE", "older than configure.py/config txt", "build/tools/configure.exe"));
        else
            avp_vec_push(c, REQ(gen[i], "OK", "current", ""));
    }

    char *asm_dir = join2(root, "build/R5WEA4/asm");
    bool have_asm = avp_isdir(asm_dir);
    if (!have_asm)
        avp_vec_push(c, REQ("asm", "MISSING", "build/R5WEA4/asm", "tools/bin/ninja.exe (dtk split)"));
    else
        avp_vec_push(c, REQ("asm", "OK", "build/R5WEA4/asm", ""));
    if (have_asm) avp_vec_push(c, doctor_check_asm_listings(d));
    char *cj = join2(root, "build/R5WEA4/classes.json");
    if (!avp_isfile(cj)) {
        avp_vec_push(c, OPT("classes.json", "MISSING", "rule checker skips R13 slot checks", "build/tools/classes.exe scan"));
    } else {
        double t_tree = have_asm ? newest_in_tree(asm_dir) : 0.0;
        const char *sym = join2(root, "config/R5WEA4/symbols.txt");
        double t_sym = newest(&sym, 1);
        double t_ref = t_tree > t_sym ? t_tree : t_sym;
        if (mtime_or_raise(cj) < t_ref)
            avp_vec_push(c, REQ("classes.json", "STALE", "older than the asm or symbols.txt (false R13 findings)",
                                "build/tools/classes.exe scan"));
        else
            avp_vec_push(c, REQ("classes.json", "OK", "current", ""));
    }

    const DoctorPin *pin = doctor_pin("sjiswrap");
    char *p = join2(root, pin->path);
    if (!avp_isfile(p)) {
        avp_vec_push(c, OPT("sjiswrap", "MISSING", pin->path, "downloaded by ninja"));
    } else {
        bool ok = strcmp(file_hex(AVP_HASH_SHA256, p), pin->sha256) == 0;
        avp_vec_push(c, OPT("sjiswrap", ok ? "OK" : "MISMATCH", FMT("%s %s", pin->path, pin->version),
                            "delete it and let ninja download it"));
    }

    avp_vec_push(c, doctor_check_venv(d));
    avp_vec_push(c, doctor_check_permuter(d));
    char *bu = join2(root, "build/binutils");
    bool objdump = avp_isfile(join2(bu, "powerpc-eabi-objdump.exe")) || avp_isfile(join2(bu, "powerpc-eabi-objdump"));
    avp_vec_push(c, OPT("binutils", objdump ? "OK" : "MISSING", objdump ? "build/binutils (objdump)" : "build/binutils",
                        "build/tools/doctor.exe --fix binutils"));

    char *hd = doctor_hooks_dir(A, root);
    char *hp = hd ? join2(hd, "pre-commit") : NULL;
    if (hp && avp_isfile(hp)) {
        bool mine = doctor_hook_shim(hp) != NULL; /* the C or the Python shim */
        avp_vec_push(c, OPT("hooks", mine ? "OK" : "WARN", mine ? "pre-commit installed" : "a foreign pre-commit hook is installed",
                            "build/tools/doctor.exe --install-hooks"));
    } else {
        avp_vec_push(c, OPT("hooks", "MISSING", "pre-commit not installed (user's choice)",
                            "build/tools/doctor.exe --install-hooks"));
    }

    char *b = join2(root, "build");
    if (avp_isdir(b)) {
        AvpStrVec names = {0};
        if (!avp_listdir(A, b, &names)) shsm_raise_io();
        AvpStrVec stray = {0};
        for (size_t i = 0; i < names.n; i++) {
            bool ok = false;
            for (size_t k = 0; doctor_build_root_ok[k] && !ok; k++) ok = avp_str_eqz(names.v[i], doctor_build_root_ok[k]);
            if (!ok) avp_vec_push(&stray, names.v[i]);
        }
        qsort(stray.v, stray.n, sizeof(AvpStr), cmp_str);
        char *detail = "clean";
        if (stray.n) {
            const AvpStr *shown = SP(avp_str_join(A, AVP_S(", "), stray.v, stray.n > 12 ? 12 : stray.n));
            const char *more = stray.n > 12 ? " ..." : "";
            i64 count = (i64)stray.n;
            detail = FMT("%d stray: %S%s", count, shown, more);
        }
        avp_vec_push(c, OPT("build-root", stray.n ? "WARN" : "OK", detail, "review and delete by hand"));
    }

    avp_vec_push(c, doctor_check_ghidra(d));
    avp_vec_push(c, doctor_check_index(d));
    doctor_check_advisory(d, c);
    doctor_check_slots(d, c);
}
