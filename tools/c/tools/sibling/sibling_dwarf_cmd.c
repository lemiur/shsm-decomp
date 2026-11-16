/* sibling: the DWARF subcommands (types, fn, cu) over the sibling's ELF. */
#include "sibling_dwarf.h"

#include "avp/fs.h"
#include "avp/map.h"
#include "avp/sort.h"
#include "shsm_mangle.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ---- loading ---- */

typedef struct {
    const char *path;
    SdInfo *d;
} Dw;

static const char *meta_value(AvpArena *a, Sibling *sb, const char *key) {
    sqlite3_stmt *st = NULL;
    const char *v = NULL;
    if (sqlite3_prepare_v2(sb->db, "select value from meta where key=?", -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
    if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_text(st, 0))
        v = avp_strdup(a, (const char *)sqlite3_column_text(st, 0));
    sqlite3_finalize(st);
    return v;
}

static const char *meta_elf(AvpArena *a, Sibling *sb) { return meta_value(a, sb, "elf"); }

static const char *opt(AvpArena *a, AvpArgs *args, const char *key) {
    if (!avp_args_has(args, key) || avp_args_is_none(args, key)) return NULL;
    return avp_cz(a, avp_args_str(args, key));
}

/* The ELF to read: --elf, else the one index recorded, else the default; --build picks the Release or Debug ELF
 * beside it (Final in the file name replaced). `build` NULL: the --build option. */
static const char *elf_path(AvpArena *a, Sibling *sb, AvpArgs *args, const char *build) {
    const char *p = opt(a, args, "elf");
    if (!p) p = meta_elf(a, sb);
    if (!p) p = shsm_repo_shared_path(a, sb->repo, "orig/_ref/gormiti_wii/Exe/GormitiFinal.elf");
    if (!build) build = opt(a, args, "build");
    if (!build || strcmp(build, "final") == 0) return p;
    AvpStr head, tail;
    avp_path_split(p, &head, &tail);
    i64 at = avp_str_rfind(tail, AVP_S("Final"), 0, (i64)tail.n);
    if (at < 0) shsm_raise(SHSM_EXC_USAGE, "--build %s: the ELF %s has no 'Final' in its name to replace (pass --elf)", build, p);
    const char *word = strcmp(build, "release") == 0 ? "Release" : "Debug";
    const char *name = avp_sprintf(a, "%.*s%s%.*s", (int)at, tail.p, word, (int)(tail.n - (size_t)at - 5), tail.p + at + 5);
    return avp_path_join(a, avp_cz(a, head), name);
}

static Dw load_path(AvpArena *a, const char *path) {
    Dw w = {0};
    w.path = path;
    if (!avp_isfile(w.path)) shsm_raise(SHSM_EXC_FILE_NOT_FOUND, "sibling ELF not found: %s (pass --elf)", w.path);
    AvpBytes b = {0};
    if (!avp_read_all(a, w.path, &b)) shsm_raise_io();
    SdSections sec;
    if (!sd_elf_sections(avp_strn((const char *)b.v, b.n), &sec))
        shsm_raise(SHSM_EXC_VALUE, "%s: not a big-endian ELF32 with .debug_info and .debug_abbrev", w.path);
    w.d = sd_parse(a, &sec);
    return w;
}

static void print_notes(AvpArena *a, const SdInfo *d, ShsmOutput *out) {
    for (size_t k = 0; k < d->notes.n && k < 5; k++)
        shsm_output_linez(out, avp_sprintf(a, "note: %.*s", AVP_SF(d->notes.v[k])));
    if (d->notes.n > 5) shsm_output_linez(out, avp_sprintf(a, "note: %zu more reader notes (--json lists them)", d->notes.n - 5));
}

static AvpJson *notes_json(AvpArena *a, const SdInfo *d) {
    AvpJson *n = avp_json_arr(a);
    for (size_t k = 0; k < d->notes.n; k++) avp_json_arr_push(a, n, avp_json_str(a, d->notes.v[k]));
    return n;
}

static u32 idx(const SdInfo *d, const SdDie *x) { return sd_index(d, x); }

/* ---- types ---- */

typedef struct {
    AvpArena *a;
    const SdInfo *d;
    AvpMap *seen;       /* names already rendered or queued */
    AVP_VEC(u32) queue; /* DIE offsets of dependencies to render (--full) */
    AVP_VEC(const char *) forward; /* names only reached through pointers */
    bool full;
} Deps;

static const char *access_kw(u8 acc) { return acc == 2 ? "protected" : acc == 3 ? "private" : "public"; }

static const char *kw(unsigned tag) {
    return tag == SD_TAG_class_type ? "class" : tag == SD_TAG_union_type ? "union"
           : tag == SD_TAG_enumeration_type ? "enum" : "struct";
}

/* Record the named types a member type depends on: by value (render) or through a pointer (forward). */
static void depend(Deps *dp, u32 type, bool by_value, int depth) {
    if (!dp || !dp->full || !type || depth > 32) return;
    const SdDie *x = sd_die_at(dp->d, type);
    if (!x) return;
    switch (x->tag) {
    case SD_TAG_pointer_type:
    case SD_TAG_reference_type:
    case SD_TAG_ptr_to_member_type: depend(dp, x->type, false, depth + 1); return;
    case SD_TAG_array_type:
    case SD_TAG_const_type:
    case SD_TAG_volatile_type: depend(dp, x->type, by_value, depth + 1); return;
    case SD_TAG_subroutine_type: return;
    default: break;
    }
    if (!sd_is_named_type(x->tag) || !x->name) return;
    bool created;
    AvpVal *v = avp_map_put(dp->seen, avp_str(x->name), &created);
    if (by_value || x->tag == SD_TAG_typedef) {
        if (created || v->i == 1) {
            v->i = 2;
            avp_vec_push(&dp->queue, x->off);
        }
    } else if (created) {
        v->i = 1;
        avp_vec_push(&dp->forward, x->name);
    }
}

static void pad_comment(AvpBuf *b, size_t line_start, const char *comment) {
    size_t col = b->n - line_start;
    while (col < 52) { avp_buf_putc(b, ' '); col++; }
    avp_buf_printf(b, " // %s", comment);
}

static void render(AvpBuf *b, Deps *dp, const SdDie *x, const char *indent, const char *typedef_name);

static void render_enum(AvpBuf *b, const SdInfo *d, const SdDie *x, const char *indent, const char *name) {
    avp_buf_printf(b, "%senum %s {", indent, name);
    int n = 0;
    for (u32 k = idx(d, x) + 1; k < x->next; k++) {
        const SdDie *e = &d->dies[k];
        if (e->parent != idx(d, x) || e->tag != SD_TAG_enumerator) continue;
        avp_buf_printf(b, "%s\n%s    %s = %lld", n++ ? "," : "", indent, e->name ? e->name : "?", (long long)e->const_value);
    }
    avp_buf_printf(b, "\n%s}", indent);
}

static void render_aggregate(AvpBuf *b, Deps *dp, const SdDie *x, const char *indent, const char *name) {
    AvpArena *a = dp->a;
    const SdInfo *d = dp->d;
    u32 i = idx(d, x);
    /* header with bases */
    avp_buf_printf(b, "%s%s %s", indent, kw(x->tag), name);
    u32 cursor = 0;
    int nb = 0;
    for (u32 k = i + 1; k < x->next; k++) {
        const SdDie *m = &d->dies[k];
        if (m->parent != i || m->tag != SD_TAG_inheritance) continue;
        const SdDie *bt = sd_die_at(d, m->type);
        u32 bsize = sd_type_size(d, m->type);
        avp_buf_printf(b, "%s%s%s %s /* 0x%02X, size 0x%X */", nb++ ? ", " : " : ", m->virtuality ? "virtual " : "",
                       access_kw(m->accessibility), sd_type_name(a, d, m->type), m->member_loc, bsize);
        if (m->member_loc + bsize > cursor) cursor = m->member_loc + bsize;
        if (bt) depend(dp, m->type, true, 0);
    }
    avp_buf_printf(b, " { // size 0x%X\n", x->byte_size);
    u32 bases_end = cursor;
    const char *in2 = avp_sprintf(a, "%s    ", indent);
    bool is_union = x->tag == SD_TAG_union_type;
    for (u32 k = i + 1; k < x->next; k++) {
        const SdDie *m = &d->dies[k];
        if (m->parent != i || m->tag == SD_TAG_inheritance) continue;
        if (m->tag != SD_TAG_member) {
            const char *tn = sd_tag_name(m->tag);
            avp_buf_printf(b, "%s// %s %s (not shown)\n", in2, tn ? tn : avp_sprintf(a, "tag 0x%x", m->tag), m->name ? m->name : "");
            continue;
        }
        u32 off = (m->flags & SD_F_MEMBER_LOC) ? m->member_loc : 0;
        u32 size = (m->flags & SD_F_BITS) && (m->flags & SD_F_SIZE) ? m->byte_size : sd_type_size(d, m->type);
        if (!is_union && off > cursor) {
            size_t ls = b->n;
            avp_buf_printf(b, "%s// gap", in2);
            const char *what = avp_sprintf(a, "0x%02X-0x%02X (%u byte%s)%s", cursor, off, off - cursor, off - cursor == 1 ? "" : "s",
                                           off - cursor == 4 && cursor == bases_end && (cursor & 3) == 0 ? " vtable pointer?" : "");
            pad_comment(b, ls, what);
            avp_buf_putc(b, '\n');
        }
        size_t ls = b->n;
        avp_buf_putz(b, in2);
        const SdDie *mt = sd_die_at(d, m->type);
        if (mt && sd_is_named_type(mt->tag) && !mt->name && mt->tag != SD_TAG_typedef) {
            /* an anonymous aggregate member: render it inline */
            AvpBuf inner;
            avp_buf_init(&inner, a);
            render(&inner, dp, mt, "", "");
            AvpStr body = avp_str_rstrip(avp_buf_str(&inner));
            AvpStrVec lines = avp_str_split(body, AVP_S("\n"), -1);
            for (size_t q = 0; q < lines.n; q++) avp_buf_printf(b, "%s%.*s", q ? avp_sprintf(a, "\n%s", in2) : "", AVP_SF(lines.v[q]));
            avp_vec_free(&lines);
            avp_buf_printf(b, " %s;", m->name ? m->name : "");
        } else {
            avp_buf_putz(b, sd_decl(a, d, m->type, m->name ? m->name : ""));
            if (m->flags & SD_F_BITS) avp_buf_printf(b, " : %u", m->bit_size);
            avp_buf_putc(b, ';');
            depend(dp, m->type, true, 0);
        }
        const char *c = (m->flags & SD_F_BITS)
                            ? avp_sprintf(a, "0x%02X bits %u-%u from the MSB of a %u-byte unit", off, m->bit_offset,
                                          m->bit_offset + m->bit_size - 1, m->byte_size)
                            : avp_sprintf(a, "0x%02X", off);
        pad_comment(b, ls, c);
        avp_buf_putc(b, '\n');
        if (off + size > cursor) cursor = off + size;
    }
    if (!is_union && x->byte_size > cursor) {
        size_t ls = b->n;
        avp_buf_printf(b, "%s// tail", in2);
        pad_comment(b, ls, avp_sprintf(a, "0x%02X-0x%02X (%u bytes)%s", cursor, x->byte_size, x->byte_size - cursor,
                                       x->byte_size - cursor == 4 && cursor == bases_end ? " vtable pointer?" : ""));
        avp_buf_putc(b, '\n');
    }
    avp_buf_printf(b, "%s}", indent);
}

static void render(AvpBuf *b, Deps *dp, const SdDie *x, const char *indent, const char *typedef_name) {
    AvpArena *a = dp->a;
    const SdInfo *d = dp->d;
    if (x->tag == SD_TAG_typedef) {
        const SdDie *t = sd_die_at(d, x->type);
        if (t && sd_is_named_type(t->tag) && t->tag != SD_TAG_typedef && !t->name) {
            avp_buf_printf(b, "%stypedef ", indent);
            AvpBuf inner;
            avp_buf_init(&inner, a);
            render(&inner, dp, t, indent, "");
            avp_buf_put(b, avp_str_lstrip(avp_buf_str(&inner)));
            avp_buf_printf(b, " %s;\n", x->name ? x->name : "?");
            return;
        }
        avp_buf_printf(b, "%stypedef %s;\n", indent, sd_decl(a, d, x->type, x->name ? x->name : "?"));
        depend(dp, x->type, true, 0);
        return;
    }
    const char *name = x->name ? x->name : (typedef_name ? typedef_name : "");
    if (x->tag == SD_TAG_enumeration_type) render_enum(b, d, x, indent, name);
    else render_aggregate(b, dp, x, indent, name);
    if (x->name) avp_buf_putc(b, ';');
    if (x->tag == SD_TAG_enumeration_type) avp_buf_printf(b, " // size 0x%X", x->byte_size);
    avp_buf_putc(b, '\n');
}

static bool is_definition(const SdDie *x) {
    if (x->tag == SD_TAG_typedef) return true;
    return (x->flags & SD_F_CHILDREN) && x->next > 0 && (x->byte_size > 0 || x->tag == SD_TAG_enumeration_type);
}

typedef struct {
    const char *text;
    const SdDie *die;
    i64 count;
    AVP_VEC(u32) cus;
} Layout;

static int cmp_cz(const void *x, const void *y) { return strcmp(*(const char *const *)x, *(const char *const *)y); }

static int types_list(AvpArena *a, const Dw *w, ShsmOutput *out) {
    const SdInfo *d = w->d;
    AvpMap *m = avp_map_new();
    AVP_VEC(const char *) names = {0};
    typedef struct { const char *name; unsigned tag; u32 size, members, defs; } Row;
    AVP_VEC(Row) rows = {0};
    for (size_t k = 0; k < d->ndies; k++) {
        const SdDie *x = &d->dies[k];
        if (!sd_is_aggregate(x->tag) || !x->name || !is_definition(x)) continue;
        bool created;
        AvpVal *v = avp_map_put(m, avp_str(x->name), &created);
        if (created) {
            u32 members = 0;
            for (u32 q = (u32)k + 1; q < x->next; q++)
                if (d->dies[q].parent == k && d->dies[q].tag == SD_TAG_member) members++;
            Row r = {x->name, x->tag, x->byte_size, members, 0};
            avp_vec_push(&rows, r);
            v->i = (i64)rows.n - 1;
            avp_vec_push(&names, x->name);
        }
        rows.v[v->i].defs++;
    }
    qsort(names.v, names.n, sizeof(char *), cmp_cz);
    shsm_output_linez(out, avp_sprintf(a, "%zu named class/struct/union types with a definition in %s", names.n, w->path));
    AvpJson *arr = avp_json_arr(a);
    for (size_t k = 0; k < names.n; k++) {
        Row *r = &rows.v[avp_map_get(m, avp_str(names.v[k]))->i];
        shsm_output_linez(out, avp_sprintf(a, "%-6s %-48s size 0x%-5X %4u members  %4u units", kw(r->tag), r->name, r->size, r->members, r->defs));
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("name"), avp_json_strz(a, r->name));
        avp_json_set(a, o, AVP_S("kind"), avp_json_strz(a, kw(r->tag)));
        avp_json_set(a, o, AVP_S("size"), avp_json_int(a, r->size));
        avp_json_set(a, o, AVP_S("members"), avp_json_int(a, r->members));
        avp_json_set(a, o, AVP_S("units"), avp_json_int(a, r->defs));
        avp_json_arr_push(a, arr, o);
    }
    shsm_output_data(out, "types", arr);
    avp_vec_free(&names);
    avp_vec_free(&rows);
    avp_map_free(m);
    return 0;
}

