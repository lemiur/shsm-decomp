/* flagsweep: see flagsweep.h. Function names follow tools/flagsweep.py. */
#include "flagsweep.h"

#include <ctype.h>

#include "permute.h"
#include "shsm_text.h"

static const char *const ARG_FLAGS[] = {
    "-fp_contract", "-use_lmw_stmw", "-inline", "-str", "-pool", "-schedule", "-func_align", "-ipa", "-char",
    "-Cpp_exceptions", "-RTTI", "-opt", "-fp", "-align", "-enum", "-proc", "-pragma", "-i", "-I", "-sdatathreshold",
    "-sdata2threshold", "-w", "-maxerrors", "-cwd", "-lang", "-prefix", "-include", "-requireprotos", "-exceptions",
    NULL};
static const char *const KEYED_FLAGS[] = {"-pragma", "-i", "-I", "-include", "-prefix", NULL};

static bool in_list(AvpStr s, const char *const *list)
{
    for (size_t k = 0; list[k]; k++)
        if (avp_str_eqz(s, list[k])) return true;
    return false;
}

static bool strs_has(const AvpStrVec *v, AvpStr s)
{
    for (size_t k = 0; k < v->n; k++)
        if (avp_str_eq(v->v[k], s)) return true;
    return false;
}

/* ---- flag handling ---------------------------------------------------------------------------------------------- */

FsFlags fs_parse_flags(const AvpStr *tokens, size_t n)
{
    FsFlags out = {0};
    for (size_t i = 0; i < n;) {
        AvpStr t = tokens[i];
        if (in_list(t, ARG_FLAGS) && i + 1 < n &&
            (!avp_str_startswith(tokens[i + 1], AVP_S("-")) || in_list(t, KEYED_FLAGS))) {
            avp_vec_push(&out, ((FsFlag){t, tokens[i + 1]}));
            i += 2;
        } else {
            avp_vec_push(&out, ((FsFlag){t, {0}}));
            i++;
        }
    }
    return out;
}

FsFlags fs_parse_spec(AvpArena *a, AvpStr text)
{
    FsFlags none = {0};
    if (!avp_str_strip(text).n) return none;
    AvpStrVec words = {0};
    const char *err = NULL;
    if (!shsm_shlex_split(a, text, &words, &err)) avp_throw(AVP_ERR_OTHER, "ValueError: %s", err);
    return fs_parse_flags(words.v, words.n);
}

AvpStrVec fs_flag_tokens(const FsFlags *flags)
{
    AvpStrVec out = {0};
    for (size_t k = 0; k < flags->n; k++) {
        avp_vec_push(&out, flags->v[k].name);
        if (flags->v[k].arg.p) avp_vec_push(&out, flags->v[k].arg);
    }
    return out;
}

AvpStr fs_flag_str(AvpArena *a, FsFlag f)
{
    AvpStr q = permute_shlex_quote(a, f.name);
    if (!f.arg.p) return q;
    return avp_str(avp_sprintf(a, "%.*s %.*s", AVP_SF(q), AVP_SF(permute_shlex_quote(a, f.arg))));
}

AvpStr fs_family(AvpArena *a, FsFlag f)
{
    AvpStr n = f.name;
    if (avp_str_startswith(n, AVP_S("-O"))) return AVP_S("-O");
    if (in_list(n, KEYED_FLAGS) || (avp_str_startswith(n, AVP_S("-D")) && n.n > 2))
        return avp_str(avp_sprintf(a, "%.*s %.*s", AVP_SF(n), AVP_SF(f.arg)));
    i64 eq = avp_str_findc(n, '=');
    if (eq >= 0) return avp_strn(n.p, (size_t)eq);
    return n;
}

