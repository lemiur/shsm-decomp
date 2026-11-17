/* mwt_run: one unit's compile under the debugger and the calls of its object (mwt_run.h). */
#include "mwt_run.h"

#include "cc_elf.h"
#include "shsm_text.h"
#include "wdbg_run.h"

static bool words_eq(const AvpStrVec *flags, size_t at, const AvpStrVec *w) {
    if (at + w->n > flags->n) return false;
    for (size_t k = 0; k < w->n; k++)
        if (!avp_str_eq(flags->v[at + k], w->v[k])) return false;
    return true;
}

static bool g_quiet;

void mwt_set_quiet(bool quiet) { g_quiet = quiet; }

bool mwt_job_init(AvpArena *a, MwtJob *j, const char *unit, const char *src, const AvpStrVec *adds,
                  const AvpStrVec *drops, const char *outdir, const char **why) {
    memset(j, 0, sizeof *j);
    j->quiet = g_quiet;
    j->repo = shsm_repo_find(NULL);
    const char *name = strncmp(unit, "main/", 5) == 0 ? unit : avp_sprintf(a, "main/%s", unit);
    const ShsmUnit *u = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        u = shsm_get_unit(j->repo, name);
        avp_try_end(&t);
    } else {
        *why = avp_sprintf(a, "unknown unit %s", unit);
        return false;
    }
    if (!u || !u->source || shsm_unit_is_asm(u) || !u->has_cflags || !u->mw_version) {
        *why = avp_sprintf(a, "%s is not a compiled C/C++ unit with flags in build.ninja", name);
        return false;
    }
    j->unit = u;
    j->build = avp_cz(a, avp_str_replace(a, avp_str(u->mw_version), AVP_S("\\"), AVP_S("/"), -1));
    j->table = mwt_build_find(j->build);
    if (!j->table) {
        *why = avp_sprintf(a, "%s compiles with %s; mwtrace has addresses for Wii/1.0 and Wii/1.0a only", name, j->build);
        return false;
    }
    if (!mwt_build_verify(a, j->repo->root, j->table, why)) return false;
    j->source = src ? avp_path_abspath(a, src) : avp_path_join(a, j->repo->root, u->source);
    j->scratch = src != NULL;
    if (!avp_exists(j->source)) {
        *why = avp_sprintf(a, "no such source file: %s", j->source);
        return false;
    }
    for (size_t i = 0; i < u->cflags.n; i++) avp_vec_push(&j->flags, u->cflags.v[i]);
    for (size_t i = 0; drops && i < drops->n; i++) {
        AvpStrVec w = {0};
        const char *err = NULL;
        if (!shsm_shlex_split(a, drops->v[i], &w, &err) || !w.n) {
            *why = avp_sprintf(a, "--drop-flag %.*s: %s", AVP_SF(drops->v[i]), err ? err : "no words");
            return false;
        }
        size_t removed = 0;
        for (size_t at = 0; at < j->flags.n;) {
            if (words_eq(&j->flags, at, &w)) {
                for (size_t k = 0; k < w.n; k++) avp_vec_remove(&j->flags, at);
                removed++;
            } else {
                at++;
            }
        }
        if (!removed) {
            *why = avp_sprintf(a, "--drop-flag %.*s: not in the flags of %s", AVP_SF(drops->v[i]), name);
            return false;
        }
        avp_vec_push(&j->dropped, drops->v[i]);
    }
    for (size_t i = 0; adds && i < adds->n; i++) {
        AvpStrVec w = {0};
        const char *err = NULL;
        if (!shsm_shlex_split(a, adds->v[i], &w, &err)) {
            *why = avp_sprintf(a, "--flag %.*s: %s", AVP_SF(adds->v[i]), err);
            return false;
        }
        for (size_t k = 0; k < w.n; k++) avp_vec_push(&j->flags, w.v[k]);
        avp_vec_push(&j->added, adds->v[i]);
    }
    j->outdir = avp_path_abspath(a, outdir);
    if (!avp_makedirs(j->outdir, true)) {
        *why = avp_sprintf(a, "cannot create %s", j->outdir);
        return false;
    }
    return true;
}