static int cmd_types(AvpArena *a, const Dw *w, AvpArgs *args, ShsmOutput *out) {
    const SdInfo *d = w->d;
    const char *want = opt(a, args, "type");
    if (!want) return types_list(a, w, out);
    bool full = avp_args_has(args, "full") && avp_args_bool(args, "full");
    AVP_VEC(const SdDie *) hits = {0};
    const char *lookup = want, *qual_note = NULL;
    for (int pass = 0; pass < 2 && !hits.n; pass++) {
        if (pass == 1) {
            const char *colon = strrchr(want, ':');
            if (!colon || colon == want) break;
            lookup = colon + 1;
            qual_note = "DWARF names here are unqualified (MWCC emits no namespaces): matched on the last component";
        }
        for (size_t k = 0; k < d->ndies; k++)
            if (sd_is_named_type(d->dies[k].tag) && d->dies[k].name && strcmp(d->dies[k].name, lookup) == 0)
                avp_vec_push(&hits, &d->dies[k]);
    }
    if (!hits.n) {
        AvpStr lw = avp_str_lower(a, avp_str(lookup));
        AvpMap *seen = avp_map_new();
        AVP_VEC(const char *) cand = {0};
        for (size_t k = 0; k < d->ndies; k++) {
            const SdDie *x = &d->dies[k];
            if (!sd_is_named_type(x->tag) || !x->name) continue;
            if (!avp_str_contains(avp_str_lower(a, avp_str(x->name)), lw)) continue;
            bool created;
            avp_map_put(seen, avp_str(x->name), &created);
            if (created) avp_vec_push(&cand, x->name);
        }
        qsort(cand.v, cand.n, sizeof(char *), cmp_cz);
        shsm_output_linez(out, avp_sprintf(a, "no type named %s; %zu names contain it", want, cand.n));
        AvpJson *arr = avp_json_arr(a);
        for (size_t k = 0; k < cand.n; k++) {
            shsm_output_linez(out, avp_sprintf(a, "  %s", cand.v[k]));
            avp_json_arr_push(a, arr, avp_json_strz(a, cand.v[k]));
        }
        shsm_output_data(out, "candidates", arr);
        avp_map_free(seen);
        avp_vec_free(&cand);
        avp_vec_free(&hits);
        return cand.n ? 0 : 1;
    }
    /* distinct layouts */
    bool any_def = false;
    for (size_t k = 0; k < hits.n; k++) any_def |= is_definition(hits.v[k]);
    AVP_VEC(Layout) lays = {0};
    AvpMap *by_text = avp_map_new();
    size_t decls = 0;
    Deps plain = {a, d, NULL, {0}, {0}, false};
    for (size_t k = 0; k < hits.n; k++) {
        const SdDie *x = hits.v[k];
        if (any_def && !is_definition(x)) { decls++; continue; }
        AvpBuf b;
        avp_buf_init(&b, a);
        render(&b, &plain, x, "", NULL);
        /* a C unit says struct where a C++ unit says class: the same layout */
        AvpStr key = avp_buf_str(&b);
        i64 sp = avp_str_findc(key, ' ');
        if (sd_is_aggregate(x->tag) && sp > 0) key = avp_str_slice(key, sp, (i64)key.n);
        bool created;
        AvpVal *v = avp_map_put(by_text, key, &created);
        if (created) {
            Layout l = {0};
            l.text = avp_buf_cz(&b);
            l.die = x;
            avp_vec_push(&lays, l);
            v->i = (i64)lays.n - 1;
        }
        lays.v[v->i].count++;
        avp_vec_push(&lays.v[v->i].cus, x->cu);
    }
    if (qual_note) shsm_output_linez(out, avp_sprintf(a, "note: %s", qual_note));
    shsm_output_linez(out, avp_sprintf(a, "%s: %zu definitions in %s, %zu distinct layout%s%s", lookup, hits.n - decls, w->path,
                                       lays.n, lays.n == 1 ? "" : "s",
                                       decls ? avp_sprintf(a, " (%zu declarations without members not counted)", decls) : ""));
    AvpJson *arr = avp_json_arr(a);
    Deps dp = {a, d, avp_map_new(), {0}, {0}, full};
    avp_map_put(dp.seen, avp_str(lookup), NULL)->i = 2;
    for (size_t k = 0; k < lays.n; k++) {
        Layout *l = &lays.v[k];
        const SdCu *cu = &d->cus[l->cus.v[0]];
        shsm_output_linez(out, "");
        shsm_output_linez(out, avp_sprintf(a, "// layout %zu of %zu: %lld unit%s, e.g. %s (DIE 0x%x)", k + 1, lays.n, (long long)l->count,
                                           l->count == 1 ? "" : "s", cu->name ? cu->name : "?", l->die->off));
        AvpBuf b;
        avp_buf_init(&b, a);
        render(&b, &dp, l->die, "", NULL);
        shsm_output_line(out, avp_str_rstrip(avp_buf_str(&b)));
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("text"), avp_json_strz(a, l->text));
        avp_json_set(a, o, AVP_S("units"), avp_json_int(a, l->count));
        avp_json_set(a, o, AVP_S("example_unit"), avp_json_strz(a, cu->name ? cu->name : ""));
        avp_json_set(a, o, AVP_S("die"), avp_json_int(a, l->die->off));
        avp_json_arr_push(a, arr, o);
        avp_vec_free(&l->cus);
    }
    shsm_output_data(out, "layouts", arr);
    if (full) {
        AvpJson *deps = avp_json_arr(a);
        if (dp.queue.n) shsm_output_linez(out, "");
        if (dp.queue.n) shsm_output_linez(out, "// ---- types it depends on (by value; one layout each, from the same unit) ----");
        for (size_t q = 0; q < dp.queue.n; q++) {
            const SdDie *x = sd_die_at(d, dp.queue.v[q]);
            if (!x) continue;
            AvpBuf b;
            avp_buf_init(&b, a);
            render(&b, &dp, x, "", NULL);
            shsm_output_line(out, avp_str_rstrip(avp_buf_str(&b)));
            avp_json_arr_push(a, deps, avp_json_str(a, avp_str_rstrip(avp_buf_str(&b))));
        }
        AvpJson *fw = avp_json_arr(a);
        if (dp.forward.n) {
            AvpBuf b;
            avp_buf_init(&b, a);
            avp_buf_putz(&b, "// reached through pointers only:");
            for (size_t q = 0; q < dp.forward.n; q++) {
                AvpVal *v = avp_map_get(dp.seen, avp_str(dp.forward.v[q]));
                if (v && v->i == 2) continue;
                avp_buf_printf(&b, " %s", dp.forward.v[q]);
                avp_json_arr_push(a, fw, avp_json_strz(a, dp.forward.v[q]));
            }
            shsm_output_linez(out, avp_buf_cz(&b));
        }
        shsm_output_data(out, "dependencies", deps);
        shsm_output_data(out, "forward", fw);
    }
    shsm_output_data(out, "notes", notes_json(a, d));
    avp_vec_free(&dp.queue);
    avp_vec_free(&dp.forward);
    avp_map_free(dp.seen);
    avp_map_free(by_text);
    avp_vec_free(&lays);
    avp_vec_free(&hits);
    return 0;
}

