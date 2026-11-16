/* ml_steps: merge_lane.py's step_preflight, step_rebase, step_gate, step_layout, step_ledger, step_merge. */
#include "merge_lane.h"
#include "shsm_cli.h"
#include <string.h>

#define F(...) ML_F(ml, __VA_ARGS__)
#define EV(...) ml_ev(ml, ev, F(__VA_ARGS__))

static const char *strip_cz(Ml *ml, AvpStr s) { return avp_cz(ml->a, avp_str_strip(s)); }
static const char *se_or_so(Ml *ml, const MlGit *g) { return strip_cz(ml, g->err.n ? g->err : g->out); }

static AvpStrVec nonblank_lines(AvpStr s) {
    AvpStrVec lines = avp_str_splitlines(s, false), res = {0};
    for (size_t i = 0; i < lines.n; i++)
        if (avp_str_strip(lines.v[i]).n) avp_vec_push(&res, lines.v[i]);
    return res;
}

static const char *join_first(Ml *ml, const AvpStrVec *v, size_t n, const char *sep) {
    size_t k = v->n < n ? v->n : n;
    return avp_cz(ml->a, avp_str_join(ml->a, avp_str(sep), v->v, k));
}

/* ---- preflight -------------------------------------------------------------------------------------------------- */
const char *ml_step_preflight(Ml *ml, AvpStrVec *ev) {
    AvpArena *a = ml->a;
    if (!avp_isdir(ml->slot)) ml_fail(ml, F("slot %s is not a directory", ml->slot), NULL);
    MlGit g = ML_GIT(ml, "rev-parse", "--show-toplevel");
    if (g.rc != 0) ml_fail(ml, F("%s is not a git worktree: %s", ml->slot, strip_cz(ml, g.err)), NULL);
    const char *top = strip_cz(ml, g.out);
    if (!ml_norm_eq(a, top, ml->slot)) ml_fail(ml, F("%s is inside a worktree rooted at %s, not its root", ml->slot, top), NULL);
    MlWorktrees wts = ml_worktrees(ml);
    bool listed = false;
    for (size_t i = 0; i < wts.n; i++) listed |= ml_norm_eq(a, wts.v[i].path, ml->slot);
    if (!listed) ml_fail(ml, F("%s is not in `git worktree list`", ml->slot), NULL);
    MlGit c1 = ML_GIT(ml, "rev-parse", "--git-common-dir");
    MlGit c2 = ML_GIT_IN(ml, ml->repo, "rev-parse", "--git-common-dir");
    if (c2.rc != 0) ml_fail(ml, F("--repo %s is not a git repository", ml->repo), NULL);
    const char *cd1 = ml_norm(a, avp_path_join(a, ml->slot, strip_cz(ml, c1.out)));
    const char *cd2 = ml_norm(a, avp_path_join(a, ml->repo, strip_cz(ml, c2.out)));
    if (strcmp(cd1, cd2)) ml_fail(ml, F("%s belongs to another repository (%s, expected %s)", ml->slot, cd1, cd2), NULL);
    EV("slot %s is a worktree of this repository", ml->slot);
    AvpStrVec ops = ml_op_in_progress(ml, NULL);
    if (ops.n) {
        const char *first = avp_cz(a, ops.v[0]);
        const char *msg = F("%s in progress in the slot; finish it (git %s --continue) or abort it first",
                            join_first(ml, &ops, ops.n, "/"), first);
        ml_fail(ml, msg, NULL);
    }
    AvpStrVec dirty = ml_status_lines(ml, NULL, true);
    if (dirty.n)
        ml_fail(ml, F("slot tree is not clean (%d entries): %s", dirty.n, join_first(ml, &dirty, 6, "; ")), NULL);
    EV("tree clean, no rebase/merge in progress");
    if (!strcmp(ml->branch, ml->onto)) ml_fail(ml, F("--branch and --onto are both %s", ml->branch), NULL);
    const char *tip = ml_rev(ml, avp_sprintf(a, "refs/heads/%s", ml->branch), NULL);
    if (!tip) ml_fail(ml, F("branch %s does not exist (refs/heads/%s)", ml->branch, ml->branch), NULL);
    const char *onto_tip = ml_rev(ml, avp_sprintf(a, "refs/heads/%s", ml->onto), NULL);
    if (!onto_tip) ml_fail(ml, F("onto branch %s does not exist (refs/heads/%s)", ml->onto, ml->onto), NULL);
    ml->onto_rev = onto_tip;
    EV("pinned %s at %s for this run", ml->onto, onto_tip);
    const char *ref = avp_sprintf(a, "refs/heads/%s", ml->branch);
    for (size_t i = 0; i < wts.n; i++)
        if (wts.v[i].branch && !strcmp(wts.v[i].branch, ref) && !ml_norm_eq(a, wts.v[i].path, ml->slot))
            ml_fail(ml, F("branch %s is checked out in another worktree (%s)", ml->branch, wts.v[i].path), NULL);
    EV("branch %s at %s, %s at %s", ml->branch, ml_short(a, tip), ml->onto, ml_short(a, onto_tip));
    MlGit m = ML_GIT(ml, "merge-base", onto_tip, tip);
    const char *mb = strip_cz(ml, m.out);
    if (m.rc != 0 || !*mb) ml_fail(ml, F("%s and %s have no merge base", ml->onto, ml->branch), NULL);
    MlGit d = ML_GIT(ml, "diff", "--name-only", mb, tip);
    if (d.rc != 0)
        ml_fail(ml, F("git diff --name-only %s %s failed (exit %d): %s", ml_short(a, mb), ml_short(a, tip), d.rc,
                      strip_cz(ml, d.err)), NULL);
    AvpStrVec files = nonblank_lines(d.out);
    ml->lane_files = files;
    MlGit cnt = ML_GIT(ml, "rev-list", "--count", avp_sprintf(a, "%s..%s", mb, tip));
    EV("%s commits on the lane since the merge base %s; %d files differ", strip_cz(ml, cnt.out), ml_short(a, mb), files.n);
    AvpStr bytes;
    AvpMap *cfg_new = ml_parse_objects(a, ml_show_file(ml, tip, "configure.py", &bytes) ? ml_decode(a, bytes) : AVP_S(""));
    AvpMap *cfg_old = ml_parse_objects(a, ml_show_file(ml, mb, "configure.py", &bytes) ? ml_decode(a, bytes) : AVP_S(""));
    AvpMap *units = avp_map_new(); /* unit -> const char * status, insertion order */
    for (size_t i = 0; i < files.n; i++) {
        if (!avp_str_startswith(files.v[i], AVP_S("src/"))) continue;
        AvpStr rel = avp_str_slice(files.v[i], 4, AVP_END);
        AvpVal *n = avp_map_get(cfg_new, rel);
        if (n) avp_map_put(units, avp_str(ml_unit_of(a, rel)), NULL)->p = (void *)F("%s (source edited)", (const char *)n->p);
        else if (avp_map_get(cfg_old, rel))
            avp_map_put(units, avp_str(ml_unit_of(a, rel)), NULL)->p = (void *)"removed from configure.py";
    }
    AvpSet *all = avp_set_new();
    AvpMapIter it = {0};
    while (avp_map_next(cfg_new, &it)) avp_set_add(all, it.key);
    it = (AvpMapIter){0};
    while (avp_map_next(cfg_old, &it)) avp_set_add(all, it.key);
    AvpStrVec sorted = avp_set_sorted(all);
    for (size_t i = 0; i < sorted.n; i++) {
        AvpVal *va = avp_map_get(cfg_old, sorted.v[i]), *vb = avp_map_get(cfg_new, sorted.v[i]);
        const char *sa = va ? va->p : NULL, *sb = vb ? vb->p : NULL;
        if (sa && sb && !strcmp(sa, sb)) continue;
        const char *what = !sa ? F("added as %s", sb) : (!sb ? F("removed (was %s)", sa) : F("%s -> %s", sa, sb));
        AvpStr u = avp_str(ml_unit_of(a, sorted.v[i]));
        AvpVal *old = avp_map_get(units, u);
        const char *suffix = "";
        if (old && strstr((const char *)old->p, " (")) {
            /* ", " + old.split(" (")[1][:-1] */
            AvpStrVec sp = avp_str_split(avp_str(old->p), AVP_S(" ("), -1);
            AvpStr second = sp.v[1];
            suffix = F(", %S", ml_sp(ml->a, avp_u8_slice(second, 0, -1)));
        }
        avp_map_put(units, u, NULL)->p = (void *)F("%s%s", what, suffix);
    }
    it = (AvpMapIter){0};
    while (avp_map_next(units, &it)) avp_vec_push(&ml->lane_units, ((MlUnit){avp_cz(a, it.key), it.val->p}));
    if (ml->lane_units.n) {
        EV("lane units (%d):", ml->lane_units.n);
        for (size_t i = 0; i < ml->lane_units.n; i++) EV("    %s  %s", ml->lane_units.v[i].unit, ml->lane_units.v[i].status);
    } else {
        EV("lane touches no configure.py unit");
    }
    return F("lane %s, %d unit(s)", ml->branch, ml->lane_units.n);
}

