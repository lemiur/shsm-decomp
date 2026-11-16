/* check_docs verbs: the "Project adapter" table of docs/tools.md against the tools' --help (check_verbs and its helpers). */
#include "check_docs.h"
#include "shsm_cli.h"
#include "shsm_re.h"

#define HELP_TIMEOUT 120.0

/* ---- the adapter table ----------------------------------------------------------------------------------------- */
static bool only_chars(AvpStr s, const char *set) {
    for (size_t i = 0; i < s.n; i++)
        if (!strchr(set, s.p[i]) || s.p[i] == 0) return false;
    return true;
}

CdRows cd_adapter_rows(AvpArena *a, const char *root) {
    CdRows rows = {0};
    AvpStrVec lines = cd_read_lines(a, avp_path_join(a, root, CD_VERBS_DOC));
    int state = 0;
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr line = lines.v[i];
        if (state == 0 && avp_str_startswith(line, AVP_S("## Project adapter"))) {
            state = 1;
        } else if (state == 1 && avp_str_startswith(line, AVP_S("| verb"))) {
            state = 2;
        } else if (state == 2) {
            if (!avp_str_startswith(line, AVP_S("|"))) break;
            AvpStrVec cells = cd_split_cells(a, line);
            if (cells.n >= 3 && !only_chars(cells.v[0], "-: ")) {
                CdRow r = {(i64)i + 1, avp_str_replace(a, cells.v[0], AVP_S("`"), AVP_S(""), -1), cells.v[2]};
                avp_vec_push(&rows, r);
            }
        } else if (state == 1 && avp_str_startswith(line, AVP_S("## "))) {
            break;
        }
    }
    return rows;
}

/* ---- commands -------------------------------------------------------------------------------------------------- */
/* One alternative per CdInvKind: 1 a project tool, 2 a pinned binary, 3 and 4 a Python command, 5 a program that is
 * accepted as written (only as the first word of its command). */
#define INVOKE_RE                                                                                                  \
    "(?<![\\w./-])build/tools/([\\w.-]+?)\\.exe(?![\\w.-])"                                                         \
    "|(?<![\\w./-])(tools/bin/[\\w.-]+?)(?:\\.exe)?(?![\\w.-])"                                                     \
    "|((?<![\\w.-])(?:[\\w.-]+/)*(?:python3?|py)(?:\\.exe)?\\s+(?:-I\\s+)?"                                         \
    "(?:-m\\s+tools(?:\\.\\w+)*|(?:[\\w.-]+/)*[\\w.-]+\\.py))"                                                      \
    "|(?<![\\w./-])(tools/[\\w./-]+\\.py)"                                                                          \
    "|^\\s*(tools/build_c\\.cmd|ctest(?:\\.exe)?|cmake(?:\\.exe)?|git|cp|rm)(?![\\w.-])"

CdInvocations cd_parse_invocations(AvpArena *a, AvpStr span) {
    CdInvocations found = {0};
    AvpStrVec segs = shsm_re_split(shsm_re("&&|;|(?<!\\\\)\\|(?=\\s)|(?<=\\s)(?<!\\\\)\\|", 0), span, 0);
    for (size_t i = 0; i < segs.n; i++) {
        AvpStr seg = segs.v[i];
        ShsmReMatch m;
        if (!shsm_re_search(shsm_re(INVOKE_RE, 0), seg, 0, AVP_END, &m)) continue;
        CdInvocation inv = {0};
        if (shsm_re_matched(&m, 1) && shsm_re_group(&m, 1).n) {
            inv.kind = CD_INV_TOOL;
            AvpStr name = shsm_re_group(&m, 1);
            inv.program = avp_str(avp_sprintf(a, "build/tools/%.*s.exe", AVP_SF(name)));
        } else if (shsm_re_matched(&m, 2) && shsm_re_group(&m, 2).n) {
            inv.kind = CD_INV_BIN;
            inv.program = avp_str_dup(a, shsm_re_group(&m, 2));
        } else if (shsm_re_matched(&m, 3) && shsm_re_group(&m, 3).n) {
            inv.kind = CD_INV_PYTHON;
            inv.program = shsm_re_sub(a, shsm_re("\\s+", 0), AVP_S(" "), shsm_re_group(&m, 3), 0);
        } else if (shsm_re_matched(&m, 4) && shsm_re_group(&m, 4).n) {
            inv.kind = CD_INV_PYTHON;
            inv.program = avp_str_dup(a, shsm_re_group(&m, 4));
        } else {
            inv.kind = CD_INV_OTHER;
            inv.program = avp_str_dup(a, shsm_re_group(&m, 5));
        }
        inv.args = avp_str_slice(seg, shsm_re_end(&m, 0), AVP_END);
        avp_vec_push(&found, inv);
    }
    return found;
}