FsFlags fs_apply_axis(AvpArena *a, const FsFlags *flags, const FsFlags *alt, const AvpStrVec *families)
{
    FsFlags out = {0};
    if (!alt->n) {
        for (size_t k = 0; k < flags->n; k++)
            if (!strs_has(families, fs_family(a, flags->v[k]))) avp_vec_push(&out, flags->v[k]);
        return out;
    }
    /* last[family] = index of its last flag */
    AvpMap *last = avp_map_new();
    for (size_t k = 0; k < flags->n; k++) {
        AvpStr fam = fs_family(a, flags->v[k]);
        if (strs_has(families, fam)) avp_map_put(last, fam, NULL)->i = (i64)k;
    }
    if (!avp_map_len(last)) {
        avp_map_free(last);
        for (size_t k = 0; k < flags->n; k++) avp_vec_push(&out, flags->v[k]);
        for (size_t k = 0; k < alt->n; k++) avp_vec_push(&out, alt->v[k]);
        return out;
    }
    bool *drop = avp_alloc(a, flags->n + 1);
    i64 pos = -1;
    AvpMapIter it = {0};
    while (avp_map_next(last, &it)) {
        drop[it.val->i] = true;
        if (it.val->i > pos) pos = it.val->i;
    }
    avp_map_free(last);
    size_t kept_before = 0;
    for (i64 k = 0; k < pos; k++) kept_before += !drop[k];
    FsFlags kept = {0};
    for (size_t k = 0; k < flags->n; k++)
        if (!drop[k]) avp_vec_push(&kept, flags->v[k]);
    for (size_t k = 0; k < kept_before; k++) avp_vec_push(&out, kept.v[k]);
    for (size_t k = 0; k < alt->n; k++) avp_vec_push(&out, alt->v[k]);
    for (size_t k = kept_before; k < kept.n; k++) avp_vec_push(&out, kept.v[k]);
    avp_vec_free(&kept);
    return out;
}

FsAxis fs_parse_axis(AvpArena *a, AvpStr text)
{
    FsAxis ax = {0};
    AvpStrVec parts = avp_str_split(text, AVP_S("|"), -1);
    ax.n = parts.n;
    ax.alts = avp_alloc(a, parts.n * sizeof *ax.alts);
    ax.labels = avp_alloc(a, parts.n * sizeof *ax.labels);
    AvpStrVec names = {0};
    for (size_t k = 0; k < parts.n; k++) {
        ax.alts[k] = fs_parse_spec(a, parts.v[k]);
        for (size_t f = 0; f < ax.alts[k].n; f++) {
            AvpStr fam = fs_family(a, ax.alts[k].v[f]);
            if (!strs_has(&ax.families, fam)) avp_vec_push(&ax.families, fam);
            if (!strs_has(&names, ax.alts[k].v[f].name)) avp_vec_push(&names, ax.alts[k].v[f].name);
        }
    }
    if (!ax.families.n) avp_throw(AVP_ERR_OTHER, "--vary %s names no flag", avp_cz(a, avp_repr_str(a, text)));
    avp_sort_strs(names.v, names.n);
    for (size_t k = 0; k < parts.n; k++) {
        if (ax.alts[k].n) {
            AvpStr *strs = avp_alloc(a, ax.alts[k].n * sizeof *strs);
            for (size_t f = 0; f < ax.alts[k].n; f++) strs[f] = fs_flag_str(a, ax.alts[k].v[f]);
            ax.labels[k] = avp_str_join(a, AVP_S(" "), strs, ax.alts[k].n);
        } else {
            ax.labels[k] = avp_str_concat(a, AVP_S("no "), avp_str_join(a, AVP_S("/"), names.v, names.n));
        }
    }
    avp_vec_free(&parts);
    avp_vec_free(&names);
    return ax;
}

static bool has_compiler(AvpArena *a, const char *compilers, AvpStr name)
{
    AvpStr native = avp_str_replace(a, name, AVP_S("/"), AVP_S("\\"), -1);
    return avp_exists(avp_sprintf(a, "%s\\%.*s\\mwcceppc.exe", compilers, AVP_SF(native)));
}

