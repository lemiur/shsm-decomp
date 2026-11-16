/* ml_conflict: what a rebase conflict in configure.py or config/<ver>/splits.txt looks like, and the resolution that
 * keeps both sides' distinct units. Two lanes that add units side by side conflict there on every rebase; the hunks are
 * shown compactly and, when each one is a list of whole units, the union is offered as a suggestion that
 * `--take-union-units` applies (never by default).
 *
 * During a rebase the conflict markers read "ours" = the branch rebased onto (main) and "theirs" = the lane's commit.
 *   configure.py   a hunk side is a run of single-line `Object(Status, "path"),` entries. The union keeps main's entries
 *                  in place and adds the lane's other entries after them; for a path both sides name, the lane's line.
 *   splits.txt     a hunk side is a run of unit blocks (`path:` and its indented lines, up to the next header). The union
 *                  keeps main's blocks, adds the lane's other blocks, and takes the lane's block for a path both name.
 * Anything else in a hunk (a partial block, a multi-line Object, other code) is not resolved: the hunk is shown and the
 * file stays conflicted. */
#include "merge_lane.h"

#include <string.h>

typedef struct {
    AvpStrVec ours, theirs;
    size_t first_line; /* 1-based line of the <<<<<<< marker */
    bool resolvable;
    AvpStrVec merged;  /* the union, when resolvable */
    const char *why;   /* when not */
    size_t kept, added, lane_wins;
    AvpStrVec lane_win_names;
    AvpStr blankln;    /* an empty line in the file's line ending */
} Hunk;
typedef AVP_VEC(Hunk) Hunks;

static AvpStr chomp(AvpStr s) {
    while (s.n && (s.p[s.n - 1] == '\r' || s.p[s.n - 1] == ' ' || s.p[s.n - 1] == '\t')) s.n--;
    return s;
}
static bool blank(AvpStr s) { return !chomp(avp_str_lstrip(s)).n; }

/* The unit path of a single-line `Object(Status, "path"),` entry; false for any other line. */
static bool object_key(AvpStr line, AvpStr *key) {
    AvpStr t = avp_str_strip(line);
    if (!avp_str_startswith(t, AVP_S("Object("))) return false;
    int depth = 0;
    bool in_str = false;
    for (size_t i = 0; i < t.n; i++) {
        char c = t.p[i];
        if (c == '"') in_str = !in_str;
        else if (!in_str && c == '(') depth++;
        else if (!in_str && c == ')') depth--;
    }
    if (depth != 0 || in_str) return false;
    i64 q = avp_str_findc(t, '"');
    if (q < 0) return false;
    AvpStr rest = {t.p + q + 1, t.n - (size_t)q - 1};
    i64 e = avp_str_findc(rest, '"');
    if (e <= 0) return false;
    *key = avp_strn(rest.p, (size_t)e);
    return true;
}

/* A splits.txt unit header: a non-indented line ending in ':' that is not the Sections: line. */
static bool block_header(AvpStr line, AvpStr *key) {
    AvpStr t = chomp(line);
    if (!t.n || t.p[0] == ' ' || t.p[0] == '\t' || t.p[t.n - 1] != ':') return false;
    *key = avp_strn(t.p, t.n - 1);
    return true;
}

typedef struct {
    AvpStr key;
    size_t a, b; /* line range [a, b) of the side, trailing blank lines excluded */
    bool trailing_blank;
} Block;
typedef AVP_VEC(Block) Blocks;

static bool parse_blocks(const AvpStrVec *side, Blocks *out) {
    size_t n = side->n, i = 0;
    while (i < n && blank(side->v[i])) i++;
    if (i >= n) return false;
    while (i < n) {
        AvpStr key;
        if (!block_header(side->v[i], &key)) return false;
        size_t j = i + 1;
        AvpStr k2;
        while (j < n && !block_header(side->v[j], &k2)) j++;
        size_t end = j;
        while (end > i + 1 && blank(side->v[end - 1])) end--;
        avp_vec_push(out, ((Block){key, i, end, end < j}));
        i = j;
    }
    return true;
}

