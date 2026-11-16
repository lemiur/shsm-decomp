/* wdbg.exe iro-scan: the iro_gate spec over the units, one line per non-exact function: is the IR optimiser on, and
 * a flag when it is off in a function whose siblings in the unit have it on.
 *
 * Each unit is one run of the single-run path (this executable, `wdbg <src> <unit> <outdir> <spec>`), so the compiler
 * build, the flags and the spec's build check are the ones a hand run has. The logs are summarised from
 * <outdir>/<unit>/log.txt; a unit whose run failed says so (the spec refuses a build it has no addresses for). */
#include "wdbg_iro.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shsm_cli.h"
#include "shsm_mangle.h"
#include "shsm_report.h"
#include "shsm_units.h"

#define IRO_DEFAULT_SPEC "tools/c/tools/wdbg/specs/iro_gate.wdbg"

typedef struct {
    const char *name;  /* the compiler's object name: the source name ("getEvent", "__ct"), not the mangled symbol */
    int on;
    const char *first; /* what stopped it, "" when on */
} IroRow;
typedef AVP_VEC(IroRow) IroRows;

static const char *usage(void) {
    return "usage: wdbg.exe iro-scan [UNIT ...] [--status S[,S]] [--all-functions] [-j N] [--outdir DIR] [--spec FILE]\n"
           "\n"
           "Runs the iro_gate spec over each UNIT (default: every unit whose status is --status, default\n"
           "NonMatching,Equivalent) and prints one line per function that is not exact in build/<ver>/report.json:\n"
           "  <unit> <function> <percent>  IRO on|off [first refusal]  [FLAG: off, k of n siblings on]\n"
           "The flag marks a function whose IR optimiser is off while others of its unit have it on (a pointer of an\n"
           "aligned typedef compared with NULL, or a member read through one, turns it off for the whole function).\n"
           "--all-functions also lists the exact ones; -j N runs N units at once (default 4); logs under --outdir\n"
           "(default build/wdbg/iro/<unit>/log.txt).";
}

/* The rows of one log: `IRO <name> optimise=<0|1> <first>` lines in compile order; *build the `compiler:` line. */
static IroRows read_log(AvpArena *a, const char *path, const char **build) {
    IroRows rows = {0};
    *build = NULL;
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8_REPLACE, &text)) return rows;
    AvpFileLines it;
    avp_file_lines_init(&it, text, AVP_NL_DEFAULT);
    AvpStr line;
    while (avp_file_lines_next(&it, &line)) {
        if (avp_str_startswith(line, AVP_S("compiler: "))) {
            AvpStr rest = {line.p + 10, line.n - 10};
            i64 sp = avp_str_findc(rest, ' ');
            *build = avp_cz(a, sp < 0 ? rest : avp_strn(rest.p, (size_t)sp));
            continue;
        }
        if (!avp_str_startswith(line, AVP_S("IRO "))) continue;
        AvpStr rest = avp_str_strip((AvpStr){line.p + 4, line.n - 4});
        i64 sp = avp_str_findc(rest, ' ');
        if (sp < 0) continue;
        AvpStr name = avp_strn(rest.p, (size_t)sp);
        AvpStr tail = avp_str_strip((AvpStr){rest.p + sp, rest.n - (size_t)sp});
        if (!avp_str_startswith(tail, AVP_S("optimise="))) continue;
        int on = tail.n > 9 && tail.p[9] == '1';
        AvpStr first = avp_str_strip((AvpStr){tail.p + 9 + 1, tail.n - 10});
        avp_vec_push(&rows, ((IroRow){avp_cz(a, name), on, avp_cz(a, first)}));
    }
    return rows;
}

/* The name the compiler gives a symbol's object: the base name as written ("getEvent", "__ct"). */
static const char *object_name(AvpArena *a, const char *symbol) {
    ShsmDemangled *p = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        p = shsm_mangle_parse(a, symbol);
        avp_try_end(&t);
    }
    if (p && !p->is_data) return p->raw_name && *p->raw_name ? p->raw_name : p->name;
    return symbol;
}

typedef struct {
    const ShsmUnit *u;
    const char *dir;
    AvpProc *proc;
} Job;