/* ---- fn ---- */

typedef struct {
    u32 sib_low, sib_high, shsm_low, shsm_size;
    bool paired;
} AddrMap;

static u32 map_addr(void *ctx, u32 x) {
    const AddrMap *m = ctx;
    if (!m->paired || x < m->sib_low || x > m->sib_high) return x;
    return x - m->sib_low + m->shsm_low;
}

static bool parse_addr(const char *s, u32 *out) {
    const char *p = s;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    else if (strlen(p) != 8) return false;
    if (!*p) return false;
    for (const char *q = p; *q; q++)
        if (!isxdigit((unsigned char)*q)) return false;
    *out = (u32)strtoul(p, NULL, 16);
    return true;
}

static AvpStr base_name(const char *dw) {
    AvpStr s = avp_str(dw);
    i64 k = avp_str_findc(s, '(');
    return k >= 0 ? avp_str_slice(s, 0, k) : s;
}

static void print_scope(AvpArena *a, const SdInfo *d, const SdDie *scope, int level, AddrMap *map, ShsmOutput *out, AvpJson *arr,
                        bool params) {
    u32 i = idx(d, scope);
    const char *ind = avp_sprintf(a, "%*s", 2 + 2 * level, "");
    for (u32 k = i + 1; k < scope->next; k++) {
        const SdDie *x = &d->dies[k];
        if (x->parent != i) continue;
        /* the parameters pass prints only the function's parameters; the locals pass everything else */
        if (params != (x->tag == SD_TAG_formal_parameter && level == 0)) continue;
        if (x->tag == SD_TAG_lexical_block) {
            AvpJson *o = avp_json_obj(a), *inner = avp_json_arr(a);
            shsm_output_linez(out, avp_sprintf(a, "%s{ block %08X-%08X", ind, map_addr(map, x->low), map_addr(map, x->high)));
            print_scope(a, d, x, level + 1, map, out, inner, false);
            shsm_output_linez(out, avp_sprintf(a, "%s}", ind));
            avp_json_set(a, o, AVP_S("block"), avp_json_strz(a, avp_sprintf(a, "%08X-%08X", map_addr(map, x->low), map_addr(map, x->high))));
            avp_json_set(a, o, AVP_S("locals"), inner);
            avp_json_arr_push(a, arr, o);
            continue;
        }
        if (x->tag == SD_TAG_formal_parameter || x->tag == SD_TAG_variable) {
            const char *decl = sd_decl(a, d, x->type, x->name ? x->name : "?");
            const char *loc = sd_location_text(a, d, x, map_addr, map);
            AvpBuf b;
            avp_buf_init(&b, a);
            avp_buf_printf(&b, "%s%s", ind, decl);
            pad_comment(&b, 0, avp_sprintf(a, "%s%s", loc, x->decl_line ? avp_sprintf(a, "  (line %u)", x->decl_line) : ""));
            shsm_output_linez(out, avp_buf_cz(&b));
            AvpJson *o = avp_json_obj(a);
            avp_json_set(a, o, AVP_S("name"), avp_json_strz(a, x->name ? x->name : ""));
            avp_json_set(a, o, AVP_S("type"), avp_json_strz(a, sd_type_name(a, d, x->type)));
            avp_json_set(a, o, AVP_S("location"), avp_json_strz(a, loc));
            avp_json_set(a, o, AVP_S("line"), avp_json_int(a, x->decl_line));
            avp_json_arr_push(a, arr, o);
            continue;
        }
        const char *tn = sd_tag_name(x->tag);
        shsm_output_linez(out, avp_sprintf(a, "%s(%s %s: not interpreted)", ind, tn ? tn : avp_sprintf(a, "tag 0x%x", x->tag), x->name ? x->name : ""));
    }
}

