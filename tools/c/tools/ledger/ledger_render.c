/* ledger: rendering and splicing (fmt_pct ... generate of tools/ledger.py). */
#include "ledger.h"
#include <string.h>

#define HDR_NUMBERS "| measure | functions | code bytes |"
#define HDR_ROWS "| unit | function | address | size | status | naming |"
#define SEP_NUMBERS "|---|---|---|"
#define SEP_ROWS "|---|---|---|---|---|---|"
#define UNCONFIRMED "unit name unconfirmed"
#define ASM_NOTE "ORIGINAL-ASM when linked; not counted"
#define HELPER_NOTE "C with an inline-asm block; listed helper when linked"

static const char *const SDK_GROUPS[] = {"revolution/NdevExi2AD/", "MSL_C/", "Runtime.PPCEABI.H/", "MetroTRK/",
                                         "libjpeg/", "revolution/"};
#define N_GROUPS 6
static const char *const AUTO_NAMING[] = {"placeholder",
                                          "placeholder (compiler-generated name)",
                                          "compiler-generated thunk",
                                          "compiler-generated thunk, placeholder",
                                          "open (placeholder)",
                                          UNCONFIRMED};

AvpStr ledger_fmt_pct(AvpArena *a, double p) {
    if (p >= 100.0) return AVP_S("100%");
    double r = avp_py_round(p, 1);
    if (99.9 < r) r = 99.9; /* min(round(p, 1), 99.9) */
    return avp_pyfmt(a, "%.1f%%", r);
}

AvpStrVec ledger_auto_naming(AvpStr name) {
    AvpStrVec out = {0};
    AvpStr target;
    if (ledger_thunk_target(name, &target)) {
        avp_vec_push(&out, ledger_is_placeholder(target) ? AVP_S("compiler-generated thunk, placeholder")
                                                         : AVP_S("compiler-generated thunk"));
    } else if (avp_str_startswith(name, AVP_S("__sinit_"))) {
        avp_vec_push(&out, AVP_S("placeholder (compiler-generated name)"));
    } else if (ledger_is_placeholder(name)) {
        avp_vec_push(&out, AVP_S("placeholder"));
    }
    return out;
}

const char *ledger_note_class(AvpStr note) {
    if (!note.p || !note.n) return NULL;
    if (avp_str_contains(note, AVP_S("MATCH-PENDING"))) return "pending";
    if (avp_str_contains(note, AVP_S("ORIGINAL-ASM"))) return "asm";
    if (avp_str_contains(note, AVP_S("listed helper")) || avp_str_contains(note, AVP_S("inline-asm"))) return "helper";
    return NULL;
}

static AvpStr join_rules(AvpArena *a, const AvpStrVec *rules) {
    if (!rules->n) return AVP_S("see exceptions.md");
    return avp_str_join(a, AVP_S(", "), rules->v, rules->n);
}

AvpStr ledger_status_cell(AvpArena *a, const LedgerFn *f, AvpStr old_base, AvpStr old_note) {
    if (f->linked) {
        if (strcmp(f->kind, "ORIGINAL-ASM") == 0) return AVP_S("ORIGINAL-ASM");
        if (strcmp(f->kind, "MATCH-PENDING") == 0) {
            bool keep = old_note.n && avp_str_startswith(old_base, AVP_S("MATCH-PENDING"));
            AvpStr note = keep ? old_note : join_rules(a, &f->rules);
            return avp_pyfmt(a, "MATCH-PENDING (%S)", &note);
        }
        return AVP_S("matched");
    }
    AvpStr score;
    if (!f->pct) score = f->asm_fn ? AVP_S("asm") : AVP_S("not written");
    else score = ledger_fmt_pct(a, avp_json_f64(f->pct, 0.0));
    const char *cls = f->asm_fn ? "asm" : f->pending ? "pending" : f->asm_block ? "helper" : NULL;
    AvpStr cell = avp_pyfmt(a, "parked, %S", &score);
    if (cls) {
        AvpStr dflt;
        if (strcmp(cls, "asm") == 0) dflt = AVP_S(ASM_NOTE);
        else if (strcmp(cls, "pending") == 0) {
            AvpStr r = join_rules(a, &f->rules);
            dflt = avp_pyfmt(a, "MATCH-PENDING: %S, exceptions.md", &r);
        } else dflt = AVP_S(HELPER_NOTE);
        const char *oc = ledger_note_class(old_note);
        AvpStr note = (oc && strcmp(oc, cls) == 0) ? old_note : dflt;
        cell = avp_pyfmt(a, "%S (%S)", &cell, &note);
    }
    return cell;
}

static bool is_auto(AvpStr t) {
    for (size_t i = 0; i < sizeof AUTO_NAMING / sizeof AUTO_NAMING[0]; i++)
        if (avp_str_eqz(t, AUTO_NAMING[i])) return true;
    return false;
}