AvpStrVec cd_command_tokens(AvpArena *a, AvpStr text) {
    text = avp_str_replace(a, text, AVP_S("\\|"), AVP_S(" | "), -1);
    text = shsm_re_sub(a, shsm_re("\"[^\"]*\"|'[^']*'", 0), AVP_S(" "), text, 0);
    text = shsm_re_sub(a, shsm_re("[\\[\\](){}]", 0), AVP_S(" "), text, 0);
    AvpStrVec parts = shsm_re_split(shsm_re("[\\s|]+", 0), text, 0);
    AvpStrVec out = {0};
    for (size_t i = 0; i < parts.n; i++)
        if (parts.v[i].n) avp_vec_push(&out, parts.v[i]);
    return out;
}

bool cd_is_flag(AvpStr tok) {
    return shsm_re_match(shsm_re("^--[A-Za-z][\\w-]*(=.*)?$", 0), tok, 0, AVP_END, NULL) ||
           shsm_re_match(shsm_re("^-[A-Za-z]$", 0), tok, 0, AVP_END, NULL);
}

static AvpStr flag_name(AvpStr tok) {
    AvpStr before, after;
    avp_str_partition(tok, AVP_S("="), &before, &after);
    return before;
}

bool cd_is_word(AvpStr tok) { return shsm_re_match(shsm_re("^[a-z][a-z0-9_-]*$", 0), tok, 0, AVP_END, NULL); }

/* ---- help ------------------------------------------------------------------------------------------------------ */
typedef struct {
    bool done;
    const char *text; /* NULL: cannot be read */
    AvpSet *flags;    /* flags(sub), computed on first use */
} HelpText;

typedef struct {
    const char *root;
    CdInvKind kind;  /* CD_INV_TOOL or CD_INV_BIN */
    AvpStr program;  /* build/tools/match.exe, tools/bin/ninja */
    AvpStr base;     /* match.exe, ninja */
    const char *exe; /* the executable run for its help; NULL: it does not exist */
    const CdHelpEnv *env;
    AvpMap *cache; /* key "-" for no subcommand, "+<sub>" for a subcommand */
    bool subs_done;
    AvpSet *subs; /* NULL: the help names no subcommands */
} Help;

static AvpArena *A(void) { return avp_arena(); }

static Help *help_new(const char *root, const CdInvocation *inv, const CdHelpEnv *env) {
    Help *h = AVP_NEW(A(), Help);
    h->root = root;
    h->kind = inv->kind;
    h->program = inv->program;
    i64 slash = avp_str_rfind(inv->program, AVP_S("/"), 0, AVP_END);
    h->base = slash >= 0 ? avp_str_slice(inv->program, slash + 1, AVP_END) : inv->program;
    if (h->kind == CD_INV_TOOL) {
        h->exe = cd_tool_exe(A(), env->exe_dir, root, avp_cz(A(), h->base));
    } else {
        AvpStr native = avp_str_replace(A(), inv->program, AVP_S("/"), AVP_S("\\"), -1);
        char *p = avp_path_join(A(), root, avp_cz(A(), native));
        char *pexe = avp_sprintf(A(), "%s.exe", p);
        if (avp_isfile(p))
            h->exe = p;
        else if (avp_isfile(pexe))
            h->exe = pexe;
    }
    h->env = env;
    h->cache = avp_map_new();
    return h;
}

