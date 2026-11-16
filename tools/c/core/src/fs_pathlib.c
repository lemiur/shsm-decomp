/* The pathlib.PureWindowsPath / Path operations the tools use: normalisation (str(Path(p))), name/stem/suffix/parent,
 * with_suffix/with_name, relative_to, parts, case-insensitive ordering, resolve and glob/rglob. */
#include "fs_priv.h"

typedef AVP_VEC(AvpStr) Parts;

/* _parse_path: a drive "//server/share" or "//?/unc/server/share" without a root is given the root "\\". */
static bool unc_needs_root(const char *d, size_t n) {
    if (n < 2 || d[0] != '\\' || d[1] != '\\' || d[n - 1] == '\\') return false;
    size_t parts = 1;
    for (size_t i = 0; i < n; i++)
        if (d[i] == '\\') parts++;
    if (parts == 6) return true;
    if (parts != 4) return false;
    size_t k = 2;
    while (k < n && d[k] != '\\') k++;
    size_t len = k - 2;
    const char *x = d + 2;
    if (len == 0 || (len == 1 && (x[0] == '?' || x[0] == '.')) || (len == 2 && x[0] == '?' && x[1] == '.')) return false;
    return true;
}

/* Parses p the way PurePath does: the drive and root as one anchor string, the tail components without empty and
 * "." ones. The views point into the (backslash-normalised) copy made in `a`. */
static char *parse(AvpArena *a, const char *p, AvpStr *anchor, Parts *tail) {
    size_t n = strlen(p);
    char *q = (char *)avp_alloc_noz(a, n + 1);
    for (size_t i = 0; i <= n; i++) q[i] = p[i] == '/' ? '\\' : p[i];
    AvpStr d, r, t;
    avp_path_splitroot(q, &d, &r, &t);
    anchor->p = q;
    anchor->n = d.n + r.n;
    if (!r.n && unc_needs_root(q, d.n)) {
        char *an = (char *)avp_alloc_noz(a, d.n + 2);
        memcpy(an, q, d.n);
        an[d.n] = '\\';
        an[d.n + 1] = 0;
        anchor->p = an;
        anchor->n = d.n + 1;
    }
    const char *s = t.p, *end = t.p + t.n;
    while (s < end) {
        const char *e = s;
        while (e < end && *e != '\\') e++;
        size_t len = (size_t)(e - s);
        if (len && !(len == 1 && s[0] == '.')) {
            AvpStr c = {s, len};
            avp_vec_push(tail, c);
        }
        s = e + 1;
    }
    return q;
}

static char *format(AvpArena *a, AvpStr anchor, const AvpStr *tail, size_t n) {
    FsBuf b;
    fsb_init(&b, a);
    fsb_put(&b, anchor.p, anchor.n);
    for (size_t i = 0; i < n; i++) {
        if (i) fsb_putc(&b, '\\');
        fsb_put(&b, tail[i].p, tail[i].n);
    }
    if (anchor.n == 0) {
        if (n == 0) fsb_putc(&b, '.');
        else {
            /* a first component that looks like a drive is protected with "." as pathlib does */
            AvpStr d, rest;
            char *z = (char *)fs_dup(avp_arena(), tail[0].p, tail[0].n).p;
            avp_path_splitdrive(z, &d, &rest);
            if (d.n) {
                FsBuf c;
                fsb_init(&c, a);
                fsb_putz(&c, ".\\");
                fsb_put(&c, b.p, b.n);
                if (!a) avp_xfree(b.p);
                return fsb_cz(&c);
            }
        }
    }
    return fsb_cz(&b);
}

char *avp_pl_str(AvpArena *a, const char *p) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    AvpStr anchor;
    Parts tail = {0};
    parse(sc, p, &anchor, &tail);
    char *r = format(a, anchor, tail.v, tail.n);
    avp_vec_free(&tail);
    fs_tmp_done(sc, mk);
    return r;
}

AvpStr avp_pl_name(const char *p) {
    AvpStr empty = {p, 0};
    AvpStr d, r, t;
    avp_path_splitroot(p, &d, &r, &t);
    size_t end = t.n;
    for (;;) {
        while (end && (t.p[end - 1] == '\\' || t.p[end - 1] == '/')) end--;
        size_t i = end;
        while (i && t.p[i - 1] != '\\' && t.p[i - 1] != '/') i--;
        if (end - i == 1 && t.p[i] == '.') {
            end = i; /* a "." component is dropped by pathlib */
            continue;
        }
        if (end == i) return empty;
        AvpStr s = {t.p + i, end - i};
        return s;
    }
}