/* ---- rebase ----------------------------------------------------------------------------------------------------- */
static const char *advice(Ml *ml, const char *kind, AvpStr path) {
    const char *t;
    if (!strcmp(kind, "generated"))
        t = "generated file: take main's side (during a rebase `ours` is {onto}): git checkout --ours -- {path} && git add "
            "{path}; the update step regenerates it";
    else if (!strcmp(kind, "ledger"))
        t = "score ledger: outside the `<!-- ledger:begin/end -->` blocks merge per row by hand (each row from the side "
            "that achieved it); inside a block take either side, then regenerate with `build/tools/ledger.exe progress "
            "--report config/R5WEA4/baseline_report.json` (never hand-merge the generated blocks, never take a side "
            "wholesale)";
    else if (!strcmp(kind, "exceptions"))
        t = "exception record: merge by hand, keeping every row from both sides";
    else
        t = "source or configuration: resolve by hand; this tool never auto-resolves it";
    AvpStr s = avp_str_replace(ml->a, avp_str(t), AVP_S("{path}"), path, -1);
    s = avp_str_replace(ml->a, s, AVP_S("{onto}"), avp_str(ml->onto), -1);
    return avp_cz(ml->a, s);
}

/* stopped_ledger_commit: the (sha, subject) of a stopped rebase's commit that touches only the ledger files. */
static bool stopped_ledger_commit(Ml *ml, const char **sha, const char **subject) {
    *sha = ml_rev(ml, "REBASE_HEAD", NULL);
    if (!*sha) return false;
    MlGit g = ML_GIT(ml, "diff-tree", "--no-commit-id", "--name-only", "-r", *sha);
    AvpStrVec files = {0}, lines = avp_str_splitlines(g.out, false);
    for (size_t i = 0; i < lines.n; i++)
        if (avp_str_strip(lines.v[i]).n) avp_vec_push(&files, avp_str_strip_chars(avp_str_strip(lines.v[i]), AVP_S("\"")));
    if (g.rc != 0 || !files.n) return false;
    for (size_t i = 0; i < files.n; i++)
        if (!avp_str_eqz(files.v[i], ML_BASELINE) && !avp_str_eqz(files.v[i], ML_PROGRESS)) return false;
    MlGit l = ML_GIT(ml, "log", "-1", "--format=%s", *sha);
    *subject = strip_cz(ml, l.out);
    return true;
}