/* The subprogram starting at addr, else the one whose range holds it. */
static const SdDie *subprogram_at(const SdInfo *d, u32 addr) {
    for (size_t k = 0; k < d->ndies; k++)
        if (d->dies[k].tag == SD_TAG_subprogram && (d->dies[k].flags & SD_F_LOW) && d->dies[k].low == addr) return &d->dies[k];
    for (size_t k = 0; k < d->ndies; k++) {
        const SdDie *x = &d->dies[k];
        if (x->tag == SD_TAG_subprogram && (x->flags & SD_F_LOW) && x->low <= addr && addr < x->high) return x;
    }
    return NULL;
}

/* The function of the Debug ELF with the same DWARF name (in the same unit when several share it), NULL when there is
 * no Debug ELF beside the one read or it holds no such function. *dbg receives the loaded ELF. */
static const SdDie *debug_twin(AvpArena *a, Sibling *sb, AvpArgs *args, const SdInfo *d, const SdDie *fn, Dw *dbg) {
    if (!fn->name) return NULL;
    AvpTry t;
    if (!AVP_TRY(&t)) return NULL; /* no Debug ELF: nothing to compare */
    *dbg = load_path(a, elf_path(a, sb, args, "debug"));
    avp_try_end(&t);
    const char *cu = d->cus[fn->cu].name;
    const SdDie *best = NULL;
    for (size_t k = 0; k < dbg->d->ndies; k++) {
        const SdDie *x = &dbg->d->dies[k];
        if (x->tag != SD_TAG_subprogram || !(x->flags & SD_F_LOW) || !x->name || strcmp(x->name, fn->name) != 0) continue;
        const char *xcu = dbg->d->cus[x->cu].name;
        if (!best || (cu && xcu && strcmp(cu, xcu) == 0)) best = x;
    }
    return best;
}

static void print_this(AvpArena *a, const SdInfo *d, const SdDie *fn, ShsmOutput *out, AvpJson *o, const char *from) {
    for (u32 k = idx(d, fn) + 1; k < fn->next; k++) {
        const SdDie *x = &d->dies[k];
        if (x->parent != idx(d, fn) || x->tag != SD_TAG_formal_parameter || !x->name || strcmp(x->name, "this") != 0) continue;
        const SdDie *p = sd_die_at(d, x->type);
        const char *cls = p && p->tag == SD_TAG_pointer_type ? sd_type_name(a, d, p->type) : sd_type_name(a, d, x->type);
        shsm_output_linez(out, avp_sprintf(a, "this: %s%s", cls, from));
        avp_json_set(a, o, AVP_S("this"), avp_json_strz(a, cls));
    }
}