/* PurePath.stem / suffix of CPython 3.14: leading dots of the name never start a suffix, a single "." is a suffix. */
AvpStr avp_pl_suffix(const char *p) {
    AvpStr name = avp_pl_name(p);
    size_t lead = 0;
    while (lead < name.n && name.p[lead] == '.') lead++;
    for (size_t i = name.n; i-- > lead;)
        if (name.p[i] == '.') {
            AvpStr s = {name.p + i, name.n - i};
            return s;
        }
    AvpStr none = {name.p + name.n, 0};
    return none;
}

AvpStr avp_pl_stem(const char *p) {
    AvpStr name = avp_pl_name(p);
    for (size_t i = name.n; i-- > 0;)
        if (name.p[i] == '.') {
            /* the stem must contain at least one non-dot character */
            for (size_t k = 0; k < i; k++)
                if (name.p[k] != '.') {
                    AvpStr s = {name.p, i};
                    return s;
                }
            break;
        }
    return name;
}

char *avp_pl_parent(AvpArena *a, const char *p) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    AvpStr anchor;
    Parts tail = {0};
    parse(sc, p, &anchor, &tail);
    char *r = format(a, anchor, tail.v, tail.n ? tail.n - 1 : 0);
    avp_vec_free(&tail);
    fs_tmp_done(sc, mk);
    return r;
}

static char *empty_name_error(const char *p) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    FsBuf b;
    fsb_init(&b, NULL);
    fsb_putz(&b, "WindowsPath(");
    avp_fs_repr(&b, avp_path_as_posix(sc, avp_pl_str(sc, p)));
    fsb_putz(&b, ") has an empty name");
    avp_fs_fail_value("%s", fsb_cz(&b));
    avp_xfree(b.p);
    fs_tmp_done(sc, mk);
    return NULL;
}

static char *invalid_error(const char *what, const char *arg) {
    FsBuf b;
    fsb_init(&b, NULL);
    fsb_printf(&b, "Invalid %s ", what);
    avp_fs_repr(&b, arg);
    avp_fs_fail_value("%s", fsb_cz(&b));
    avp_xfree(b.p);
    return NULL;
}

char *avp_pl_with_name(AvpArena *a, const char *p, const char *name) {
    if (!*name || strchr(name, '\\') || strchr(name, '/') || strcmp(name, ".") == 0) return invalid_error("name", name);
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    AvpStr anchor;
    Parts tail = {0};
    parse(sc, p, &anchor, &tail);
    char *res = NULL;
    if (tail.n == 0) empty_name_error(p);
    else {
        tail.v[tail.n - 1].p = name;
        tail.v[tail.n - 1].n = strlen(name);
        res = format(a, anchor, tail.v, tail.n);
    }
    avp_vec_free(&tail);
    fs_tmp_done(sc, mk);
    return res;
}

char *avp_pl_with_suffix(AvpArena *a, const char *p, const char *suffix) {
    AvpStr stem = avp_pl_stem(p);
    if (stem.n == 0) return empty_name_error(p);
    if (*suffix && *suffix != '.') return invalid_error("suffix", suffix);
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    size_t sl = strlen(suffix);
    char *nn = (char *)avp_alloc_noz(sc, stem.n + sl + 1);
    memcpy(nn, stem.p, stem.n);
    memcpy(nn + stem.n, suffix, sl + 1);
    char *r = avp_pl_with_name(a, p, nn);
    fs_tmp_done(sc, mk);
    return r;
}

AvpStrVec avp_pl_parts(AvpArena *a, const char *p) {
    AvpStr anchor;
    Parts tail = {0};
    parse(a, p, &anchor, &tail);
    AvpStrVec out = {0};
    if (anchor.n) avp_vec_push(&out, fs_dup(a, anchor.p, anchor.n));
    for (size_t i = 0; i < tail.n; i++) avp_vec_push(&out, fs_dup(a, tail.v[i].p, tail.v[i].n));
    avp_vec_free(&tail);
    return out;
}