static bool has_label(const AvpStrVec *v, const char *s) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_eqz(v->v[i], s)) return true;
    return false;
}

static bool rebase_in_progress(Ml *ml) {
    AvpStrVec ops = ml_op_in_progress(ml, NULL);
    return has_label(&ops, "rebase");
}

/* The conflicted configure.py / splits.txt of the stopped rebase: their hunks into ev (apply false) or, with apply, their
 * union written and staged (--take-union-units). Returns how many of the unmerged paths it resolved and staged. */
static size_t union_pass(Ml *ml, AvpStrVec *ev, const MlUnmergedVec *um, bool apply) {
    AvpArena *a = ml->a;
    size_t done = 0;
    for (size_t i = 0; i < um->n; i++) {
        const char *path = avp_cz(a, um->v[i].path);
        if (strcmp(ml_classify_conflict(um->v[i].path), "manual") != 0) continue;
        AvpStr text;
        AvpBytes raw = {0};
        if (!avp_read_all(a, avp_path_join(a, ml->slot, path), &raw)) continue;
        text = (AvpStr){(const char *)raw.v, raw.n};
        AvpStrVec report = {0};
        AvpStr resolved = {0};
        bool ok = ml_conflict_report(ml, path, text, &report, &resolved);
        if (!report.n) continue; /* not a file this knows */
        if (!apply) {
            for (size_t k = 0; k < report.n; k++) EV("        %S", &report.v[k]);
            if (ok && !ml->take_union) EV("        suggestion: re-run with --take-union-units to apply the union of %s (nothing is applied without it)", path);
            continue;
        }
        if (!ok) continue;
        const char *file = avp_path_join(a, ml->slot, path);
        if (!avp_write_file(file, resolved.p, resolved.n)) shsm_raise_io();
        MlGit ad = ML_GIT(ml, "add", "--", path);
        if (ad.rc != 0) continue;
        EV("--take-union-units: resolved %s with the union of both sides' units and staged it", path);
        done++;
    }
    return done;
}

const char *ml_step_rebase(Ml *ml, AvpStrVec *ev) {
    AvpArena *a = ml->a;
    MlGit g = ML_GIT(ml, "rev-parse", "--abbrev-ref", "HEAD");
    if (strcmp(strip_cz(ml, g.out), ml->branch)) {
        g = ML_GIT(ml, "checkout", ml->branch);
        if (g.rc != 0) ml_fail(ml, F("git checkout %s failed: %s", ml->branch, se_or_so(ml, &g)), NULL);
        EV("checked out %s", ml->branch);
    }
    const char *before = ml_rev(ml, "HEAD", NULL);
    AvpEnv *env = avp_env_current(a);
    avp_env_set(env, "GIT_EDITOR", "true");
    avp_env_set(env, "GIT_SEQUENCE_EDITOR", "true");
    g = ml_git(ml, NULL, env, ML_ARGV_("rebase", ml_onto_ref(ml)));
    AvpStr text = avp_str_strip(avp_str_concat(a, g.out, g.err));
    const char *sk_sha[ML_SKIP_LIMIT], *sk_subj[ML_SKIP_LIMIT];
    size_t nskip = 0;
    while (g.rc != 0 && rebase_in_progress(ml) && ml_unmerged(ml).n &&
           nskip < ML_SKIP_LIMIT) {
        const char *sha, *subject;
        if (!stopped_ledger_commit(ml, &sha, &subject)) break;
        sk_sha[nskip] = sha, sk_subj[nskip] = subject, nskip++;
        g = ml_git(ml, NULL, env, ML_ARGV_("rebase", "--skip"));
        text = avp_str_strip(avp_str_concat(a, g.out, g.err));
    }
    for (size_t i = 0; i < nskip; i++)
        EV("skipped the lane's generated ledger commit %s \"%s\" (it conflicted with %s and touches only %s; --update "
           "regenerates them)",
           ml_short(a, sk_sha[i]), sk_subj[i], ml->onto, ML_BASELINE " and " ML_PROGRESS);
    /* --take-union-units: a stopped commit whose only conflicts are lists of whole units in configure.py / splits.txt gets
     * the union of both sides' units (the lane's version of a unit both name) and the rebase goes on with the next commit */
    for (int round = 0; ml->take_union && round < 25 && g.rc != 0 && rebase_in_progress(ml); round++) {
        MlUnmergedVec cur = ml_unmerged(ml);
        if (!cur.n || union_pass(ml, ev, &cur, true) != cur.n) break;
        g = ml_git(ml, NULL, env, ML_ARGV_("rebase", "--continue"));
        text = avp_str_strip(avp_str_concat(a, g.out, g.err));
    }
    MlUnmergedVec um = ml_unmerged(ml);
    if (g.rc != 0 || um.n || ml_op_in_progress(ml, NULL).n) {
        EV("git rebase %s stopped (exit %d)", ml->onto, g.rc);
        if (text.n) {
            AvpStrVec lines = avp_str_splitlines(text, false);
            for (size_t i = lines.n > 8 ? lines.n - 8 : 0; i < lines.n; i++) EV("    %S", &lines.v[i]);
        }
        if (um.n) {
            EV("unmerged paths (%d):", um.n);
            for (size_t i = 0; i < um.n; i++) {
                EV("    %S %S", &um.v[i].code, &um.v[i].path);
                ml_ev(ml, ev, F("        -> %s", advice(ml, ml_classify_conflict(um.v[i].path), um.v[i].path)));
            }
            union_pass(ml, ev, &um, false);
        }
        EV("then: git -C %s add <paths> && git -C %s rebase --continue, and run this tool again (or git rebase --abort)",
           ml->slot, ml->slot);
        EV("nothing was built; the slot is left mid-rebase for you");
        ml_fail(ml, ml_join_ev(ml, ev), F("rebase stopped: %d unmerged path(s)", um.n));
    }
    const char *after = ml_rev(ml, "HEAD", NULL);
    ml->tip = after;
    if (before && after && strcmp(before, after)) {
        MlGit td = ML_GIT(ml, "diff", "--quiet", before, after, "--", "tools/c");
        if (td.rc == 1) {
            ml->tools_changed = true;
            EV("the rebase changed tools/c: build/tools will be rebuilt before the gate");
        }
    }
    MlGit c = ML_GIT(ml, "rev-list", "--count", avp_sprintf(a, "%s..%s", ml_onto_ref(ml), after ? after : "None"));
    const char *n = strip_cz(ml, c.out);
    EV("rebased onto %s: %s -> %s, %s commit(s) ahead of %s", ml->onto, ml_short(a, before), ml_short(a, after), n,
       ml->onto);
    return F("now %s commit(s) ahead of %s", n, ml->onto);
}