static int cmd_fn(AvpArena *a, Sibling *sb, const Dw *w, AvpArgs *args, ShsmOutput *out) {
    const SdInfo *d = w->d;
    const char *target = opt(a, args, "target");
    bool sib_only = avp_args_has(args, "sibling") && avp_args_bool(args, "sibling");
    u32 addr = 0;
    bool is_addr = parse_addr(target, &addr);
    AddrMap map = {0};
    const SibFn *shsm = NULL;
    u32 sib_addr = 0;
    bool resolved = false, by_symbol = false, same_named = false;
    if (!sib_only && sibling_has_pairs(sb)) {
        SibFnVec s = sibling_load_shsm(a, sb);
        for (size_t k = 0; k < s.n && !shsm; k++)
            if (is_addr ? (u32)s.v[k].address == addr : strcmp(s.v[k].name, target) == 0) shsm = &s.v[k];
        if (shsm) {
            if (shsm->peer < 0) {
                /* no pair: the sibling function of the same name is another revision's copy of it */
                size_t nsame = 0;
                SibFnVec g = sibling_load_sibling(a, sb);
                i64 j = sibling_same_named(&g, shsm->name, &nsame);
                if (nsame != 1)
                    shsm_raise(SHSM_EXC_KEY, "%s (0x%08X) has no sibling counterpart in the pairing%s (pass --sibling to read %s as a sibling %s)",
                               shsm->name, (u32)shsm->address,
                               nsame ? avp_sprintf(a, " and %zu same-named sibling functions", nsame) : "", target,
                               is_addr ? "address" : "name");
                sib_addr = (u32)g.v[j].address;
                same_named = true;
            } else {
                sib_addr = (u32)shsm->peer;
            }
            resolved = true;
        }
    }
    if (!resolved && is_addr) { sib_addr = addr; resolved = true; }
    if (!resolved && sibling_has_index(sb)) {
        SibFnVec g = sibling_load_sibling(a, sb);
        for (size_t k = 0; k < g.n; k++)
            if (strcmp(g.v[k].name, target) == 0) { sib_addr = (u32)g.v[k].address; resolved = by_symbol = true; break; }
    }
    const char *build = opt(a, args, "build");
    bool cross = build && strcmp(build, "final") != 0 && (shsm || by_symbol);
    const SdDie *fn = NULL;
    if (resolved && cross) {
        /* the pairing and the symbol table are the Final build's: find the function there, then by name here */
        Dw fw = load_path(a, elf_path(a, sb, args, "final"));
        const SdDie *ff = subprogram_at(fw.d, sib_addr);
        if (!ff || !ff->name) shsm_raise(SHSM_EXC_KEY, "no DWARF subprogram at 0x%08X in %s", sib_addr, fw.path);
        const char *fcu = fw.d->cus[ff->cu].name;
        for (size_t k = 0; k < d->ndies; k++) {
            const SdDie *x = &d->dies[k];
            if (x->tag != SD_TAG_subprogram || !(x->flags & SD_F_LOW) || !x->name || strcmp(x->name, ff->name) != 0) continue;
            const char *xcu = d->cus[x->cu].name;
            if (!fn || (fcu && xcu && strcmp(fcu, xcu) == 0)) fn = x;
        }
        if (!fn) shsm_raise(SHSM_EXC_KEY, "%s (Final 0x%08X) has no subprogram of that name in %s", ff->name, sib_addr, w->path);
    } else if (resolved) {
        fn = subprogram_at(d, sib_addr);
        if (!fn) shsm_raise(SHSM_EXC_KEY, "no DWARF subprogram covers sibling address 0x%08X in %s", sib_addr, w->path);
    } else {
        /* a DWARF name: exact, or the part before the parameter list, or the demangled form of the target */
        const char *dem = shsm_demangle_or_name(a, target);
        AvpStr want = base_name(dem ? dem : target);
        AVP_VEC(const SdDie *) hits = {0};
        for (size_t k = 0; k < d->ndies; k++) {
            const SdDie *x = &d->dies[k];
            if (x->tag != SD_TAG_subprogram || !x->name || !(x->flags & SD_F_LOW)) continue;
            if (strcmp(x->name, target) == 0 || (dem && strcmp(x->name, dem) == 0)) { avp_vec_clear(&hits); avp_vec_push(&hits, x); break; }
            if (avp_str_eq(base_name(x->name), want)) avp_vec_push(&hits, x);
        }
        if (!hits.n) shsm_raise(SHSM_EXC_KEY, "%s: no SHSM symbol, sibling symbol or DWARF subprogram of that name", target);
        if (hits.n > 1) {
            shsm_output_linez(out, avp_sprintf(a, "%s is ambiguous: %zu subprograms", target, hits.n));
            AvpJson *arr = avp_json_arr(a);
            for (size_t k = 0; k < hits.n; k++) {
                shsm_output_linez(out, avp_sprintf(a, "  0x%08X  %s  (%s)", hits.v[k]->low, hits.v[k]->name, d->cus[hits.v[k]->cu].name));
                avp_json_arr_push(a, arr, avp_json_strz(a, avp_sprintf(a, "0x%08X %s", hits.v[k]->low, hits.v[k]->name)));
            }
            shsm_output_data(out, "candidates", arr);
            avp_vec_free(&hits);
            return 1;
        }
        fn = hits.v[0];
        avp_vec_free(&hits);
    }
    const SdCu *cu = &d->cus[fn->cu];
    SdLines lines = cu->has_stmt ? sd_lines(a, d, cu->stmt_list) : (SdLines){0};
    AvpJson *o = avp_json_obj(a);
    shsm_output_linez(out, avp_sprintf(a, "%s  sibling %08X-%08X (0x%X bytes)", fn->name ? fn->name : "?", fn->low, fn->high, fn->high - fn->low));
    avp_json_set(a, o, AVP_S("name"), avp_json_strz(a, fn->name ? fn->name : ""));
    avp_json_set(a, o, AVP_S("sibling_low"), avp_json_int(a, fn->low));
    avp_json_set(a, o, AVP_S("sibling_high"), avp_json_int(a, fn->high));
    if (shsm && same_named) {
        shsm_output_linez(out, avp_sprintf(a, "pair: NONE for SHSM %s %08X (0x%llX bytes); this is the same-named sibling function%s (0x%X bytes)."
                                              " NOT EXACT: another revision's body, addresses below are the sibling's own, untranslated",
                                           shsm->name, (u32)shsm->address, (unsigned long long)shsm->size,
                                           cross ? avp_sprintf(a, " of the %s build (matched by name)", build) : "", fn->high - fn->low));
        avp_json_set(a, o, AVP_S("shsm"), avp_json_strz(a, shsm->name));
        avp_json_set(a, o, AVP_S("shsm_address"), avp_json_int(a, shsm->address));
        avp_json_set(a, o, AVP_S("same_named_fallback"), avp_json_bool(true));
        avp_json_set(a, o, AVP_S("offsets_reliable"), avp_json_bool(false));
    } else if (shsm && cross) {
        shsm_output_linez(out, avp_sprintf(a, "pair: SHSM %s %08X, tier %s, with the Final build's copy at %08X; this is the %s build's copy (matched by name): addresses are its own, not translated",
                                           shsm->name, (u32)shsm->address, SIBLING_TIERS[shsm->tier], sib_addr, build));
        avp_json_set(a, o, AVP_S("shsm"), avp_json_strz(a, shsm->name));
        avp_json_set(a, o, AVP_S("offsets_reliable"), avp_json_bool(false));
    } else if (shsm) {
        bool exact = shsm->tier == SIB_EXACT || shsm->tier == SIB_EXACT_SEQ;
        map.paired = true;
        map.sib_low = fn->low;
        map.sib_high = fn->high;
        map.shsm_low = (u32)shsm->address;
        map.shsm_size = (u32)shsm->size;
        bool same_size = shsm->size == (i64)(fn->high - fn->low);
        shsm_output_linez(out, avp_sprintf(a, "pair: SHSM %s %08X (0x%llX bytes), tier %s; addresses below are SHSM addresses (sibling - 0x%08X + 0x%08X)%s",
                                           shsm->name, (u32)shsm->address, (unsigned long long)shsm->size, SIBLING_TIERS[shsm->tier],
                                           fn->low, (u32)shsm->address,
                                           exact && same_size ? "" : "; WARNING: the bodies are not identical, offsets are approximate"));
        avp_json_set(a, o, AVP_S("shsm"), avp_json_strz(a, shsm->name));
        avp_json_set(a, o, AVP_S("shsm_address"), avp_json_int(a, shsm->address));
        avp_json_set(a, o, AVP_S("tier"), avp_json_strz(a, SIBLING_TIERS[shsm->tier]));
        avp_json_set(a, o, AVP_S("offsets_reliable"), avp_json_bool(exact && same_size));
    }
    const char *decl_file = fn->decl_file ? sd_file_name(&lines, fn->decl_file) : "?";
    shsm_output_linez(out, avp_sprintf(a, "unit: %s", cu->name ? cu->name : "?"));
    shsm_output_linez(out, avp_sprintf(a, "declared: %s:%u%s", decl_file, fn->decl_line, (fn->flags & SD_F_EXTERNAL) ? "" : " (static)"));
    shsm_output_linez(out, avp_sprintf(a, "returns: %s", sd_type_name(a, d, fn->type)));
    avp_json_set(a, o, AVP_S("unit"), avp_json_strz(a, cu->name ? cu->name : ""));
    avp_json_set(a, o, AVP_S("decl_file"), avp_json_strz(a, decl_file));
    avp_json_set(a, o, AVP_S("decl_line"), avp_json_int(a, fn->decl_line));
    avp_json_set(a, o, AVP_S("returns"), avp_json_strz(a, sd_type_name(a, d, fn->type)));
    if (fn->flags & SD_F_FRAME) {
        SdLocVec fb = sd_loclist(a, d, fn->frame_base, cu->has_pc ? cu->low : 0);
        const char *t = fb.n ? fb.v[0].expr : "?";
        shsm_output_linez(out, avp_sprintf(a, "frame base (fb): %s", t));
        avp_json_set(a, o, AVP_S("frame_base"), avp_json_strz(a, t));
        avp_vec_free(&fb);
    }
    /* the Debug build's copy of the function: it keeps parameters and locals the Final build dropped */
    Dw dbg = {0};
    const SdDie *twin = NULL;
    if (!(build && strcmp(build, "debug") == 0)) twin = debug_twin(a, sb, args, d, fn, &dbg);
    bool no_params = sd_param_count(d, fn) == 0;
    bool params_from_debug = no_params && twin && sd_param_count(dbg.d, twin) > 0;
    print_this(a, params_from_debug ? dbg.d : d, params_from_debug ? twin : fn, out, o, params_from_debug ? " (Debug build)" : "");
    AvpJson *params = avp_json_arr(a), *locals = avp_json_arr(a);
    shsm_output_linez(out, "parameters:");
    print_scope(a, d, fn, 0, &map, out, params, true);
    if (!avp_json_len(params)) shsm_output_linez(out, "  (none recorded)");
    if (params_from_debug) {
        /* its registers and frame offsets are the Debug build's own */
        AddrMap none = {0};
        AvpJson *dparams = avp_json_arr(a);
        shsm_output_linez(out, "parameters (Debug build; the selected build records none):");
        print_scope(a, dbg.d, twin, 0, &none, out, dparams, true);
        avp_json_set(a, o, AVP_S("parameters_debug"), dparams);
    }
    shsm_output_linez(out, "locals:");
    print_scope(a, d, fn, 0, &map, out, locals, false);
    if (twin) {
        AvpStrVec have = sd_local_names(a, d, fn), want = sd_local_names(a, dbg.d, twin);
        AvpStrVec lost = sd_names_missing(&have, &want);
        if (lost.n) {
            AvpBuf b;
            avp_buf_init(&b, a);
            avp_buf_printf(&b, "  %zu more local%s in the Debug build (", lost.n, lost.n == 1 ? "" : "s");
            for (size_t k = 0; k < lost.n && k < 8; k++) {
                if (k) avp_buf_put(&b, AVP_S(", "));
                avp_buf_put(&b, lost.v[k]);
            }
            avp_buf_printf(&b, "%s): `sibling.exe fn '%s' --build debug`", lost.n > 8 ? ", ..." : "", fn->name ? fn->name : target);
            shsm_output_linez(out, avp_buf_cz(&b));
            AvpJson *arr = avp_json_arr(a);
            for (size_t k = 0; k < lost.n; k++) avp_json_arr_push(a, arr, avp_json_str(a, lost.v[k]));
            avp_json_set(a, o, AVP_S("locals_only_in_debug"), arr);
        }
        avp_vec_free(&have);
        avp_vec_free(&want);
        avp_vec_free(&lost);
    }
    avp_json_set(a, o, AVP_S("parameters"), params);
    avp_json_set(a, o, AVP_S("locals"), locals);
    size_t ninl = 0;
    for (size_t k = 0; k < d->ndies; k++) ninl += d->dies[k].tag == SD_TAG_inlined_subroutine;
    if (!ninl) shsm_output_linez(out, "inlined: this ELF has no DW_TAG_inlined_subroutine DIEs (MWCC does not record inlining)");
    /* line table */
    AvpJson *rows = avp_json_arr(a);
    if (!lines.ok) {
        shsm_output_linez(out, avp_sprintf(a, "lines: %s", lines.error ? lines.error : "no line program"));
    } else {
        const char *main_file = NULL;
        u32 last_line = 0, last_file = 0;
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t k = 0; k < lines.rows.n; k++) {
            const SdLineRow *r = &lines.rows.v[k];
            if (r->end_sequence || r->address < fn->low || r->address >= fn->high) continue;
            if (!main_file) {
                main_file = sd_file_name(&lines, r->file);
                shsm_output_linez(out, avp_sprintf(a, "lines (%s):", main_file));
            }
            AvpJson *row = avp_json_obj(a);
            avp_json_set(a, row, AVP_S("line"), avp_json_int(a, r->line));
            avp_json_set(a, row, AVP_S("address"), avp_json_int(a, map_addr(&map, r->address)));
            if (map.paired) avp_json_set(a, row, AVP_S("sibling_address"), avp_json_int(a, r->address));
            avp_json_set(a, row, AVP_S("file"), avp_json_strz(a, sd_file_name(&lines, r->file)));
            avp_json_arr_push(a, rows, row);
            if (r->line == last_line && r->file == last_file && b.n) {
                avp_buf_printf(&b, " %08X", map_addr(&map, r->address));
                continue;
            }
            if (b.n) shsm_output_linez(out, avp_buf_cz(&b));
            avp_buf_init(&b, a);
            const char *f = sd_file_name(&lines, r->file);
            avp_buf_printf(&b, "  %5u: %08X", r->line, map_addr(&map, r->address));
            if (strcmp(f, main_file) != 0) avp_buf_printf(&b, "   (%s)", f);
            last_line = r->line;
            last_file = r->file;
        }
        if (b.n) shsm_output_linez(out, avp_buf_cz(&b));
        if (!main_file) shsm_output_linez(out, "lines: none in this range");
    }
    avp_json_set(a, o, AVP_S("lines"), rows);
    shsm_output_data(out, "function", o);
    shsm_output_data(out, "notes", notes_json(a, d));
    return 0;
}