AvpStrVec fs_resolve_versions(AvpArena *a, AvpStr text, const char *compilers)
{
    AvpStrVec out = {0};
    AvpStrVec items = avp_str_split(text, AVP_S(","), -1);
    for (size_t i = 0; i < items.n; i++) {
        AvpStr item = avp_str_strip(items.v[i]);
        if (!item.n) continue;
        AvpStrVec names = {0};
        if (avp_str_eqz(item, "all")) {
            static const char *const plats[] = {"Wii", "GC"};
            for (size_t p = 0; p < 2; p++) {
                const char *d = permute_join(a, compilers, plats[p]);
                AvpStrVec vs = {0};
                if (!avp_isdir(d) || !avp_listdir(a, d, &vs)) continue;
                avp_sort_strs(vs.v, vs.n);
                for (size_t v = 0; v < vs.n; v++)
                    if (avp_exists(avp_sprintf(a, "%s\\%.*s\\mwcceppc.exe", d, AVP_SF(vs.v[v]))))
                        avp_vec_push(&names, avp_str(avp_sprintf(a, "%s/%.*s", plats[p], AVP_SF(vs.v[v]))));
                avp_vec_free(&vs);
            }
        } else {
            avp_vec_push(&names, avp_str_replace(a, item, AVP_S("\\"), AVP_S("/"), -1));
        }
        for (size_t k = 0; k < names.n; k++) {
            AvpStr n = names.v[k];
            if (avp_str_findc(n, '/') < 0) n = avp_str_concat(a, AVP_S("Wii/"), n);
            if (!has_compiler(a, compilers, n)) avp_throw(AVP_ERR_OTHER, "unknown compiler version: %.*s", AVP_SF(n));
            if (!strs_has(&out, n)) avp_vec_push(&out, n);
        }
        avp_vec_free(&names);
    }
    avp_vec_free(&items);
    if (!out.n) avp_throw(AVP_ERR_OTHER, "no compiler versions given");
    return out;
}

static bool tokens_equal(const FsFlags *x, const FsFlags *y)
{
    AvpStrVec tx = fs_flag_tokens(x), ty = fs_flag_tokens(y);
    bool eq = tx.n == ty.n;
    for (size_t k = 0; eq && k < tx.n; k++) eq = avp_str_eq(tx.v[k], ty.v[k]);
    avp_vec_free(&tx);
    avp_vec_free(&ty);
    return eq;
}

static FsFlags copy_flags(const FsFlags *f)
{
    FsFlags out = {0};
    for (size_t k = 0; k < f->n; k++) avp_vec_push(&out, f->v[k]);
    return out;
}

