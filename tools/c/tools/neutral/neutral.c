/* neutral: the pure helpers of tools/neutral.py (parse_depfile, norm, remap_argv, key_argv, rel_dep, include_dirs,
 * reach) and tools/common/cli.py's compiler_messages / first_error, which the compile errors are reported with. */
#include "neutral.h"
#include "shsm_re.h"

static const char *const TREE_DIRS[2] = {"include", "src"};

static bool in_tree_dirs(AvpStr first) {
    for (int i = 0; i < 2; i++)
        if (avp_str_eqz(first, TREE_DIRS[i])) return true;
    return false;
}

/* path.split("/")[0] */
static AvpStr first_component(AvpStr p) {
    AvpStr before, after;
    if (avp_str_partition(p, AVP_S("/"), &before, &after)) return before;
    return p;
}

static AvpStr slashes(AvpArena *a, AvpStr s) { return avp_str_replace(a, s, AVP_S("\\"), AVP_S("/"), -1); }

/* ---- depfiles ------------------------------------------------------------------------------------------------- */

AvpStrVec neutral_parse_depfile_text(AvpArena *a, AvpStr text) {
    AvpStrVec lines = avp_str_splitlines(text, false), out = {0};
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr s = avp_str_strip(lines.v[i]);
        if (avp_str_endswith(s, AVP_S("\\"))) s = avp_str_strip(avp_str_slice(s, 0, (i64)s.n - 1));
        if (i == 0) {
            AvpStr before, after;
            s = avp_str_partition(s, AVP_S(": "), &before, &after) ? avp_str_strip(after) : AVP_S("");
        }
        if (s.n) avp_vec_push(&out, avp_str_replace(a, s, AVP_S("\\ "), AVP_S(" "), -1));
    }
    avp_vec_free(&lines);
    return out;
}

AvpStrVec neutral_parse_depfile(AvpArena *a, const char *path) {
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8_REPLACE, &text)) shsm_raise_io();
    return neutral_parse_depfile_text(a, text);
}

/* ---- paths and argv --------------------------------------------------------------------------------------------- */

const char *neutral_norm(AvpArena *a, const char *p) {
    return avp_cz(a, slashes(a, avp_str(avp_path_normcase(a, avp_path_abspath(a, p)))));
}

AvpStrVec neutral_remap_argv(AvpArena *a, const AvpStrVec *argv, const char *root, const char *old_tree,
                             const char *source_rel) {
    AvpStrVec out = {0};
    for (size_t i = 0; i < argv->n; i++) avp_vec_push(&out, argv->v[i]);
    for (size_t i = 0; i + 1 < out.n; i++) {
        if (!avp_str_eqz(out.v[i], "-i")) continue;
        AvpStr d = slashes(a, out.v[i + 1]);
        const char *dz = avp_cz(a, d);
        if (!avp_path_isabs(dz) && in_tree_dirs(first_component(d)))
            out.v[i + 1] = avp_str(avp_path_join(a, old_tree, dz));
    }
    const char *src_abs = neutral_norm(a, avp_path_join(a, root, source_rel));
    for (size_t i = 0; i < out.n; i++) {
        const char *w = avp_cz(a, out.v[i]);
        if ((avp_path_isabs(w) || strcmp(w, source_rel) == 0) &&
            strcmp(neutral_norm(a, avp_path_join(a, root, w)), src_abs) == 0)
            out.v[i] = avp_str(avp_path_join(a, old_tree, source_rel));
    }
    return out;
}

AvpStrVec neutral_key_argv(AvpArena *a, const AvpStrVec *argv, const char *root, const char *old_tree,
                           const char *outdir) {
    struct {
        AvpStr p;
        const char *tag;
    } reps[3] = {{avp_str(neutral_norm(a, outdir)), "<OUT>"},
                 {avp_str(neutral_norm(a, old_tree)), "<OLD>"},
                 {avp_str(neutral_norm(a, root)), "<ROOT>"}};
    for (int k = 0; k < 3; k++) reps[k].p = avp_str_lower(a, reps[k].p);
    AvpStrVec res = {0};
    for (size_t i = 0; i < argv->n; i++) {
        AvpStr n = slashes(a, argv->v[i]);
        AvpStr nl = avp_str_lower(a, n);
        for (int k = 0; k < 3; k++) {
            if (avp_str_startswith(nl, reps[k].p)) {
                n = avp_str_concat(a, avp_str(reps[k].tag), avp_str_slice(n, (i64)reps[k].p.n, (i64)n.n));
                break;
            }
        }
        avp_vec_push(&res, n);
    }
    return res;
}

const char *neutral_rel_dep(AvpArena *a, const char *path, const char *root, const char *old_tree) {
    AvpStr n = slashes(a, avp_str(avp_path_abspath(a, avp_path_join(a, root, path))));
    AvpStr nl = avp_str_lower(a, n);
    const char *bases[2] = {old_tree, root};
    for (int k = 0; k < 2; k++) {
        AvpStr b = slashes(a, avp_str(avp_path_abspath(a, bases[k])));
        AvpStr bl = avp_str_concat(a, avp_str_lower(a, b), AVP_S("/"));
        if (avp_str_startswith(nl, bl)) return avp_cz(a, avp_str_slice(n, (i64)b.n + 1, (i64)n.n));
    }
    return avp_cz(a, n);
}