/* ---- cu ---- */

typedef struct {
    u32 funcs, funcs_with_vars, types, members, vars;
} CuStats;

static CuStats cu_stats(const SdInfo *d, const SdCu *cu) {
    CuStats s = {0};
    if (cu->die == SD_NONE) return s;
    const SdDie *top = &d->dies[cu->die];
    for (u32 k = cu->die + 1; k < top->next; k++) {
        const SdDie *x = &d->dies[k];
        if (x->tag == SD_TAG_member) s.members++;
        if (x->tag == SD_TAG_variable && x->depth > 1) s.vars++;
        if (x->depth != 1) continue;
        if (x->tag == SD_TAG_subprogram && (x->flags & SD_F_LOW)) {
            s.funcs++;
            for (u32 q = k + 1; q < x->next; q++)
                if (d->dies[q].tag == SD_TAG_variable || d->dies[q].tag == SD_TAG_formal_parameter) { s.funcs_with_vars++; break; }
        }
        if (sd_is_aggregate(x->tag) && x->name && is_definition(x)) s.types++;
    }
    return s;
}

static int cmd_cu(AvpArena *a, const Dw *w, AvpArgs *args, ShsmOutput *out) {
    const SdInfo *d = w->d;
    const char *want = opt(a, args, "path");
    AvpStr lw = want ? avp_str_lower(a, avp_str(want)) : AVP_S("");
    AVP_VEC(const SdCu *) hits = {0};
    for (size_t k = 0; k < d->ncus; k++) {
        const SdCu *cu = &d->cus[k];
        if (want && !(cu->name && avp_str_contains(avp_str_lower(a, avp_str(cu->name)), lw))) continue;
        avp_vec_push(&hits, cu);
    }
    AvpJson *arr = avp_json_arr(a);
    if (hits.n == 1 && want) {
        const SdCu *cu = hits.v[0];
        CuStats s = cu_stats(d, cu);
        shsm_output_linez(out, avp_sprintf(a, "%s", cu->name ? cu->name : "?"));
        shsm_output_linez(out, avp_sprintf(a, "producer %s; pc %08X-%08X; %u functions (%u with parameters or locals), %u types, line program %s",
                                           cu->producer ? cu->producer : "?", cu->low, cu->high, s.funcs, s.funcs_with_vars, s.types,
                                           cu->has_stmt ? "yes" : "no"));
        const SdDie *top = &d->dies[cu->die];
        shsm_output_linez(out, "functions:");
        for (u32 k = cu->die + 1; k < top->next; k++) {
            const SdDie *x = &d->dies[k];
            if (x->depth != 1 || x->tag != SD_TAG_subprogram || !(x->flags & SD_F_LOW)) continue;
            shsm_output_linez(out, avp_sprintf(a, "  %08X %5X  %s", x->low, x->high - x->low, x->name ? x->name : "?"));
            AvpJson *o = avp_json_obj(a);
            avp_json_set(a, o, AVP_S("kind"), avp_json_strz(a, "function"));
            avp_json_set(a, o, AVP_S("name"), avp_json_strz(a, x->name ? x->name : ""));
            avp_json_set(a, o, AVP_S("address"), avp_json_int(a, x->low));
            avp_json_set(a, o, AVP_S("size"), avp_json_int(a, x->high - x->low));
            avp_json_arr_push(a, arr, o);
        }
        shsm_output_linez(out, "types:");
        for (u32 k = cu->die + 1; k < top->next; k++) {
            const SdDie *x = &d->dies[k];
            if (x->depth != 1 || !sd_is_named_type(x->tag) || !x->name || x->tag == SD_TAG_typedef || !is_definition(x)) continue;
            shsm_output_linez(out, avp_sprintf(a, "  %-6s %-40s size 0x%X", kw(x->tag), x->name, x->byte_size));
            AvpJson *o = avp_json_obj(a);
            avp_json_set(a, o, AVP_S("kind"), avp_json_strz(a, kw(x->tag)));
            avp_json_set(a, o, AVP_S("name"), avp_json_strz(a, x->name));
            avp_json_set(a, o, AVP_S("size"), avp_json_int(a, x->byte_size));
            avp_json_arr_push(a, arr, o);
        }
        shsm_output_data(out, "items", arr);
    } else {
        shsm_output_linez(out, avp_sprintf(a, "%zu compile units%s in %s (%zu DIEs in all, %zu reader notes)", hits.n, want ? avp_sprintf(a, " matching %s", want) : "", w->path, d->ndies, d->notes.n));
        shsm_output_linez(out, "low      high      funcs w/vars types members lines  unit");
        for (size_t k = 0; k < hits.n; k++) {
            const SdCu *cu = hits.v[k];
            CuStats s = cu_stats(d, cu);
            shsm_output_linez(out, avp_sprintf(a, "%08X %08X %6u %6u %5u %7u %5s  %s", cu->low, cu->high, s.funcs, s.funcs_with_vars, s.types,
                                               s.members, cu->has_stmt ? "yes" : "no", cu->name ? cu->name : "?"));
            AvpJson *o = avp_json_obj(a);
            avp_json_set(a, o, AVP_S("name"), avp_json_strz(a, cu->name ? cu->name : ""));
            avp_json_set(a, o, AVP_S("producer"), avp_json_strz(a, cu->producer ? cu->producer : ""));
            avp_json_set(a, o, AVP_S("low"), avp_json_int(a, cu->low));
            avp_json_set(a, o, AVP_S("high"), avp_json_int(a, cu->high));
            avp_json_set(a, o, AVP_S("functions"), avp_json_int(a, s.funcs));
            avp_json_set(a, o, AVP_S("functions_with_vars"), avp_json_int(a, s.funcs_with_vars));
            avp_json_set(a, o, AVP_S("types"), avp_json_int(a, s.types));
            avp_json_set(a, o, AVP_S("members"), avp_json_int(a, s.members));
            avp_json_set(a, o, AVP_S("locals"), avp_json_int(a, s.vars));
            avp_json_set(a, o, AVP_S("line_program"), avp_json_bool(cu->has_stmt));
            avp_json_arr_push(a, arr, o);
        }
        shsm_output_data(out, "units", arr);
    }
    shsm_output_data(out, "notes", notes_json(a, d));
    avp_vec_free(&hits);
    return 0;
}

