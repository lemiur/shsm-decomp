/* draft.py's grading (Grader, resolve_profile, grade, grade_exit, cmd_grade) and the rejects table (load_rejects,
 * save_rejects, basis_of, cmd_reject, cmd_rejects), plus cmd_packet (brief's page, from the C brief in process).
 *
 * The Grader's external steps: the compile runs mwcceppc through shsm_compile_source (units.compile_source), the
 * comparison is lib/cc_compare (tools/common/compare.py), and the rule check runs check_rules' C library in process
 * with the same arguments the Python passes to `check_rules.py --json --no-summary --root <root> <paths>`: the JSON
 * document it would print is built and read back, so the findings are the dicts the Python decodes. */
#include "draft.h"

#include "brief.h"
#include "cc_compare.h"
#include "shsm_libraries.h"
#include "shsm_re.h"

#include <errno.h>
#include <string.h>

static const char REJECTS_HEADER[] = "symbol\tapproach\treason\tbasis";


static AvpJson *jstr_opt(AvpArena *a, const char *z) { return z ? avp_json_strz(a, z) : avp_json_null(); }

static void read_text(AvpArena *a, const char *path, AvpTextMode m, AvpStr *out) {
    if (!avp_read_text(a, path, m, out)) shsm_raise_io();
}

static void write_text(AvpArena *a, const char *path, AvpStr text) {
    AvpStr enc;
    if (!avp_encode_text(a, text, AVP_TM_UTF8_NL_LF, &enc)) shsm_raise_io();
    if (!avp_write_file(path, enc.p, enc.n)) shsm_raise_io();
}

/* ---- cli.first_error (the compiler's first real error; the same port as neutral's) ------------------------------ */

typedef struct {
    AvpStr tool, kind, file, code, message;
    bool has_line;
    i64 line;
} MwMessage;

static bool all_dashes(AvpStr s) {
    for (size_t i = 0; i < s.n; i++)
        if (s.p[i] != '-') return false;
    return true;
}

AvpStr draft_first_error(AvpArena *a, AvpStr output) {
    const ShsmRe *head = shsm_re("^### (\\S+) ([^:]+):\\s*$", 0);
    const ShsmRe *file = shsm_re("^#\\s+File:\\s*(.+?)\\s*$", 0);
    const ShsmRe *linere = shsm_re("^#\\s+(\\d+):\\s?(.*)$", 0);
    const ShsmRe *kindre = shsm_re("^#\\s+(Error|Warning):", 0);
    const ShsmRe *msgre = shsm_re("^#\\s+(?:\\((\\d+)\\)\\s*)?(.*?)\\s*$", 0);
    AVP_VEC(MwMessage) msgs = {0};
    bool have = false, marked = false;
    AvpStrVec lines = avp_str_splitlines(output, false);
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr line = avp_str_rstrip(lines.v[i]);
        ShsmReMatch m;
        if (shsm_re_match(head, line, 0, AVP_END, &m)) {
            MwMessage nm = {0};
            nm.tool = shsm_re_group(&m, 1);
            nm.kind = avp_str_strip(shsm_re_group(&m, 2));
            marked = !avp_str_eqz(nm.kind, "Compiler") && !avp_str_eqz(nm.kind, "Linker");
            avp_vec_push(&msgs, nm);
            have = true;
            continue;
        }
        if (!have) continue;
        MwMessage *cur = &msgs.v[msgs.n - 1];
        if (!avp_str_startswith(line, AVP_S("#")) || cur->message.p) continue;
        AvpStr body = avp_str_strip(avp_str_slice(line, 1, (i64)line.n));
        if (!body.n || all_dashes(body)) continue;
        if (shsm_re_match(file, line, 0, AVP_END, &m)) {
            cur->file = shsm_re_group(&m, 1);
            continue;
        }
        if (!marked) {
            if (shsm_re_match(kindre, line, 0, AVP_END, &m)) {
                cur->kind = shsm_re_group(&m, 1);
                marked = true;
                continue;
            }
            if (shsm_re_match(linere, line, 0, AVP_END, &m) && !cur->has_line) {
                cur->has_line = true;
                i64 v = 0;
                AvpStr g = shsm_re_group(&m, 1);
                for (size_t q = 0; q < g.n; q++) v = v * 10 + (g.p[q] - '0');
                cur->line = v;
            }
            continue;
        }
        if (shsm_re_match(msgre, line, 0, AVP_END, &m)) {
            cur->code = shsm_re_group(&m, 1);
            cur->message = shsm_re_group(&m, 2);
        }
    }
    AvpStr res = {0};
    for (size_t i = 0; i < msgs.n && !res.p; i++) {
        const MwMessage *mm = &msgs.v[i];
        if (!avp_str_eqz(mm->kind, "Error") || !mm->message.n) continue;
        AvpStr where = mm->tool;
        if (mm->file.p && mm->file.n) {
            AvpStr f = avp_str_replace(a, mm->file, AVP_S("\\"), AVP_S("/"), -1);
            where = avp_path_basename(avp_cz(a, f));
        }
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_put(&b, where);
        if (mm->has_line) avp_buf_printf(&b, ":%lld", (long long)mm->line);
        avp_buf_putz(&b, ": ");
        if (mm->code.n) avp_buf_printf(&b, "(%.*s) ", AVP_SF(mm->code));
        avp_buf_put(&b, mm->message);
        res = avp_str_dup(a, avp_buf_str(&b));
    }
    avp_vec_free(&msgs);
    avp_vec_free(&lines);
    return res;
}

/* ---- Grader ----------------------------------------------------------------------------------------------------- */

static bool off_drive(AvpArena *a, const char *p, const char *root) {
    char *rel;
    return !avp_path_relpath(a, p, root, &rel);
}