AvpStrVec neutral_include_dirs(AvpArena *a, const ShsmUnit *u) {
    AvpStrVec out = {0};
    for (size_t i = 0; i + 1 < u->cflags.n; i++)
        if (avp_str_eqz(u->cflags.v[i], "-i"))
            avp_vec_push(&out, avp_str_rstrip_chars(slashes(a, u->cflags.v[i + 1]), AVP_S("/")));
    return out;
}

const char *neutral_safe_name(AvpArena *a, const char *path) {
    char *s = avp_strdup(a, path);
    for (char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
                  c == '.' || c == '-';
        if (!ok) *p = '_';
    }
    /* a non-ASCII code point is one "_" in Python: collapse the continuation bytes */
    char *w = s;
    for (const char *p = path, *q = s; *p; p++, q++) {
        if (((unsigned char)*p & 0xC0) == 0x80) continue;
        *w++ = *q;
    }
    *w = 0;
    return s;
}

/* ---- reach -------------------------------------------------------------------------------------------------------- */

AvpMap *neutral_reach(AvpArena *a, ShsmUnit *const *units, size_t nunits, const AvpStr *files, size_t nfiles,
                      const AvpMap *deps, const char *root, const char **note) {
    AvpSet *fset = avp_set_new(), *lower = avp_set_new();
    AvpStrVec fl = {0};
    for (size_t i = 0; i < nfiles; i++) {
        AvpStr f = slashes(a, files[i]);
        if (avp_set_add(fset, f)) avp_vec_push(&fl, f);
        avp_set_add(lower, avp_str_lower(a, f));
    }
    AvpMap *out = avp_map_new();
    bool cr = false;
    for (size_t k = 0; k < nunits; k++) {
        const ShsmUnit *u = units[k];
        if (!shsm_unit_has_source(u) || shsm_unit_is_asm(u)) continue; /* assembled by as, not MWCC */
        AvpStr name = avp_str(u->name);
        if (avp_set_has(fset, slashes(a, avp_str(u->source)))) {
            avp_map_put(out, name, &cr)->p = "source changed";
            continue;
        }
        if (deps) {
            const char *obj = "";
            if (u->base_obj && avp_path_isabs(u->base_obj)) {
                char *rel = NULL;
                obj = avp_path_relpath(a, u->base_obj, root, &rel) ? rel : u->base_obj;
            } else if (u->base_obj) {
                obj = u->base_obj;
            }
            AvpVal *v = avp_map_get(deps, slashes(a, avp_str(obj)));
            const AvpStrVec *dl = v ? v->p : NULL;
            for (size_t i = 0; dl && i < dl->n; i++) {
                if (avp_set_has(lower, avp_str_lower(a, dl->v[i]))) {
                    avp_map_put(out, name, &cr)->p = avp_sprintf(a, "includes %.*s", AVP_SF(dl->v[i]));
                    break;
                }
            }
        } else {
            AvpStrVec inc = neutral_include_dirs(a, u);
            bool hit = false;
            for (size_t i = 0; i < fl.n && !hit; i++)
                for (size_t j = 0; j < inc.n && !hit; j++)
                    if (avp_str_startswith(fl.v[i], avp_str_concat(a, inc.v[j], AVP_S("/")))) {
                        avp_map_put(out, name, &cr)->p = avp_sprintf(a, "include path holds %.*s", AVP_SF(fl.v[i]));
                        hit = true;
                    }
            avp_vec_free(&inc);
        }
    }
    *note = deps ? "ninja dependency records"
                 : "no ninja dependency records (no build): every unit whose -i directories contain a changed file";
    avp_vec_free(&fl);
    avp_set_free(fset);
    avp_set_free(lower);
    return out;
}

/* ---- compiler messages (cli.compiler_messages, cli.first_error) --------------------------------------------------- */

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

AvpStr neutral_first_error(AvpArena *a, AvpStr output) {
    ShsmRe *head = shsm_re("^### (\\S+) ([^:]+):\\s*$", 0);
    ShsmRe *file = shsm_re("^#\\s+File:\\s*(.+?)\\s*$", 0);
    ShsmRe *linere = shsm_re("^#\\s+(\\d+):\\s?(.*)$", 0);
    ShsmRe *kindre = shsm_re("^#\\s+(Error|Warning):", 0);
    ShsmRe *msgre = shsm_re("^#\\s+(?:\\((\\d+)\\)\\s*)?(.*?)\\s*$", 0);
    AVP_VEC(MwMessage) msgs = {0};
    MwMessage *cur = NULL;
    size_t curi = 0;
    bool marked = false;
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
            curi = msgs.n - 1;
            cur = &msgs.v[curi];
            continue;
        }
        if (!cur || !avp_str_startswith(line, AVP_S("#")) || cur->message.p) continue;
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
        AvpStr where = mm->file.p ? avp_path_basename(avp_cz(a, slashes(a, mm->file))) : mm->tool;
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

/* Both side directories are removed; the old side's is created again. An existing directory is how -MMD's -o is told
 * from a file name when the directory name has a dot (Runtime.PPCEABI.H___mem), so it exists before its command line
 * is built. */
void neutral_prepare_dirs(const char *new_dir, const char *old_dir) {
    const char *dirs[2] = {new_dir, old_dir};
    for (int k = 0; k < 2; k++)
        if (avp_isdir(dirs[k]) && !avp_rmtree(dirs[k], 0)) shsm_raise_io();
    if (!avp_makedirs(old_dir, true)) shsm_raise_io();
}
