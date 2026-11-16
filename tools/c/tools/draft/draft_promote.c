/* draft.py's Promoter and promote (and cmd_promote): put a draft that grades MATCH into the tree as a parked
 * NonMatching unit through unit new, names rename, ninja, match, gatecheck layout, the rule checker and neutral,
 * undoing every change on a failed step. The steps run the checkout's C tools (build/tools/<tool>.exe) where the
 * Python ran the Python tools (DEVIATIONS.md, D-draft-9); the printed step names stay the Python's. */
#include "draft.h"

#include <stdlib.h>
#include <string.h>

static AvpStr last_line(AvpStr text) {
    /* (text.strip().split("\n") or [""])[-1] */
    AvpStr s = avp_str_strip(text);
    i64 nl = avp_str_rfind(s, AVP_S("\n"), 0, AVP_END);
    return nl >= 0 ? avp_str_slice(s, nl + 1, AVP_END) : s;
}

/* ---- the real Promoter ------------------------------------------------------------------------------------------- */

static ShsmRun git(DraftPromoter *p, const char *const *args, size_t n) {
    AvpArena *a = avp_arena();
    const char **av = AVP_NEWN(a, const char *, n + 1);
    av[0] = "git";
    for (size_t i = 0; i < n; i++) av[i + 1] = args[i];
    return shsm_run(a, av, n + 1, p->repo->root, 0);
}

AvpStrVec draft_promoter_dirty(DraftPromoter *p) {
    AvpArena *a = avp_arena();
    static const char *const ARGS[] = {"status", "--porcelain", "-uall"};
    ShsmRun r = git(p, ARGS, 3);
    AvpStrVec lines = avp_str_split(r.out, AVP_S("\n"), -1), out = {0};
    for (size_t i = 0; i < lines.n; i++)
        if (avp_str_strip(lines.v[i]).n) avp_vec_push(&out, avp_str_dup(a, avp_u8_slice(lines.v[i], 3, AVP_END)));
    avp_vec_free(&lines);
    return out;
}

ShsmRun draft_promoter_tool(DraftPromoter *p, const char *const *argv, size_t n) {
    AvpArena *a = avp_arena();
    const ShsmRepo *r = p->repo;
    const char **av = AVP_NEWN(a, const char *, n + 1);
    for (size_t i = 0; i < n; i++) av[i] = argv[i];
    /* ninja from the checkout root with an absolute path, as the Python's ninja_argv; the other tools are the
     * checkout's C executables */
    av[0] = !strcmp(argv[0], "ninja") ? shsm_repo_tool(a, r, "ninja")
                                      : avp_join(a, r->root, "build", "tools", avp_sprintf(a, "%s.exe", argv[0]));
    return shsm_run(a, av, n, r->root, 3600);
}

bool draft_promoter_local_only(DraftPromoter *p, const char *name) {
    AvpArena *a = avp_arena();
    AvpStrVec lines = {0};
    if (!avp_read_lines(a, p->repo->symbols_txt, AVP_TM_UTF8_REPLACE, &lines)) shsm_raise_io();
    AvpStr prefix = avp_str(avp_sprintf(a, "%s = ", name));
    size_t same = 0;
    bool all_local = true;
    for (size_t i = 0; i < lines.n; i++) {
        if (!avp_str_startswith(lines.v[i], prefix)) continue;
        same++;
        if (avp_str_find(lines.v[i], AVP_S("scope:local"), 0, AVP_END) < 0) all_local = false;
    }
    avp_vec_free(&lines);
    return same > 0 && all_local;
}

