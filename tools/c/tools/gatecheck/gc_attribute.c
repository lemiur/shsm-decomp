/* gatecheck attribute: compare the built DOL with retail and attribute each differing range to a section, unit and
 * symbol (gatecheck.py header_differences, byte_runs, merge_runs, diff_dols, classify_word, shift_hint, attribute,
 * format_attribution, elf_dol_mismatch). */
#include "gatecheck.h"

AvpStrVec gc_header_differences(AvpArena *a, const ShsmDol *retail, const ShsmDol *built) {
    AvpStrVec out = {0};
    for (int i = 0; i < 18; i++) {
        i64 ra = retail->addrs[i], rsz = retail->sizes[i], ba = built->addrs[i], bsz = built->sizes[i];
        if (ra == ba && rsz == bsz) continue;
        char *kind = i < 7 ? avp_sprintf(a, "text %d", i) : avp_sprintf(a, "data %d", i - 7);
        char *tail = ra == ba ? avp_sprintf(a, "  (size %+lld)", (long long)(bsz - rsz)) : "";
        avp_vec_push(&out, avp_str(avp_sprintf(a, "section %s: retail addr %s size %s, built addr %s size %s%s", kind,
                                               gc_hx(a, ra, 8), gc_hx(a, rsz, 1), gc_hx(a, ba, 8), gc_hx(a, bsz, 1),
                                               tail)));
    }
    if (retail->bss_addr != built->bss_addr || retail->bss_size != built->bss_size)
        avp_vec_push(&out, avp_str(avp_sprintf(a, "bss: retail %s size %s, built %s size %s",
                                               gc_hx(a, retail->bss_addr, 8), gc_hx(a, retail->bss_size, 1),
                                               gc_hx(a, built->bss_addr, 8), gc_hx(a, built->bss_size, 1))));
    if (retail->entry != built->entry)
        avp_vec_push(&out, avp_str(avp_sprintf(a, "entry: retail %s, built %s", gc_hx(a, retail->entry, 8),
                                               gc_hx(a, built->entry, 8))));
    return out;
}

void gc_byte_runs(const u8 *x, const u8 *y, i64 n, i64 base, GcRunVec *out) {
    i64 start = -1;
    for (i64 k = 0; k < n; k++) {
        if (start < 0) {
            /* skip equal stretches quickly */
            i64 j = k;
            while (j + 8 <= n && memcmp(x + j, y + j, 8) == 0) j += 8;
            k = j;
            if (k >= n) break;
        }
        if (x[k] != y[k]) {
            if (start < 0) start = k;
        } else if (start >= 0) {
            avp_vec_push(out, ((GcRun){base + start, base + k, 0}));
            start = -1;
        }
    }
    if (start >= 0) avp_vec_push(out, ((GcRun){base + start, base + n, 0}));
}

GcRunVec gc_merge_runs(const GcRunVec *runs, i64 gap) {
    GcRunVec out = {0};
    for (size_t i = 0; i < runs->n; i++) {
        i64 s = runs->v[i].start, e = runs->v[i].end;
        if (out.n && s - avp_vec_last(&out).end <= gap) {
            GcRun *p = &out.v[out.n - 1];
            p->nbytes += e - s;
            p->end = e;
        } else {
            avp_vec_push(&out, ((GcRun){s, e, e - s}));
        }
    }
    return out;
}

static void coalesce(GcRunVec *out, i64 s, i64 e) {
    if (out->n && avp_vec_last(out).end == s)
        out->v[out->n - 1].end = e;
    else
        avp_vec_push(out, ((GcRun){s, e, 0}));
}