void ml_require_no_unmerged(Ml *ml) {
    MlUnmergedVec um = ml_unmerged(ml);
    if (um.n) {
        AvpStrVec paths = {0};
        for (size_t i = 0; i < um.n; i++) avp_vec_push(&paths, um.v[i].path);
        ml_fail(ml, F("unmerged paths in the slot, not building: %s", join_first(ml, &paths, paths.n, ", ")), NULL);
    }
    AvpStrVec ops = ml_op_in_progress(ml, NULL);
    if (ops.n) ml_fail(ml, F("%s in progress in the slot, not building", join_first(ml, &ops, ops.n, "/")), NULL);
}

/* ---- the slot's own tools ---------------------------------------------------------------------------------------- */

/* build/tools in a slot is built from the slot's tools/c. A rebase brings main's tool changes in (a rule-checker
 * allowance, a ledger fix), and the rules and ledger steps then judge the rebased tree with the lane's old checker. The
 * stamp holds the tools/c tree the executables were built from; the tools are stale when it names another tree, or when
 * the rebase of this run changed tools/c and no stamp can tell. */
#define ML_TOOLS_STAMP "build/tools/.tools-c-tree"

bool ml_tools_stale(Ml *ml, const char **why) {
    AvpArena *a = ml->a;
    MlGit t = ML_GIT(ml, "rev-parse", "HEAD:tools/c");
    const char *tree = t.rc == 0 ? strip_cz(ml, t.out) : NULL;
    AvpStr stamp = {0};
    bool have = ml_read_text(a, avp_path_join(a, ml->slot, ML_TOOLS_STAMP), &stamp);
    const char *st = have ? avp_cz(a, avp_str_strip(stamp)) : NULL;
    if (tree && st && *st && strcmp(st, tree) != 0) {
        *why = F("build/tools was built from tools/c %s, the tree has %s", ml_short(a, st), ml_short(a, tree));
        return true;
    }
    if (ml->tools_changed && !(tree && st && !strcmp(st, tree))) {
        *why = "the rebase changed tools/c, and no stamp shows build/tools was rebuilt from it";
        return true;
    }
    return false;
}

void ml_tools_guard(Ml *ml, AvpStrVec *ev) {
    AvpArena *a = ml->a;
    const char *why;
    if (!ml_tools_stale(ml, &why)) return;
    EV("build/tools is stale: %s; rebuilding with tools/build_c.cmd", why);
    /* the running executable cannot be overwritten, but it can be renamed away: the new one is for the next run */
    const char *exe = avp_path_join(a, ml->slot, "build/tools/merge_lane.exe");
    const char *old = avp_sprintf(a, "%s.old", exe);
    if (avp_isfile(exe)) {
        if (avp_isfile(old)) avp_remove(old);
        avp_replace(exe, old);
    }
    AvpStr text;
    i64 rc = ml_run_cmd(ml, "build_tools", "tools-build.log", NULL, NULL, &text);
    EV("tools/build_c.cmd: exit %d (log %s)", rc, ml_log_path(ml, "tools-build.log"));
    if (rc != 0) {
        ml_ev_tail(ml, ev, text, 15);
        ml_fail(ml, ml_join_ev(ml, ev), F("rebuilding build/tools exited %d", rc));
    }
    MlGit t = ML_GIT(ml, "rev-parse", "HEAD:tools/c");
    if (t.rc == 0) {
        AvpStr stamp = avp_str(strip_cz(ml, t.out));
        const char *sp = avp_path_join(a, ml->slot, ML_TOOLS_STAMP);
        avp_makedirs(avp_cz(a, avp_path_dirname(sp)), true);
        if (!avp_write_file(sp, stamp.p, stamp.n)) shsm_raise_io();
    }
    EV("build/tools rebuilt from the current tools/c; this process still runs the old merge_lane.exe, whose rule checker is "
       "linked in: run the same command again");
    ml_fail(ml, ml_join_ev(ml, ev), "rebuilt build/tools; run again");
}