/* The help command of the tool or of one of its subcommands (ninja: -h). */
static AvpStrVec help_argv(const Help *h, const char *sub) {
    AvpStrVec v = {0};
    avp_vec_push(&v, avp_str(h->exe));
    if (h->kind == CD_INV_BIN && avp_str_eqz(h->base, "ninja")) {
        avp_vec_push(&v, AVP_S("-h"));
        return v;
    }
    if (sub) avp_vec_push(&v, avp_str(sub));
    avp_vec_push(&v, AVP_S("--help"));
    return v;
}

static AvpStr cache_key(const char *sub) { return sub ? avp_str(avp_sprintf(A(), "+%s", sub)) : AVP_S("-"); }

static HelpText *help_slot(Help *h, const char *sub) {
    bool created;
    AvpVal *v = avp_map_put(h->cache, cache_key(sub), &created);
    if (created) v->p = AVP_NEW(A(), HelpText);
    return v->p;
}

static char **argv_cz(const AvpStrVec *v) {
    char **out = avp_alloc(A(), sizeof(char *) * (v->n + 1));
    for (size_t i = 0; i < v->n; i++) out[i] = avp_cz(A(), v->v[i]);
    out[v->n] = NULL;
    return out;
}

static const char *help_text(Help *h, const char *sub) {
    HelpText *t = help_slot(h, sub);
    if (!t->done) {
        AvpStrVec argv = help_argv(h, sub);
        t->text = h->env->run(h->env->ctx, (const char *const *)argv_cz(&argv), argv.n, h->root);
        t->done = true;
    }
    return t->text;
}

static bool truthy(const char *t) { return t && t[0]; }

static bool help_argparse_style(Help *h, const char *sub) {
    const char *t = help_text(h, sub);
    return truthy(t) && shsm_re_match(shsm_re("\\s*usage:", 0), avp_str(t), 0, AVP_END, NULL);
}

/* The subcommands the help names: argparse's "{a,b} ...", or a pinned binary's Commands: section. NULL: none. */
static AvpSet *help_subcommands(Help *h) {
    if (h->subs_done) return h->subs;
    h->subs_done = true;
    const char *t = help_text(h, NULL);
    if (!truthy(t)) return NULL;
    AvpStr text = avp_str(t);
    ShsmReMatch m;
    if (shsm_re_search(shsm_re("\\{([\\w,-]+)\\}\\s*\\.\\.\\.", 0), text, 0, AVP_END, &m)) {
        h->subs = avp_set_new();
        AvpStrVec names = avp_str_split(shsm_re_group(&m, 1), AVP_S(","), -1);
        for (size_t i = 0; i < names.n; i++) avp_set_add(h->subs, names.v[i]);
        return h->subs;
    }
    if (h->kind == CD_INV_BIN) {
        if (shsm_re_search(shsm_re("(?ms)^(?:Commands|SUBCOMMANDS|Subcommands):\\s*\\n(.*?)(?:\\n\\S|\\Z)", 0), text, 0,
                           AVP_END, &m)) {
            h->subs = avp_set_new();
            ShsmReFindall fa = shsm_re_findall(shsm_re("(?m)^\\s{2,}([a-z][\\w-]*)\\b", 0), shsm_re_group(&m, 1), 0,
                                               AVP_END);
            for (size_t i = 0; i < fa.count; i++) avp_set_add(h->subs, shsm_re_findall_at(&fa, i, 0));
            shsm_re_findall_free(&fa);
            return h->subs;
        }
    }
    return NULL;
}