void gc_diff_dols(const ShsmDol *retail, const ShsmDol *built, GcRunVec *runs, GcRunVec *only_r, GcRunVec *only_b) {
    AvpI64Vec bounds = {0};
    const ShsmDol *ds[2] = {retail, built};
    for (int d = 0; d < 2; d++)
        for (size_t k = 0; k < ds[d]->nsegs; k++) {
            avp_vec_push(&bounds, ds[d]->segs[k].addr);
            avp_vec_push(&bounds, shsm_dol_seg_end(&ds[d]->segs[k]));
        }
    avp_sort_i64(bounds.v, bounds.n);
    size_t m = 0;
    for (size_t i = 0; i < bounds.n; i++)
        if (!m || bounds.v[m - 1] != bounds.v[i]) bounds.v[m++] = bounds.v[i];
    GcRunVec raw = {0};
    for (size_t i = 0; i + 1 < m; i++) {
        i64 lo = bounds.v[i], hi = bounds.v[i + 1];
        const ShsmDolSegment *rs = shsm_dol_segment_at(retail, lo), *bs = shsm_dol_segment_at(built, lo);
        if (rs && bs)
            gc_byte_runs(rs->data + (lo - rs->addr), bs->data + (lo - bs->addr), hi - lo, lo, &raw);
        else if (rs)
            coalesce(only_r, lo, hi);
        else if (bs)
            coalesce(only_b, lo, hi);
    }
    *runs = gc_merge_runs(&raw, 3);
    avp_vec_free(&raw);
    avp_vec_free(&bounds);
}

const char *gc_classify_word(AvpArena *a, u32 r, u32 b) {
    if ((r >> 26) != (b >> 26)) return "opcode";
    u32 x = r ^ b;
    AvpBuf t;
    avp_buf_init(&t, a);
    const char *tags[4];
    int n = 0;
    if (x & 0x03E00000) tags[n++] = "rD/rS";
    if (x & 0x001F0000) tags[n++] = "rA";
    if (x & 0x0000FFFF) tags[n++] = "low16";
    if (x & 0xFC000000) tags[n++] = "op";
    if (((r >> 26) == 16 || (r >> 26) == 18) && (x & 0x03FFFFFC)) {
        tags[0] = "branch target";
        n = 1;
    }
    if (!n) return "flags";
    for (int i = 0; i < n; i++) {
        if (i) avp_buf_putc(&t, '+');
        avp_buf_putz(&t, tags[i]);
    }
    return avp_buf_cz(&t);
}

static int distinct_bytes(AvpStr s) {
    bool seen[256] = {0};
    int n = 0;
    for (size_t i = 0; i < s.n; i++)
        if (!seen[(u8)s.p[i]]) {
            seen[(u8)s.p[i]] = true;
            n++;
        }
    return n;
}

bool gc_shift_hint(AvpArena *a, const ShsmDol *retail, const ShsmDol *built, i64 pos, i64 *out) {
    i64 lo = pos + 0x40;
    AvpStr x;
    bool hx = shsm_dol_read(a, built, lo, 128, &x);
    for (i64 k = 1; k < 65; k++) {
        i64 offs[2] = {k, -k};
        for (int t = 0; t < 2; t++) {
            AvpStr y;
            if (!hx || !shsm_dol_read(a, retail, lo - offs[t], 128, &y)) continue;
            if (!avp_str_eq(x, y) || distinct_bytes(x) < 8) continue;
            *out = offs[t];
            return true;
        }
    }
    return false;
}

/* ---- attribute --------------------------------------------------------------------------------------------------- */

static const char *key_part(const char *s) { return s ? s : "\x01<None>"; }