bool mwt_job_run(AvpArena *a, const MwtJob *j, AvpStr spec_text, const char *spec_name, AvpStr *log, double *secs,
                 i64 *exit_code, const char **why) {
    const char *spec_path = avp_path_join(a, j->outdir, spec_name);
    if (!avp_write_file(spec_path, spec_text.p, spec_text.n)) {
        *why = avp_sprintf(a, "cannot write %s", spec_path);
        return false;
    }
    const char *obj = avp_path_join(a, j->outdir, "o.o");
    avp_remove(obj); /* a failed compile must not leave an old object to compare against */
    AvpStr root_fwd = avp_str_replace(a, avp_str(avp_path_normpath(a, j->repo->root)), AVP_S("\\"), AVP_S("/"), -1);
    const char *exe = avp_sprintf(a, "%.*s/tools/compilers/%s/mwcceppc.exe", AVP_SF(root_fwd), j->build);
    const char **argv = AVP_NEWN(a, const char *, j->flags.n + 6);
    size_t n = 0;
    argv[n++] = exe;
    for (size_t i = 0; i < j->flags.n; i++) argv[n++] = avp_cz(a, j->flags.v[i]);
    argv[n++] = "-c";
    argv[n++] = j->source;
    argv[n++] = "-o";
    argv[n++] = obj;
    WdbgRun r = {.exe = exe, .build = j->build, .argv = argv, .argc = n, .cwd = avp_cz(a, root_fwd), .outdir = j->outdir,
                 .spec_path = spec_path, .spec_text = spec_text,
                 .output_path = j->quiet ? avp_path_join(a, j->outdir, "compiler.txt") : NULL};
    const char *cls = NULL, *msg = NULL;
    double t0 = avp_perf_counter();
    bool ok = wdbg_run(a, &r, &cls, &msg, exit_code);
    *secs = avp_perf_counter() - t0;
    if (!ok) {
        *why = cls ? avp_sprintf(a, "%s: %s", cls, msg) : msg;
        return false;
    }
    if (!avp_read_text(a, avp_path_join(a, j->outdir, "log.txt"), AVP_TM_UTF8_REPLACE, log)) {
        *why = avp_sprintf(a, "no log in %s", j->outdir);
        return false;
    }
    return true;
}

/* The function symbol of section shndx that holds offset off (for a call through a section symbol). */
static AvpStr function_at(const CcElf *e, u32 shndx, u32 off) {
    for (size_t i = 1; i < e->nsymbols; i++) {
        const CcSym *s = &e->symbols[i];
        if (s->type == CC_STT_FUNC && s->shndx == shndx && off >= s->value && off < s->value + (s->size ? s->size : 1))
            return s->name;
    }
    return AVP_S("");
}

bool mwt_obj_calls(AvpArena *a, const char *path, MwtObjFuncs *out, const char **why) {
    memset(out, 0, sizeof *out);
    AvpBytes data = {0};
    if (!avp_read_all(a, path, &data)) {
        *why = avp_sprintf(a, "cannot read %s (the compile wrote no object)", path);
        return false;
    }
    const CcElf *e = cc_elf_parse(a, data.v, data.n, why);
    if (!e) return false;
    for (size_t i = 1; i < e->nsymbols; i++) {
        const CcSym *s = &e->symbols[i];
        if (s->type != CC_STT_FUNC || s->shndx == 0 || s->shndx >= e->nsections) continue;
        const CcSection *sec = &e->sections[s->shndx];
        MwtObjFunc f = {s->name, {0}};
        for (size_t k = 0; k < sec->nrelocs; k++) {
            const CcReloc *r = &sec->relocs[k];
            if (r->offset < s->value || r->offset >= s->value + s->size) continue;
            if (r->type != CC_R_REL24 || r->sym >= e->nsymbols) continue;
            const CcSym *t = &e->symbols[r->sym];
            AvpStr target = t->name;
            if (t->type == CC_STT_SECTION) target = function_at(e, t->shndx, (u32)(t->value + (u32)r->addend));
            avp_vec_push(&f.calls, ((MwtCall){r->offset - s->value, target}));
        }
        avp_vec_push(out, f);
    }
    return true;
}

const MwtObjFunc *mwt_obj_find(const MwtObjFuncs *fs, AvpStr name) {
    for (size_t i = 0; i < fs->n; i++)
        if (avp_str_eq(fs->v[i].name, name)) return &fs->v[i];
    return NULL;
}