static AvpSet *help_flags(Help *h, const char *sub) {
    HelpText *slot = help_slot(h, sub);
    if (slot->flags) return slot->flags;
    const char *t = help_text(h, sub);
    AvpStr text = avp_str(t ? t : "");
    AvpSet *s = avp_set_new();
    ShsmReFindall fa = shsm_re_findall(shsm_re("(?<![\\w-])(--?[A-Za-z][\\w-]*)", 0), text, 0, AVP_END);
    for (size_t i = 0; i < fa.count; i++) avp_set_add(s, shsm_re_findall_at(&fa, i, 0));
    shsm_re_findall_free(&fa);
    help_slot(h, sub)->flags = s;
    return s;
}

/* ---- the real runner ------------------------------------------------------------------------------------------- */
char *cd_run_help(void *ctx, const char *const *argv, size_t argc, const char *root) {
    (void)ctx;
    (void)argc;
    AvpArena *a = avp_arena();
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = argv, .cwd = root, .in = AVP_IO_NULL, .out = AVP_IO_CAPTURE,
                                             .err = AVP_IO_CAPTURE, .text = true, .errors = AVP_ERRORS_REPLACE,
                                             .timeout = HELP_TIMEOUT, .arena = a});
    if (r.status == AVP_PROC_START_FAILED || r.status == AVP_PROC_TIMEOUT) return NULL;
    if (r.returncode != 0 && r.returncode != 1) return NULL;
    AvpStr out = avp_str_concat(a, r.out, r.err);
    if (!avp_str_strip(out).n) return NULL;
    /* the tools' report of an internal error (shsm_locale.c) is a one-line error: no help in it */
    if (avp_str_contains(out, AVP_S(".exe: error: "))) return NULL;
    char *copy = avp_xmalloc(out.n + 1);
    memcpy(copy, out.p, out.n);
    copy[out.n] = 0;
    return copy;
}

/* ---- parallel help runs ---------------------------------------------------------------------------------------- */
typedef struct {
    Help *h;
    const char *sub;
    const char *const *argv;
    size_t argc;
    char *result;
} Job;

static int job_run(void *ctx, size_t i) {
    Job *jobs = ctx;
    Job *j = &jobs[i];
    j->result = j->h->env->run(j->h->env->ctx, j->argv, j->argc, j->h->root);
    return 0;
}

/* Fetches the help texts of the (help, sub) pairs not yet cached, several at a time; the results land in the
 * caches as help_text would put them. */
static void prefetch(Help **helps, const char **subs, size_t n, int jobs_max) {
    AVP_VEC(Job) jobs = {0};
    for (size_t i = 0; i < n; i++) {
        Help *h = helps[i];
        HelpText *t = help_slot(h, subs[i]);
        if (t->done) continue;
        bool dup = false;
        for (size_t k = 0; k < jobs.n && !dup; k++)
            dup = jobs.v[k].h == h && ((!jobs.v[k].sub && !subs[i]) ||
                                       (jobs.v[k].sub && subs[i] && strcmp(jobs.v[k].sub, subs[i]) == 0));
        if (dup) continue;
        AvpStrVec argv = help_argv(h, subs[i]);
        Job j = {h, subs[i], (const char *const *)argv_cz(&argv), argv.n, NULL};
        avp_vec_push(&jobs, j);
    }
    if (!jobs.n) return;
    int threads = jobs_max < 1 ? 1 : jobs_max;
    if ((size_t)threads > jobs.n) threads = (int)jobs.n;
    avp_pool_for(NULL, threads, job_run, jobs.v, jobs.n);
    for (size_t k = 0; k < jobs.n; k++) {
        HelpText *t = help_slot(jobs.v[k].h, jobs.v[k].sub);
        t->text = jobs.v[k].result ? avp_strdup(A(), jobs.v[k].result) : NULL;
        t->done = true;
        free(jobs.v[k].result);
    }
    avp_vec_free(&jobs);
}