void draft_promoter_rename(DraftPromoter *p, const char *old, const char *new_) {
    AvpArena *a = avp_arena();
    const char *argv[9] = {"names", "rename", old, new_, "--apply", "--root", p->repo->root};
    size_t n = 7;
    if (draft_promoter_local_only(p, new_)) argv[n++] = "--local";
    /* a generated name has no source spelling to edit */
    if (strncmp(new_, "__sinit_", 8) == 0) argv[n++] = "--symbols-only";
    ShsmRun r = p->tool(p, argv, n);
    if (shsm_run_ok(&r)) return;
    AvpStrVec all = avp_str_split(avp_str_strip(shsm_run_output(a, &r)), AVP_S("\n"), -1);
    /* the first CONFLICT: line, else the last non-blank line, else "failed" */
    AvpStr why = {0}, last = {0};
    for (size_t i = 0; i < all.n; i++) {
        if (!avp_str_strip(all.v[i]).n) continue;
        if (!why.p && avp_str_startswith(all.v[i], AVP_S("CONFLICT:"))) why = all.v[i];
        last = all.v[i];
    }
    if (!why.p) why = last.p ? last : AVP_S("failed");
    avp_throw(DRAFT_EXC_RUNTIME, "%s", avp_cz(a, avp_pyfmt(a, "names.exe rename %s %s: %S", old, new_, &why)));
}

AvpStrVec draft_promoter_undo(DraftPromoter *p, const AvpStrVec *before) {
    AvpArena *a = avp_arena();
    AvpSet *had = avp_set_new();
    for (size_t i = 0; i < before->n; i++) avp_set_add(had, before->v[i]);
    AvpStrVec now = p->dirty(p), changed = {0};
    for (size_t i = 0; i < now.n; i++)
        if (!avp_set_has(had, now.v[i])) avp_vec_push(&changed, now.v[i]);
    avp_set_free(had);
    static const char *const LS[] = {"ls-files"};
    ShsmRun ls = git(p, LS, 1);
    AvpSet *tracked = avp_set_new();
    AvpStrVec tl = avp_str_split(ls.out, AVP_S("\n"), -1);
    for (size_t i = 0; i < tl.n; i++) avp_set_add(tracked, tl.v[i]);
    avp_vec_free(&tl);
    for (size_t i = 0; i < changed.n; i++) {
        AvpStr q = avp_str_strip_chars(avp_str_strip(changed.v[i]), AVP_S("\""));
        const char *qz = avp_cz(a, q);
        if (avp_set_has(tracked, q)) {
            const char *args[3] = {"checkout", "--", qz};
            git(p, args, 3);
        } else {
            const char *full = avp_path_join(a, p->repo->root, qz);
            if (avp_isfile(full) && !avp_remove(full)) shsm_raise_io();
        }
    }
    avp_set_free(tracked);
    return changed;
}

static DraftUnitOf real_unit_of(DraftPromoter *p, i64 address) { return draft_world_unit_of(p->W, address); }

static const char *real_unit_source(DraftPromoter *p, const char *unit_name) {
    const ShsmUnit *u = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        u = shsm_get_unit(p->repo, unit_name);
        avp_try_end(&t);
    } else {
        if (!shsm_exc_is_key(t.code)) avp_throw(t.code, "%s", t.msg);
        u = NULL;
    }
    return u && u->source && u->source[0] ? u->source : NULL;
}

void draft_promoter_init(DraftPromoter *p, DraftWorld *W) {
    memset(p, 0, sizeof *p);
    p->W = W;
    p->repo = W->repo;
    p->dirty = draft_promoter_dirty;
    p->tool = draft_promoter_tool;
    p->rename = draft_promoter_rename;
    p->undo = draft_promoter_undo;
    p->unit_of = real_unit_of;
    p->unit_source = real_unit_source;
}

/* ---- promote ----------------------------------------------------------------------------------------------------- */

static void say(AvpStrVec *lines, AvpStr s) { avp_vec_push(lines, s); }

static AvpStr join_strs(AvpArena *a, const AvpStr *v, size_t n, const char *sep) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < n; i++) {
        if (i) avp_buf_putz(&b, sep);
        avp_buf_put(&b, v[i]);
    }
    return avp_buf_str(&b);
}

static int cmp_str(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_str_cmp(*(const AvpStr *)x, *(const AvpStr *)y);
}

/* a tool step: "name: ok|FAILED", RuntimeError(last output line) on a failure */
static void check(DraftPromoter *pr, AvpStrVec *lines, const char *name, const char *const *argv, size_t n) {
    AvpArena *a = avp_arena();
    ShsmRun r = pr->tool(pr, argv, n);
    bool ok = shsm_run_ok(&r);
    say(lines, avp_pyfmt(a, "%s: %s", name, ok ? "ok" : "FAILED"));
    if (!ok) avp_throw(DRAFT_EXC_RUNTIME, "%s", avp_cz(a, last_line(shsm_run_output(a, &r))));
}

