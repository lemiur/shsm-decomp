/* check_docs paths: every repository path written in the documents exists (check_paths and its helpers). */
#include "check_docs.h"
#include "shsm_cli.h"
#include "shsm_re.h"

static const char *const PATH_ROOTS[] = {"tools", "config", "docs", "include", "src"};
#define NROOTS (sizeof PATH_ROOTS / sizeof PATH_ROOTS[0])
#define PATH_RE "(?<![\\w./<>:@-])((?:tools|config|docs|include|src)/[^\\s`'\"(),;|\\[\\]]*)"
#define FOREIGN_RE "^[\\w.-]+/[\\w.-]+$"
#define PLACEHOLDER_SEG "^(?:X|N|NN|F|Foo|Bar|U|V|slotN|ver|unit|path)(?:\\.[\\w+]+)?$"
#define OWNER_REPO_RE \
    "(?<![\\w/.-])(?!(?:tools|config|docs|include|src|build|orig|ref|Wii|GC)/)[A-Za-z0-9][\\w-]*/[A-Za-z][\\w.-]*"
#define HISTORY_RE "\\b(?:removed|deleted|renamed|no longer|was dropped|formerly)\\b"

static AvpArena *A(void) { return avp_arena(); }

static bool is_root(AvpStr s, bool with_build_orig) {
    for (size_t i = 0; i < NROOTS; i++)
        if (avp_str_eqz(s, PATH_ROOTS[i])) return true;
    return with_build_orig && (avp_str_eqz(s, "build") || avp_str_eqz(s, "orig"));
}

static AvpStr first_seg(AvpStr s) {
    AvpStr before, after;
    avp_str_partition(s, AVP_S("/"), &before, &after);
    return before;
}

const char *cd_clean_path(AvpArena *a, AvpStr token) {
    AvpStr t = avp_str_rstrip_chars(token, AVP_S(".,:;!?'\""));
    t = shsm_re_sub(a, shsm_re("::.*$", 0), AVP_S(""), t, 0);
    t = shsm_re_sub(a, shsm_re(":\\d+(?:[-:]\\d+)?$", 0), AVP_S(""), t, 0);
    t = avp_str_rstrip_chars(t, AVP_S(".,:;"));
    if (!t.n || !is_root(first_seg(t), false) || avp_str_findc(t, '/') < 0) return NULL;
    return avp_cz(a, t);
}

bool cd_is_placeholder(AvpStr seg) {
    if (shsm_re_search(shsm_re("[<>*?{}$]|\\.\\.\\.|\xE2\x80\xA6", 0), seg, 0, AVP_END, NULL)) return true;
    return shsm_re_match(shsm_re(PLACEHOLDER_SEG, 0), seg, 0, AVP_END, NULL) ||
           shsm_re_match(shsm_re("^[A-Z]{1,2}$", 0), seg, 0, AVP_END, NULL);
}

AvpStrVec cd_path_tokens(AvpArena *a, AvpStr span) {
    (void)a;
    AvpStrVec out = {0};
    AvpStrVec words = avp_str_split(span, AVP_S(""), -1);
    for (size_t i = 0; i < words.n; i++) {
        ShsmReIter it;
        ShsmReMatch m;
        shsm_re_iter_init(&it, shsm_re(PATH_RE, 0), words.v[i], 0, AVP_END);
        while (shsm_re_iter_next(&it, &m)) {
            AvpStr prev = i ? words.v[i - 1] : AVP_S("");
            if (shsm_re_start(&m, 0) == 0 && shsm_re_match(shsm_re(FOREIGN_RE, 0), prev, 0, AVP_END, NULL) &&
                !is_root(first_seg(prev), true))
                continue;
            avp_vec_push(&out, shsm_re_group(&m, 1));
        }
    }
    return out;
}

/* ---- PathChecker: exact-case existence, one listing per folder ------------------------------------------------- */
typedef struct {
    const char *root;
    AvpMap *listing; /* folder -> AvpSet* (NULL: cannot be listed) */
} PathChecker;

