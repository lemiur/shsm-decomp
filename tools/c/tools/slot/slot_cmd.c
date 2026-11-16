/* slot: the subcommands of tools/slot.py (cmd_list ... cmd_conflicts). */
#include "slot.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static ShsmGitRun git(AvpArena *a, const char *cwd, const char *const *args, size_t n) {
    ShsmGitOpts o = {.quotepath_off = true};
    return shsm_git_run(a, cwd, args, n, &o);
}

static const char *base_name(AvpArena *a, const char *p) { return avp_cz(a, avp_path_basename(p)); }

static const char *join_strs(AvpArena *a, const char *sep, const AvpStrVec *v, size_t limit) {
    size_t n = v->n < limit ? v->n : limit;
    return avp_cz(a, avp_str_join(a, avp_str(sep), v->v, n));
}

static const char *last_line(AvpArena *a, const ShsmGitRun *r) {
    AvpStrVec lines = avp_str_splitlines(shsm_git_failure_text(r), false);
    if (!lines.n) slot_fail(SLOT_UNCAUGHT, "IndexError: list index out of range");
    return avp_cz(a, lines.v[lines.n - 1]);
}

static bool unmerged(AvpArena *a, const char *path) {
    /* slot.unmerged: any failure of `merge-base --is-ancestor` counts as "not merged", as in the Python */
    return shsm_git_is_ancestor(a, path, "HEAD", SLOT_MAIN_REF) != 0;
}

static bool given(const char *s) { return s && *s; }

/* ---- list ------------------------------------------------------------------------------------------------------ */

int slot_cmd_list(AvpArena *a, SlotRepo *r, const SlotArgs *args) {
    (void)args;
    double now = avp_time();
    for (size_t i = 0; i < r->n; i++) {
        const SlotTree *t = &r->trees[i];
        const char *p = t->path;
        const char *name = base_name(a, p);
        if (!avp_isdir(p)) {
            printf("%s  %s  MISSING-DIR%s\n", name, p, t->prunable ? " prunable" : "");
            continue;
        }
        bool ismain = slot_same(a, p, r->main);
        const char *state = shsm_git_dirty(a, p) ? "dirty" : "clean";
        const char *um = (ismain || !unmerged(a, p)) ? "" : " unmerged";
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_printf(&b, "%s  %s  %.7s %s %s%s", name, p, t->head, t->branch ? t->branch : "(detached)", state, um);
        if (ismain) {
            avp_buf_putz(&b, "  main-checkout");
        } else {
            avp_buf_printf(&b, "  lock:%s", slot_lock_text(a, slot_read_lock(a, r, p), now));
            if (t->locked) avp_buf_putz(&b, "  git-locked");
            avp_buf_printf(&b, "  build:%s", avp_isdir(avp_path_join(a, p, "build")) ? "yes" : "no");
            SlotLinkState ls[SLOT_NSHARED];
            slot_link_states(a, p, r->main, ls);
            avp_buf_putz(&b, "  links: ");
            bool first = true;
            for (size_t k = 0; k < SLOT_NSHARED; k++) {
                if (!strcmp(ls[k].state, "n/a")) continue;
                avp_buf_printf(&b, "%s%s=%s", first ? "" : " ", SLOT_SHARED[k].abbr, ls[k].state);
                first = false;
            }
        }
        printf("%s\n", avp_buf_cz(&b));
    }
    return 0;
}

/* ---- new / link ------------------------------------------------------------------------------------------------ */

static AvpStrVec create_links(AvpArena *a, const SlotRepo *r, const char *slot) {
    AvpStrVec made = {0};
    for (size_t i = 0; i < SLOT_NSHARED; i++) {
        const char *rel = SLOT_SHARED[i].rel;
        const char *src = avp_path_join(a, r->main, rel);
        const char *dst = avp_path_join(a, slot, rel);
        if (!avp_isdir(src)) {
            if (SLOT_SHARED[i].required) printf("WARN main checkout lacks %s\n", rel);
            continue;
        }
        if (avp_lexists(dst) || slot_is_link(dst)) continue;
        if (!avp_makedirs(avp_cz(a, avp_path_dirname(dst)), true)) shsm_raise_io(); /* build/ and orig/ may not exist yet */
        if (!shsm_link_create_junction(avp_path_abspath(a, dst), avp_path_abspath(a, src))) shsm_raise_io();
        avp_vec_push(&made, avp_str(rel));
    }
    return made;
}