/* ---- gate ------------------------------------------------------------------------------------------------------- */
static bool has_ci(AvpStr ln, const char *word) {
    AvpStr low = avp_str_lower(avp_scratch(), ln);
    AvpStr w = avp_str_lower(avp_scratch(), avp_str(word));
    return ML_FIND(low, w) >= 0;
}

const char *ml_step_gate(Ml *ml, AvpStrVec *ev) {
    AvpArena *a = ml->a;
    ml_require_no_unmerged(ml);
    ml_tools_guard(ml, ev);
    MlWorktrees wts = ml_worktrees(ml);
    const char *build = avp_path_join(a, ml->slot, "build");
    if (wts.n && ml_norm_eq(a, wts.v[0].path, ml->slot))
        ml_fail(ml, "the slot is the repository's main checkout; refusing to delete its build/", NULL);
    if (strcmp(avp_cz(a, avp_path_dirname(ml_norm(a, build))), ml_norm(a, ml->slot)))
        ml_fail(ml, F("unexpected build path %s", build), NULL);
    /* A clean rebuild empties build/ but keeps what is not an output of the game build: the C tools (build/tools, which hold
     * configure.exe and this program), their CMake tree (build/tools-cmake), the sibling index (build/index, built from
     * orig/_ref, read by no step of the gate) and build/binutils (the downloaded powerpc-eabi tools, a link in a slot).
     * classes.json is derived from the game build and rebuilt after a green gate, below. */
    static const char *const keep[] = {"tools", "tools-cmake", "index", "binutils"};
    AvpStrVec names = {0};
    if (avp_isdir(build) && avp_listdir(a, build, &names)) {
        for (size_t i = 0; i < names.n; i++) {
            const char *name = avp_cz(a, names.v[i]);
            bool kept = false;
            for (size_t k = 0; k < sizeof keep / sizeof keep[0] && !kept; k++) kept = _stricmp(name, keep[k]) == 0;
            if (kept) continue;
            const char *p = avp_path_join(a, build, name);
            if (avp_isdir(p) && !avp_isjunction(p) && !avp_islink(p)) avp_rmtree(p, AVP_RMTREE_CLEAR_READONLY);
            else avp_remove(p);
            if (avp_exists(p) || avp_isjunction(p) || avp_islink(p)) ml_fail(ml, F("could not delete %s", p), NULL);
        }
    }
    EV("emptied %s (kept tools, tools-cmake, index, binutils)", build);
    double t0 = avp_time();
    AvpStr text;
    i64 rc = ml_run_cmd(ml, "configure", "gate-configure.log", NULL, NULL, &text);
    if (rc != 0) {
        EV("configure.exe exit %d (log %s)", rc, ml_log_path(ml, "gate-configure.log"));
        ml_ev_tail(ml, ev, text, 15);
        ml_fail(ml, ml_join_ev(ml, ev), NULL);
    }
    rc = ml_run_cmd(ml, "ninja", "gate-ninja.log", NULL, NULL, &text);
    double dt = avp_time() - t0;
    AvpStrVec lines = avp_str_splitlines(text, false);
    const char *ok_line = NULL;
    for (size_t i = 0; i < lines.n && !ok_line; i++)
        if (ML_FIND(lines.v[i], AVP_S("main.dol: OK")) >= 0) ok_line = strip_cz(ml, lines.v[i]);
    EV("clean rebuild: exit %d in %.0fs, log %s", rc, dt, ml_log_path(ml, "gate-ninja.log"));
    if (rc == 0 && ok_line) {
        EV("gate line: %s", ok_line);
        /* the wipe took classes.json with build/R5WEA4; the asm it is scanned from is back, so rebuild it now (the rules
         * step does too, but a run that stops after the gate would leave the slot without it) */
        AvpStr ctext;
        i64 crc = ml_run_cmd(ml, "classes_scan", "gate-classes.log", NULL, NULL, &ctext);
        EV("classes.json rebuilt: classes.exe scan exit %d (log %s)", crc, ml_log_path(ml, "gate-classes.log"));
        return ok_line;
    }
    if (rc != 0) EV("ninja exited %d", rc);
    if (!ok_line) EV("the success line `main.dol: OK` was not printed");
    AvpStrVec errs = {0};
    for (size_t i = 0; i < lines.n; i++)
        if (has_ci(lines.v[i], "error") || has_ci(lines.v[i], "FAILED") || has_ci(lines.v[i], "mismatch"))
            avp_vec_push(&errs, avp_str_strip(lines.v[i]));
    if (errs.n) {
        for (size_t i = 0; i < errs.n && i < 12; i++) EV("    %S", &errs.v[i]);
    } else {
        ml_ev_tail(ml, ev, text, 10);
    }
    AvpStr att;
    i64 rc2 = ml_run_cmd(ml, "attribute", "gate-attribute.log", NULL, NULL, &att);
    EV("gatecheck.exe attribute (exit %d, log %s):", rc2, ml_log_path(ml, "gate-attribute.log"));
    AvpStrVec al = avp_str_splitlines(att, false);
    const char *diff_line = NULL;
    for (size_t i = 0; i < al.n && i < 70; i++)
        if (avp_str_strip(al.v[i]).n) EV("    %S", &al.v[i]);
    for (size_t i = 0; i < al.n && !diff_line; i++)
        if (avp_str_startswith(avp_str_strip(al.v[i]), AVP_S("differing bytes"))) diff_line = strip_cz(ml, al.v[i]);
    ml_fail(ml, ml_join_ev(ml, ev),
            F("RED: ninja exit %d, no `main.dol: OK` line%s", rc, diff_line ? F("; gatecheck: %s", diff_line) : ""));
}

