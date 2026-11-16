/* wall_describe: the referent describer and classify_result of tools/wall.py (L871-945, L979-993): _read_string,
 * describer, _looks_string, _read_dol_string, classify_result. See wall_classify.h. */
#include "wall_classify.h"
#include "avp/fmt.h"
#include "avp/json.h"

#include <string.h>

static void jset(AvpArena *a, AvpJson *o, const char *k, AvpJson *v) { avp_json_set(a, o, avp_str(k), v); }

/* bytes.decode("latin-1"), kept as UTF-8 */
static AvpStr latin1(AvpArena *a, const u8 *p, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putn(&b, "", 0);
    for (size_t i = 0; i < n; i++) avp_buf_putcp(&b, p[i]);
    return avp_strn(avp_buf_cz(&b), b.n);
}

/* _read_string(data, off, limit=80) */
static AvpStr read_string(AvpArena *a, AvpStr data, i64 off, i64 limit) {
    i64 n = (i64)data.n;
    if (off < 0 || off > n) return AVP_S("");
    i64 hi = off + 256 < n ? off + 256 : n, end = -1;
    for (i64 k = off; k < hi; k++)
        if (data.p[k] == 0) {
            end = k;
            break;
        }
    i64 stop = end >= 0 ? end : off + limit;
    if (stop > n) stop = n;
    if (stop - off > limit) stop = off + limit;
    return latin1(a, (const u8 *)data.p + off, (size_t)(stop > off ? stop - off : 0));
}

/* _read_dol_string(dol, addr, limit=80) */
static AvpStr read_dol_string(AvpArena *a, const CcDol *dol, i64 addr, size_t limit) {
    u8 buf[80];
    size_t n = 0;
    while (n < limit && n < sizeof buf) {
        AvpStr b;
        if (!cc_dol_read(dol, addr + (i64)n, 1, &b) || b.p[0] == 0) break;
        buf[n++] = (u8)b.p[0];
    }
    return latin1(a, buf, n);
}

/* _looks_string(b) */
static bool looks_string(const u8 *p, size_t n) {
    if (n <= 1 || p[n - 1] != 0) return false;
    for (size_t i = 0; i + 1 < n; i++)
        if (!((p[i] >= 32 && p[i] < 127) || p[i] == 9 || p[i] == 10 || p[i] == 13)) return false;
    return true;
}

static u64 be(const u8 *p, size_t n) {
    u64 v = 0;
    for (size_t i = 0; i < n; i++) v = v << 8 | p[i];
    return v;
}

/* struct.unpack(">f" / ">d", b)[0] */
static double unpack(const u8 *p, size_t n) {
    if (n == 8) {
        u64 v = be(p, 8);
        double d;
        memcpy(&d, &v, 8);
        return d;
    }
    u32 v = (u32)be(p, 4);
    float f;
    memcpy(&f, &v, 4);
    return (double)f;
}

/* ctx.retail_address(ref) (tools/codecmp.py Context.retail_address): false when it is None. */
static bool retail_address(CcContext *c, const CcRef *ref, i64 *ra) {
    const AvpI64Vec *cands = cc_project_addresses(c->project, ref->name);
    if (cands && cands->n) {
        i64 pick = cands->v[0];
        if (cands->n > 1) {
            CcUnitSecs s = cc_project_unit_sections(c->project, c->unit);
            size_t inside = 0;
            for (size_t k = 0; k < cands->n; k++) {
                bool in = false;
                for (size_t q = 0; q < s.n && !in; q++) in = s.v[q].start <= cands->v[k] && cands->v[k] < s.v[q].end;
                if (in) inside++, pick = cands->v[k];
            }
            if (inside != 1) return false;
        }
        *ra = pick + ref->off;
        return true;
    }
    i64 emb;
    if (cc_embedded_address(ref->name, &emb)) {
        *ra = emb + ref->off;
        return true;
    }
    return false;
}

typedef struct {
    const char *kind;
    bool is_str, has_value;
    AvpStr s;
    double f;
} Value;

static AvpJson *value_json(AvpArena *a, const Value *v) {
    if (!v->has_value) return avp_json_null();
    return v->is_str ? avp_json_str(a, v->s) : avp_json_float(a, v->f);
}