static void resolve_configure(AvpArena *a, Hunk *h) {
    (void)a;
    AvpStr k, k2;
    for (size_t i = 0; i < h->ours.n; i++)
        if (!object_key(h->ours.v[i], &k)) {
            h->why = "main's side has a line that is not a single-line Object(...) entry";
            return;
        }
    for (size_t i = 0; i < h->theirs.n; i++)
        if (!object_key(h->theirs.v[i], &k)) {
            h->why = "the lane's side has a line that is not a single-line Object(...) entry";
            return;
        }
    for (size_t i = 0; i < h->ours.n; i++) {
        object_key(h->ours.v[i], &k);
        AvpStr line = h->ours.v[i];
        for (size_t j = 0; j < h->theirs.n; j++) {
            object_key(h->theirs.v[j], &k2);
            if (avp_str_eq(k, k2)) {
                if (!avp_str_eq(chomp(h->ours.v[i]), chomp(h->theirs.v[j]))) {
                    line = h->theirs.v[j];
                    h->lane_wins++;
                    avp_vec_push(&h->lane_win_names, avp_str_dup(a, k));
                }
                break;
            }
        }
        avp_vec_push(&h->merged, line);
        h->kept++;
    }
    for (size_t j = 0; j < h->theirs.n; j++) {
        object_key(h->theirs.v[j], &k2);
        bool in_ours = false;
        for (size_t i = 0; i < h->ours.n && !in_ours; i++) {
            object_key(h->ours.v[i], &k);
            in_ours = avp_str_eq(k, k2);
        }
        if (!in_ours) {
            avp_vec_push(&h->merged, h->theirs.v[j]);
            h->added++;
        }
    }
    h->resolvable = true;
}

static void resolve_splits(AvpArena *a, Hunk *h) {
    Blocks bo = {0}, bt = {0};
    if (!parse_blocks(&h->ours, &bo)) {
        h->why = "main's side does not start with a unit header (a partial block)";
        return;
    }
    if (!parse_blocks(&h->theirs, &bt)) {
        h->why = "the lane's side does not start with a unit header (a partial block)";
        return;
    }
    bool trailing = bo.v[bo.n - 1].trailing_blank || bt.v[bt.n - 1].trailing_blank;
    AVP_VEC(const Block *) order = {0};
    AVP_VEC(const AvpStrVec *) from = {0};
    for (size_t i = 0; i < bo.n; i++) {
        const Block *use = &bo.v[i];
        const AvpStrVec *side = &h->ours;
        for (size_t j = 0; j < bt.n; j++)
            if (avp_str_eq(bo.v[i].key, bt.v[j].key)) {
                bool same = (bo.v[i].b - bo.v[i].a) == (bt.v[j].b - bt.v[j].a);
                for (size_t q = 0; same && q < bo.v[i].b - bo.v[i].a; q++)
                    same = avp_str_eq(chomp(h->ours.v[bo.v[i].a + q]), chomp(h->theirs.v[bt.v[j].a + q]));
                if (!same) {
                    use = &bt.v[j];
                    side = &h->theirs;
                    h->lane_wins++;
                    avp_vec_push(&h->lane_win_names, avp_str_dup(a, bo.v[i].key));
                }
                break;
            }
        avp_vec_push(&order, use);
        avp_vec_push(&from, side);
        h->kept++;
    }
    for (size_t j = 0; j < bt.n; j++) {
        bool in_ours = false;
        for (size_t i = 0; i < bo.n && !in_ours; i++) in_ours = avp_str_eq(bo.v[i].key, bt.v[j].key);
        if (!in_ours) {
            avp_vec_push(&order, &bt.v[j]);
            avp_vec_push(&from, &h->theirs);
            h->added++;
        }
    }
    for (size_t o = 0; o < order.n; o++) {
        for (size_t q = order.v[o]->a; q < order.v[o]->b; q++) avp_vec_push(&h->merged, from.v[o]->v[q]);
        if (o + 1 < order.n || trailing) avp_vec_push(&h->merged, h->blankln);
    }
    h->resolvable = true;
}

typedef enum { CK_CONFIGURE, CK_SPLITS } CKind;

/* Splits text into lines (the line end "\r" stays on the line) and finds the conflict hunks. false when the text has no
 * complete hunk. *plain holds every line outside hunks, with the hunk's index at the place it occurred. */
static bool parse_conflicts(AvpArena *a, AvpStr text, CKind kind, Hunks *hunks, AvpStrVec *plain, AvpI64Vec *slots) {
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    int state = 0;
    Hunk cur = {0};
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr ln = chomp(lines.v[i]);
        if (avp_str_startswith(ln, AVP_S("<<<<<<< ")) || avp_str_eq(ln, AVP_S("<<<<<<<"))) {
            if (state) return false;
            state = 1;
            cur = (Hunk){0};
            cur.first_line = i + 1;
            cur.blankln = avp_str_contains(text, AVP_S("\r\n")) ? AVP_S("\r") : AVP_S("");
            continue;
        }
        if (state == 1 && (avp_str_startswith(ln, AVP_S("||||||| ")) || avp_str_eq(ln, AVP_S("|||||||")))) {
            state = 2; /* diff3: the base lines are skipped */
            continue;
        }
        if ((state == 1 || state == 2) && avp_str_eq(ln, AVP_S("======="))) {
            state = 3;
            continue;
        }
        if (state == 3 && (avp_str_startswith(ln, AVP_S(">>>>>>> ")) || avp_str_eq(ln, AVP_S(">>>>>>>")))) {
            if (kind == CK_CONFIGURE) resolve_configure(a, &cur);
            else resolve_splits(a, &cur);
            avp_vec_push(hunks, cur);
            avp_vec_push(slots, (i64)hunks->n - 1);
            avp_vec_push(plain, AVP_S(""));
            state = 0;
            continue;
        }
        if (state == 1) avp_vec_push(&cur.ours, lines.v[i]);
        else if (state == 3) avp_vec_push(&cur.theirs, lines.v[i]);
        else if (state == 0) {
            avp_vec_push(plain, lines.v[i]);
            avp_vec_push(slots, -1);
        }
    }
    return state == 0 && hunks->n > 0;
}