/* `N` and `slotN` both mean <base>/slotN; any other argument is a path. A path inside the checkout is refused: a nested
 * worktree there gets junctions into its parent, and removing it follows them. */
static const char *slot_path_for_new(AvpArena *a, const SlotRepo *r, const char *arg) {
    const char *num = strncmp(arg, "slot", 4) == 0 ? arg + 4 : arg;
    bool digits = *num != 0;
    for (const char *s = num; *s; s++)
        if (*s < '0' || *s > '9') digits = false;
    const char *path = digits ? avp_path_abspath(a, avp_path_join(a, r->base, avp_sprintf(a, "slot%s", num)))
                              : avp_path_abspath(a, arg);
    const char *key = slot_norm(a, path), *root = slot_norm(a, r->main);
    size_t n = strlen(root);
    while (n && (root[n - 1] == '/' || root[n - 1] == '\\')) n--;
    if (strncmp(key, root, n) == 0 && (key[n] == '/' || key[n] == '\\' || key[n] == 0))
        slot_fail(SLOT_REFUSAL, "a slot may not sit inside the checkout (%s)", path);
    return path;
}

int slot_cmd_new(AvpArena *a, SlotRepo *r, const SlotArgs *args) {
    const char *path = slot_path_for_new(a, r, args->slot);
    if (avp_lexists(path)) slot_fail(SLOT_REFUSAL, "%s exists", path);
    const char *rev = given(args->from_rev) ? args->from_rev : SLOT_MAIN_REF;
    const char *cmd[6];
    size_t n = 0;
    cmd[n++] = "worktree";
    cmd[n++] = "add";
    if (given(args->branch)) {
        if (shsm_git_branch_exists(a, r->cwd, args->branch)) {
            cmd[n++] = path;
            cmd[n++] = args->branch;
        } else {
            cmd[n++] = "-b";
            cmd[n++] = args->branch;
            cmd[n++] = path;
            cmd[n++] = rev;
        }
    } else {
        cmd[n++] = "--detach";
        cmd[n++] = path;
        cmd[n++] = rev;
    }
    ShsmGitRun g = git(a, r->cwd, cmd, n);
    if (!shsm_git_ok(&g)) slot_fail(SLOT_REFUSAL, "git worktree add: %s", last_line(a, &g));
    AvpStrVec made = create_links(a, r, path);
    printf("created %s at %s; links: %s\n", path, given(args->branch) ? args->branch : rev,
           made.n ? join_strs(a, ", ", &made, made.n) : "none");
    SlotHealth h = slot_health(a, path, r->main);
    for (size_t i = 0; i < h.problems.n; i++) printf("FAIL %.*s\n", AVP_SF(h.problems.v[i]));
    return h.problems.n ? 1 : 0;
}

int slot_cmd_link(AvpArena *a, SlotRepo *r, const SlotArgs *args) {
    SlotTree t = slot_repo_find(a, r, args->slot);
    if (slot_same(a, t.path, r->main)) slot_fail(SLOT_REFUSAL, "that is the main checkout");
    if (!avp_isdir(t.path)) slot_fail(SLOT_REFUSAL, "no such slot: %s", t.path);
    AvpStrVec made = create_links(a, r, t.path);
    printf("links created: %s\n", made.n ? join_strs(a, ", ", &made, made.n) : "none");
    return 0;
}

/* ---- check ----------------------------------------------------------------------------------------------------- */