static AvpStr naming_cell(AvpArena *a, const LedgerFn *f, const LedgerUnit *u, AvpStr old_naming) {
    AvpStrVec toks = ledger_auto_naming(f->name);
    if (u->unconfirmed && strcmp(u->section, "game") != 0) avp_vec_push(&toks, AVP_S(UNCONFIRMED));
    if (old_naming.n) {
        AvpStrVec parts = avp_str_split(old_naming, AVP_S("; "), -1);
        for (size_t i = 0; i < parts.n; i++) {
            AvpStr t = avp_str_strip(parts.v[i]);
            if (!t.n || is_auto(t)) continue;
            bool dup = false;
            for (size_t k = 0; k < toks.n && !dup; k++) dup = avp_str_eq(toks.v[k], t);
            if (!dup) avp_vec_push(&toks, t);
        }
        avp_vec_free(&parts);
    }
    AvpStr s = avp_str_join(a, AVP_S("; "), toks.v, toks.n);
    avp_vec_free(&toks);
    return s;
}

static AvpStr unit_cell(AvpArena *a, const LedgerUnit *u) {
    if (u->unconfirmed && strcmp(u->section, "game") == 0) return avp_pyfmt(a, "`%S` (" UNCONFIRMED ")", &u->name);
    return avp_pyfmt(a, "`%S`", &u->name);
}

/* parse_old_rows */
typedef struct {
    AvpStrVec order[2]; /* sdk, game */
    AvpMap *notes;      /* "unit\0addr" -> AvpStr[2] (status, naming) */
    i64 count;
} OldRows;

static AvpStr note_key(AvpArena *a, AvpStr u, i64 addr) {
    return avp_pyfmt(a, "%S\x01%d", &u, addr);
}

static OldRows parse_old_rows(AvpArena *a, AvpStr text) {
    OldRows o = {0};
    o.notes = avp_map_new();
    int section = -1;
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    for (size_t li = 0; li < lines.n; li++) {
        AvpStr ln = lines.v[li];
        if (avp_str_startswith(ln, AVP_S("## "))) {
            section = avp_str_startswith(ln, AVP_S("## Game code"))        ? 1
                      : avp_str_startswith(ln, AVP_S("## Function status")) ? 0
                                                                            : -1;
            continue;
        }
        AvpStr g[6];
        if (section < 0 || !ledger_match_row(ln, g)) continue;
        AvpStr u = avp_str_replace(a, g[0], AVP_S("`"), AVP_S(""), -1);
        u = avp_str_strip(avp_str_replace(a, u, AVP_S(" (" UNCONFIRMED ")"), AVP_S(""), -1));
        bool known = false;
        for (size_t k = 0; k < o.order[section].n && !known; k++) known = avp_str_eq(o.order[section].v[k], u);
        if (!known) avp_vec_push(&o.order[section], u);
        u64 addr = 0;
        avp_parse_uint(avp_str_slice(g[2], 2, (i64)g[2].n), 16, &addr);
        AvpStr *pair = AVP_NEWN(a, AvpStr, 2);
        pair[0] = g[4];
        pair[1] = g[5];
        bool c;
        avp_map_put(o.notes, note_key(a, u, (i64)addr), &c)->p = pair;
        o.count++;
    }
    avp_vec_free(&lines);
    return o;
}

static int group_rank(AvpStr name) {
    for (int i = 0; i < N_GROUPS; i++)
        if (avp_str_startswith(name, avp_str(SDK_GROUPS[i]))) return i;
    return N_GROUPS;
}

typedef struct {
    int rank;
    i64 addr;
    AvpStr name;
} PureKey;
static PureKey pure_key(const LedgerUnit *u) {
    PureKey k = {strcmp(u->section, "sdk") == 0 ? group_rank(u->name) : 0, ledger_unit_first_addr(u), u->name};
    return k;
}
static int key_cmp(PureKey x, PureKey y) {
    if (x.rank != y.rank) return x.rank < y.rank ? -1 : 1;
    if (x.addr != y.addr) return x.addr < y.addr ? -1 : 1;
    return avp_str_cmp(x.name, y.name);
}
static int unit_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return key_cmp(pure_key(*(LedgerUnit *const *)x), pure_key(*(LedgerUnit *const *)y));
}