const char **draft_local_copies(AvpArena *a, const ShsmRepo *r, const char *const *paths, size_t n) {
    const char **out = AVP_NEWN(a, const char *, n ? n : 1);
    AvpPtrVec far = {0};
    for (size_t i = 0; i < n; i++) {
        out[i] = paths[i];
        if (off_drive(a, paths[i], r->root)) avp_vec_push(&far, avp_cz(a, avp_path_dirname(avp_path_abspath(a, paths[i]))));
    }
    if (!far.n) {
        avp_vec_free(&far);
        return out;
    }
    char *base;
    if (!avp_path_commonpath(a, (const char *const *)far.v, far.n, &base)) shsm_raise_io();
    avp_vec_free(&far);
    AvpStr low = avp_str_lower(a, avp_str(base));
    const char *dst = avp_join(a, draft_root(a, r), "_rules", avp_cz(a, avp_str_slice(avp_str(avp_sha1_hex(a, low.p, low.n)), 0, 12)));
    for (size_t i = 0; i < n; i++) {
        if (!off_drive(a, paths[i], r->root)) continue;
        char *rel;
        if (!avp_path_relpath(a, avp_path_abspath(a, paths[i]), base, &rel)) shsm_raise_io();
        const char *q = avp_path_join(a, dst, rel);
        if (!avp_makedirs(avp_cz(a, avp_path_dirname(q)), true)) shsm_raise_io();
        if (!avp_copyfile(paths[i], q)) shsm_raise_io();
        out[i] = q;
    }
    return out;
}

DraftRules draft_rules(AvpArena *a, const ShsmRepo *r, const char *const *paths, size_t n) {
    DraftRules res = {0};
    const char **local = draft_local_copies(a, r, paths, n);
    /* check_rules --json --no-summary --root ROOT PATHS, the checkout's C checker run as the Python runs its own
     * (a process per grade: the checker holds the whole source tree and classes.json while it runs) */
    const char **argv = AVP_NEWN(a, const char *, n + 6);
    argv[0] = avp_join(a, r->root, "build", "tools", "check_rules.exe");
    argv[1] = "--json";
    argv[2] = "--no-summary";
    argv[3] = "--root";
    argv[4] = r->root;
    for (size_t i = 0; i < n; i++) argv[5 + i] = local[i];
    ShsmRun run = shsm_run(a, argv, n + 5, r->root, 600);
    AvpJsonError je;
    AvpJson *d = avp_json_parse(a, run.out, &je);
    if (!d) {
        /* (res.stderr or res.stdout or "").strip().split("\n")[:1] */
        AvpStr t = avp_str_strip(run.err.n ? run.err : run.out);
        i64 nl = avp_str_find(t, AVP_S("\n"), 0, AVP_END);
        res.error = avp_cz(a, nl >= 0 ? avp_strn(t.p, (size_t)nl) : t);
        return res;
    }
    res.findings = avp_json_arr(a);
    AvpJson *fs = avp_json_getz(d, "findings");
    for (size_t i = 0; fs && avp_json_is_arr(fs) && i < avp_json_len(fs); i++) {
        AvpJson *x = avp_json_at(fs, i);
        if (avp_json_is_obj(x) && avp_str_eqz(avp_json_get_str(x, "tier", (AvpStr){0}), "match"))
            avp_json_arr_push(a, res.findings, x);
    }
    AvpJson *r13 = avp_json_getz(d, "r13");
    AvpJson *note = avp_json_is_obj(r13) ? avp_json_getz(r13, "note") : NULL;
    if (note && avp_json_is_str(note) && avp_json_truthy(note) &&
        avp_str_find(avp_json_str_or(note, (AvpStr){0}), AVP_S("skipped"), 0, AVP_END) >= 0)
        res.r13 = avp_cz(a, avp_json_str_or(note, (AvpStr){0}));
    return res;
}

/* ---- resolve_profile -------------------------------------------------------------------------------------------- */

static const ShsmUnit *named_unit(const ShsmRepo *r, const char *name) {
    AvpTry t;
    const ShsmUnit *u = NULL;
    if (AVP_TRY(&t)) {
        u = shsm_get_unit(r, name);
        avp_try_end(&t);
    } else {
        if (!shsm_exc_is_key(t.code)) avp_throw(t.code, "%s", t.msg);
        shsm_raise(SHSM_EXC_USAGE, "no unit %s", name);
    }
    return u;
}

static bool truthy_str(const AvpJson *v) { return v && avp_json_is_str(v) && avp_json_len(v) > 0; }

const ShsmUnit *draft_resolve_profile(DraftWorld *W, i64 target_addr, const char *lib, const AvpJson *meta,
                                      const char *profile, const char *as_unit, const char **how) {
    AvpArena *a = W->a;
    bool meta_t = meta && avp_json_truthy(meta);
    const char *mprof = NULL, *mwhy = "twin unit";
    if (meta_t) {
        AvpJson *p = avp_json_getz(meta, "profile");
        if (avp_json_truthy(p)) mprof = avp_cz(a, avp_json_str_or(p, (AvpStr){0}));
    }
    if (meta) {
        AvpJson *w = avp_json_getz(meta, "profile_why");
        if (avp_json_truthy(w)) mwhy = avp_cz(a, avp_json_str_or(w, (AvpStr){0}));
    }
    const char *names[3] = {profile, mprof, as_unit};
    const char *hows[3] = {"--profile", mwhy, "--as-unit"};
    for (int i = 0; i < 3; i++)
        if (names[i] && names[i][0]) {
            *how = hows[i];
            return named_unit(W->repo, names[i]);
        }
    if (target_addr != DRAFT_NONE) {
        DraftProfile p;
        if (draft_infer_profile(a, W->repo, target_addr, lib, lib != NULL, &p) && p.unit && p.unit[0]) {
            *how = avp_cz(a, avp_pyfmt(a, "nearest source unit of library %s", lib));
            return shsm_get_unit(W->repo, p.unit);
        }
    }
    shsm_raise(SHSM_EXC_USAGE, "no profile: give --profile UNIT");
}

/* ---- grade -------------------------------------------------------------------------------------------------------- */