int slot_cmd_check(AvpArena *a, SlotRepo *r, const SlotArgs *args) {
    SlotTree *trees;
    size_t n = 0;
    if (given(args->slot)) {
        trees = AVP_NEW(a, SlotTree);
        trees[n++] = slot_repo_find(a, r, args->slot);
    } else {
        trees = AVP_NEWN(a, SlotTree, r->n);
        for (size_t i = 0; i < r->n; i++)
            if (!slot_same(a, r->trees[i].path, r->main)) trees[n++] = r->trees[i];
    }
    int bad = 0;
    for (size_t i = 0; i < n; i++) {
        const char *p = trees[i].path, *name = base_name(a, p);
        if (slot_same(a, p, r->main)) {
            printf("%s: main checkout (nothing to check)\n", name);
            continue;
        }
        if (!avp_isdir(p)) {
            printf("%s: FAIL directory missing\n", name);
            bad++;
            continue;
        }
        if (args->fix && !trees[i].unknown) {
            AvpStrVec made = create_links(a, r, p); /* never replaces a folder or a copy that is there */
            if (made.n) printf("%s: links created: %s\n", name, join_strs(a, ", ", &made, made.n));
        }
        SlotHealth h = slot_health(a, p, r->main);
        if (trees[i].unknown) avp_vec_insert(&h.problems, 0, AVP_S("not a worktree of this repository"));
        if (h.problems.n) printf("%s: FAIL %s\n", name, join_strs(a, "; ", &h.problems, h.problems.n));
        else printf("%s: OK\n", name);
        if (h.extra_links.n) {
            AvpStrVec rels = {0};
            for (size_t k = 0; k < h.extra_links.n; k++) {
                char *rel;
                if (!avp_path_relpath(a, h.extra_links.v[k].p, p, &rel)) shsm_raise_io();
                avp_vec_push(&rels, avp_str(rel));
            }
            printf("%s: other links: %s\n", name, join_strs(a, ", ", &rels, rels.n));
            avp_vec_free(&rels);
        }
        bad += h.problems.n ? 1 : 0;
    }
    return bad ? 1 : 0;
}

/* ---- reset / remove -------------------------------------------------------------------------------------------- */

static void check_lock_free(AvpArena *a, const SlotRepo *r, const char *path, const char *owner) {
    AvpJson *lock = slot_read_lock(a, r, path);
    if (lock && !slot_lock_owner_is(lock, owner))
        slot_fail(SLOT_REFUSAL, "locked by %s (use --force)", slot_lock_text(a, lock, avp_time()));
}

static void refuse_dirty_unmerged(AvpArena *a, const SlotRepo *r, const SlotTree *t, const char *owner) {
    if (shsm_git_dirty(a, t->path)) slot_fail(SLOT_REFUSAL, "%s is dirty (use --force)", t->path);
    if (unmerged(a, t->path))
        slot_fail(SLOT_REFUSAL, "%s has commits not in %s (use --force)", t->branch ? t->branch : "HEAD", SLOT_MAIN_REF);
    check_lock_free(a, r, t->path, owner);
}

int slot_cmd_reset(AvpArena *a, SlotRepo *r, const SlotArgs *args) {
    SlotTree t = slot_repo_find(a, r, args->slot);
    const char *p = t.path;
    if (t.unknown || !avp_isdir(p)) slot_fail(SLOT_REFUSAL, "not a worktree: %s", p);
    if (slot_same(a, p, r->main)) slot_fail(SLOT_REFUSAL, "refusing to reset the main checkout");
    if (!args->force) refuse_dirty_unmerged(a, r, &t, args->owner);
    const char *rev = given(args->to) ? args->to : SLOT_MAIN_REF;
    const char *cmd[4];
    size_t n = 0;
    cmd[n++] = "checkout";
    cmd[n++] = "--detach";
    if (args->force) cmd[n++] = "--force";
    cmd[n++] = rev;
    ShsmGitRun g = git(a, p, cmd, n);
    if (!shsm_git_ok(&g)) slot_fail(SLOT_REFUSAL, "git checkout: %s", last_line(a, &g));
    static const char *const sh[] = {"rev-parse", "--short", "HEAD"};
    ShsmGitRun h = git(a, p, sh, 3);
    printf("%s: detached at %s (%.*s)\n", base_name(a, p), rev, AVP_SF(avp_str_strip(h.out)));
    return 0;
}