/* ---- layout ----------------------------------------------------------------------------------------------------- */
static bool diff_or_err(AvpStr ln) {
    /* re.match(r"(DIFF|ERR)\b", ln) */
    AvpStr rest;
    if (avp_str_startswith(ln, AVP_S("DIFF"))) rest = avp_str_slice(ln, 4, AVP_END);
    else if (avp_str_startswith(ln, AVP_S("ERR"))) rest = avp_str_slice(ln, 3, AVP_END);
    else return false;
    if (!rest.n) return true;
    unsigned char c = (unsigned char)rest.p[0];
    bool word = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80;
    return !word;
}

const char *ml_step_layout(Ml *ml, AvpStrVec *ev) {
    ml_require_no_unmerged(ml);
    AvpStr text;
    i64 rc = ml_run_cmd(ml, "layout", "layout.log", NULL, NULL, &text);
    AvpStrVec lines = avp_str_splitlines(text, false), bad = {0};
    const char *summary = NULL;
    for (size_t i = 0; i < lines.n; i++) {
        if (!summary && avp_str_startswith(lines.v[i], AVP_S("units checked"))) summary = strip_cz(ml, lines.v[i]);
        if (diff_or_err(lines.v[i])) avp_vec_push(&bad, avp_str_strip(lines.v[i]));
    }
    EV("gatecheck layout --all --status Matching: exit %d (log %s)", rc, ml_log_path(ml, "layout.log"));
    if (summary) ml_ev(ml, ev, summary);
    if (rc != 0 || bad.n) {
        if (bad.n) {
            for (size_t i = 0; i < bad.n && i < 30; i++) EV("    %S", &bad.v[i]);
        } else {
            ml_ev_tail(ml, ev, text, 15);
        }
        ml_fail(ml, ml_join_ev(ml, ev), NULL);
    }
    if (!summary) {
        ml_ev_tail(ml, ev, text, 5);
        ml_ev(ml, ev, "layout printed no `units checked` line");
        ml_fail(ml, ml_join_ev(ml, ev), NULL);
    }
    return summary;
}

/* ---- ledger ----------------------------------------------------------------------------------------------------- */
/* A failed ledger step leaves nothing behind: it began on a clean tree, so the two files it rewrites (the baseline copy and
 * the regenerated progress ledger) go back to HEAD, index and working tree, before the step reports its failure. */
static AVP_NORETURN void ledger_fail(Ml *ml, AvpStrVec *ev, const char *summary) {
    MlGit r = ML_GIT(ml, "checkout", "HEAD", "--", ML_BASELINE, ML_PROGRESS);
    if (r.rc == 0) EV("restored %s and %s to HEAD (the update did not complete)", ML_BASELINE, ML_PROGRESS);
    else EV("could not restore %s and %s (git checkout exit %d): %s", ML_BASELINE, ML_PROGRESS, r.rc, se_or_so(ml, &r));
    ml_fail(ml, ml_join_ev(ml, ev), summary);
}

/* ---- splits.txt order -------------------------------------------------------------------------------------------- */

static bool splits_header(AvpStr line) {
    AvpStr t = avp_str_rstrip(line);
    return t.n && t.p[0] != ' ' && t.p[0] != '\t' && t.p[t.n - 1] == ':' && !avp_str_eqz(t, "Sections:");
}

/* A splits.txt as its preamble (the Sections: list) and its unit blocks, each a header with its indented lines, trailing
 * blank lines dropped, joined with \n. */
static void splits_blocks(Ml *ml, AvpStr text, AvpStr *preamble, AvpStrVec *blocks) {
    AvpStrVec lines = avp_str_splitlines(text, false);
    size_t i = 0;
    while (i < lines.n && !splits_header(lines.v[i])) i++;
    *preamble = avp_str_rstrip(avp_str_join(ml->a, AVP_S("\n"), lines.v, i));
    while (i < lines.n) {
        size_t j = i + 1;
        while (j < lines.n && !splits_header(lines.v[j])) j++;
        size_t end = j;
        while (end > i + 1 && !avp_str_strip(lines.v[end - 1]).n) end--;
        avp_vec_push(blocks, avp_str_join(ml->a, AVP_S("\n"), lines.v + i, end - i));
        i = j;
    }
}

/* dtk sorts splits.txt by address when the gate builds, so a lane that added its units out of order leaves the tree
 * dirty with nothing but that file. When the only difference to HEAD is the order of whole unit blocks, commit it in the
 * lane (the build has already used the sorted file) and go on; any other change is left for the clean-tree check. */