/* side_retail(ref) */
static AvpJson *side_retail(AvpArena *a, const WallDescriber *w, const CcRef *ref, Value *v) {
    i64 ra = 0;
    bool has_ra = retail_address(w->ctx, ref, &ra);
    const ShsmSymbol *s = w->symtab ? shsm_sym_by_name(w->symtab, ref->name) : NULL;
    *v = (Value){"global"};
    if (s && s->type && strcmp(s->type, "function") == 0) v->kind = "function";
    const CcDol *dol = cc_project_dol(w->ctx->project);
    AvpStr data = avp_str(s ? s->data : NULL);
    if (has_ra && dol) {
        bool s48 = s && s->has_size && (s->size == 4 || s->size == 8);
        if (avp_str_startswith(data, AVP_S("string")) || avp_str_startswith(ref->name, AVP_S("@stringBase"))) {
            AvpStr b;
            if (cc_dol_read(dol, ra, 1, &b)) {
                v->kind = "string";
                v->is_str = v->has_value = true;
                v->s = read_dol_string(a, dol, ra, 80);
            }
        } else if (avp_str_eqz(data, "float") || avp_str_eqz(data, "double") ||
                   (s48 && avp_str_startswith(ref->name, AVP_S("@")))) {
            size_t n = avp_str_eqz(data, "double") || (s && s->has_size && s->size == 8) ? 8 : 4;
            AvpStr b;
            if (cc_dol_read(dol, ra, (i64)n, &b)) {
                v->kind = n == 8 ? "double" : "float";
                v->has_value = true;
                v->f = unpack((const u8 *)b.p, n);
            }
        }
    }
    AvpJson *d = avp_json_obj(a);
    jset(a, d, "name", avp_json_str(a, ref->name));
    jset(a, d, "address", has_ra ? avp_json_int(a, ra) : avp_json_null());
    jset(a, d, "kind", avp_json_strz(a, v->kind));
    jset(a, d, "value", value_json(a, v));
    return d;
}

static bool sec_in(AvpStr sec, const char *const *names) {
    for (; *names; names++)
        if (avp_str_eqz(sec, *names)) return true;
    return false;
}

/* side_compiled(row) */
static AvpJson *side_compiled(AvpArena *a, const WallDescriber *w, const CcRow *row, Value *v) {
    const CcReloc *rel = row->reloc;
    const CcElf *elf = w->cfn->elf;
    AvpJson *d = avp_json_obj(a);
    *v = (Value){"unknown"};
    if (!rel || !elf) {
        jset(a, d, "name", row->has_ref ? avp_json_str(a, row->ref.name) : avp_json_null());
        jset(a, d, "kind", avp_json_strz(a, v->kind));
        jset(a, d, "value", avp_json_null());
        return d;
    }
    const CcSym *sym = &elf->symbols[rel->sym];
    AvpStr sec = cc_elf_section_name(elf, sym->shndx);
    v->kind = sym->type == CC_STT_FUNC || avp_str_startswith(sec, AVP_S(".text")) ? "function" : "global";
    if (0 < sym->shndx && sym->shndx < elf->nsections) {
        AvpStr data = elf->sections[sym->shndx].data;
        i64 off = (i64)sym->value + rel->addend;
        static const char *const strsecs[] = {".rodata", ".data", ".sdata", NULL};
        static const char *const fsecs[] = {".sdata2", ".rodata", NULL};
        bool looks = false;
        if (sec_in(sec, strsecs) && sym->size && off >= 0 && off <= (i64)data.n) {
            i64 hi = off + sym->size < (i64)data.n ? off + sym->size : (i64)data.n;
            looks = looks_string((const u8 *)data.p + off, (size_t)(hi - off));
        }
        if (avp_str_startswith(sym->name, AVP_S("@stringBase")) || looks) {
            v->kind = "string";
            v->is_str = v->has_value = true;
            v->s = read_string(a, data, off, 80);
        } else if ((sym->size == 4 || sym->size == 8) && sec_in(sec, fsecs) &&
                   avp_str_startswith(sym->name, AVP_S("@"))) {
            v->kind = sym->size == 8 ? "double" : "float";
            /* struct.unpack of a short slice raised in the Python; the value stays None here */
            if (off >= 0 && off + sym->size <= (i64)data.n) {
                v->has_value = true;
                v->f = unpack((const u8 *)data.p + off, sym->size);
            }
        }
    }
    jset(a, d, "name", avp_json_str(a, sym->name.n ? sym->name : sec));
    jset(a, d, "kind", avp_json_strz(a, v->kind));
    jset(a, d, "value", value_json(a, v));
    return d;
}