int slot_cmd_remove(AvpArena *a, SlotRepo *r, const SlotArgs *args) {
    SlotTree t = slot_repo_find(a, r, args->slot);
    const char *p = t.path;
    if (slot_same(a, p, r->main)) slot_fail(SLOT_REFUSAL, "refusing to remove the main checkout");
    if (t.unknown) slot_fail(SLOT_REFUSAL, "not a worktree of this repository: %s", p);
    if (avp_isdir(p) && !args->force) refuse_dirty_unmerged(a, r, &t, args->owner);
    if (avp_isdir(p)) {
        AvpStrVec links = slot_scan_links(a, p);
        for (size_t i = 0; i < links.n; i++) {
            const char *link = links.v[i].p;
            if (!strcmp(slot_norm(a, link), slot_norm(a, p)))
                slot_fail(SLOT_REFUSAL, "the worktree directory itself is a link: %s", p);
            slot_remove_link(link);
            char *rel;
            if (!avp_path_relpath(a, link, p, &rel)) shsm_raise_io();
            printf("unlinked %s\n", rel);
        }
        AvpStrVec left = slot_scan_links(a, p);
        if (left.n) slot_fail(SLOT_REFUSAL, "reparse points remain, not removing: %s", join_strs(a, ", ", &left, 5));
    }
    const char *cmd[4];
    size_t n = 0;
    cmd[n++] = "worktree";
    cmd[n++] = "remove";
    if (args->force) cmd[n++] = "--force";
    cmd[n++] = p;
    ShsmGitRun g = git(a, r->cwd, cmd, n);
    if (!shsm_git_ok(&g)) slot_fail(SLOT_REFUSAL, "git worktree remove: %s", last_line(a, &g));
    const char *lp = slot_lock_path(a, r, p);
    if (avp_isfile(lp) && !avp_remove(lp)) shsm_raise_io();
    printf("removed %s\n", p);
    return 0;
}

/* ---- lock / unlock / who --------------------------------------------------------------------------------------- */

int slot_cmd_lock(AvpArena *a, SlotRepo *r, const SlotArgs *args) {
    SlotTree t = slot_repo_find(a, r, args->slot);
    const char *p = t.path;
    if (t.unknown) slot_fail(SLOT_REFUSAL, "not a worktree of this repository: %s", p);
    const char *lp = slot_lock_path(a, r, p);
    if (!avp_makedirs(avp_cz(a, avp_path_dirname(lp)), true)) shsm_raise_io();
    AvpJson *cur = slot_read_lock(a, r, p);
    if (cur && !slot_lock_owner_is(cur, args->owner) && !args->force)
        slot_fail(SLOT_REFUSAL, "held by %s", slot_lock_text(a, cur, avp_time()));
    AvpDateTime now = avp_datetime_now_utc();
    now.microsecond = 0; /* isoformat(timespec="seconds") */
    char iso[48];
    avp_datetime_isoformat(&now, 'T', iso);
    AvpJson *d = avp_json_obj(a);
    avp_json_set(a, d, AVP_S("owner"), avp_json_strz(a, args->owner));
    avp_json_set(a, d, AVP_S("lane"), args->lane ? avp_json_strz(a, args->lane) : avp_json_null());
    avp_json_set(a, d, AVP_S("slot"), avp_json_strz(a, p));
    avp_json_set(a, d, AVP_S("time"), avp_json_strz(a, iso));
    avp_json_set(a, d, AVP_S("epoch"), avp_json_float(a, avp_time()));
    avp_json_set(a, d, AVP_S("pid"), avp_json_int(a, (i64)avp_getppid()));
    avp_json_set(a, d, AVP_S("host"), avp_json_strz(a, avp_hostname(a)));
    avp_json_set(a, d, AVP_S("ttl_hours"), avp_json_float(a, args->ttl));
    AvpStr text = avp_json_dumps(a, d, NULL); /* json.dump(data, f): ", " and ": ", ensure_ascii */
    if (!cur) {
        AvpFile *f = avp_fopen(lp, "x", AVP_TM_UTF8);
        if (!f) {
            if (avp_io_is(EEXIST))
                slot_fail(SLOT_REFUSAL, "lock taken concurrently: %s", slot_lock_text(a, slot_read_lock(a, r, p), avp_time()));
            shsm_raise_io();
        }
        bool ok = avp_fwrite_text(f, text);
        if (!avp_fclose(f) || !ok) shsm_raise_io();
    } else {
        const char *tmp = avp_sprintf(a, "%s.tmp", lp);
        if (!avp_write_text(tmp, text, AVP_TM_UTF8) || !avp_replace(tmp, lp)) shsm_raise_io();
    }
    printf("%s locked by %s\n", base_name(a, p), args->owner);
    return 0;
}