/* ---- var ---- */

/* The address of a variable's DW_OP_addr location ("@0x8074F1C0"); false for a register, frame or list location. */
static bool var_address(AvpArena *a, const SdInfo *d, const SdDie *x, u32 *out) {
    if (!(x->flags & (SD_F_LOC | SD_F_LOCLIST))) return false;
    const char *t = sd_location_text(a, d, x, NULL, NULL);
    if (!t || strncmp(t, "@0x", 3) != 0) return false;
    *out = (u32)strtoul(t + 3, NULL, 16);
    return true;
}

static int cmd_var(AvpArena *a, Sibling *sb, const Dw *w, AvpArgs *args, ShsmOutput *out) {
    const SdInfo *d = w->d;
    const char *target = opt(a, args, "target");
    u32 addr = 0;
    bool is_addr = parse_addr(target, &addr);
    SibMapData m = {0};
    bool in_map = false;
    const char *map_path = meta_value(a, sb, "map");
    if (map_path && avp_isfile(map_path)) {
        AvpBytes b = {0};
        if (!avp_read_all(a, map_path, &b)) shsm_raise_io();
        in_map = sibling_map_find(a, avp_strn((const char *)b.v, b.n), is_addr ? NULL : target, addr, &m);
    }
    const char *name = in_map ? m.name : (is_addr ? NULL : target);
    AvpJson *o = avp_json_obj(a), *arr = avp_json_arr(a);
    if (in_map) {
        shsm_output_linez(out, avp_sprintf(a, "%s%s", m.name, is_addr && addr != (u32)m.address
                                                          ? avp_sprintf(a, "  (0x%08X is at +0x%X)", addr, addr - (u32)m.address) : ""));
        shsm_output_linez(out, avp_sprintf(a, "link map: 0x%08llX size 0x%llX align %lld in %s, %s%s%s",
                                           (long long)m.address, (long long)m.size, (long long)m.align, m.section, m.lib,
                                           *m.lib ? " " : "", m.object));
        avp_json_set(a, o, AVP_S("name"), avp_json_strz(a, m.name));
        avp_json_set(a, o, AVP_S("address"), avp_json_int(a, m.address));
        avp_json_set(a, o, AVP_S("size"), avp_json_int(a, m.size));
        avp_json_set(a, o, AVP_S("section"), avp_json_strz(a, m.section));
        avp_json_set(a, o, AVP_S("lib"), avp_json_strz(a, m.lib));
        avp_json_set(a, o, AVP_S("object"), avp_json_strz(a, m.object));
    } else {
        shsm_output_linez(out, avp_sprintf(a, "%s: not in the link map's data sections%s", target,
                                           is_addr ? "" : " (the DWARF is searched by that name)"));
    }
    size_t nhits = 0;
    for (size_t k = 0; k < d->ndies; k++) {
        const SdDie *x = &d->dies[k];
        if (x->tag != SD_TAG_variable || !x->name) continue;
        bool by_name = name && strcmp(x->name, name) == 0;
        u32 at = 0;
        bool has_at = (by_name || is_addr || in_map) && var_address(a, d, x, &at);
        if (!(by_name && has_at) && !((is_addr || in_map) && has_at && at == (in_map ? (u32)m.address : addr))) continue;
        const SdCu *cu = &d->cus[x->cu];
        SdLines lines = cu->has_stmt ? sd_lines(a, d, cu->stmt_list) : (SdLines){0};
        const char *file = x->decl_file ? sd_file_name(&lines, x->decl_file) : "?";
        bool local = x->parent != SD_NONE && d->dies[x->parent].tag != SD_TAG_compile_unit;
        shsm_output_linez(out, avp_sprintf(a, "DWARF: %s  // 0x%08X, 0x%X bytes%s%s", sd_decl(a, d, x->type, x->name), at,
                                           sd_type_size(d, x->type), (x->flags & SD_F_EXTERNAL) ? "" : ", static",
                                           local ? ", a local of a function" : ""));
        shsm_output_linez(out, avp_sprintf(a, "  unit: %s", cu->name ? cu->name : "?"));
        shsm_output_linez(out, avp_sprintf(a, "  declared: %s:%u", file, x->decl_line));
        AvpJson *e = avp_json_obj(a);
        avp_json_set(a, e, AVP_S("name"), avp_json_strz(a, x->name));
        avp_json_set(a, e, AVP_S("type"), avp_json_strz(a, sd_type_name(a, d, x->type)));
        avp_json_set(a, e, AVP_S("declaration"), avp_json_strz(a, sd_decl(a, d, x->type, x->name)));
        avp_json_set(a, e, AVP_S("address"), avp_json_int(a, at));
        avp_json_set(a, e, AVP_S("size"), avp_json_int(a, sd_type_size(d, x->type)));
        avp_json_set(a, e, AVP_S("external"), avp_json_bool((x->flags & SD_F_EXTERNAL) != 0));
        avp_json_set(a, e, AVP_S("local_static"), avp_json_bool(local));
        avp_json_set(a, e, AVP_S("unit"), avp_json_strz(a, cu->name ? cu->name : ""));
        avp_json_set(a, e, AVP_S("decl_file"), avp_json_strz(a, file));
        avp_json_set(a, e, AVP_S("decl_line"), avp_json_int(a, x->decl_line));
        avp_json_arr_push(a, arr, e);
        nhits++;
    }
    if (!nhits && !in_map)
        shsm_raise(SHSM_EXC_KEY, "%s: no variable of that name or address in the link map or the DWARF", target);
    if (!nhits) shsm_output_linez(out, "DWARF: no variable with a fixed address of that name (not recorded in this build)");
    avp_json_set(a, o, AVP_S("variables"), arr);
    shsm_output_data(out, "variable", o);
    return 0;
}