/* ---- checks ---------------------------------------------------------------------------------------------------- */
static bool flag_search(AvpStr flag, AvpStr text) {
    AvpStr esc = shsm_re_escape(A(), flag);
    return shsm_re_search(shsm_re_try(avp_sprintf(A(), "(?<![\\w-])%.*s(?![\\w-])", AVP_SF(esc)), 0), text, 0, AVP_END,
                          NULL);
}

static bool accepted(Help *h, AvpStr flag, const char *sub) {
    if (avp_set_has(help_flags(h, sub), flag)) return true;
    if (sub && avp_set_has(help_flags(h, NULL), flag)) return true;
    if (!help_argparse_style(h, sub)) {
        const char *ts = help_text(h, sub), *t0 = help_text(h, NULL);
        AvpStr both = avp_str_concat(A(), avp_str(ts ? ts : ""), avp_str(t0 ? t0 : ""));
        return flag_search(flag, both);
    }
    return false;
}

static AvpStr sorted_join(const AvpSet *s) {
    AvpStrVec v = avp_set_sorted(s);
    return avp_str_join(A(), AVP_S(", "), v.v, v.n);
}

static void check_args(CdReport *rep, i64 lineno, AvpStr verb, Help *h, const AvpStrVec *toks) {
    AvpStr name = h->program;
    AvpSet *subs = help_subcommands(h);
    const char *sub = NULL;
    if (toks->n && cd_is_word(toks->v[0]) && subs) {
        if (avp_set_has(subs, toks->v[0])) {
            sub = avp_cz(A(), toks->v[0]);
        } else if (!avp_str_startswith(h->base, AVP_S("ninja"))) {
            AvpStr has = sorted_join(subs);
            cd_report_add(rep, "verbs", CD_VERBS_DOC, lineno, false, "`%.*s` row: %.*s has no subcommand `%.*s` (has: %.*s)",
                          AVP_SF(verb), AVP_SF(name), AVP_SF(toks->v[0]), AVP_SF(has));
        }
    }
    if (sub && !help_text(h, sub)) {
        cd_report_add(rep, "verbs", CD_VERBS_DOC, lineno, false, "`%.*s` row: the help of `%.*s %s` cannot be read",
                      AVP_SF(verb), AVP_SF(name), sub);
        return;
    }
    for (size_t i = 0; i < toks->n; i++) {
        if (cd_is_flag(toks->v[i]) && !accepted(h, flag_name(toks->v[i]), sub)) {
            AvpStr f = flag_name(toks->v[i]);
            cd_report_add(rep, "verbs", CD_VERBS_DOC, lineno, false, "`%.*s` row: %.*s%s%s does not accept %.*s",
                          AVP_SF(verb), AVP_SF(name), sub ? " " : "", sub ? sub : "", AVP_SF(f));
        }
    }
}