int slot_cmd_unlock(AvpArena *a, SlotRepo *r, const SlotArgs *args) {
    SlotTree t = slot_repo_find(a, r, args->slot);
    AvpJson *cur = slot_read_lock(a, r, t.path);
    if (!cur) {
        printf("%s not locked\n", base_name(a, t.path));
        return 0;
    }
    if (given(args->owner) && !slot_lock_owner_is(cur, args->owner) && !args->force)
        slot_fail(SLOT_REFUSAL, "held by %s", slot_lock_text(a, cur, avp_time()));
    if (!avp_remove(avp_cz(a, avp_json_get_str(cur, "file", AVP_S(""))))) shsm_raise_io();
    printf("%s unlocked (was %s)\n", base_name(a, t.path), slot_lock_text(a, cur, avp_time()));
    return 0;
}

int slot_cmd_who(AvpArena *a, SlotRepo *r, const SlotArgs *args) {
    (void)args;
    const char *d = avp_path_join(a, r->common, SLOT_LOCK_DIR);
    AvpStrVec names = {0};
    if (avp_isdir(d)) {
        AvpStrVec all = {0};
        if (!avp_listdir(a, d, &all)) shsm_raise_io();
        for (size_t i = 0; i < all.n; i++)
            if (avp_str_endswith(all.v[i], AVP_S(".json"))) avp_vec_push(&names, all.v[i]);
        avp_sort_strs(names.v, names.n);
    }
    if (!names.n) printf("no locks\n");
    double now = avp_time();
    for (size_t i = 0; i < names.n; i++) {
        const char *slot = avp_cz(a, (AvpStr){names.v[i].p, names.v[i].n - 5});
        AvpJson *lock = slot_read_lock(a, r, avp_path_join(a, r->base, slot));
        if (!lock) slot_fail(SLOT_UNCAUGHT, "AttributeError: 'NoneType' object has no attribute 'get'");
        bool known = false;
        for (size_t k = 0; k < r->n && !known; k++) known = avp_str_eqz(avp_path_basename(r->trees[k].path), slot);
        printf("%s: %s pid=%s host=%s since %s%s\n", slot, slot_lock_text(a, lock, now),
               slot_py_str(a, avp_json_getz(lock, "pid")), slot_py_str(a, avp_json_getz(lock, "host")),
               slot_py_str(a, avp_json_getz(lock, "time")), known ? "" : " (no such worktree)");
    }
    return 0;
}

/* ---- conflicts ------------------------------------------------------------------------------------------------- */

/* (clean, conflicted files) of merging y into x, without touching any working tree. */
static bool merge_tree(AvpArena *a, const char *cwd, const char *x, const char *y, AvpStrVec *conf) {
    const char *args[] = {"merge-tree", "--write-tree", "--name-only", "--no-messages", x, y};
    ShsmGitRun g = git(a, cwd, args, 6);
    if (!g.started || g.timed_out || (g.rc != 0 && g.rc != 1))
        slot_fail(SLOT_TOOL_ERROR, "git merge-tree %s %s: %.*s", x, y, AVP_SF(shsm_git_failure_text(&g)));
    AvpStrVec lines = avp_str_splitlines(g.out, false);
    for (size_t i = 1; i < lines.n; i++)
        if (avp_str_strip(lines.v[i]).n) avp_vec_push(conf, lines.v[i]);
    return g.rc == 0;
}