static bool values_equal(const Value *x, const Value *y) {
    if (x->has_value != y->has_value) return false;
    if (!x->has_value) return true;
    if (x->is_str != y->is_str) return false;
    return x->is_str ? avp_str_eq(x->s, y->s) : x->f == y->f;
}

AvpJson *wall_describe(void *describer, AvpArena *a, size_t i, size_t j) {
    const WallDescriber *w = describer;
    const CcRow *r = &w->rfn->rows[i], *c = &w->cfn->rows[j];
    Value va, vb;
    AvpJson *ja;
    if (r->has_ref) {
        ja = side_retail(a, w, &r->ref, &va);
    } else {
        va = (Value){"unknown"};
        ja = avp_json_obj(a);
        jset(a, ja, "name", avp_json_str(a, r->text));
        jset(a, ja, "kind", avp_json_strz(a, "unknown"));
        jset(a, ja, "value", avp_json_null());
    }
    AvpJson *jb = side_compiled(a, w, c, &vb);
    bool same_kind = strcmp(va.kind, vb.kind) == 0;
    AvpStr same;
    if (same_kind && (!strcmp(va.kind, "string") || !strcmp(va.kind, "float") || !strcmp(va.kind, "double")))
        same = avp_pyfmt(a, values_equal(&va, &vb) ? "same %s value" : "different %s value", va.kind);
    else if (same_kind && !strcmp(va.kind, "function"))
        same = AVP_S("a different function");
    else if (!strcmp(va.kind, "function") || !strcmp(vb.kind, "function"))
        same = AVP_S("function versus data");
    else
        same = same_kind ? AVP_S("a different global") : avp_pyfmt(a, "%s versus %s", va.kind, vb.kind);
    AvpJson *d = avp_json_obj(a);
    jset(a, d, "retail", ja);
    jset(a, d, "compiled", jb);
    jset(a, d, "same", avp_json_str(a, same));
    return d;
}

AvpJson *wall_classify_result(AvpArena *a, const char *unit, const CcCompareResult *res, const ShsmRepo *r,
                              const ShsmSymbolTable *symtab) {
    CcRepo cr = {r->root, r->version};
    CcContext *ctx = cc_repo_context(&cr, avp_str(unit));
    WallDescriber w = {res->retail_fn, res->compiled_fn, ctx, symtab};
    static const CcTdList none = {0};
    const CcTdList *targets = res->target_mismatches ? res->target_mismatches : &none;
    size_t nunres = res->unresolved ? res->unresolved->n : 0;
    AvpJson *d = wall_classify(a, res->retail_fn, res->compiled_fn, targets, wall_describe, &w, ctx->project, nunres);
    jset(a, d, "name", avp_json_str(a, res->name));
    jset(a, d, "retail_name", avp_json_str(a, res->retail_name));
    jset(a, d, "exact_codecmp", avp_json_bool(res->exact));
    if (res->exact) {
        jset(a, d, "exact", avp_json_bool(true));
        jset(a, d, "wall", avp_json_null());
        jset(a, d, "verdict", avp_json_strz(a, "EXACT"));
        jset(a, d, "classes", avp_json_arr(a));
    } else if (avp_json_truthy(avp_json_getz(d, "exact"))) {
        /* every row agrees here but codecmp found a difference this classifier does not see */
        jset(a, d, "exact", avp_json_bool(false));
        jset(a, d, "wall", avp_json_null());
        jset(a, d, "classes", avp_json_arr(a));
        jset(a, d, "verdict",
             avp_json_str(a, avp_pyfmt(a,
                                       "EQUIVALENT-LOOKING: rows agree, but codecmp reports %d target mismatch(es) "
                                       "and %d unresolved target(s)",
                                       targets->n, nunres)));
    }
    return d;
}