/* str.lower() as pathlib compares: U+0130 lowers to "i" + U+0307, a final capital sigma to U+03C2. */
static char *lower_all(AvpArena *a, const char *s) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    FsBuf b;
    fsb_init(&b, sc);
    const char *p = s, *end = s + strlen(s);
    while (p < end) {
        u32 c = avp_fs_u8_next(&p, end);
        if (c == 0x130) fsb_putz(&b, "i\xCC\x87");
        else if (c == 0x3A3) {
            /* a final sigma follows a letter and is not followed by one */
            bool prev_letter = b.n > 0, next_letter = false;
            if (p < end) {
                const char *q = p;
                u32 nx = avp_fs_u8_next(&q, end);
                WORD t = 0;
                wchar_t w = (wchar_t)nx;
                next_letter = nx <= 0xFFFF && GetStringTypeW(CT_CTYPE1, &w, 1, &t) && (t & C1_ALPHA);
            }
            if (prev_letter) {
                const char *lp = b.p + b.n;
                do lp--; while (lp > b.p && ((unsigned char)*lp & 0xC0) == 0x80);
                const char *q = lp;
                u32 pv = avp_fs_u8_next(&q, b.p + b.n);
                WORD t = 0;
                wchar_t w = (wchar_t)pv;
                prev_letter = pv <= 0xFFFF && GetStringTypeW(CT_CTYPE1, &w, 1, &t) && (t & C1_ALPHA);
            }
            if (prev_letter && !next_letter) fsb_putcp(&b, 0x3C2);
            else fsb_putcp(&b, 0x3C3);
        } else fsb_putcp(&b, c);
    }
    char *r = avp_path_normcase(a, fsb_cz(&b));
    fs_tmp_done(sc, mk);
    return r;
}

/* Whether the paths (already parsed) have equal anchors and `base` is a prefix of `path`, compared case-insensitively. */
static bool prefix_of(AvpArena *sc, AvpStr aa, const Parts *ta, AvpStr ab, const Parts *tb) {
    if (strcmp(lower_all(sc, fs_cz(sc, aa)), lower_all(sc, fs_cz(sc, ab))) != 0) return false;
    if (tb->n > ta->n) return false;
    for (size_t i = 0; i < tb->n; i++)
        if (strcmp(lower_all(sc, fs_cz(sc, ta->v[i])), lower_all(sc, fs_cz(sc, tb->v[i]))) != 0) return false;
    return true;
}

bool avp_pl_is_relative_to(const char *p, const char *base) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    AvpStr aa, ab;
    Parts ta = {0}, tb = {0};
    parse(sc, p, &aa, &ta);
    parse(sc, base, &ab, &tb);
    bool r = prefix_of(sc, aa, &ta, ab, &tb);
    avp_vec_free(&ta);
    avp_vec_free(&tb);
    fs_tmp_done(sc, mk);
    return r;
}

bool avp_pl_relative_to(AvpArena *a, const char *p, const char *base, char **out) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    AvpStr aa, ab;
    Parts ta = {0}, tb = {0};
    parse(sc, p, &aa, &ta);
    parse(sc, base, &ab, &tb);
    bool ok = prefix_of(sc, aa, &ta, ab, &tb);
    if (ok) {
        AvpStr none = {"", 0};
        *out = format(a, none, ta.v + tb.n, ta.n - tb.n);
    } else {
        FsBuf b;
        fsb_init(&b, NULL);
        avp_fs_repr(&b, avp_pl_str(sc, p));
        fsb_putz(&b, " is not in the subpath of ");
        avp_fs_repr(&b, avp_pl_str(sc, base));
        avp_fs_fail_value("%s", fsb_cz(&b));
        avp_xfree(b.p);
    }
    avp_vec_free(&ta);
    avp_vec_free(&tb);
    fs_tmp_done(sc, mk);
    return ok;
}

int avp_pl_cmp(const char *x, const char *y) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    char *sx = lower_all(sc, avp_pl_str(sc, x)), *sy = lower_all(sc, avp_pl_str(sc, y));
    /* list comparison of the components split at "\" */
    int r = 0;
    const char *px = sx, *py = sy;
    for (;;) {
        const char *ex = strchr(px, '\\'), *ey = strchr(py, '\\');
        size_t lx = ex ? (size_t)(ex - px) : strlen(px), ly = ey ? (size_t)(ey - py) : strlen(py);
        size_t m = lx < ly ? lx : ly;
        int c = memcmp(px, py, m);
        if (c == 0) c = lx < ly ? -1 : lx > ly ? 1 : 0;
        if (c) {
            r = c < 0 ? -1 : 1;
            break;
        }
        if (!ex || !ey) {
            r = ex ? 1 : ey ? -1 : 0;
            break;
        }
        px = ex + 1;
        py = ey + 1;
    }
    fs_tmp_done(sc, mk);
    return r;
}

char *avp_pl_resolve(AvpArena *a, const char *p) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    char *r = avp_pl_str(a, avp_path_realpath(sc, p));
    fs_tmp_done(sc, mk);
    return r;
}