/* A span that names no program: its words and flags are checked against the row's tools. */
static void check_orphan(CdReport *rep, i64 lineno, AvpStr verb, Help **helps, size_t nh, const AvpStrVec *toks) {
    if (!toks->n) return;
    for (size_t i = 0; i < nh; i++) {
        AvpSet *subs = help_subcommands(helps[i]);
        if (subs && avp_set_len(subs) && avp_set_has(subs, toks->v[0])) {
            check_args(rep, lineno, verb, helps[i], toks);
            return;
        }
    }
    AvpStrVec flags = {0};
    for (size_t i = 0; i < toks->n; i++)
        if (cd_is_flag(toks->v[i])) avp_vec_push(&flags, flag_name(toks->v[i]));
    if (cd_is_word(toks->v[0]) && toks->n > 1 &&
        (avp_str_startswith(toks->v[1], AVP_S("<")) || cd_is_flag(toks->v[1]))) {
        size_t with_subs = 0;
        Help *last = NULL;
        for (size_t i = 0; i < nh; i++) {
            AvpSet *s = help_subcommands(helps[i]);
            if (s && avp_set_len(s)) {
                with_subs++;
                last = helps[i];
            }
        }
        if (with_subs == 1 && nh == 1) {
            AvpStr has = sorted_join(help_subcommands(last));
            cd_report_add(rep, "verbs", CD_VERBS_DOC, lineno, false,
                          "`%.*s` row: `%.*s` reads as a subcommand, but %.*s has none such (has: %.*s)", AVP_SF(verb),
                          AVP_SF(toks->v[0]), AVP_SF(helps[0]->program), AVP_SF(has));
            return;
        }
    }
    if (!avp_str_startswith(toks->v[0], AVP_S("-")) && !flags.n) return;
    for (size_t f = 0; f < flags.n; f++) {
        bool ok = false;
        for (size_t i = 0; i < nh && !ok; i++) {
            Help *h = helps[i];
            if (accepted(h, flags.v[f], NULL)) {
                ok = true;
                break;
            }
            AvpSet *subs = help_subcommands(h);
            if (subs) {
                size_t pos = 0;
                AvpStr s;
                while (avp_set_next(subs, &pos, &s))
                    if (accepted(h, flags.v[f], avp_cz(A(), s))) {
                        ok = true;
                        break;
                    }
            }
        }
        if (!ok) {
            AvpStrVec names = {0};
            for (size_t i = 0; i < nh; i++) avp_vec_push(&names, helps[i]->program);
            AvpStr joined = avp_str_join(A(), AVP_S(", "), names.v, names.n);
            cd_report_add(rep, "verbs", CD_VERBS_DOC, lineno, false, "`%.*s` row: no tool of the row (%.*s) accepts %.*s",
                          AVP_SF(verb), AVP_SF(joined), AVP_SF(flags.v[f]));
        }
    }
}

/* ---- the project's tools, and where the documents name them ---------------------------------------------------- */
/* The executables the tree builds: each tools/c/tools/<name>/ with a main.c gives build/tools/<name>.exe. */
static AvpStrVec project_tools(const char *root) {
    AvpStrVec out = {0};
    const char *dir = avp_join(A(), root, "tools", "c", "tools");
    AvpDirEntries ents = {0};
    if (!avp_isdir(dir) || !avp_scandir(A(), dir, &ents)) return out;
    for (size_t i = 0; i < ents.n; i++) {
        AvpStr name = ents.v[i].name;
        if (!ents.v[i].is_dir || avp_str_startswith(name, AVP_S("."))) continue;
        if (avp_isfile(avp_join(A(), dir, avp_cz(A(), name), "main.c"))) avp_vec_push(&out, name);
    }
    avp_sort_strs(out.v, out.n);
    return out;
}

typedef struct {
    AvpStr doc;
    i64 line;
} Hit;
typedef AVP_VEC(Hit) Hits;

static Hits *tool_mentions(const char *root, const AvpStrVec *tools) {
    size_t n = tools->n;
    const ShsmRe **pats = avp_alloc(A(), sizeof(*pats) * (n ? n : 1));
    Hits *hits = avp_alloc(A(), sizeof(Hits) * (n ? n : 1));
    for (size_t i = 0; i < n; i++) {
        AvpStr e = shsm_re_escape(A(), tools->v[i]);
        pats[i] = shsm_re_try(avp_sprintf(A(), "(?<![\\w.-])%.*s\\.exe(?![\\w-])", AVP_SF(e)), 0);
    }
    AvpStrVec docs = cd_doc_files(A(), root);
    for (size_t d = 0; d < docs.n; d++) {
        AvpStrVec lines = cd_read_lines(A(), avp_path_join(A(), root, avp_cz(A(), docs.v[d])));
        for (size_t k = 0; k < lines.n; k++) {
            AvpStr line = lines.v[k];
            if (!avp_str_contains(line, AVP_S(".exe"))) continue;
            shsm_re_assume_valid(line);
            for (size_t i = 0; i < n; i++)
                if (shsm_re_search(pats[i], line, 0, AVP_END, NULL)) {
                    Hit h = {docs.v[d], (i64)k + 1};
                    avp_vec_push(&hits[i], h);
                }
        }
    }
    return hits;
}