static AvpSet *entries(PathChecker *pc, const char *d) {
    bool created;
    AvpVal *v = avp_map_put(pc->listing, avp_str(d), &created);
    if (created) {
        AvpStrVec names = {0};
        if (avp_listdir(A(), d, &names)) {
            AvpSet *s = avp_set_new();
            for (size_t i = 0; i < names.n; i++) avp_set_add(s, names.v[i]);
            v = avp_map_get(pc->listing, avp_str(d));
            v->p = s;
        } else {
            v = avp_map_get(pc->listing, avp_str(d));
            v->p = NULL;
        }
    }
    return v->p;
}

static bool pc_exists(PathChecker *pc, AvpStr rel) {
    const char *cur = pc->root;
    AvpStrVec segs = avp_str_split(rel, AVP_S("/"), -1);
    for (size_t i = 0; i < segs.n; i++) {
        AvpStr seg = segs.v[i];
        if (!seg.n || avp_str_eqz(seg, ".")) continue;
        if (avp_str_eqz(seg, "..")) {
            cur = avp_cz(A(), avp_path_dirname(cur));
            continue;
        }
        AvpSet *ent = entries(pc, cur);
        if (!ent || !avp_set_has(ent, seg)) return false;
        cur = avp_path_join(A(), cur, avp_cz(A(), seg));
    }
    return true;
}

static const char *pc_check(PathChecker *pc, AvpStr token) {
    const char *rel = cd_clean_path(A(), token);
    if (!rel) return NULL;
    AvpStr r = avp_str(rel);
    AvpStrVec segs = avp_str_split(avp_str_rstrip_chars(r, AVP_S("/")), AVP_S("/"), -1);
    AvpStrVec fixed = {0};
    for (size_t i = 0; i < segs.n; i++) {
        if (cd_is_placeholder(segs.v[i])) {
            AvpStr prefix = avp_str_join(A(), AVP_S("/"), fixed.v, fixed.n);
            if (pc_exists(pc, prefix)) return NULL;
            return avp_sprintf(A(), "%s: %.*s does not exist (before the placeholder)", rel, AVP_SF(prefix));
        }
        avp_vec_push(&fixed, segs.v[i]);
    }
    if (pc_exists(pc, r)) return NULL;
    return avp_sprintf(A(), "%s does not exist", rel);
}

/* ---- other projects' trees ------------------------------------------------------------------------------------- */
static int by_len_desc(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const AvpStr *a = x, *b = y;
    return a->n < b->n ? 1 : a->n > b->n ? -1 : 0;
}

/* foreign_projects(root): NULL when docs/references.md names none. Python orders the alternatives by length over a
 * set (hash order for equal lengths); here equal lengths keep the order of first appearance (D-check_docs-3). */
static const ShsmRe *foreign_projects(const char *root) {
    const char *p = avp_path_join(A(), root, "docs\\references.md");
    if (!avp_isfile(p)) return NULL;
    AvpSet *names = avp_set_new();
    AvpStrVec lines = cd_read_lines(A(), p);
    for (size_t i = 0; i < lines.n; i++) {
        ShsmReMatch m;
        if (!shsm_re_match(shsm_re("^\\|\\s*`?([A-Za-z0-9_.-]+)/([A-Za-z0-9_.-]+)", 0), lines.v[i], 0, AVP_END, &m))
            continue;
        AvpStr owner = shsm_re_group(&m, 1), repo = shsm_re_group(&m, 2);
        AvpStr both = avp_str(avp_sprintf(A(), "%.*s/%.*s", AVP_SF(owner), AVP_SF(repo)));
        avp_set_add(names, shsm_re_escape(A(), both));
        AvpStr eo = shsm_re_escape(A(), owner);
        avp_set_add(names, avp_str(avp_sprintf(A(), "%.*s/[\\w.-]+", AVP_SF(eo))));
        AvpStr shrt = repo;
        if (avp_u8_len(shrt) >= 4 && !avp_str_eqz(shrt, "Decomp") && !avp_str_eqz(shrt, "key") &&
            !avp_str_eqz(shrt, "forecast")) {
            avp_set_add(names, shsm_re_escape(A(), shrt));
            AvpStr head, tail;
            if (avp_str_partition(shrt, AVP_S("-"), &head, &tail) && avp_u8_len(head) >= 4)
                avp_set_add(names, shsm_re_escape(A(), head));
        }
    }
    if (!avp_set_len(names)) return NULL;
    AvpStrVec v = {0};
    size_t pos = 0;
    AvpStr s;
    while (avp_set_next(names, &pos, &s)) avp_vec_push(&v, s);
    avp_sort(v.v, v.n, sizeof(AvpStr), by_len_desc, NULL, false);
    AvpStr alt = avp_str_join(A(), AVP_S("|"), v.v, v.n);
    return shsm_re_try(avp_sprintf(A(), "(?<![\\w/-])(?:%.*s)(?![\\w-])", AVP_SF(alt)), 0);
}