LedgerUnitVec ledger_order_units(AvpArena *a, const LedgerUnitVec *units, const AvpStrVec *old_order) {
    (void)a;
    AvpMap *by_name = avp_map_new();
    for (size_t i = 0; i < units->n; i++) {
        bool c;
        avp_map_put(by_name, units->v[i]->name, &c)->p = units->v[i];
    }
    LedgerUnitVec out = {0};
    AvpSet *in_out = avp_set_new();
    for (size_t i = 0; i < old_order->n; i++) {
        AvpVal *v = avp_map_get(by_name, old_order->v[i]);
        if (v) {
            avp_vec_push(&out, (LedgerUnit *)v->p);
            avp_set_add(in_out, old_order->v[i]);
        }
    }
    LedgerUnitVec fresh = {0};
    for (size_t i = 0; i < units->n; i++)
        if (!avp_set_has(in_out, units->v[i]->name)) avp_vec_push(&fresh, units->v[i]);
    avp_sort(fresh.v, fresh.n, sizeof *fresh.v, unit_cmp, NULL, false);
    for (size_t i = 0; i < fresh.n; i++) {
        LedgerUnit *u = fresh.v[i];
        PureKey k = pure_key(u);
        LedgerUnit *pred = NULL, *succ = NULL;
        for (size_t j = 0; j < out.n; j++) {
            PureKey xk = pure_key(out.v[j]);
            if (xk.rank != k.rank) continue;
            int c = key_cmp(xk, k);
            if (c < 0 && (!pred || key_cmp(xk, pure_key(pred)) > 0)) pred = out.v[j];
            if (c > 0 && (!succ || key_cmp(xk, pure_key(succ)) < 0)) succ = out.v[j];
        }
        size_t pos = out.n;
        LedgerUnit *anchor = pred ? pred : succ;
        if (anchor) {
            for (size_t j = 0; j < out.n; j++)
                if (avp_str_eq(out.v[j]->name, anchor->name)) {
                    pos = pred ? j + 1 : j;
                    break;
                }
        }
        avp_vec_insert(&out, pos, u);
    }
    avp_vec_free(&fresh);
    avp_map_free(by_name);
    avp_set_free(in_out);
    return out;
}

static AvpStr render_rows(AvpArena *a, const LedgerUnitVec *units, AvpMap *notes, AvpStrVec *warnings) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, HDR_ROWS "\n" SEP_ROWS);
    for (size_t i = 0; i < units->n; i++) {
        const LedgerUnit *u = units->v[i];
        AvpStr ucell = unit_cell(a, u);
        for (size_t k = 0; k < u->fns.n; k++) {
            const LedgerFn *f = u->fns.v[k];
            AvpVal *ov = avp_map_get(notes, note_key(a, u->name, f->addr));
            AvpStr old_status = ov ? ((AvpStr *)ov->p)[0] : AVP_S("");
            AvpStr old_naming = ov ? ((AvpStr *)ov->p)[1] : AVP_S("");
            AvpStr old_base = {NULL, 0}, old_note = {NULL, 0};
            if (old_status.n) ledger_parse_status_cell(old_status, &old_base, &old_note);
            AvpStr cell = ledger_status_cell(a, f, old_base, old_note);
            if (warnings && old_note.n && ledger_note_class(old_note) && !avp_str_contains(cell, old_note))
                avp_vec_push(warnings, avp_pyfmt(a,
                                                 "%S::%S: the old status note \"%S\" is dropped (nothing in the checker or "
                                                 "docs/exceptions.md supports it)",
                                                 &u->name, &f->name, &old_note));
            AvpStr naming = naming_cell(a, f, u, old_naming);
            avp_buf_pyfmt(&b, "\n| %S | `%S` | 0x%08X | %d | %S | %S |", &ucell, &f->name, f->addr, f->size, &cell,
                          &naming);
        }
    }
    return avp_buf_str(&b);
}

static AvpStr render_numbers(AvpArena *a, const LedgerNumbers *n) {
    return avp_pyfmt(a,
                     HDR_NUMBERS "\n" SEP_NUMBERS "\n"
                                 "| **matched under the rules** (headline) | %d / %d | %d / %d |\n"
                                 "| bytes matched (linked from source, excluding `ORIGINAL-ASM`) | %d / %d | %d / %d |\n"
                                 "| `ORIGINAL-ASM` (linked, listed apart, outside both counts and totals) | %d | %d |\n"
                                 "| linked from source in total (objdiff \"complete\") | %d | %d |\n"
                                 "| `Matching` units | %d / %d | |",
                     n->rules_n, n->total_n, n->rules_b, n->total_b, n->bytes_n, n->total_n, n->bytes_b, n->total_b,
                     n->asm_n, n->asm_b, n->linked_n, n->linked_b, n->units_complete, n->units_total);
}

/* find_table: false when not found. */
static bool find_table(const AvpStrVec *lines, const char *header, const char *after_heading, size_t *s, size_t *e) {
    size_t start = 0;
    bool found = false;
    for (size_t i = 0; i < lines->n; i++)
        if (avp_str_startswith(lines->v[i], avp_str(after_heading))) {
            start = i + 1;
            found = true;
            break;
        }
    if (!found) return false;
    for (size_t i = start; i < lines->n; i++) {
        if (avp_str_startswith(lines->v[i], avp_str(header))) {
            size_t j = i;
            while (j < lines->n && avp_str_startswith(lines->v[j], AVP_S("|"))) j++;
            *s = i;
            *e = j;
            return true;
        }
        if (avp_str_startswith(lines->v[i], AVP_S("## "))) return false;
    }
    return false;
}