/* ---- check_verbs ----------------------------------------------------------------------------------------------- */
typedef struct {
    AvpStr span;
    CdInvocations inv;
} Span;
typedef AVP_VEC(Span) Spans;

static bool vec_has(const AvpStrVec *v, AvpStr s) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_eq(v->v[i], s)) return true;
    return false;
}

static bool checked(CdInvKind k) { return k == CD_INV_TOOL || k == CD_INV_BIN; }

void cd_check_verbs(const char *root, CdReport *rep, const CdHelpEnv *env) {
    CdRows rows = cd_adapter_rows(A(), root);
    if (!rows.n) {
        cd_report_add(rep, "verbs", CD_VERBS_DOC, 0, false, "no \"Project adapter\" table found");
        return;
    }
    AvpMap *helps = avp_map_new(); /* program -> Help* */
    AVP_VEC(Help *) order = {0};   /* the helps in order of first appearance */
    Spans *parsed = avp_alloc(A(), sizeof(Spans) * rows.n);
    for (size_t r = 0; r < rows.n; r++) {
        AvpStrVec spans = cd_code_spans(A(), rows.v[r].cell);
        for (size_t i = 0; i < spans.n; i++) {
            Span sp = {spans.v[i], cd_parse_invocations(A(), spans.v[i])};
            for (size_t k = 0; k < sp.inv.n; k++) {
                if (!checked(sp.inv.v[k].kind)) continue;
                bool created;
                AvpVal *v = avp_map_put(helps, sp.inv.v[k].program, &created);
                if (created) {
                    v->p = help_new(root, &sp.inv.v[k], env);
                    avp_vec_push(&order, (Help *)v->p);
                }
            }
            avp_vec_push(&parsed[r], sp);
        }
    }

    /* every tool's own help first, then the help of each subcommand */
    {
        AVP_VEC(Help *) hs = {0};
        AVP_VEC(const char *) ss = {0};
        for (size_t i = 0; i < order.n; i++)
            if (order.v[i]->exe) {
                avp_vec_push(&hs, order.v[i]);
                avp_vec_push(&ss, (const char *)NULL);
            }
        prefetch(hs.v, ss.v, hs.n, env->jobs);
        hs.n = ss.n = 0;
        for (size_t i = 0; i < order.n; i++) {
            Help *h = order.v[i];
            if (!h->exe) continue;
            AvpSet *subs = help_subcommands(h);
            if (!subs) continue;
            AvpStrVec sorted = avp_set_sorted(subs);
            for (size_t k = 0; k < sorted.n; k++) {
                avp_vec_push(&hs, h);
                avp_vec_push(&ss, (const char *)avp_cz(A(), sorted.v[k]));
            }
        }
        prefetch(hs.v, ss.v, hs.n, env->jobs);
        avp_vec_free(&hs);
        avp_vec_free(&ss);
    }

    AvpSet *unreadable = avp_set_new();
    AvpSet *invoked = avp_set_new();
    for (size_t r = 0; r < rows.n; r++) {
        i64 lineno = rows.v[r].lineno;
        AvpStr verb = rows.v[r].verb;
        AvpStrVec row_tools = {0}, row_python = {0};
        for (size_t i = 0; i < parsed[r].n; i++)
            for (size_t k = 0; k < parsed[r].v[i].inv.n; k++) {
                const CdInvocation *inv = &parsed[r].v[i].inv.v[k];
                AvpStrVec *into = checked(inv->kind) ? &row_tools : inv->kind == CD_INV_PYTHON ? &row_python : NULL;
                if (into && !vec_has(into, inv->program)) avp_vec_push(into, inv->program);
            }
        for (size_t i = 0; i < row_python.n; i++)
            cd_report_add(rep, "verbs", CD_VERBS_DOC, lineno, false,
                          "`%.*s` row: `%.*s` names a Python tool; the commands are build/tools/<name>.exe",
                          AVP_SF(verb), AVP_SF(row_python.v[i]));
        for (size_t i = 0; i < row_tools.n; i++) avp_set_add(invoked, row_tools.v[i]);
        AVP_VEC(Help *) live = {0};
        for (size_t i = 0; i < row_tools.n; i++) {
            AvpStr s = row_tools.v[i];
            Help *h = avp_map_get(helps, s)->p;
            if (!h->exe) {
                if (h->kind == CD_INV_TOOL)
                    cd_report_add(rep, "verbs", CD_VERBS_DOC, lineno, false,
                                  "`%.*s` row: %.*s does not exist beside check_docs.exe or in build/tools ("
                                  CD_BUILD_HINT ")",
                                  AVP_SF(verb), AVP_SF(s));
                else
                    cd_report_add(rep, "verbs", CD_VERBS_DOC, lineno, false, "`%.*s` row: %.*s does not exist",
                                  AVP_SF(verb), AVP_SF(s));
                continue;
            }
            if (!help_text(h, NULL)) {
                if (!avp_set_has(unreadable, s)) {
                    avp_set_add(unreadable, s);
                    AvpStrVec argv = help_argv(h, NULL);
                    AvpStr rest = avp_str_join(A(), AVP_S(" "), argv.v + 1, argv.n - 1);
                    cd_report_add(rep, "verbs", CD_VERBS_DOC, lineno, false,
                                  "`%.*s` row: the help of %.*s cannot be read (%.*s %.*s); its flags are unverified",
                                  AVP_SF(verb), AVP_SF(s), AVP_SF(s), AVP_SF(rest));
                }
            } else {
                avp_vec_push(&live, h);
            }
        }
        for (size_t i = 0; i < parsed[r].n; i++) {
            Span *sp = &parsed[r].v[i];
            if (sp->inv.n) {
                for (size_t k = 0; k < sp->inv.n; k++) {
                    for (size_t l = 0; l < live.n; l++)
                        if (avp_str_eq(live.v[l]->program, sp->inv.v[k].program)) {
                            AvpStrVec toks = cd_command_tokens(A(), sp->inv.v[k].args);
                            check_args(rep, lineno, verb, live.v[l], &toks);
                            break;
                        }
                }
            } else if (live.n && !row_python.n) { /* a row naming a Python tool: whose words? */
                AvpStrVec toks = cd_command_tokens(A(), sp->span);
                check_orphan(rep, lineno, verb, live.v, live.n, &toks);
            }
        }
        avp_vec_free(&live);
    }
    avp_vec_free(&order);

    AvpStrVec tools = project_tools(root);
    Hits *mentions = tool_mentions(root, &tools);
    for (size_t i = 0; i < tools.n; i++) {
        AvpStr exe = avp_str(avp_sprintf(A(), "build/tools/%.*s.exe", AVP_SF(tools.v[i])));
        if (avp_set_has(invoked, exe)) continue;
        Hits *where = &mentions[i];
        const char *file = avp_sprintf(A(), "tools/c/tools/%.*s/main.c", AVP_SF(tools.v[i]));
        if (where->n) {
            AvpBuf b;
            avp_buf_init(&b, A());
            for (size_t k = 0; k < where->n && k < 4; k++) {
                if (k) avp_buf_putz(&b, ", ");
                avp_buf_printf(&b, "%.*s:%lld", AVP_SF(where->v[k].doc), (long long)where->v[k].line);
            }
            if (where->n > 4) avp_buf_putz(&b, " ...");
            cd_report_add(rep, "verbs", file, 1, true, "%.*s is not in the verb table; mentioned in %zu doc line(s): %s",
                          AVP_SF(exe), where->n, avp_buf_cz(&b));
        } else {
            cd_report_add(rep, "verbs", file, 1, true, "%.*s is not in the verb table and mentioned in no document",
                          AVP_SF(exe));
        }
    }
}