static void scan_unit(AvpArena *a, const Job *j, const ShsmUnitReport *ur, bool all, int *nflag, int *nrow) {
    const char *build;
    IroRows rows = read_log(a, avp_path_join(a, j->dir, "log.txt"), &build);
    if (!rows.n) {
        printf("%s: no IRO lines in %s/log.txt (the run failed or the build has no IR optimiser rows)\n", j->u->name, j->dir);
        return;
    }
    size_t on = 0;
    for (size_t i = 0; i < rows.n; i++) on += (size_t)rows.v[i].on;
    printf("%s: compiler %s, %zu functions compiled, IR optimiser on in %zu, off in %zu\n", j->u->name,
           build ? build : "?", rows.n, on, rows.n - on);
    if (!ur) return;
    for (size_t k = 0; k < ur->nfunctions; k++) {
        const ShsmFunctionReport *f = &ur->functions[k];
        if (!all && f->fuzzy >= 100.0) continue;
        const char *oname = object_name(a, f->name);
        /* the n-th report function of this source name pairs with the n-th compiled object of it */
        size_t nth = 0, nrep = 0, nobj = 0;
        for (size_t q = 0; q < ur->nfunctions; q++)
            if (!strcmp(object_name(a, ur->functions[q].name), oname)) {
                if (q < k) nth++;
                nrep++;
            }
        const IroRow *hit = NULL;
        size_t seen = 0;
        for (size_t i = 0; i < rows.n; i++)
            if (!strcmp(rows.v[i].name, oname)) {
                if (seen++ == nth) hit = &rows.v[i];
                nobj++;
            }
        if (nobj != nrep) { /* a name with another count of objects (inlined bodies, templates): use it only if all agree */
            hit = NULL;
            size_t o_on = 0;
            for (size_t i = 0; i < rows.n; i++)
                if (!strcmp(rows.v[i].name, oname)) o_on += (size_t)rows.v[i].on;
            if (nobj && (o_on == 0 || o_on == nobj))
                for (size_t i = 0; i < rows.n && !hit; i++)
                    if (!strcmp(rows.v[i].name, oname)) hit = &rows.v[i];
        }
        (*nrow)++;
        if (!hit) {
            printf("%s %s %.2f%%  IRO ?  (no unambiguous compiled object named %s)\n", j->u->name, f->name, f->fuzzy, oname);
            continue;
        }
        size_t sib_on = 0, sib_n = 0;
        for (size_t i = 0; i < rows.n; i++)
            if (&rows.v[i] != hit) {
                sib_n++;
                sib_on += (size_t)rows.v[i].on;
            }
        if (hit->on) {
            printf("%s %s %.2f%%  IRO on\n", j->u->name, f->name, f->fuzzy);
        } else if (sib_on) {
            (*nflag)++;
            printf("%s %s %.2f%%  IRO OFF  %s  FLAG: off, %zu of %zu siblings on\n", j->u->name, f->name, f->fuzzy,
                   hit->first, sib_on, sib_n);
        } else {
            printf("%s %s %.2f%%  IRO off  %s  (no sibling has it on)\n", j->u->name, f->name, f->fuzzy, hit->first);
        }
    }
}