static CcRepo cc_repo(const ShsmRepo *r) { return (CcRepo){r->root, r->version}; }

static AVP_NORETURN void raise_cc(const CcError *e) {
    int code = e->kind == CC_ERR_USAGE ? SHSM_EXC_USAGE
               : e->kind == CC_ERR_KEY ? SHSM_EXC_KEY
               : e->kind == CC_ERR_OS  ? SHSM_EXC_OS
                                       : SHSM_EXC_VALUE;
    shsm_raise(code, "%.*s", AVP_SF(e->msg));
}

/* Grader.compare: exact, and the count of differing rows (rename-explained target mismatches excluded). false with
 * *keyerr set: the KeyError compare_function raised. */
static bool grader_compare(AvpArena *a, const ShsmRepo *r, AvpStr asm_unit, const char *obj, const char *cname,
                           const char *rname, const char *asm_path, const AvpMap *compiled, const AvpMap *renames,
                           bool *exact, i64 *nd, const char **keyerr) {
    CcRepo cr = cc_repo(r);
    CcError err = {0};
    CcCompareResult *res = cc_compare_function_unit(a, &cr, asm_unit, obj, avp_str(cname), avp_str(rname), true,
                                                    compiled, asm_path, &err);
    if (!res) {
        if (err.kind == CC_ERR_KEY) {
            *keyerr = avp_cz(a, shsm_exc_str(a, SHSM_EXC_KEY, avp_cz(a, err.msg)));
            return false;
        }
        raise_cc(&err);
    }
    if (res->exact) {
        *exact = true;
        *nd = 0;
        return true;
    }
    AvpSet *ok_i = avp_set_new();
    for (size_t i = 0; res->target_mismatches && i < res->target_mismatches->n; i++) {
        const CcTargetDiff *t = &res->target_mismatches->v[i];
        if (draft_rename_explains(a, t->retail, t->compiled, t->detail, renames))
            avp_set_add(ok_i, avp_str(avp_sprintf(a, "%zu", t->i)));
    }
    i64 n = 0;
    for (size_t i = 0; i < res->npairs; i++) {
        const CcPair *p = &res->pairs[i];
        if (!cc_pair_differs(p)) continue;
        if (p->status == CC_EQUAL && p->target && !strcmp(p->target, "mismatch") && p->retail &&
            avp_set_has(ok_i, avp_str(avp_sprintf(a, "%zu", p->retail->index))))
            continue;
        n++;
    }
    avp_set_free(ok_i);
    *exact = n == 0;
    *nd = n;
    return true;
}

static i64 hex_value(AvpStr s, bool *ok) {
    AvpStr t = avp_str_strip(s);
    if (t.n >= 2 && t.p[0] == '0' && (t.p[1] == 'x' || t.p[1] == 'X')) t = avp_str_slice(t, 2, (i64)t.n);
    i64 v = 0;
    *ok = t.n > 0;
    for (size_t i = 0; i < t.n && *ok; i++) {
        char c = t.p[i];
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0 && c == '_') continue;
        if (d < 0) *ok = false;
        else v = v * 16 + d;
    }
    return v;
}

typedef struct {
    i64 address;
    const char *name, *cls;
} Wanted;
typedef AVP_VEC(Wanted) WantedVec;

typedef struct {
    const AvpJson *meta;
    AvpStrVec sinits;
} UnmarkedCtx;

/* grade()'s unmarked(addr): the compiled counterpart of the twin draft's own target when no definition marks it. */
static bool unmarked(AvpArena *a, const UnmarkedCtx *c, i64 addr, Wanted *out) {
    if (!c->meta || !avp_json_truthy(c->meta)) return false;
    AvpJson *ma = avp_json_getz(c->meta, "address");
    bool ok = true;
    i64 mv = ma ? hex_value(avp_json_str_or(ma, (AvpStr){0}), &ok) : 0;
    if (!ok) shsm_raise(SHSM_EXC_VALUE, "invalid literal for int() with base 16");
    if (mv != addr) return false;
    AvpJson *tw = avp_json_getz(c->meta, "twin");
    AvpStr twin = tw && avp_json_is_str(tw) ? avp_json_str_or(tw, (AvpStr){0}) : AVP_S("");
    if (avp_str_startswith(twin, AVP_S("__sinit_")) && c->sinits.n) {
        *out = (Wanted){addr, avp_cz(a, c->sinits.v[0]), NULL};
        return true;
    }
    AvpJson *comp = avp_json_getz(c->meta, "compiled");
    if (!comp) shsm_raise(SHSM_EXC_KEY, "compiled");
    const char *cls = NULL;
    const char *name = draft_demangled_parts(a, avp_cz(a, avp_json_str_or(comp, (AvpStr){0})), &cls);
    *out = (Wanted){addr, name, cls};
    return true;
}

static AvpJson *fn_json(AvpArena *a, const DraftGradeFn *e) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("retail"), jstr_opt(a, e->retail));
    avp_json_set(a, o, AVP_S("address"), avp_json_str(a, avp_pyfmt(a, "0x%08X", e->address)));
    if (e->marked) {
        avp_json_set(a, o, AVP_S("name"), jstr_opt(a, e->name));
        avp_json_set(a, o, AVP_S("class"), jstr_opt(a, e->cls));
        avp_json_set(a, o, AVP_S("compiled"), jstr_opt(a, e->compiled));
    }
    avp_json_set(a, o, AVP_S("status"), avp_json_strz(a, e->status));
    avp_json_set(a, o, AVP_S("rows"), e->rows == DRAFT_NONE ? avp_json_null() : avp_json_int(a, e->rows));
    if (e->error) avp_json_set(a, o, AVP_S("error"), avp_json_strz(a, e->error));
    return o;
}

/* renames.setdefault(k, v) on both views */
static void rename_default(AvpArena *a, AvpJson *ren, AvpMap *map, AvpStr k, AvpStr v) {
    if (avp_json_has(ren, k)) return;
    avp_json_set(a, ren, k, avp_json_str(a, v));
    draft_smap_set(a, map, k, v);
}