/* `git diff --name-only ONTO...BRANCH` as a sorted set. */
static AvpStrVec touched(AvpArena *a, const char *cwd, const char *onto, const char *branch) {
    const char *args[] = {"diff", "--name-only", avp_sprintf(a, "%s...%s", onto, branch)};
    ShsmGitRun g = git(a, cwd, args, 3);
    if (!shsm_git_ok(&g)) {
        /* the check=True message (ToolError) */
        AvpStrVec lines = avp_str_splitlines(shsm_git_failure_text(&g), false);
        slot_fail(SLOT_TOOL_ERROR, "git diff --name-only %s: [%s]", args[2],
                  lines.n ? avp_cz(a, avp_repr_str(a, lines.v[lines.n - 1])) : "");
    }
    AvpStrVec lines = avp_str_splitlines(g.out, false), out = {0};
    for (size_t i = 0; i < lines.n; i++)
        if (lines.v[i].n) avp_vec_push(&out, lines.v[i]);
    avp_sort_strs(out.v, out.n);
    size_t k = 0;
    for (size_t i = 0; i < out.n; i++)
        if (!k || !avp_str_eq(out.v[k - 1], out.v[i])) out.v[k++] = out.v[i];
    out.n = k;
    return out;
}

/* short(files): the first five of the sorted names, then " +N more". */
static const char *short_list(AvpArena *a, AvpStrVec *files) {
    avp_sort_strs(files->v, files->n);
    const char *s = join_strs(a, ", ", files, 5);
    return files->n > 5 ? avp_sprintf(a, "%s +%zu more", s, files->n - 5) : s;
}

int slot_cmd_conflicts(AvpArena *a, SlotRepo *r, const SlotArgs *args) {
    const char *onto = args->onto;
    AvpStrVec branches = {0};
    for (size_t i = 0; i < args->branches.n; i++) avp_vec_push(&branches, args->branches.v[i]);
    if (!branches.n) {
        const char *bl[] = {"branch", "--no-merged", onto, "--format=%(refname:short)"};
        ShsmGitRun g = git(a, r->cwd, bl, 4);
        if (!shsm_git_ok(&g)) {
            AvpStrVec lines = avp_str_splitlines(shsm_git_failure_text(&g), false);
            slot_fail(SLOT_TOOL_ERROR, "git branch --no-merged %s --format=%%(refname:short): [%s]", onto,
                      lines.n ? avp_cz(a, avp_repr_str(a, lines.v[lines.n - 1])) : "");
        }
        branches = avp_str_split(g.out, (AvpStr){0}, -1);
    }
    if (!branches.n) {
        printf("no branches to check (all merged into %s)\n", onto);
        return 0;
    }
    int bad = 0;
    AvpStrVec *files = AVP_NEWN(a, AvpStrVec, branches.n);
    for (size_t i = 0; i < branches.n; i++) {
        const char *b = avp_cz(a, branches.v[i]);
        files[i] = touched(a, r->cwd, onto, b);
        AvpStrVec conf = {0};
        if (merge_tree(a, r->cwd, onto, b, &conf)) {
            printf("%s: clean onto %s (%zu files)\n", b, onto, files[i].n);
        } else {
            bad++;
            printf("%s: CONFLICT onto %s: %s\n", b, onto, short_list(a, &conf));
        }
    }
    for (size_t i = 0; i < branches.n; i++)
        for (size_t j = i + 1; j < branches.n; j++) {
            AvpStrVec shared = {0};
            for (size_t x = 0; x < files[i].n; x++)
                for (size_t y = 0; y < files[j].n; y++)
                    if (avp_str_eq(files[i].v[x], files[j].v[y])) {
                        avp_vec_push(&shared, files[i].v[x]);
                        break;
                    }
            if (!shared.n) continue;
            const char *x = avp_cz(a, branches.v[i]), *y = avp_cz(a, branches.v[j]);
            AvpStrVec conf = {0};
            if (merge_tree(a, r->cwd, x, y, &conf)) {
                printf("%s x %s: %zu shared (%s), merge clean\n", x, y, shared.n, short_list(a, &shared));
            } else {
                bad++;
                printf("%s x %s: %zu shared, CONFLICT: %s\n", x, y, shared.n, short_list(a, &conf));
            }
        }
    return bad ? 1 : 0;
}