FsVariants fs_build_variants(AvpArena *a, AvpStr base_version, const FsFlags *base_flags, const AvpStrVec *versions,
                             const AvpStrVec *axes, const AvpStrVec *sets, const AvpStrVec *drops, bool include_base,
                             i64 max_variants)
{
    AvpStrVec vers = {0};
    if (versions && versions->n) {
        for (size_t k = 0; k < versions->n; k++) avp_vec_push(&vers, versions->v[k]);
    } else {
        avp_vec_push(&vers, base_version);
    }
    size_t naxes = axes ? axes->n : 0;
    FsAxis *parsed = avp_alloc(a, (naxes + 1) * sizeof *parsed);
    for (size_t k = 0; k < naxes; k++) parsed[k] = fs_parse_axis(a, axes->v[k]);
    i64 total = (i64)vers.n;
    for (size_t k = 0; k < naxes; k++) total *= (i64)parsed[k].n;
    if (total > max_variants)
        avp_throw(AVP_ERR_OTHER, "%lld variants exceed --max %lld; narrow an axis or raise --max", (long long)total,
                  (long long)max_variants);
    FsFlags base = copy_flags(base_flags);
    for (size_t s = 0; sets && s < sets->n; s++) {
        FsFlags fl = fs_parse_spec(a, sets->v[s]);
        for (size_t f = 0; f < fl.n; f++) {
            FsFlags one = {0};
            avp_vec_push(&one, fl.v[f]);
            AvpStrVec fam = {0};
            avp_vec_push(&fam, fs_family(a, fl.v[f]));
            base = fs_apply_axis(a, &base, &one, &fam);
        }
    }
    for (size_t s = 0; drops && s < drops->n; s++) {
        FsFlags fl = fs_parse_spec(a, drops->v[s]);
        for (size_t f = 0; f < fl.n; f++) {
            FsFlags none = {0};
            AvpStrVec fam = {0};
            avp_vec_push(&fam, fs_family(a, fl.v[f]));
            base = fs_apply_axis(a, &base, &none, &fam);
        }
    }
    FsVariants out = {0};
    size_t *combo = avp_alloc(a, (naxes + 1) * sizeof *combo);
    for (size_t v = 0; v < vers.n; v++) {
        memset(combo, 0, (naxes + 1) * sizeof *combo);
        for (;;) { /* itertools.product: the last axis varies fastest */
            FsVariant var = {0};
            var.version = vers.v[v];
            var.flags = copy_flags(&base);
            for (size_t k = 0; k < naxes; k++) {
                var.flags = fs_apply_axis(a, &var.flags, &parsed[k].alts[combo[k]], &parsed[k].families);
                avp_vec_push(&var.choices, parsed[k].labels[combo[k]]);
            }
            avp_vec_push(&out, var);
            size_t k = naxes;
            while (k > 0 && ++combo[k - 1] == parsed[k - 1].n) combo[--k] = 0;
            if (k == 0) break;
        }
    }
    bool any = false;
    for (size_t k = 0; k < out.n; k++) {
        out.v[k].current = avp_str_eq(out.v[k].version, base_version) && tokens_equal(&out.v[k].flags, base_flags);
        any |= out.v[k].current;
    }
    if (include_base && !any) {
        FsVariant var = {0};
        var.version = base_version;
        var.flags = copy_flags(base_flags);
        avp_vec_push(&var.choices, AVP_S("(unit as is)"));
        var.current = true;
        avp_vec_push(&out, var);
    }
    for (size_t k = 0; k < out.n; k++) {
        out.v[k].id = avp_str(avp_sprintf(a, "v%zu", k));
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_put(&b, out.v[k].version);
        for (size_t c = 0; c < out.v[k].choices.n; c++) {
            avp_buf_putz(&b, " | ");
            avp_buf_put(&b, out.v[k].choices.v[c]);
        }
        out.v[k].label = avp_buf_str(&b);
    }
    return out;
}

/* ---- row normalisation and comparison --------------------------------------------------------------------------- */

static bool is_hex_upper(u8 c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); }
static bool is_word(u8 c) { return c == '_' || isalnum(c) || c >= 0x80; }
static bool is_digit(u8 c) { return c >= '0' && c <= '9'; }

static size_t hex_run(AvpStr s, size_t i)
{
    while (i < s.n && is_hex_upper((u8)s.p[i])) i++;
    return i;
}