AvpStrVec draft_clone_origin_dirs(AvpArena *a, const char *repo_include, const AvpStrVec *roots,
                                 const char *const *headers, size_t nheaders) {
    AvpStrVec out = {0};
    AvpSet *seen = avp_set_new();
    for (size_t k = 0; k < nheaders; k++) {
        const char *hdir = avp_cz(a, avp_path_dirname(headers[k]));
        for (size_t i = 0; i < roots->n; i++) {
            char *rel = NULL;
            if (!avp_path_relpath(a, hdir, avp_cz(a, roots->v[i]), &rel)) {
                avp_io_clear();
                continue;
            }
            if (!rel[0] || !strcmp(rel, ".") || avp_str_startswith(avp_str(rel), AVP_S(".."))) continue; /* the root itself, or not under it */
            const char *orig = avp_path_join(a, repo_include, rel);
            if (!avp_isdir(orig) || !avp_set_add(seen, avp_str(orig))) continue;
            avp_vec_push(&out, avp_str(orig));
        }
    }
    avp_set_free(seen);
    return out;
}

DraftGrade *draft_grade(DraftWorld *W, const char *target, const char *file_arg, const char *as_unit,
                        const char *profile, const AvpJson *meta, const char *workdir) {
    AvpArena *a = W->a;
    const ShsmRepo *repo = W->repo;
    DraftGrade *g = AVP_NEW(a, DraftGrade);
    const char *file = avp_path_abspath(a, file_arg);
    AvpStr text;
    read_text(a, file, AVP_TM_UTF8_REPLACE, &text);
    if (!meta) {
        const char *mp = avp_path_join(a, avp_cz(a, avp_path_dirname(file)), "meta.json");
        if (avp_exists(mp)) {
            AvpStr mt;
            read_text(a, mp, AVP_TM_UTF8, &mt);
            AvpJsonError je;
            meta = avp_json_parse(a, mt, &je);
            if (!meta) shsm_raise(SHSM_EXC_VALUE, "%.*s", AVP_SF(avp_json_error_str(a, &je)));
        }
    }
    bool meta_t = meta && avp_json_truthy(meta);
    AvpStrVec includes = {0};
    if (meta_t) {
        AvpJson *inc = avp_json_getz(meta, "include_dir");
        if (avp_json_truthy(inc)) avp_vec_push(&includes, avp_json_str_or(inc, (AvpStr){0}));
    }
    AvpPtrVec extra_hdrs = {0};
    for (size_t i = 0; i < includes.n; i++) {
        AvpWalk *wk = avp_walk_open(avp_cz(a, includes.v[i]), true);
        AvpWalkItem it = {0};
        while (wk && avp_walk_next(wk, &it)) {
            const char *dp = avp_cz(a, it.dirpath);
            for (size_t k = 0; k < it.files.n; k++)
                if (avp_str_endswith(it.files.v[k], AVP_S(".h")))
                    avp_vec_push(&extra_hdrs, avp_path_join(a, dp, avp_cz(a, it.files.v[k])));
        }
        if (wk) avp_walk_close(wk);
    }
    DraftMarks marks = draft_marked_functions(a, text);
    /* renames: the twin draft's map, then the compiled names graded under another name */
    AvpJson *ren = avp_json_obj(a);
    AvpMap *renmap = avp_map_new();
    if (meta) {
        AvpJson *sub = avp_json_getz(meta, "subst");
        AvpJson *rm = sub && avp_json_is_obj(sub) ? avp_json_getz(sub, "renames") : NULL;
        for (size_t i = 0; rm && avp_json_is_obj(rm) && i < avp_json_len(rm); i++) {
            const AvpJsonMember *m = avp_json_member(rm, i);
            avp_json_set(a, ren, m->key, m->val);
            draft_smap_set(a, renmap, m->key, avp_json_str_or(m->val, (AvpStr){0}));
        }
    }
    bool has_rng = false;
    i64 r0 = 0, r1 = 0;
    if (target && target[0]) has_rng = draft_parse_range(avp_str(target), &r0, &r1);
    i64 taddr = DRAFT_NONE;
    if (!has_rng && target && target[0]) {
        ShsmSymRow row;
        if (!shsm_asmindex_symbol(a, draft_world_ix(W), avp_str(target), &row)) {
            if (!as_unit) shsm_raise(SHSM_EXC_USAGE, "unknown symbol %s", target);
        } else {
            taddr = row.address;
        }
    }
    const char *lib = NULL;
    if (meta_t) {
        AvpJson *l = avp_json_getz(meta, "lib");
        if (l && !avp_json_is_null(l)) lib = avp_cz(a, avp_json_str_or(l, (AvpStr){0}));
    }
    if (!lib && taddr != DRAFT_NONE) lib = shsm_library_of(a, taddr, repo);
    const char *how = NULL;
    const ShsmUnit *unit = draft_resolve_profile(W, taddr != DRAFT_NONE ? taddr : (marks.n ? marks.v[0].address : DRAFT_NONE),
                                                 lib, meta, profile, as_unit, &how);
    /* the screen over the draft and the cloned headers */
    AvpJson *hits = avp_json_arr(a);
    i64 nhits = 0;
    const char *h0file = NULL;
    DraftScreenHit h0 = {0};
    for (size_t i = 0; i <= extra_hdrs.n; i++) {
        const char *p = i ? extra_hdrs.v[i - 1] : file;
        AvpStr t = text;
        if (i) read_text(a, p, AVP_TM_UTF8_REPLACE, &t);
        DraftScreenHits hs = draft_screen(t);
        const char *base = avp_cz(a, avp_path_basename(p));
        for (size_t k = 0; k < hs.n; k++) {
            if (!nhits) {
                h0file = base;
                h0 = hs.v[k];
            }
            nhits++;
            AvpJson *h = avp_json_arr(a);
            avp_json_arr_push(a, h, avp_json_strz(a, base));
            avp_json_arr_push(a, h, avp_json_int(a, hs.v[k].line));
            avp_json_arr_push(a, h, avp_json_strz(a, hs.v[k].label));
            avp_json_arr_push(a, h, avp_json_str(a, hs.v[k].text));
            avp_json_arr_push(a, hits, h);
        }
        avp_vec_free(&hs);
    }
    g->profile = unit->name;
    g->profile_how = how;
    g->compiler = unit->mw_version;
    AvpStr prof = avp_pyfmt(a, "profile %s (%s, %s)", draft_plain_unit(a, unit->name), how, unit->mw_version);
    AvpStr stem, ext;
    avp_path_splitext(avp_cz(a, avp_path_basename(file)), &stem, &ext);
    const char *stemz = avp_cz(a, stem);
    const char *wd = workdir ? workdir : avp_join(a, draft_root(a, repo), "_grade", stemz);
    const char *obj = avp_path_join(a, wd, avp_sprintf(a, "%s.o", stemz));
    /* Grader.compile */
    AvpPtrVec flags = {0};
    for (size_t i = 0; i < includes.n; i++) {
        avp_vec_push(&flags, (void *)"-i");
        avp_vec_push(&flags, (void *)avp_cz(a, includes.v[i]));
    }
    /* the folders the cloned headers came from, so their bare-name includes resolve as they do in include/ */
    AvpStrVec origin = draft_clone_origin_dirs(a, repo->include_dir, &includes, (const char *const *)extra_hdrs.v,
                                               extra_hdrs.n);
    for (size_t i = 0; i < origin.n; i++) {
        avp_vec_push(&flags, (void *)"-i");
        avp_vec_push(&flags, (void *)avp_cz(a, origin.v[i]));
    }
    ShsmCompileOpts co = {.extra_flags = (const char *const *)flags.v, .nextra = flags.n, .sjis = -1};
    ShsmRun cr = shsm_compile_source(a, repo, unit, file, obj, &co, 300);
    AvpStrVec lines = {0};
    if (!(cr.returncode == 0 && !cr.timed_out)) {
        AvpStr out = shsm_run_output(a, &cr);
        AvpStr err = draft_first_error(a, out);
        if (!err.p) {
            AvpStrVec ls = avp_str_split(avp_str_strip(out), AVP_S("\n"), -1);
            err = ls.n ? ls.v[0] : AVP_S("");
        }
        const char *e = err.n ? avp_cz(a, err) : "unknown error";
        g->verdict = "FAIL";
        g->reason = avp_sprintf(a, "compile: %s", e);
        avp_vec_push(&lines, avp_pyfmt(a, "GRADE FAIL compile: %s", e));
        avp_vec_push(&lines, prof);
    } else {
        CcRepo ccr = cc_repo(repo);
        CcError cerr = {0};
        AvpMap *compiled = cc_load_compiled(a, &ccr, obj, NULL, NULL, NULL, &cerr);
        if (!compiled) raise_cc(&cerr);
        AvpPtrVec names = {0};
        UnmarkedCtx uc = {meta, {0}};
        AvpMapIter mi = {0};
        while (avp_map_next(compiled, &mi)) {
            const char *nm = avp_cz(a, mi.key);
            avp_vec_push(&names, (void *)nm);
            if (avp_str_startswith(mi.key, AVP_S("__sinit_"))) avp_vec_push(&uc.sinits, mi.key);
        }
        WantedVec wanted = {0};
        if ((as_unit && as_unit[0]) || has_rng) {
            for (size_t i = 0; i < marks.n; i++)
                avp_vec_push(&wanted, ((Wanted){marks.v[i].address, marks.v[i].name, marks.v[i].cls}));
        } else if (taddr != DRAFT_NONE) {
            for (size_t i = 0; i < marks.n; i++)
                if (marks.v[i].address == taddr)
                    avp_vec_push(&wanted, ((Wanted){marks.v[i].address, marks.v[i].name, marks.v[i].cls}));
            Wanted u;
            if (!wanted.n && unmarked(a, &uc, taddr, &u)) avp_vec_push(&wanted, u);
            if (!wanted.n) {
                bool in = false;
                for (size_t i = 0; i < names.n && !in; i++) in = !strcmp(names.v[i], target);
                if (in) avp_vec_push(&wanted, ((Wanted){taddr, target, NULL}));
            }
        }
        if (has_rng) {
            AvpSet *marked = avp_set_new();
            for (size_t i = 0; i < marks.n; i++) avp_set_add(marked, avp_str(avp_sprintf(a, "%lld", (long long)marks.v[i].address)));
            AvpPtrVec fns = {0};
            shsm_sym_functions(draft_world_st(W), &fns);
            for (size_t i = 0; i < fns.n; i++) {
                const ShsmSymbol *fn = fns.v[i];
                i64 ad = (i64)fn->address;
                if (!(r0 <= ad && ad < r1) || avp_set_has(marked, avp_str(avp_sprintf(a, "%lld", (long long)ad)))) continue;
                Wanted u;
                if (unmarked(a, &uc, ad, &u)) {
                    avp_vec_push(&wanted, u);
                } else {
                    avp_vec_push(&g->functions, ((DraftGradeFn){.retail = fn->name, .address = ad, .marked = false,
                                                                .status = "MISSING", .rows = DRAFT_NONE}));
                }
            }
            avp_set_free(marked);
        }
        for (size_t wi = 0; wi < wanted.n; wi++) {
            const Wanted *w = &wanted.v[wi];
            ShsmSymRow rrow;
            bool have = shsm_asmindex_symbol_at(a, draft_world_ix(W), w->address, &rrow);
            DraftGradeFn e = {.retail = have ? avp_cz(a, rrow.name) : NULL, .address = w->address, .marked = true,
                              .name = w->name, .cls = w->cls, .rows = DRAFT_NONE};
            const char *cname = draft_compiled_name_for(a, (const char *const *)names.v, names.n, w->name, w->cls);
            e.compiled = cname;
            if (have && cname && strcmp(cname, e.retail)) rename_default(a, ren, renmap, rrow.name, avp_str(cname));
            if (!have || !cname) {
                e.status = "MISSING";
            } else {
                const char *asm_path = rrow.file.n ? avp_path_join(a, repo->asm_dir, avp_cz(a, rrow.file)) : NULL;
                bool exact = false;
                i64 nd = 0;
                const char *kerr = NULL;
                if (grader_compare(a, repo, rrow.unit, obj, cname, e.retail, asm_path, compiled, renmap, &exact, &nd,
                                   &kerr)) {
                    e.status = exact ? "EXACT" : "DIFF";
                    e.rows = nd;
                } else {
                    e.status = "MISSING";
                    e.error = kerr;
                }
            }
            avp_vec_push(&g->functions, e);
        }
        size_t nfn = g->functions.n, n_ok = 0;
        for (size_t i = 0; i < nfn; i++) n_ok += !strcmp(g->functions.v[i].status, "EXACT");
        if (nfn && n_ok == nfn) {
            g->verdict = "MATCH";
            avp_vec_push(&lines, avp_pyfmt(a, "GRADE MATCH %d/%d", n_ok, nfn));
        } else if (!nfn) {
            g->verdict = "FAIL";
            g->reason = "no function to compare (mark each with its // 0x8XXXXXXX address comment)";
            avp_vec_push(&lines, avp_pyfmt(a, "GRADE FAIL %s", g->reason));
        } else {
            const DraftGradeFn *worst = NULL;
            for (size_t i = 0; i < nfn; i++) {
                const DraftGradeFn *e = &g->functions.v[i];
                if (!strcmp(e->status, "EXACT")) continue;
                if (!worst) {
                    worst = e;
                    continue;
                }
                bool em = !strcmp(e->status, "MISSING"), wm = !strcmp(worst->status, "MISSING");
                i64 er = e->rows == DRAFT_NONE ? 0 : e->rows, wr = worst->rows == DRAFT_NONE ? 0 : worst->rows;
                if (em > wm || (em == wm && er > wr)) worst = e;
            }
            g->verdict = "PARTIAL";
            g->reason = avp_cz(a, avp_pyfmt(a, "%s %s", avp_cz(a, avp_str_lower(a, avp_str(worst->status))), worst->retail));
            AvpStr rows = worst->rows == DRAFT_NONE ? AVP_S("") : avp_pyfmt(a, " (%d differing rows)", worst->rows);
            avp_vec_push(&lines, avp_pyfmt(a, "GRADE PARTIAL %d/%d worst %s %s%S", n_ok, nfn, worst->retail,
                                           worst->status, &rows));
        }
        avp_vec_push(&lines, prof);
    }
    /* Grader.rules over the draft and the cloned headers */
    AvpPtrVec rpaths = {0};
    avp_vec_push(&rpaths, (void *)file);
    for (size_t i = 0; i < extra_hdrs.n; i++) avp_vec_push(&rpaths, extra_hdrs.v[i]);
    DraftRules rr = draft_rules(a, repo, (const char *const *)rpaths.v, rpaths.n);
    size_t nf = rr.findings ? avp_json_len(rr.findings) : 0;
    if (rr.error) {
        avp_vec_push(&lines, avp_pyfmt(a, "rules: not run (%s)", rr.error));
    } else if (rr.r13) {
        avp_vec_push(&lines,
                     avp_pyfmt(a, "rules: %d [match] finding(s); R13 not checked: run build/tools/classes.exe scan", nf));
    } else if (nf) {
        AvpJson *f0 = avp_json_at(rr.findings, 0);
        AvpJson *line = avp_json_getz(f0, "line");
        AvpStr ls = !line || avp_json_is_null(line) ? AVP_S("None")
                    : avp_json_is_str(line)          ? avp_json_str_or(line, (AvpStr){0})
                                                     : avp_pyfmt(a, "%d", avp_json_i64(line, 0));
        AvpStr path = avp_json_get_str(f0, "path", AVP_S(""));
        AvpStr base = avp_path_basename(avp_cz(a, path));
        AvpStr rule = avp_json_get_str(f0, "rule", AVP_S("None")), msg = avp_json_get_str(f0, "message", AVP_S("None"));
        avp_vec_push(&lines, avp_pyfmt(a, "rules: %d [match] finding(s); first %S:%S %S %S", nf, &base, &ls, &rule, &msg));
    } else {
        avp_vec_push(&lines, AVP_S("rules: no [match] finding"));
    }
    if (nhits) {
        avp_vec_push(&lines, avp_pyfmt(a, "screen: %d decompiler artefact(s); first %s:%d %s `%S`", nhits, h0file,
                                       h0.line, h0.label, &h0.text));
    } else {
        avp_vec_push(&lines, AVP_S("screen: clean"));
    }
    if (as_unit && as_unit[0])
        avp_vec_push(&lines, AVP_S("layout: checked by promote (gatecheck.exe layout needs the unit configured)"));
    g->blocking = nf > 0 || nhits > 0 || rr.error != NULL || rr.r13 != NULL;
    for (size_t i = 0; i < lines.n && i < 8; i++) avp_vec_push(&g->lines, lines.v[i]);
    g->renames = ren;
    g->rules = rr;
    /* the result dict, in the Python's key order */
    AvpJson *d = avp_json_obj(a);
    avp_json_set(a, d, AVP_S("target"), jstr_opt(a, target));
    avp_json_set(a, d, AVP_S("file"), avp_json_strz(a, file));
    avp_json_set(a, d, AVP_S("profile"), avp_json_strz(a, unit->name));
    avp_json_set(a, d, AVP_S("profile_how"), avp_json_strz(a, how));
    avp_json_set(a, d, AVP_S("compiler"), jstr_opt(a, unit->mw_version));
    AvpJson *fa = avp_json_arr(a);
    for (size_t i = 0; i < g->functions.n; i++) avp_json_arr_push(a, fa, fn_json(a, &g->functions.v[i]));
    avp_json_set(a, d, AVP_S("functions"), fa);
    avp_json_set(a, d, AVP_S("screen"), hits);
    avp_json_set(a, d, AVP_S("rules"), rr.findings ? rr.findings : avp_json_arr(a));
    avp_json_set(a, d, AVP_S("verdict"), avp_json_strz(a, g->verdict));
    if (g->reason) avp_json_set(a, d, AVP_S("reason"), avp_json_strz(a, g->reason));
    avp_json_set(a, d, AVP_S("r13_note"), jstr_opt(a, rr.r13));
    if (rr.error) {
        AvpJson *ea = avp_json_arr(a);
        avp_json_arr_push(a, ea, avp_json_strz(a, rr.error));
        avp_json_set(a, d, AVP_S("rules_error"), ea);
    }
    avp_json_set(a, d, AVP_S("renames"), ren);
    avp_json_set(a, d, AVP_S("blocking"), avp_json_bool(g->blocking));
    AvpJson *la = avp_json_arr(a);
    for (size_t i = 0; i < g->lines.n; i++) avp_json_arr_push(a, la, avp_json_str(a, g->lines.v[i]));
    avp_json_set(a, d, AVP_S("lines"), la);
    g->doc = d;
    return g;
}