/* The confidence word of one side of a range: "high", "medium", "low", "block edge" (the edge of the dtk asm block:
 * no cut of the plan lies there) or "none" when the range does not start or end at a unit of the plan. ok: the side is
 * a high cut. */
static const char *side_word(bool is_plan, bool aligned, bool edge, bool has_conf, double conf, bool *ok) {
    *ok = false;
    if (!is_plan || !aligned) return "none";
    if (edge) return "block edge";
    if (!has_conf) return "none";
    const char *w = up_conf(true, conf);
    *ok = !strcmp(w, "high");
    return w;
}

/* The guard against a one-function unit: a range derived by the tool (not given with --text) that is not an
 * already-configured split and holds a single function (a static initialiser on its own, usually) is a real unit only
 * when unit_plan cuts it out with high confidence on both sides. Returns the refusal text, or NULL. */
static const char *small_unit_refusal(DraftPromoter *pr, const DraftGrade *g, i64 range_start, i64 range_end,
                                      const char *rtxt) {
    AvpArena *a = avp_arena();
    if (g->functions.n != 1) return NULL;
    /* one function: the unit that holds it is the whole range, or the range is no cut of the plan */
    DraftUnitOf u0 = pr->unit_of(pr, range_start), u1 = u0;
    if (!strcmp(u0.kind, "unit")) return NULL;
    bool is_plan = !strcmp(u0.kind, "plan") && !strcmp(u1.kind, "plan");
    bool ok0, ok1;
    const char *w0 = side_word(is_plan, u0.has_range && u0.start == range_start, u0.start_edge, u0.has_start_conf,
                               u0.start_conf, &ok0);
    const char *w1 = side_word(is_plan, u1.has_range && u1.end == range_end, u1.end_edge, u1.has_conf, u1.conf, &ok1);
    if (ok0 && ok1) return NULL;
    const DraftGradeFn *e = &g->functions.v[0];
    const char *name = e->retail && e->retail[0] ? e->retail : e->compiled ? e->compiled : "?";
    bool sinit = strstr(name, "sinit") != NULL || (e->compiled && strstr(e->compiled, "sinit") != NULL);
    return avp_sprintf(a,
                       "REFUSED: %s holds only %s (%s) and is not a configured split; unit_plan confidence at its start: "
                       "%s, at its end: %s (high needed on both). A one-function range is usually a cut in the wrong place, "
                       "and unit new infers the unit's data ownership from the range. Pass --allow-small-unit when other "
                       "evidence (a class's file name, the neighbouring splits) says it is a unit of its own.",
                       rtxt, sinit ? "a static initialiser" : "one function", name, w0, w1);
}