GcAttribution *gc_attribute(AvpArena *a, const ShsmDol *retail, const ShsmDol *built, const GcUnitMap *units,
                            const GcSymbolMap *built_syms, const GcSymbolMap *retail_syms, i64 words) {
    GcAttribution *res = AVP_NEW(a, GcAttribution);
    gc_diff_dols(retail, built, &res->runs, &res->only_retail, &res->only_built);
    res->header = gc_header_differences(a, retail, built);
    AvpMap *groups = avp_map_new();
    char segbuf[4];
    for (size_t ri = 0; ri < res->runs.n; ri++) {
        i64 rs = res->runs.v[ri].start, re_ = res->runs.v[ri].end, nb = res->runs.v[ri].nbytes;
        i64 pos = rs;
        while (pos < re_) {
            const char *unit, *section;
            size_t uidx = 0;
            gc_unit_lookup(units, pos, &unit, &section, &uidx);
            if (!section) {
                section = gc_unit_section_name(units, pos);
                if (!section) {
                    const ShsmDolSegment *seg = shsm_dol_segment_at(built, pos);
                    if (!seg) seg = shsm_dol_segment_at(retail, pos);
                    section = seg ? avp_strdup(a, shsm_dol_seg_name(seg, segbuf)) : "?";
                }
            }
            GcHit sym = {0}, rsym = {0};
            bool has_sym = built_syms && gc_symbol_lookup(built_syms, pos, &sym);
            bool has_rsym = retail_syms && gc_symbol_lookup(retail_syms, pos, &rsym);
            i64 piece_end = re_;
            if (has_sym && sym.inside) {
                i64 e = sym.addr + (sym.size > 1 ? sym.size : 1);
                if (e < piece_end) piece_end = e;
            } else if (built_syms) {
                i64 nx;
                if (gc_symbol_next_start(built_syms, pos, &nx) && nx && nx < piece_end) piece_end = nx;
            }
            if (unit && units->ranges[uidx].end < piece_end) piece_end = units->ranges[uidx].end;
            if (piece_end < pos + 1) piece_end = pos + 1;
            const char *sname;
            i64 saddr;
            if (has_sym) {
                if (sym.inside) {
                    sname = sym.name;
                    saddr = sym.addr;
                } else {
                    sname = avp_sprintf(a, "<after %s>", sym.name);
                    saddr = sym.addr + sym.size;
                }
            } else if (has_rsym) {
                sname = avp_sprintf(a, "<retail %s>", rsym.name);
                saddr = rsym.addr;
            } else {
                sname = "<no symbol>";
                saddr = 0;
            }
            AvpStr key = avp_str(avp_sprintf(a, "%s\x02%s\x02%s\x02%lld", key_part(unit), section, sname,
                                             (long long)saddr));
            bool created;
            AvpVal *gv = avp_map_put(groups, key, &created);
            GcGroup *g;
            if (created) {
                g = AVP_NEW(a, GcGroup);
                g->unit = unit;
                g->section = section;
                g->sym = sname;
                g->sym_addr = saddr;
                g->sym_size = has_sym && sym.inside ? sym.size : 0;
                gv->p = g;
                avp_vec_push(&res->groups, g);
            } else {
                g = gv->p;
            }
            i64 n;
            if (piece_end == re_ && pos == rs) {
                n = nb;
            } else {
                AvpStr x, y;
                if (shsm_dol_read(a, retail, pos, piece_end - pos, &x) &&
                    shsm_dol_read(a, built, pos, piece_end - pos, &y)) {
                    n = 0;
                    for (size_t q = 0; q < x.n && q < y.n; q++) n += x.p[q] != y.p[q];
                } else {
                    n = piece_end - pos;
                }
            }
            g->nbytes += n;
            res->total_bytes += n;
            if (!g->has_first) {
                g->first = pos;
                g->has_first = true;
            }
            g->last_end = piece_end;
            if (has_rsym && strcmp(rsym.name, sname) != 0 && g->nretail < 3) {
                bool dup = false;
                for (int q = 0; q < g->nretail; q++) dup |= strcmp(g->retail_names[q], rsym.name) == 0;
                if (!dup) g->retail_names[g->nretail++] = rsym.name;
            }
            for (i64 wa = pos & ~(i64)3; wa < piece_end; wa += 4) {
                g->words_seen++;
                if ((i64)g->words.n < words) {
                    AvpStr rb, bb;
                    if (shsm_dol_read(a, retail, wa, 4, &rb) && shsm_dol_read(a, built, wa, 4, &bb) &&
                        !avp_str_eq(rb, bb)) {
                        GcWord w;
                        w.addr = wa;
                        memcpy(w.r, rb.p, 4);
                        memcpy(w.b, bb.p, 4);
                        avp_vec_push(&g->words, w);
                    }
                }
            }
            GcFirst *fp = NULL;
            for (size_t q = 0; q < res->first.n; q++)
                if (strcmp(res->first.v[q].section, section) == 0) fp = &res->first.v[q];
            if (!fp || pos < fp->addr) {
                if (!fp) fp = avp_vec_push_zero(&res->first);
                fp->section = section;
                fp->addr = pos;
                fp->unit = unit;
                fp->sym = sname;
                fp->saddr = saddr;
                fp->rname = has_rsym ? rsym.name : NULL;
                i64 sh = 0;
                if (nb >= 1 && !gc_shift_hint(a, retail, built, pos, &sh)) sh = 0;
                fp->shift = sh;
            }
            pos = piece_end;
        }
    }
    avp_map_free(groups);
    return res;
}

/* ---- format_attribution ------------------------------------------------------------------------------------------ */

static void put(AvpStrVec *out, AvpStr s) { avp_vec_push(out, s); }