int draft_grade_exit(const DraftGrade *g) {
    return !strcmp(g->verdict, "MATCH") && !g->blocking ? SHSM_EXIT_OK : SHSM_EXIT_FINDING;
}

static void output_doc(ShsmOutput *out, const AvpJson *doc) {
    for (size_t i = 0; i < avp_json_len(doc); i++) {
        const AvpJsonMember *m = avp_json_member(doc, i);
        shsm_output_data(out, avp_cz(avp_arena(), m->key), m->val);
    }
}

int draft_cmd_grade(DraftWorld *W, ShsmOutput *out, const char *target, const char *file, const char *as_unit,
                    const char *profile) {
    DraftGrade *g = draft_grade(W, target, file, as_unit, profile, NULL, NULL);
    for (size_t i = 0; i < g->lines.n; i++) shsm_output_line(out, g->lines.v[i]);
    output_doc(out, g->doc);
    return draft_grade_exit(g);
}

/* ---- rejects ------------------------------------------------------------------------------------------------------ */

DraftRejects draft_parse_rejects(AvpArena *a, AvpStr text) {
    DraftRejects rows = {0};
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    for (size_t i = 1; i < lines.n; i++) {
        if (!avp_str_strip(lines.v[i]).n) continue;
        AvpStrVec parts = avp_str_split(lines.v[i], AVP_S("\t"), -1);
        const char *f[4];
        for (int k = 0; k < 4; k++) f[k] = avp_cz(a, (size_t)k < parts.n ? parts.v[k] : AVP_S(""));
        avp_vec_push(&rows, ((DraftReject){f[0], f[1], f[2], f[3], false}));
        avp_vec_free(&parts);
    }
    avp_vec_free(&lines);
    return rows;
}