bool draft_promote(DraftWorld *W, const char *path_in, const char *source, const char *header, bool has_range,
                   i64 range_start, i64 range_end, bool dry_run, bool allow_small, DraftPromoter *pr,
                   DraftPromoteGradeFn grade_fn, void *grade_ctx, AvpStrVec *lines) {
    AvpArena *a = W->a;
    const bool range_given = has_range; /* --text: the caller's own evidence for the boundaries */
    const ShsmRepo *repo = W->repo;
    DraftPromoter real;
    if (!pr) {
        draft_promoter_init(&real, W);
        pr = &real;
    }
    AvpStr ps = avp_str_replace(a, avp_str(path_in), AVP_S("\\"), AVP_S("/"), -1);
    if (avp_str_startswith(ps, AVP_S("src/"))) ps = avp_str_slice(ps, 4, AVP_END);
    const char *path = avp_cz(a, ps);
    {
        AvpStr root, ext;
        avp_path_splitext(path, &root, &ext);
        if (!ext.n) {
            AvpStr sroot, sext;
            avp_path_splitext(source, &sroot, &sext);
            path = avp_sprintf(a, "%s%.*s", path, AVP_SF(sext));
        }
    }
    const char *unit_name = avp_sprintf(a, "main/%s", draft_strip_ext(a, path));
    /* 1. clean tree */
    AvpStrVec dirt = pr->dirty(pr);
    if (dirt.n && !dry_run) {
        say(lines, avp_pyfmt(a, "REFUSED: the tree is not clean (%d path(s), first %S)", (i64)dirt.n, &dirt.v[0]));
        return false;
    }
    if (dirt.n)
        say(lines, avp_pyfmt(a, "note: the tree is not clean (%d path(s)); a real promote would refuse", (i64)dirt.n));
    /* 2. unit is new (or a stub) */
    const char *usrc = pr->unit_source(pr, unit_name);
    if (usrc) {
        const char *sp = avp_path_join(a, repo->root, usrc);
        if (avp_exists(sp)) {
            AvpStr st;
            if (!avp_read_text(a, sp, AVP_TM_UTF8_REPLACE, &st)) shsm_raise_io();
            if (draft_scan_source(a, st).n) {
                say(lines, avp_pyfmt(a, "REFUSED: %s exists and is not a stub", unit_name));
                return false;
            }
        }
    }
    /* 3. range */
    AvpStr text;
    if (!avp_read_text(a, source, AVP_TM_UTF8_REPLACE, &text)) shsm_raise_io();
    DraftMarks marks = draft_marked_functions(a, text);
    const char *mp = avp_path_join(a, avp_cz(a, avp_path_dirname(source)), "meta.json");
    if (!has_range && !marks.n && avp_exists(mp)) {
        AvpStr mt;
        if (!avp_read_text(a, mp, AVP_TM_UTF8, &mt)) shsm_raise_io();
        AvpJsonError je;
        AvpJson *meta = avp_json_parse(a, mt, &je);
        if (!meta) shsm_raise(SHSM_EXC_VALUE, "%.*s", AVP_SF(avp_json_error_str(a, &je)));
        AvpJson *ur = avp_json_getz(meta, "unit_range"); /* a twin draft's proposed unit */
        if (avp_json_truthy(ur)) {
            /* (int(ur[0], 16), int(ur[1], 16)) */
            i64 v[2];
            for (int k = 0; k < 2; k++) {
                const char *s = avp_cz(a, avp_json_str_or(avp_json_at(ur, (size_t)k), AVP_S("")));
                char *end = NULL;
                v[k] = strtoll(s, &end, 16);
                if (!*s || !end || *end)
                    shsm_raise(SHSM_EXC_VALUE, "invalid literal for int() with base 16: %.*s",
                               AVP_SF(avp_repr_str(a, avp_str(s))));
            }
            range_start = v[0];
            range_end = v[1];
            has_range = true;
        }
    }
    if (!has_range) {
        if (!marks.n) {
            say(lines, AVP_S("REFUSED: no // 0x address comment in the draft and no --text"));
            return false;
        }
        i64 a0 = marks.v[0].address;
        for (size_t i = 1; i < marks.n; i++)
            if (marks.v[i].address < a0) a0 = marks.v[i].address;
        DraftUnitOf u = pr->unit_of(pr, a0);
        if (!u.has_range || !strcmp(u.kind, "asm")) {
            say(lines, AVP_S("REFUSED: .text range unknown: give --text START-END"));
            return false;
        }
        range_start = u.start;
        range_end = u.end;
    }
    const char *rtxt = avp_cz(a, avp_pyfmt(a, "%08X-%08X", range_start, range_end));
    /* 4. grade every function of the range */
    DraftGrade *g = grade_fn ? grade_fn(grade_ctx, rtxt, source) : draft_grade(W, rtxt, source, NULL, NULL, NULL, NULL);
    for (size_t i = 0; i < g->lines.n; i++) say(lines, avp_pyfmt(a, "  %S", &g->lines.v[i]));
    if (strcmp(g->verdict, "MATCH") != 0 || g->blocking) {
        say(lines, avp_pyfmt(a, "REFUSED: the draft does not grade MATCH without findings over %s", rtxt));
        return false;
    }
    if (!range_given && !allow_small) {
        const char *why = small_unit_refusal(pr, g, range_start, range_end, rtxt);
        if (why) {
            say(lines, avp_str(why));
            return false;
        }
    }
    AvpMap *renames = avp_map_new(); /* retail -> new name (const char *), insertion order */
    for (size_t i = 0; g->renames && i < avp_json_len(g->renames); i++) {
        const AvpJsonMember *m = avp_json_member(g->renames, i);
        avp_map_put(renames, m->key, NULL)->p = avp_cz(a, avp_json_str_or(m->val, AVP_S("")));
    }
    for (size_t i = 0; i < g->functions.n; i++) {
        const DraftGradeFn *e = &g->functions.v[i];
        if (e->compiled && e->compiled[0] && e->retail && e->retail[0] && strcmp(e->compiled, e->retail) != 0)
            avp_map_put(renames, avp_str(e->retail), NULL)->p = (void *)e->compiled;
    }
    AvpStrVec rn_old = {0};
    AVP_VEC(const char *) rn_new = {0};
    {
        AvpMapIter it = {0};
        while (avp_map_next(renames, &it)) {
            avp_vec_push(&rn_old, avp_str_dup(a, it.key));
            avp_vec_push(&rn_new, it.val->p);
        }
    }
    avp_map_free(renames);
    const char *hdr_dest = NULL;
    if (header && header[0]) hdr_dest = avp_path_join(a, repo->include_dir, avp_sprintf(a, "%s.h", draft_strip_ext(a, path)));
    /* neutral only matters for a header other units may include: one that exists before the promote */
    bool shared_hdr = hdr_dest && avp_exists(hdr_dest);
    const char *src_dest = avp_path_join(a, repo->src_dir, path);
    AvpStrVec plan = {0};
    avp_vec_push(&plan, avp_pyfmt(a, "unit.exe new %s --text %s", path, rtxt));
    for (size_t i = 0; i < rn_old.n; i++)
        avp_vec_push(&plan, avp_pyfmt(a, "rename %S -> %s (symbols.txt)", &rn_old.v[i], rn_new.v[i]));
    avp_vec_push(&plan, avp_pyfmt(a, "write %s", draft_rel_path(a, repo, src_dest)));
    if (hdr_dest) avp_vec_push(&plan, avp_pyfmt(a, "write %s", draft_rel_path(a, repo, hdr_dest)));
    avp_vec_push(&plan, AVP_S("ninja build.ninja (dtk split, configure.exe)"));
    avp_vec_push(&plan, avp_pyfmt(a, "match.exe %s", unit_name));
    avp_vec_push(&plan, avp_pyfmt(a, "gatecheck.exe layout %s", unit_name));
    avp_vec_push(&plan, avp_pyfmt(a, "check_rules.exe --units %s", unit_name));
    if (shared_hdr) avp_vec_push(&plan, avp_pyfmt(a, "neutral.exe %s", draft_rel_path(a, repo, hdr_dest)));
    if (dry_run) {
        const char *argv[6] = {"unit", "new", path, "--text", rtxt, "--dry-run"};
        ShsmRun r = pr->tool(pr, argv, 6);
        bool ok = shsm_run_ok(&r);
        AvpStr ll = last_line(shsm_run_output(a, &r));
        say(lines, ok ? AVP_S("unit.exe new --dry-run: ok") : avp_pyfmt(a, "unit.exe new --dry-run: FAILED: %S", &ll));
        for (size_t i = 0; i < rn_old.n; i++) say(lines, avp_pyfmt(a, "would rename %S -> %s", &rn_old.v[i], rn_new.v[i]));
        AvpStr pl = join_strs(a, plan.v, plan.n, "; ");
        say(lines, avp_pyfmt(a, "dry run: would %S", &pl));
        return ok;
    }
    AvpStrVec before = pr->dirty(pr);
    const char *volatile step = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        step = "unit.exe new";
        const char *newcmd[5] = {"unit", "new", path, "--text", rtxt};
        ShsmRun r = pr->tool(pr, newcmd, 5);
        if (!shsm_run_ok(&r)) {
            AvpStr o = avp_str_strip(shsm_run_output(a, &r));
            avp_throw(DRAFT_EXC_RUNTIME, "%s", o.n ? avp_cz(a, last_line(o)) : "failed");
        }
        step = "rename";
        for (size_t i = 0; i < rn_old.n; i++) pr->rename(pr, avp_cz(a, rn_old.v[i]), rn_new.v[i]);
        step = "write";
        if (!avp_makedirs(avp_cz(a, avp_path_dirname(src_dest)), true)) shsm_raise_io();
        if (!avp_copyfile(source, src_dest)) shsm_raise_io();
        if (hdr_dest) {
            if (!avp_makedirs(avp_cz(a, avp_path_dirname(hdr_dest)), true)) shsm_raise_io();
            if (!avp_copyfile(header, hdr_dest)) shsm_raise_io();
        }
        if (!shared_hdr && hdr_dest) say(lines, AVP_S("neutral: not needed (a new header only this unit includes)"));
        const char *obj = avp_sprintf(a, "build/%s/src/%s.o", repo->version, draft_strip_ext(a, path));
        const char *c1[2] = {"ninja", "build.ninja"}, *c2[2] = {"match", unit_name}, *c3[2] = {"ninja", obj},
                   *c4[3] = {"gatecheck", "layout", unit_name}, *c5[3] = {"check_rules", "--units", unit_name};
        step = "ninja build.ninja";
        check(pr, lines, step, c1, 2);
        step = "match.exe";
        check(pr, lines, step, c2, 2);
        step = "ninja object";
        check(pr, lines, step, c3, 2);
        step = "gatecheck layout";
        check(pr, lines, step, c4, 3);
        step = "rules";
        check(pr, lines, step, c5, 3);
        if (shared_hdr) {
            const char *c6[2] = {"neutral", shsm_repo_rel(a, repo, hdr_dest)};
            step = "neutral";
            check(pr, lines, step, c6, 2);
        }
        avp_try_end(&t);
    } else {
        AvpStr why = shsm_exc_str(a, t.code, t.msg);
        AvpStrVec undone = pr->undo(pr, &before);
        const char *regen[2] = {"ninja", "build.ninja"};
        pr->tool(pr, regen, 2);
        say(lines, avp_pyfmt(a, "FAILED at %s: %S", (const char *)step, &why));
        AvpStrVec sorted = {0};
        for (size_t i = 0; i < undone.n; i++) avp_vec_push(&sorted, undone.v[i]);
        avp_sort(sorted.v, sorted.n, sizeof *sorted.v, cmp_str, NULL, false);
        AvpStr shown = join_strs(a, sorted.v, sorted.n < 6 ? sorted.n : 6, ", ");
        say(lines, avp_pyfmt(a, "undone: %d path(s) restored (%S)", (i64)undone.n, &shown));
        avp_vec_free(&sorted);
        return false;
    }
    say(lines, avp_pyfmt(a, "promoted %s as NonMatching (uncommitted). Next: commit, then build/tools/unit.exe flip %s",
                         unit_name, path));
    return true;
}

int draft_cmd_promote(DraftWorld *W, ShsmOutput *out, const char *path, const char *file, const char *header,
                      const char *text, bool dry_run, bool allow_small) {
    AvpArena *a = W->a;
    i64 rs = 0, re = 0;
    bool has = false;
    if (text && text[0]) {
        if (!draft_parse_range(avp_str(text), &rs, &re)) shsm_raise(SHSM_EXC_USAGE, "--text START-END");
        has = true;
    }
    const char *hdr = header && header[0] ? avp_path_abspath(a, header) : NULL;
    AvpStrVec lines = {0};
    bool ok = draft_promote(W, path, avp_path_abspath(a, file), hdr, has, rs, re, dry_run, allow_small, NULL, NULL, NULL, &lines);
    AvpJson *lj = avp_json_arr(a);
    for (size_t i = 0; i < lines.n; i++) {
        shsm_output_line(out, lines.v[i]);
        avp_json_arr_push(a, lj, avp_json_str(a, lines.v[i]));
    }
    shsm_output_data(out, "ok", avp_json_bool(ok));
    shsm_output_data(out, "lines", lj);
    return ok ? 0 : SHSM_EXIT_FINDING;
}