static int cmp_first(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_cmp_i64(((const GcFirst *)x)->addr, ((const GcFirst *)y)->addr);
}

typedef struct {
    const char *unit;
    i64 first;
    AVP_VEC(GcGroup *) gs;
} UnitGroups;

static int cmp_unit_groups(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_cmp_i64((*(UnitGroups *const *)x)->first, (*(UnitGroups *const *)y)->first);
}

static bool same_unit(const char *x, const char *y) { return (!x && !y) || (x && y && strcmp(x, y) == 0); }

AvpStrVec gc_format_attribution(AvpArena *a, const GcAttribution *res, const GcUnitMap *units, i64 limit_symbols,
                                i64 limit_units, bool all_symbols, const char *built_path, const char *retail_path) {
    AvpStrVec out = {0};
    put(&out, avp_pyfmt(a, "built:  %s", built_path));
    put(&out, avp_pyfmt(a, "retail: %s", retail_path));
    if (res->header.n) {
        put(&out, AVP_S("DOL header differences:"));
        for (size_t i = 0; i < res->header.n; i++) put(&out, avp_pyfmt(a, "  %S", &res->header.v[i]));
    } else {
        put(&out, AVP_S("DOL header: sections and bss identical"));
    }
    const char *labs[2] = {"only in retail", "only in built"};
    const GcRunVec *regs[2] = {&res->only_retail, &res->only_built};
    for (int k = 0; k < 2; k++)
        for (size_t i = 0; i < regs[k]->n; i++) {
            i64 s = regs[k]->v[i].start, e = regs[k]->v[i].end;
            put(&out, avp_pyfmt(a, "%s: %s..%s (%s bytes)", labs[k], gc_hx(a, s, 8), gc_hx(a, e, 8),
                                gc_hx(a, e - s, 1)));
        }
    /* units in order of first appearance among the groups */
    AVP_VEC(UnitGroups *) ug = {0};
    for (size_t i = 0; i < res->groups.n; i++) {
        GcGroup *g = res->groups.v[i];
        UnitGroups *u = NULL;
        for (size_t q = 0; q < ug.n && !u; q++)
            if (same_unit(ug.v[q]->unit, g->unit)) u = ug.v[q];
        if (!u) {
            u = AVP_NEW(a, UnitGroups);
            u->unit = g->unit;
            u->first = g->first;
            avp_vec_push(&ug, u);
        }
        if (g->first < u->first) u->first = g->first;
        avp_vec_push(&u->gs, g);
    }
    put(&out, avp_pyfmt(a, "differing bytes: %d in %d runs; %d units, %d symbols", res->total_bytes, res->runs.n, ug.n,
                        res->groups.n));
    if (res->first.n) {
        put(&out, AVP_S("first difference per section:"));
        GcFirst *fs = AVP_NEWN(a, GcFirst, res->first.n);
        memcpy(fs, res->first.v, res->first.n * sizeof(GcFirst));
        avp_sort(fs, res->first.n, sizeof(GcFirst), cmp_first, NULL, false);
        for (size_t i = 0; i < res->first.n; i++) {
            const GcFirst *f = &fs[i];
            i64 off = f->saddr ? f->addr - f->saddr : 0;
            const char *offs = f->saddr && f->sym[0] != '<' ? avp_sprintf(a, "+0x%s", gc_hex(a, off)) : "";
            const char *rn =
                f->rname && strcmp(f->rname, f->sym) != 0 ? avp_sprintf(a, "  (retail: %s)", f->rname) : "";
            put(&out, avp_pyfmt(a, "  %-10s %s  %s [%s]  %s%s%s", f->section, gc_hx(a, f->addr, 8),
                                f->unit ? f->unit : "<no split: auto asm unit>", gc_unit_status(units, f->unit), f->sym,
                                offs, rn));
            if (f->shift)
                put(&out, avp_pyfmt(a,
                                    "             from here on the built bytes equal the retail bytes moved by %+d "
                                    "bytes: a size or alignment change at or just before this address",
                                    f->shift));
        }
    }
    avp_sort(ug.v, ug.n, sizeof(UnitGroups *), cmp_unit_groups, NULL, false);
    i64 shown_units = 0;
    for (size_t ui = 0; ui < ug.n; ui++) {
        UnitGroups *u = ug.v[ui];
        if (shown_units >= limit_units) {
            put(&out, avp_pyfmt(a, "... %d more units", (i64)ug.n - shown_units));
            break;
        }
        shown_units++;
        const char *st = gc_unit_status(units, u->unit);
        i64 total = 0;
        for (size_t q = 0; q < u->gs.n; q++) total += u->gs.v[q]->nbytes;
        put(&out, AVP_S(""));
        put(&out, avp_pyfmt(a, "== %s [%s]  %d bytes in %d symbols", u->unit ? u->unit : "<no split: auto asm unit>",
                            st, total, u->gs.n));
        if (strcmp(st, "Matching") != 0)
            put(&out, AVP_S("   note: unit is linked from retail asm; the bytes moved because an earlier Matching unit "
                            "changed size, order or alignment, or the unit is NonMatching/new"));
        for (size_t gi = 0; gi < u->gs.n; gi++) {
            const GcGroup *g = u->gs.v[gi];
            if (!all_symbols && (i64)gi >= limit_symbols) {
                put(&out, avp_pyfmt(a, "   ... %d more symbols (--all-symbols)", (i64)(u->gs.n - gi)));
                break;
            }
            bool real = g->sym[0] != '<';
            const char *where = real ? avp_sprintf(a, "%s (%s size %s)", g->sym, gc_hx(a, g->sym_addr, 8),
                                                   gc_hx(a, g->sym_size, 1))
                                     : g->sym;
            AvpBuf rn;
            avp_buf_init(&rn, a);
            if (g->nretail) {
                avp_buf_putz(&rn, "  retail: ");
                for (int q = 0; q < g->nretail; q++) {
                    if (q) avp_buf_putz(&rn, ", ");
                    avp_buf_putz(&rn, g->retail_names[q]);
                }
            }
            const char *off = g->sym_addr && real ? avp_sprintf(a, "+0x%s", gc_hex(a, g->first - g->sym_addr)) : "";
            put(&out, avp_pyfmt(a, "   %-8s %s%s", g->section, where, avp_buf_cz(&rn)));
            put(&out, avp_pyfmt(a, "            %d bytes differ, %d words touched, first %s%s", g->nbytes,
                                g->words_seen, gc_hx(a, g->first, 8), off[0] ? avp_sprintf(a, " (%s)", off) : ""));
            for (size_t wi = 0; wi < g->words.n; wi++) {
                const GcWord *w = &g->words.v[wi];
                u32 rw = (u32)w->r[0] << 24 | (u32)w->r[1] << 16 | (u32)w->r[2] << 8 | w->r[3];
                u32 bw = (u32)w->b[0] << 24 | (u32)w->b[1] << 16 | (u32)w->b[2] << 8 | w->b[3];
                const char *hint = gc_is_code_section(g->section)
                                       ? avp_sprintf(a, "  [%s]", gc_classify_word(a, rw, bw))
                                       : "";
                put(&out, avp_pyfmt(a, "            %s  retail %02X %02X %02X %02X  built %02X %02X %02X %02X%s",
                                    gc_hx(a, w->addr, 8), w->r[0], w->r[1], w->r[2], w->r[3], w->b[0], w->b[1],
                                    w->b[2], w->b[3], hint));
            }
        }
    }
    return out;
}

const char *gc_elf_dol_mismatch(AvpArena *a, const GcElf *e, const ShsmDol *dol) {
    int n = 0;
    for (size_t k = 0; k < e->nsecs; k++) {
        const GcSec *s = &e->secs[k];
        if (s->type != 1 || !(s->flags & 2) || !s->addr || !s->size) continue;
        n++;
        AvpStr got;
        if (!shsm_dol_read(a, dol, s->addr, s->size, &got))
            return avp_sprintf(a, "section %s (%s, size 0x%s) is not in the DOL", s->name, gc_hx(a, s->addr, 8),
                               gc_hex(a, s->size));
        AvpStr mine = gc_elf_section_data(e, s);
        if (!avp_str_eq(got, mine)) {
            for (i64 q = 0; q < s->size; q++) {
                if (s->off + q >= (i64)e->n) gc_raise(GC_EXC_INDEX, "index out of range");
                if ((u8)got.p[q] != e->data[s->off + q])
                    return avp_sprintf(a, "section %s differs from the DOL at %s", s->name, gc_hx(a, s->addr + q, 8));
            }
        }
    }
    if (n == 0) return "it has no loadable sections";
    return NULL;
}