DraftRejects draft_load_rejects(AvpArena *a, const ShsmRepo *r) {
    AvpStr text;
    if (!avp_read_text(a, draft_rejects_path(a, r), AVP_TM_UTF8, &text)) {
        if (avp_io_is(ENOENT)) return (DraftRejects){0};
        shsm_raise_io();
    }
    return draft_parse_rejects(a, text);
}

static AvpStr clean_field(AvpArena *a, const char *s) {
    AvpStr t = avp_str_replace(a, avp_str(s), AVP_S("\t"), AVP_S(" "), -1);
    return avp_str_replace(a, t, AVP_S("\n"), AVP_S(" "), -1);
}

AvpStr draft_format_rejects(AvpArena *a, const DraftRejects *rows) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, REJECTS_HEADER);
    for (size_t i = 0; i < rows->n; i++) {
        const DraftReject *r = &rows->v[i];
        const char *f[4] = {r->symbol, r->approach, r->reason, r->basis};
        avp_buf_putc(&b, '\n');
        for (int k = 0; k < 4; k++) {
            if (k) avp_buf_putc(&b, '\t');
            avp_buf_put(&b, clean_field(a, f[k]));
        }
    }
    avp_buf_putc(&b, '\n');
    return avp_buf_str(&b);
}

void draft_save_rejects(AvpArena *a, const ShsmRepo *r, const DraftRejects *rows) {
    write_text(a, draft_rejects_path(a, r), draft_format_rejects(a, rows));
}