static void settle_splits_order(Ml *ml, AvpStrVec *ev) {
    AvpArena *a = ml->a;
    AvpStrVec dirty = ml_status_lines(ml, NULL, true);
    if (dirty.n != 1 || !avp_str_eqz(avp_str_rstrip(dirty.v[0]), " M " ML_SPLITS)) return;
    MlGit head = ML_GIT(ml, "show", "HEAD:" ML_SPLITS);
    AvpBytes now = {0};
    if (head.rc != 0 || !avp_read_all(a, avp_path_join(a, ml->slot, ML_SPLITS), &now)) return;
    AvpStr p0, p1;
    AvpStrVec b0 = {0}, b1 = {0};
    splits_blocks(ml, head.out, &p0, &b0);
    splits_blocks(ml, (AvpStr){(const char *)now.v, now.n}, &p1, &b1);
    if (!avp_str_eq(p0, p1) || b0.n != b1.n || !b0.n) return;
    avp_sort_strs(b0.v, b0.n);
    avp_sort_strs(b1.v, b1.n);
    for (size_t i = 0; i < b0.n; i++)
        if (!avp_str_eq(b0.v[i], b1.v[i])) return;
    MlGit add = ML_GIT(ml, "add", "--", ML_SPLITS);
    MlGit cm = add.rc == 0 ? ML_GIT(ml, "commit", "-m", "split: keep units in address order") : add;
    if (cm.rc != 0) {
        EV("could not commit the re-sorted %s: %s", ML_SPLITS, se_or_so(ml, &cm));
        return;
    }
    ml->tip = ml_rev(ml, "HEAD", NULL);
    EV("committed %s: split: keep units in address order (the build re-sorted %s; %d unit blocks, same set)",
       ml_short(a, ml->tip), ML_SPLITS, (int)b0.n);
}

const char *ml_step_ledger(Ml *ml, AvpStrVec *ev) {
    AvpArena *a = ml->a;
    static const char *const NEEDS[] = {"gate", "changes", "rules", "layout"};
    AvpStrVec missing = {0};
    for (int i = 0; i < 4; i++)
        if (!avp_set_has(ml->passed, avp_str(NEEDS[i]))) avp_vec_push(&missing, avp_str(NEEDS[i]));
    if (missing.n)
        ml_fail(ml, F("--update needs these steps to have passed in this run: %s", join_first(ml, &missing, missing.n, ", ")),
                NULL);
    ml_require_no_unmerged(ml);
    ml_tools_guard(ml, ev);
    settle_splits_order(ml, ev);
    AvpStrVec dirty = ml_status_lines(ml, NULL, true);
    if (dirty.n) ml_fail(ml, F("slot tree is not clean before the update: %s", join_first(ml, &dirty, 6, "; ")), NULL);
    const char *src = avp_path_join(a, ml->slot, ML_REPORT), *dst = avp_path_join(a, ml->slot, ML_BASELINE);
    if (!avp_exists(src)) ml_fail(ml, F("%s is missing", src), NULL);
    if (!avp_copyfile(src, dst)) shsm_raise_io();
    EV("copied %s -> %s", ML_REPORT, ML_BASELINE);
    AvpStr text;
    i64 rc = ml_run_cmd(ml, "ledger_progress", "ledger-progress.log", NULL, NULL, &text);
    EV("ledger.exe progress: exit %d (log %s)", rc, ml_log_path(ml, "ledger-progress.log"));
    ml_ev_tail(ml, ev, text, 6);
    if (rc != 0) ledger_fail(ml, ev, NULL);
    rc = ml_run_cmd(ml, "ledger_exceptions", "ledger-exceptions.log", NULL, NULL, &text);
    EV("ledger.exe exceptions-check: exit %d (log %s)", rc, ml_log_path(ml, "ledger-exceptions.log"));
    ml_ev_tail(ml, ev, text, 6);
    if (rc != 0) ledger_fail(ml, ev, NULL);
    MlGit st = ML_GIT(ml, "diff", "--stat");
    EV("git diff --stat:");
    AvpStrVec sl = avp_str_splitlines(st.out, false);
    for (size_t i = 0; i < sl.n; i++) EV("    %S", &sl.v[i]);
    AvpStrVec changed = ml_status_lines(ml, NULL, true), stray = {0};
    for (size_t i = 0; i < changed.n; i++) {
        AvpStr p = avp_str_strip_chars(avp_str_strip(avp_u8_slice(changed.v[i], 3, AVP_END)), AVP_S("\""));
        if (!avp_str_eqz(p, ML_BASELINE) && !avp_str_eqz(p, ML_PROGRESS)) avp_vec_push(&stray, changed.v[i]);
    }
    if (stray.n) {
        EV("unexpected changes besides the baseline and the ledger: %s", join_first(ml, &stray, 6, "; "));
        ledger_fail(ml, ev, NULL);
    }
    if (!changed.n) {
        EV("baseline and ledger already current; nothing to commit");
        return "nothing to commit";
    }
    MlGit add = ML_GIT(ml, "add", "--", ML_BASELINE, ML_PROGRESS);
    if (add.rc != 0)
        { EV("git add -- %s %s failed (exit %d): %s", ML_BASELINE, ML_PROGRESS, add.rc, se_or_so(ml, &add)); ledger_fail(ml, ev, NULL); }
    const char *msg = F("review: %s merge (ledger, baseline)", ml->branch);
    MlGit cm = ML_GIT(ml, "commit", "-m", msg);
    if (cm.rc != 0) {
        EV("git commit failed: %s", se_or_so(ml, &cm));
        ledger_fail(ml, ev, NULL);
    }
    ml->tip = ml_rev(ml, "HEAD", NULL);
    if (ml_status_lines(ml, NULL, true).n) {
        EV("tree not clean after the commit");
        ml_fail(ml, ml_join_ev(ml, ev), NULL);
    }
    EV("committed %s: %s", ml_short(a, ml->tip), msg);
    return F("committed %s", ml_short(a, ml->tip));
}