static AvpStr cut(AvpStr s) {
    s = chomp(s);
    if (s.n > 110) s.n = 110;
    return s;
}

/* Describes the hunks of one conflicted file into report (compact) and, when every hunk has a union, the file text
 * with them resolved in *resolved. kind of file by path: configure.py, or a splits.txt. */
bool ml_conflict_report(Ml *ml, const char *path, AvpStr text, AvpStrVec *report, AvpStr *resolved) {
    AvpArena *a = ml->a;
    AvpStr p = avp_str(path);
    CKind kind;
    if (avp_str_eqz(p, "configure.py")) kind = CK_CONFIGURE;
    else if (avp_str_endswith(p, AVP_S("/splits.txt"))) kind = CK_SPLITS;
    else return false;
    Hunks hunks = {0};
    AvpStrVec plain = {0};
    AvpI64Vec slots = {0};
    if (!parse_conflicts(a, text, kind, &hunks, &plain, &slots)) {
        avp_vec_push(report, avp_str(avp_sprintf(a, "%s: the conflict markers could not be read (nested or incomplete)", path)));
        return false;
    }
    bool all = true;
    for (size_t h = 0; h < hunks.n; h++) {
        Hunk *k = &hunks.v[h];
        avp_vec_push(report, avp_str(avp_sprintf(a, "%s hunk %zu at line %zu: main %zu line(s), lane %zu line(s)", path, h + 1,
                                                 k->first_line, k->ours.n, k->theirs.n)));
        for (size_t i = 0; i < k->ours.n && i < 6; i++)
            avp_vec_push(report, avp_str(avp_sprintf(a, "    main| %.*s", AVP_SF(cut(k->ours.v[i])))));
        if (k->ours.n > 6) avp_vec_push(report, avp_str(avp_sprintf(a, "    main| ... %zu more", k->ours.n - 6)));
        for (size_t i = 0; i < k->theirs.n && i < 6; i++)
            avp_vec_push(report, avp_str(avp_sprintf(a, "    lane| %.*s", AVP_SF(cut(k->theirs.v[i])))));
        if (k->theirs.n > 6) avp_vec_push(report, avp_str(avp_sprintf(a, "    lane| ... %zu more", k->theirs.n - 6)));
        if (k->resolvable) {
            AvpStr names = k->lane_wins ? avp_str_join(a, AVP_S(", "), k->lane_win_names.v, k->lane_win_names.n) : AVP_S("");
            avp_vec_push(report, avp_str(avp_sprintf(a,
                "    union: keeps main's %zu unit(s), adds the lane's %zu, takes the lane's version for %zu named by both%s%.*s",
                k->kept, k->added, k->lane_wins, k->lane_wins ? " (" : "", AVP_SF(names))));
            if (k->lane_wins) report->v[report->n - 1] = avp_str_concat(a, report->v[report->n - 1], AVP_S(")"));
        } else {
            avp_vec_push(report, avp_str(avp_sprintf(a, "    no union offered: %s; resolve this one by hand", k->why)));
            all = false;
        }
    }
    if (!all) return false;
    /* the file with every hunk replaced by its union */
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t pi = 0;
    bool first = true;
    for (size_t i = 0; i < slots.n; i++) {
        if (slots.v[i] < 0) {
            if (!first) avp_buf_putc(&b, '\n');
            first = false;
            avp_buf_put(&b, plain.v[pi++]);
        } else {
            pi++;
            const Hunk *k = &hunks.v[slots.v[i]];
            for (size_t m = 0; m < k->merged.n; m++) {
                if (!first) avp_buf_putc(&b, '\n');
                first = false;
                avp_buf_put(&b, k->merged.v[m]);
            }
        }
    }
    *resolved = avp_buf_str(&b);
    return true;
}