/* cc.norm(line) */
static AvpStr cc_norm(AvpArena *a, AvpStr line)
{
    /* ^/\* [0-9A-F]+ [0-9A-F]+  [0-9A-F ]+ \*\/ */
    if (avp_str_startswith(line, AVP_S("/* "))) {
        size_t i = 3, e = hex_run(line, i);
        if (e > i && e < line.n && line.p[e] == ' ') {
            size_t j = e + 1, f = hex_run(line, j);
            if (f > j && f + 2 <= line.n && line.p[f] == ' ' && line.p[f + 1] == ' ') {
                size_t r0 = f + 2, r = r0;
                while (r < line.n && (is_hex_upper((u8)line.p[r]) || line.p[r] == ' ')) r++;
                if (r >= r0 + 2 && line.p[r - 1] == ' ' && r + 2 <= line.n && line.p[r] == '*' && line.p[r + 1] == '/')
                    line = avp_strn(line.p + r + 2, line.n - r - 2);
            }
        }
    }
    line = avp_str_strip(line);
    if (!line.n || line.p[0] == '.' || line.p[0] == '#') return AVP_S("");
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < line.n;) {
        if (avp_str_startswith(avp_strn(line.p + i, line.n - i), AVP_S(".L_")) && hex_run(line, i + 3) > i + 3) {
            avp_buf_putc(&b, 'L');
            i = hex_run(line, i + 3);
            continue;
        }
        avp_buf_putc(&b, line.p[i++]);
    }
    AvpStr t = avp_buf_str(&b);
    avp_buf_init(&b, a);
    for (size_t i = 0; i < t.n;) {
        bool boundary = i == 0 || !is_word((u8)t.p[i - 1]);
        size_t pre = avp_str_startswith(avp_strn(t.p + i, t.n - i), AVP_S("lbl_")) ? 4
                     : avp_str_startswith(avp_strn(t.p + i, t.n - i), AVP_S("fn_")) ? 3 : 0;
        if (boundary && pre && hex_run(t, i + pre) > i + pre) {
            avp_buf_putz(&b, "SYM");
            i = hex_run(t, i + pre);
            continue;
        }
        avp_buf_putc(&b, t.p[i++]);
    }
    t = avp_buf_str(&b);
    avp_buf_init(&b, a);
    for (size_t i = 0; i < t.n;) {
        if (shsm_ws_len(t, i)) {
            avp_buf_putc(&b, ' ');
            i = shsm_skip_ws(t, i);
        } else {
            avp_buf_putc(&b, t.p[i++]);
        }
    }
    return avp_buf_str(&b);
}

/* _EXTRA at i: the end of the match, 0 when none */
static size_t extra_at(AvpStr s, size_t i)
{
    /* "?@(?:\d+|stringBase\d+)"? */
    size_t j = i < s.n && s.p[i] == '"' ? i + 1 : i;
    if (j < s.n && s.p[j] == '@') {
        size_t k = j + 1, e = k;
        while (e < s.n && is_digit((u8)s.p[e])) e++;
        if (e == k && avp_str_startswith(avp_strn(s.p + k, s.n - k), AVP_S("stringBase"))) {
            size_t d = k + 10;
            e = d;
            while (e < s.n && is_digit((u8)s.p[e])) e++;
            if (e == d) e = k;
        }
        if (e > k) return e < s.n && s.p[e] == '"' ? e + 1 : e;
    }
    /* \bjumptable_[0-9A-F]+ */
    if ((i == 0 || !is_word((u8)s.p[i - 1])) && avp_str_startswith(avp_strn(s.p + i, s.n - i), AVP_S("jumptable_"))) {
        size_t e = hex_run(s, i + 10);
        if (e > i + 10) return e;
    }
    /* \.\.\.\w+\.\d+ */
    if (avp_str_startswith(avp_strn(s.p + i, s.n - i), AVP_S("..."))) {
        size_t k = i + 3, e = k;
        while (e < s.n && is_word((u8)s.p[e])) e++;
        if (e > k && e < s.n && s.p[e] == '.') {
            size_t d = e + 1, f = d;
            while (f < s.n && is_digit((u8)s.p[f])) f++;
            if (f > d) return f;
        }
    }
    return 0;
}

AvpStr fs_norm_row(AvpArena *a, AvpStr line)
{
    AvpStr r = cc_norm(a, line);
    if (!r.n) return r;
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < r.n;) {
        size_t e = extra_at(r, i);
        if (e) {
            avp_buf_putz(&b, "SYM");
            i = e;
        } else {
            avp_buf_putc(&b, r.p[i++]);
        }
    }
    return avp_buf_str(&b);
}

/* legacy_rows(fn) */
static AvpStr *legacy_rows(AvpArena *a, const CcFn *fn, size_t *n)
{
    AvpStr *out = avp_alloc(a, (fn->nrows + 1) * sizeof *out);
    size_t k = 0;
    for (size_t r = 0; r < fn->nrows; r++) {
        AvpStr x = fs_norm_row(a, fn->rows[r].text);
        if (x.n) out[k++] = x;
    }
    *n = k;
    return out;
}