/* ---- merge ------------------------------------------------------------------------------------------------------ */
const char *ml_step_merge(Ml *ml, AvpStrVec *ev) {
    AvpArena *a = ml->a;
    if (!avp_set_has(ml->passed, AVP_S("ledger"))) ml_fail(ml, "--merge needs the ledger update to have passed in this run", NULL);
    ml_require_no_unmerged(ml);
    if (ml_status_lines(ml, NULL, true).n) ml_fail(ml, "slot tree is not clean", NULL);
    const char *tip = ml_rev(ml, avp_sprintf(a, "refs/heads/%s", ml->branch), NULL);
    const char *head = ml_rev(ml, "HEAD", NULL);
    if (!tip || !head || strcmp(tip, head))
        ml_fail(ml, F("slot HEAD (%s) is not the tip of %s (%s)", ml_short(a, head ? head : "?"), ml->branch,
                      ml_short(a, tip ? tip : "?")), NULL);
    const char *onto_ref = avp_sprintf(a, "refs/heads/%s", ml->onto);
    const char *onto_tip = ml_rev(ml, onto_ref, NULL);
    if (!onto_tip) ml_fail(ml, F("onto branch %s does not exist", ml->onto), NULL);
    if (!ml->onto_rev) ml_fail(ml, F("the %s revision was not pinned by preflight in this run", ml->onto), NULL);
    if (strcmp(onto_tip, ml->onto_rev))
        ml_fail(ml,
                F("%s moved during this run: pinned at %s by preflight, now at %s (another merge landed?). Nothing was "
                  "merged; re-run this tool to rebase onto the new %s and check again",
                  ml->onto, ml_short(a, ml->onto_rev), ml_short(a, onto_tip), ml->onto),
                F("%s moved during the run (%s -> %s); re-run", ml->onto, ml_short(a, ml->onto_rev), ml_short(a, onto_tip)));
    MlGit anc = ML_GIT(ml, "merge-base", "--is-ancestor", onto_tip, tip);
    if (anc.rc != 0)
        ml_fail(ml, F("%s (%s) is not an ancestor of the lane tip %s: %s moved since the rebase; run this tool again",
                      ml->onto, ml_short(a, onto_tip), ml_short(a, tip), ml->onto), NULL);
    const char *holder = NULL;
    MlWorktrees wts = ml_worktrees(ml);
    for (size_t i = 0; i < wts.n && !holder; i++)
        if (wts.v[i].branch && !strcmp(wts.v[i].branch, onto_ref)) holder = wts.v[i].path;
    if (holder) {
        if (ml_norm_eq(a, holder, ml->slot)) ml_fail(ml, F("%s is checked out in the slot itself", ml->onto), NULL);
        AvpStrVec ops = ml_op_in_progress(ml, holder);
        if (ops.n)
            ml_fail(ml, F("%s in progress in %s; refusing to merge there", join_first(ml, &ops, ops.n, "/"), holder), NULL);
        AvpStrVec tracked = ml_status_lines(ml, holder, false);
        if (tracked.n)
            EV("note: %s has %d uncommitted tracked change(s); git refuses if they overlap the merge", holder, tracked.n);
        MlGit mg = ML_GIT_IN(ml, holder, "merge", "--ff-only", tip);
        if (mg.rc != 0) {
            EV("git merge --ff-only in %s failed: %s", holder, se_or_so(ml, &mg));
            ml_fail(ml, ml_join_ev(ml, ev), NULL);
        }
        EV("fast-forwarded %s in %s: %s -> %s", ml->onto, holder, ml_short(a, onto_tip), ml_short(a, tip));
    } else {
        MlGit u = ML_GIT(ml, "update-ref", "-m", avp_sprintf(a, "merge_lane: fast-forward to %s", ml->branch), onto_ref,
                         tip, onto_tip);
        if (u.rc != 0) {
            EV("git update-ref failed: %s", se_or_so(ml, &u));
            ml_fail(ml, ml_join_ev(ml, ev), NULL);
        }
        EV("%s is not checked out anywhere: moved the branch %s -> %s (compare-and-swap)", ml->onto, ml_short(a, onto_tip),
           ml_short(a, tip));
    }
    const char *now = ml_rev(ml, onto_ref, NULL);
    if (!now || strcmp(now, tip)) {
        EV("after the merge %s is at %s, expected %s", ml->onto, ml_short(a, now ? now : "?"), ml_short(a, tip));
        ml_fail(ml, ml_join_ev(ml, ev), NULL);
    }
    EV("%s is now at %s. Nothing was pushed. Park the slot: git -C %s checkout --detach %s", ml->onto, ml_short(a, tip),
       ml->slot, ml->onto);
    return F("%s -> %s", ml->onto, ml_short(a, tip));
}