static AvpStr splice(AvpArena *a, AvpStr text, const char *name, AvpStr content, bool block, bool add_markers,
                     const char **how) {
    AvpStr begin = avp_pyfmt(a, "<!-- ledger:begin %s -->", name);
    AvpStr end = avp_pyfmt(a, "<!-- ledger:end %s -->", name);
    i64 bi = avp_str_find(text, begin, 0, (i64)text.n);
    if (bi >= 0) {
        size_t inner_s = (size_t)bi + begin.n;
        i64 ei = avp_str_find(text, end, (i64)inner_s, (i64)text.n);
        if (ei >= 0) {
            AvpBuf b;
            avp_buf_init(&b, a);
            avp_buf_putn(&b, text.p, inner_s);
            if (block) avp_buf_putc(&b, '\n');
            avp_buf_put(&b, content);
            if (block) avp_buf_putc(&b, '\n');
            avp_buf_putn(&b, text.p + ei, text.n - (size_t)ei);
            *how = "marker";
            return avp_buf_str(&b);
        }
    }
    const char *how_located = add_markers ? "located+marked" : "located";
    if (strcmp(name, "gross") == 0) {
        size_t s, e;
        if (!ledger_find_gross(text, &s, &e)) {
            *how = NULL;
            return text;
        }
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_putn(&b, text.p, s);
        if (add_markers) avp_buf_put(&b, begin);
        avp_buf_put(&b, content);
        if (add_markers) avp_buf_put(&b, end);
        avp_buf_putn(&b, text.p + e, text.n - e);
        *how = how_located;
        return avp_buf_str(&b);
    }
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    size_t s = 0, e = 0;
    bool ok;
    if (strcmp(name, "numbers") == 0) ok = find_table(&lines, HDR_NUMBERS, "## Numbers", &s, &e);
    else if (strcmp(name, "status") == 0) ok = find_table(&lines, HDR_ROWS, "## Function status", &s, &e);
    else ok = find_table(&lines, HDR_ROWS, "## Game code", &s, &e);
    if (!ok) {
        avp_vec_free(&lines);
        *how = NULL;
        return text;
    }
    AvpBuf b;
    avp_buf_init(&b, a);
    bool first = true;
#define PUT_LINE(x)                          \
    do {                                     \
        if (!first) avp_buf_putc(&b, '\n'); \
        first = false;                       \
        avp_buf_put(&b, (x));                \
    } while (0)
    for (size_t i = 0; i < s; i++) PUT_LINE(lines.v[i]);
    if (add_markers) PUT_LINE(begin);
    AvpStrVec body = avp_str_split(content, AVP_S("\n"), -1);
    for (size_t i = 0; i < body.n; i++) PUT_LINE(body.v[i]);
    if (add_markers) PUT_LINE(end);
    for (size_t i = e; i < lines.n; i++) PUT_LINE(lines.v[i]);
#undef PUT_LINE
    avp_vec_free(&body);
    avp_vec_free(&lines);
    *how = how_located;
    return avp_buf_str(&b);
}

AvpStr ledger_generate(AvpArena *a, AvpStr text, const LedgerModel *m, AvpStrVec *warnings, bool add_markers,
                       const char *how[4], i64 *old_count, i64 *new_count) {
    OldRows old = parse_old_rows(a, text);
    LedgerUnitVec sec[2] = {{0}, {0}};
    i64 count = 0;
    AvpMapIter it = {0};
    while (avp_map_next(m->units, &it)) {
        LedgerUnit *u = (LedgerUnit *)it.val->p;
        if (!u->fns.n) continue;
        count += (i64)u->fns.n;
        avp_vec_push(&sec[strcmp(u->section, "game") == 0 ? 1 : 0], u);
    }
    static const char *const names[2] = {"status", "game"};
    for (int k = 0; k < 2; k++) {
        LedgerUnitVec ordered = ledger_order_units(a, &sec[k], &old.order[k]);
        text = splice(a, text, names[k], render_rows(a, &ordered, old.notes, warnings), true, add_markers, &how[k]);
        avp_vec_free(&ordered);
    }
    text = splice(a, text, "numbers", render_numbers(a, &m->numbers), true, add_markers, &how[2]);
    text = splice(a, text, "gross",
                  avp_pyfmt(a, "%d functions and %d code bytes", m->numbers.gross_n, m->numbers.gross_b), false,
                  add_markers, &how[3]);
    *old_count = old.count;
    *new_count = count;
    return text;
}