static int by_name(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return strcmp((*(TwinsFn *const *)x)->name, (*(TwinsFn *const *)y)->name);
}

const char *draft_basis_of(AvpArena *a, const ShsmRepo *r, TwinsFn *const *fns, size_t n) {
    TwinsFn **v = AVP_NEWN(a, TwinsFn *, n ? n : 1);
    for (size_t i = 0; i < n; i++) v[i] = fns[i];
    avp_sort(v, n, sizeof *v, by_name, NULL, false);
    AvpHasher h;
    avp_hasher_init(&h, AVP_HASH_SHA1);
    for (size_t i = 0; i < n; i++) {
        avp_hasher_update(&h, v[i]->name, strlen(v[i]->name));
        if (v[i]->source && v[i]->source[0]) {
            AvpBytes data;
            if (avp_read_all(a, avp_path_join(a, r->root, v[i]->source), &data)) avp_hasher_update(&h, data.v, data.n);
        }
    }
    u8 dig[20];
    avp_hasher_final(&h, dig);
    return avp_hex(a, dig, 6);
}

/* [t for t in W.tw.twins(f, "exact") if t.matched and t.source] */
static TwinsFnVec matched_exact(DraftWorld *W, const TwinsFn *f) {
    TwinsFnVec all = twins_of(W->a, draft_world_tw(W), f, TWINS_EXACT), ex = {0};
    for (size_t i = 0; i < all.n; i++)
        if (all.v[i]->matched && all.v[i]->source && all.v[i]->source[0]) avp_vec_push(&ex, all.v[i]);
    return ex;
}