/* ---- glob ------------------------------------------------------------------------------------------------------- */

/* "**" in the last position: every entry below dir (files too), links to folders are listed but not entered. */
static void pl_all_below(AvpArena *a, const char *dir, AvpStrVec *out) {
    AvpArena *sc = avp_arena_new(0);
    AvpDirEntries es;
    if (avp_scandir(sc, dir, &es)) {
        for (size_t i = 0; i < es.n; i++) {
            const char *path = avp_path_join(a, dir, es.v[i].name.p);
            avp_vec_push(out, fs_dup(a, path, strlen(path)));
            if (es.v[i].is_dir && !es.v[i].is_symlink) pl_all_below(a, path, out);
        }
        avp_vec_free(&es);
    }
    avp_arena_free(sc);
}

/* "**" before more components: dir and every folder below it (symbolic links are not entered; junctions are, as
 * pathlib's is_symlink() is false for them). */
static void pl_glob_rec(AvpArena *a, const char *dir, const AvpStr *comps, size_t nc, size_t ci, AvpStrVec *out);

static void pl_dirs_below(AvpArena *a, const char *dir, const AvpStr *comps, size_t nc, size_t ci, AvpStrVec *out) {
    pl_glob_rec(a, dir, comps, nc, ci + 1, out);
    AvpArena *sc = avp_arena_new(0);
    AvpDirEntries es;
    if (avp_scandir(sc, dir, &es)) {
        for (size_t i = 0; i < es.n; i++)
            if (es.v[i].is_dir && !es.v[i].is_symlink) pl_dirs_below(a, avp_path_join(a, dir, es.v[i].name.p), comps, nc, ci, out);
        avp_vec_free(&es);
    }
    avp_arena_free(sc);
}

static void pl_glob_rec(AvpArena *a, const char *dir, const AvpStr *comps, size_t nc, size_t ci, AvpStrVec *out) {
    if (ci == nc) return;
    AvpArena *sc = avp_arena_new(0);
    char *comp = fs_cz(sc, comps[ci]);
    bool last = ci + 1 == nc;
    if (strcmp(comp, "**") == 0) {
        if (last) {
            if (avp_isdir(dir)) avp_vec_push(out, fs_dup(a, dir, strlen(dir)));
            pl_all_below(a, dir, out);
        } else pl_dirs_below(a, dir, comps, nc, ci, out);
    } else if (!avp_glob_has_magic(comp)) {
        const char *path = avp_path_join(a, dir, comp);
        if (last ? avp_lexists(path) : avp_isdir(path)) {
            if (last) avp_vec_push(out, fs_dup(a, path, strlen(path)));
            else pl_glob_rec(a, path, comps, nc, ci + 1, out);
        }
    } else {
        AvpDirEntries es;
        if (avp_scandir(sc, dir, &es)) {
            for (size_t i = 0; i < es.n; i++) {
                if (!avp_fnmatch(es.v[i].name.p, comp)) continue;
                const char *path = avp_path_join(a, dir, es.v[i].name.p);
                if (last) avp_vec_push(out, fs_dup(a, path, strlen(path)));
                else if (es.v[i].is_dir) pl_glob_rec(a, path, comps, nc, ci + 1, out);
            }
            avp_vec_free(&es);
        }
    }
    avp_arena_free(sc);
}

AvpStrVec avp_pl_glob(AvpArena *a, const char *root, const char *pattern) {
    AvpStrVec out = {0};
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    AvpStr anchor;
    Parts comps = {0};
    parse(sc, pattern, &anchor, &comps);
    if (comps.n) pl_glob_rec(a, root, comps.v, comps.n, 0, &out);
    avp_vec_free(&comps);
    /* patterns with several "**" can reach a path twice; pathlib yields each once */
    AvpSet *seen = avp_set_new();
    size_t w = 0;
    for (size_t i = 0; i < out.n; i++)
        if (avp_set_add(seen, out.v[i])) out.v[w++] = out.v[i];
    out.n = w;
    avp_set_free(seen);
    fs_tmp_done(sc, mk);
    return out;
}

AvpStrVec avp_pl_rglob(AvpArena *a, const char *root, const char *pattern) {
    AvpArena *sc = fs_tmp();
    AvpArenaMark mk = avp_arena_mark(sc);
    char *p2 = avp_path_join(sc, "**", pattern);
    AvpStrVec r = avp_pl_glob(a, root, p2);
    fs_tmp_done(sc, mk);
    return r;
}