int wdbg_iro_scan(AvpArena *a, const char *self, int argc, char **argv) {
    AvpStrVec names = {0};
    const char *status = "NonMatching,Equivalent", *outdir = "build/wdbg/iro", *spec = IRO_DEFAULT_SPEC;
    bool all = false;
    int jobs = 4;
    for (int i = 0; i < argc; i++) {
        const char *s = argv[i];
        if (!strcmp(s, "--help") || !strcmp(s, "-h")) {
            puts(usage());
            return 0;
        } else if (!strcmp(s, "--all-functions")) {
            all = true;
        } else if ((!strcmp(s, "--status") || !strcmp(s, "--outdir") || !strcmp(s, "--spec") || !strcmp(s, "-j")) &&
                   i + 1 < argc) {
            const char *v = argv[++i];
            if (!strcmp(s, "--status")) status = v;
            else if (!strcmp(s, "--outdir")) outdir = v;
            else if (!strcmp(s, "--spec")) spec = v;
            else jobs = atoi(v) > 0 ? atoi(v) : 1;
        } else if (s[0] == '-') {
            fprintf(stderr, "wdbg.exe iro-scan: unknown option %s\n%s\n", s, usage());
            return 2;
        } else {
            avp_vec_push(&names, avp_str(s));
        }
    }
    ShsmRepo *repo = shsm_repo_find(NULL);
    const ShsmUnits *table = shsm_load_units(repo);
    ShsmReport *rep = shsm_load_report(repo);
    AvpSet *statuses = avp_set_new();
    AvpStrVec parts = avp_str_split(avp_str(status), AVP_S(","), -1);
    for (size_t i = 0; i < parts.n; i++) avp_set_add(statuses, avp_str_strip(parts.v[i]));
    avp_vec_free(&parts);

    AVP_VEC(Job) work = {0};
    if (names.n) {
        for (size_t i = 0; i < names.n; i++) {
            const ShsmUnit *u = shsm_get_unit(repo, avp_cz(a, names.v[i]));
            avp_vec_push(&work, ((Job){u, NULL, NULL}));
        }
    } else {
        for (size_t i = 0; i < table->n; i++) {
            const ShsmUnit *u = table->v[i];
            if (!u->source || shsm_unit_is_asm(u) || !u->has_cflags) continue;
            if (!avp_set_has(statuses, avp_str(u->status))) continue;
            avp_vec_push(&work, ((Job){u, NULL, NULL}));
        }
    }
    if (!work.n) {
        fprintf(stderr, "wdbg.exe iro-scan: no unit selected\n");
        return 2;
    }
    const char *root = avp_path_normpath(a, avp_root());
    int nflag = 0, nrow = 0, failed = 0;
    size_t next = 0, running = 0;
    size_t *which = avp_alloc(a, sizeof(size_t) * (size_t)jobs);
    AvpProc **slots = avp_alloc(a, sizeof(AvpProc *) * (size_t)jobs);
    size_t done = 0;
    while (done < work.n) {
        while (running < (size_t)jobs && next < work.n) {
            Job *j = &work.v[next];
            const char *dir = avp_path_join(a, avp_path_join(a, root, outdir), j->u->path);
            j->dir = dir;
            const char *argvv[] = {self, avp_path_join(a, root, j->u->source), j->u->path, dir, spec, NULL};
            AvpProcSpec ps = {0};
            ps.argv = argvv;
            ps.cwd = root;
            ps.out = AVP_IO_CAPTURE;
            ps.err = AVP_IO_STDOUT;
            ps.text = true;
            AvpProcResult fail = {0};
            AvpProc *p = avp_proc_start(&ps, &fail);
            if (!p) {
                printf("%s: cannot start wdbg: %s\n", j->u->name, fail.message ? fail.message : "?");
                failed++;
                done++;
                next++;
                continue;
            }
            for (size_t s = 0; s < (size_t)jobs; s++)
                if (!slots[s]) {
                    slots[s] = p;
                    which[s] = next;
                    break;
                }
            running++;
            next++;
        }
        if (!running) continue;
        /* wait for any slot: collect its output (a failing run prints one line on stderr) */
        AvpProc *live[64];
        size_t map[64], n = 0;
        for (size_t s = 0; s < (size_t)jobs && n < 64; s++)
            if (slots[s]) {
                live[n] = slots[s];
                map[n++] = s;
            }
        int w = avp_proc_wait_any(live, n, -1);
        if (w < 0) continue;
        size_t s = map[w];
        AvpProcResult r = {0};
        avp_proc_communicate(slots[s], NULL, 0, -1, &r);
        Job *j = &work.v[which[s]];
        avp_proc_close(slots[s]);
        slots[s] = NULL;
        running--;
        done++;
        const ShsmUnitReport *ur = shsm_report_unit(rep, j->u->name);
        if (r.returncode != 0) {
            AvpStr msg = avp_str_strip(r.out);
            i64 nl = avp_str_findc(msg, '\n');
            if (nl >= 0) msg = avp_strn(msg.p, (size_t)nl);
            printf("%s: wdbg failed (exit %lld): %.*s\n", j->u->name, (long long)r.returncode, AVP_SF(msg));
            failed++;
            continue;
        }
        scan_unit(a, j, ur, all, &nflag, &nrow);
    }
    printf("iro-scan: %zu units, %d functions listed, %d flagged (IR optimiser off where siblings have it on), %d runs failed\n",
           work.n, nrow, nflag, failed);
    avp_vec_free(&work);
    return 0;
}