/* ---- registration ---- */

bool sibling_dwarf_is_cmd(AvpStr cmd) {
    return avp_str_eqz(cmd, "types") || avp_str_eqz(cmd, "fn") || avp_str_eqz(cmd, "cu") || avp_str_eqz(cmd, "var");
}

int sibling_dwarf_run(Sibling *sb, AvpStr cmd, AvpArgs *args, ShsmOutput *out) {
    AvpArena *a = avp_arena();
    Dw w = load_path(a, elf_path(a, sb, args, NULL));
    int rc;
    if (avp_str_eqz(cmd, "types")) rc = cmd_types(a, &w, args, out);
    else if (avp_str_eqz(cmd, "fn")) rc = cmd_fn(a, sb, &w, args, out);
    else if (avp_str_eqz(cmd, "var")) rc = cmd_var(a, sb, &w, args, out);
    else rc = cmd_cu(a, &w, args, out);
    if (!avp_str_eqz(cmd, "types") || opt(a, args, "type")) print_notes(a, w.d, out);
    return rc;
}

static const char *const BUILDS[] = {"final", "release", "debug", NULL};

void sibling_dwarf_add_parsers(AvpArgSubparsers *sub, void (*shared)(AvpArgParser *)) {
    AvpArgParser *ty = avp_args_subparser(sub, "types", &(AvpArgParserDesc){.help = "a type's layout from the sibling's DWARF"});
    AvpArgParser *fn = avp_args_subparser(sub, "fn", &(AvpArgParserDesc){.help = "a function's DWARF: unit, parameters, locals, lines"});
    AvpArgParser *cu = avp_args_subparser(sub, "cu", &(AvpArgParserDesc){.help = "the sibling's compile units"});
    AvpArgParser *var = avp_args_subparser(sub, "var", &(AvpArgParserDesc){.help = "a global or static variable: type, size, unit, section, address"});
    avp_arg(var, &(AvpArgDesc){.names = "target",
                               .help = "a sibling data symbol (mangled name) or an address inside one"});
    avp_arg(ty, &(AvpArgDesc){.names = "type", .nargs = AVP_NARGS_OPT, .dflt = AVP_DEF_NONE,
                              .help = "struct, class, union, enum or typedef name (none: list every class/struct/union)"});
    avp_arg(ty, &(AvpArgDesc){.names = "--full", .action = AVP_ARG_STORE_TRUE,
                              .help = "also print the types it holds by value (pointer targets are named only)"});
    avp_arg(fn, &(AvpArgDesc){.names = "target",
                              .help = "SHSM symbol or address (resolved through the pairing), or a sibling symbol, address or DWARF name"});
    avp_arg(fn, &(AvpArgDesc){.names = "--sibling", .action = AVP_ARG_STORE_TRUE,
                              .help = "read target as a sibling name or address even when an SHSM function matches"});
    avp_arg(cu, &(AvpArgDesc){.names = "path", .nargs = AVP_NARGS_OPT, .dflt = AVP_DEF_NONE,
                              .help = "substring of a unit path (one match: its functions and types)"});
    AvpArgParser *all[] = {ty, fn, cu, var};
    for (int i = 0; i < 4; i++) {
        avp_arg(all[i], &(AvpArgDesc){.names = "--elf", .metavar = "PATH", .dflt = AVP_DEF_NONE,
                                      .help = "sibling ELF (default: the one index recorded, else orig/_ref/gormiti_wii/Exe/GormitiFinal.elf)"});
        avp_arg(all[i], &(AvpArgDesc){.names = "--build", .choices = BUILDS, .dflt = AVP_DEF_NONE,
                                      .help = "which build beside it: final (default), release or debug (Final in the file name replaced)"});
        shared(all[i]);
    }
}