int draft_cmd_reject(DraftWorld *W, ShsmOutput *out, const char *symbol, const char *reason, const char *approach) {
    AvpArena *a = W->a;
    DraftRejects all = draft_load_rejects(a, W->repo), rows = {0};
    for (size_t i = 0; i < all.n; i++)
        if (!(!strcmp(all.v[i].symbol, symbol) && !strcmp(all.v[i].approach, approach))) avp_vec_push(&rows, all.v[i]);
    const char *basis = "", *sym = symbol;
    AvpTry t;
    if (AVP_TRY(&t)) {
        TwinsFn *f = twins_resolve(draft_world_tw(W), symbol);
        TwinsFnVec ex = matched_exact(W, f);
        basis = ex.n ? draft_basis_of(a, W->repo, ex.v, ex.n) : "";
        sym = f->name;
        avp_try_end(&t);
    } else {
        if (t.code != SHSM_EXC_USAGE) avp_throw(t.code, "%s", t.msg);
        sym = symbol;
    }
    avp_vec_push(&rows, ((DraftReject){sym, approach, reason, basis, false}));
    draft_save_rejects(a, W->repo, &rows);
    shsm_output_line(out, avp_pyfmt(a, "rejected %s (%s): %s", sym, approach, reason));
    shsm_output_data(out, "symbol", avp_json_strz(a, sym));
    shsm_output_data(out, "approach", avp_json_strz(a, approach));
    shsm_output_data(out, "reason", avp_json_strz(a, reason));
    shsm_output_data(out, "basis", avp_json_strz(a, basis));
    return 0;
}

static AvpJson *reject_json(AvpArena *a, const DraftReject *r) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("symbol"), avp_json_strz(a, r->symbol));
    avp_json_set(a, o, AVP_S("approach"), avp_json_strz(a, r->approach));
    avp_json_set(a, o, AVP_S("reason"), avp_json_strz(a, r->reason));
    avp_json_set(a, o, AVP_S("basis"), avp_json_strz(a, r->basis));
    if (r->again) avp_json_set(a, o, AVP_S("again"), avp_json_bool(true));
    return o;
}

int draft_cmd_rejects(DraftWorld *W, ShsmOutput *out) {
    AvpArena *a = W->a;
    DraftRejects rows = draft_load_rejects(a, W->repo);
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < rows.n; i++) {
        DraftReject *r = &rows.v[i];
        const char *again = "";
        if (r->basis[0]) {
            AvpTry t;
            if (AVP_TRY(&t)) {
                TwinsFn *f = twins_resolve(draft_world_tw(W), r->symbol);
                TwinsFnVec ex = matched_exact(W, f);
                if (strcmp(draft_basis_of(a, W->repo, ex.v, ex.n), r->basis)) {
                    again = "  (twins changed: worth another look)";
                    r->again = true;
                }
                avp_try_end(&t);
            }
        }
        shsm_output_line(out, avp_pyfmt(a, "%-40s %-8s %s%s", r->symbol, r->approach, r->reason, again));
        avp_json_arr_push(a, arr, reject_json(a, r));
    }
    shsm_output_data(out, "rejects", arr);
    return 0;
}

/* ---- packet: brief's page (the C brief, in process) ----------------------------------------------------------- */

int draft_cmd_packet(DraftWorld *W, ShsmOutput *out, const char *target, bool unit) {
    AvpArena *a = W->a;
    AvpStrVec got = {0};
    int brief_exit = brief_page(W->repo->root, target, unit, 40, &got);
    /* r.stdout.rstrip("\n").split("\n"): the printed lines joined as the child's stdout was */
    AvpStr so = avp_str_join(a, AVP_S("\n"), got.v, got.n);
    while (so.n && so.p[so.n - 1] == '\n') so.n--;
    AvpStrVec all = avp_str_split(so, AVP_S("\n"), -1);
    AvpJson *page = avp_json_arr(a), *contract = avp_json_arr(a);
    for (size_t i = 0; i < all.n && i < 37; i++) {
        shsm_output_line(out, all.v[i]);
        avp_json_arr_push(a, page, avp_json_str(a, all.v[i]));
    }
    static const char *const CONTRACT[] = {
        "## grading contract",
        "grade compiles the draft with its unit's (or library's) flags, compares each `// 0x...`-marked function "
        "with retail (rows and ordered relocation targets) and needs: GRADE MATCH, no [match] rule finding, no "
        "decompiler artefact.",
        "The listing above is a reading aid: never paste it (Ghidra names such as param_1, uVar3, DAT_8..., "
        "undefined4 fail the screen). Command: build/tools/draft.exe grade <target> --file DRAFT",
    };
    for (size_t i = 0; i < 3; i++) {
        shsm_output_linez(out, CONTRACT[i]);
        avp_json_arr_push(a, contract, avp_json_strz(a, CONTRACT[i]));
    }
    /* the twin a `draft.exe twin` would start from, and the level it is a twin at */
    {
        TwinsFnVec twins;
        AvpTry t;
        if (AVP_TRY(&t)) {
            TwinsFn *f = draft_choose_twin(W, target, &twins);
            avp_try_end(&t);
            const char *level = draft_twin_level(f, twins.v[0]);
            shsm_output_line(out, avp_pyfmt(a, "first twin: %s (%s level, %s)", twins.v[0]->name, level,
                                            twins.v[0]->source));
            shsm_output_data(out, "twin", avp_json_strz(a, twins.v[0]->name));
            shsm_output_data(out, "twin_level", avp_json_strz(a, level));
        }
    }
    shsm_output_data(out, "page", page);
    shsm_output_data(out, "contract", contract);
    shsm_output_data(out, "brief_exit", avp_json_int(a, brief_exit));
    avp_vec_free(&got);
    return brief_exit == 0 ? 0 : SHSM_EXIT_ERROR;
}