void cd_check_paths(const char *root, CdReport *rep) {
    PathChecker pc = {root, avp_map_new()};
    const ShsmRe *foreign = foreign_projects(root);
    AvpStrVec docs = cd_doc_files(A(), root);
    for (size_t d = 0; d < docs.n; d++) {
        const char *doc = avp_cz(A(), docs.v[d]);
        AvpStrVec lines = cd_read_lines(A(), avp_path_join(A(), root, doc));
        bool fence = false;
        AvpBuf para;
        avp_buf_init(&para, A());
        avp_buf_putz(&para, "");
        for (size_t n = 0; n < lines.n; n++) {
            AvpStr line = lines.v[n];
            shsm_re_assume_valid(line);
            if (avp_str_startswith(avp_str_lstrip(line), AVP_S("```"))) {
                fence = !fence;
                para.n = 0;
                continue;
            }
            if (!avp_str_strip(line).n ||
                shsm_re_match(shsm_re("^\\s*(?:[-*] |\\d+\\. |#|\\|)", 0), line, 0, AVP_END, NULL))
                para.n = 0;
            avp_buf_putc(&para, ' ');
            avp_buf_put(&para, line);
            if (avp_str_findc(line, '/') < 0) continue;
            AvpStrVec spans = {0};
            if (fence)
                avp_vec_push(&spans, line);
            else
                spans = cd_code_spans(A(), line);
            AvpSet *seen = avp_set_new();
            for (size_t i = 0; i < spans.n; i++) {
                AvpStrVec toks = cd_path_tokens(A(), spans.v[i]);
                for (size_t k = 0; k < toks.n; k++) {
                    const char *msg = pc_check(&pc, toks.v[k]);
                    if (!msg || avp_set_has(seen, avp_str(msg))) continue;
                    avp_set_add(seen, avp_str(msg));
                    AvpStr ptext = avp_strn(para.p, para.n);
                    ShsmReMatch fm;
                    bool found = foreign && shsm_re_search(foreign, ptext, 0, AVP_END, &fm);
                    if (!found && strcmp(doc, "docs/references.md") == 0) {
                        AvpStr bare = shsm_re_sub(A(), shsm_re("`[^`]*`", 0), AVP_S(" "), ptext, 0);
                        found = shsm_re_search(shsm_re(OWNER_REPO_RE, 0), bare, 0, AVP_END, &fm);
                    }
                    if (found) {
                        AvpStr g = shsm_re_group(&fm, 0);
                        cd_report_add(rep, "paths", doc, (i64)n + 1, true, "%s (the line names %.*s: another project's tree?)",
                                      msg, AVP_SF(g));
                    } else if (shsm_re_search(shsm_re(HISTORY_RE, SHSM_RE_I), ptext, 0, AVP_END, NULL)) {
                        cd_report_add(rep, "paths", doc, (i64)n + 1, true, "%s (the line describes a removal or rename)", msg);
                    } else {
                        cd_report_add(rep, "paths", doc, (i64)n + 1, false, "%s", msg);
                    }
                }
            }
            avp_set_free(seen);
        }
    }
}