FsScore *fs_score_variant(AvpArena *a, const AvpMap *retail, const AvpMap *compiled, CcContext *ctx, bool targets)
{
    AvpMap *by_retail = avp_map_new();
    AvpMapIter it = {0};
    while (avp_map_next(compiled, &it)) {
        AvpStr r = cc_retail_name_for(it.key, retail);
        if (r.p) avp_map_put(by_retail, r, NULL)->p = it.val->p;
    }
    FsScore *out = avp_alloc(a, (avp_map_len(retail) + 1) * sizeof *out);
    size_t k = 0;
    memset(&it, 0, sizeof it);
    while (avp_map_next(retail, &it)) {
        const CcFn *want = it.val->p;
        FsScore *s = &out[k++];
        s->rows = want->nrows;
        AvpVal *got = avp_map_get(by_retail, it.key);
        if (!got) {
            *s = (FsScore){false, want->nrows, want->nrows, 0, 0, "missing"};
            continue;
        }
        if (targets) {
            CcFnResult *res = cc_compare_function(a, want, got->p, ctx, true);
            s->diff = res->edit;
            s->targets = res->targets.n;
            s->unresolved = res->unresolved.n;
        } else {
            size_t nw, ng;
            AvpStr *w = legacy_rows(a, want, &nw), *g = legacy_rows(a, got->p, &ng);
            s->diff = cc_edit_count(w, nw, g, ng);
        }
        s->exact = s->diff == 0 && s->targets == 0;
        s->status = s->exact ? "ok" : s->diff ? "diff" : "target";
    }
    avp_map_free(by_retail);
    return out;
}

bool fs_wanted(AvpStr name, const AvpStrVec *symbols)
{
    if (!symbols->n) return true;
    bool pre = name.n >= 11 && avp_str_startswith(name, AVP_S("fn_"));
    for (size_t k = 3; pre && k < 11; k++) pre = is_hex_upper((u8)name.p[k]);
    for (size_t k = 0; k < symbols->n; k++) {
        AvpStr s = symbols->v[k];
        if (avp_str_eq(name, s)) return true;
        if (avp_str_startswith(name, s) && name.n >= s.n + 2 && name.p[s.n] == '_' && name.p[s.n + 1] == '_')
            return true;
        if (pre && avp_str_startswith(s, avp_strn(name.p, 11))) return true;
    }
    return false;
}

AvpStr fs_first_error(AvpArena *a, AvpStr text)
{
    AvpStrVec raw = avp_str_splitlines(text, false), lines = {0};
    for (size_t k = 0; k < raw.n; k++) {
        AvpStr l = avp_str_strip(raw.v[k]);
        if (l.n) avp_vec_push(&lines, l);
    }
    AvpStr out = AVP_S("");
    bool found = false;
    for (size_t i = 0; i < lines.n && !found; i++) {
        AvpStr l = lines.v[i];
        if (!avp_str_contains(l, AVP_S("rror"))) continue;
        if (avp_str_endswith(l, AVP_S(":")) && i + 1 < lines.n)
            l = avp_str(avp_sprintf(a, "%.*s %.*s", AVP_SF(avp_str_lstrip_chars(l, AVP_S("# "))),
                                    AVP_SF(avp_str_lstrip_chars(lines.v[i + 1], AVP_S("# ")))));
        out = l;
        found = true;
    }
    if (!found && lines.n) out = lines.v[0];
    avp_vec_free(&raw);
    avp_vec_free(&lines);
    return avp_str_dup(a, out);
}

AvpStr fs_cell(AvpArena *a, const FsScore *f)
{
    if (f->exact) return f->unresolved ? AVP_S("ok?") : AVP_S("ok");
    if (!strcmp(f->status, "missing")) return AVP_S("--");
    if (f->diff && f->targets) return avp_str(avp_sprintf(a, "%zu+%zut", f->diff, f->targets));
    if (f->targets) return avp_str(avp_sprintf(a, "%zut", f->targets));
    return avp_str(avp_sprintf(a, "%zu", f->diff));
}
