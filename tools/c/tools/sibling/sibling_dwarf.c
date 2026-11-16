/* sibling: the DWARF 2 reader (sections, abbreviations, DIEs, type declarations, locations, line programs). */
#include "sibling_dwarf.h"

#include <string.h>

/* ---- byte cursor (big-endian) ---- */

typedef struct {
    const u8 *p, *end;
    bool bad;
} Cur;

static u32 rd_u8(Cur *c) {
    if (c->p + 1 > c->end) { c->bad = true; c->p = c->end; return 0; }
    return *c->p++;
}
static u32 rd_u16(Cur *c) {
    if (c->p + 2 > c->end) { c->bad = true; c->p = c->end; return 0; }
    u32 v = ((u32)c->p[0] << 8) | c->p[1];
    c->p += 2;
    return v;
}
static u32 rd_u32(Cur *c) {
    if (c->p + 4 > c->end) { c->bad = true; c->p = c->end; return 0; }
    u32 v = ((u32)c->p[0] << 24) | ((u32)c->p[1] << 16) | ((u32)c->p[2] << 8) | c->p[3];
    c->p += 4;
    return v;
}
static u64 rd_uleb(Cur *c) {
    u64 v = 0;
    unsigned shift = 0;
    for (;;) {
        if (c->p >= c->end) { c->bad = true; return v; }
        u8 b = *c->p++;
        if (shift < 64) v |= (u64)(b & 0x7F) << shift;
        shift += 7;
        if (!(b & 0x80)) return v;
    }
}
static i64 rd_sleb(Cur *c) {
    i64 v = 0;
    unsigned shift = 0;
    u8 b = 0;
    do {
        if (c->p >= c->end) { c->bad = true; return v; }
        b = *c->p++;
        if (shift < 64) v |= (i64)((u64)(b & 0x7F) << shift);
        shift += 7;
    } while (b & 0x80);
    if (shift < 64 && (b & 0x40)) v |= (i64)(~(u64)0 << shift);
    return v;
}
static const char *rd_str(Cur *c) {
    const u8 *s = c->p;
    while (c->p < c->end && *c->p) c->p++;
    if (c->p >= c->end) { c->bad = true; return NULL; }
    c->p++;
    return (const char *)s;
}
static void skip(Cur *c, u64 n) {
    if ((u64)(c->end - c->p) < n) { c->bad = true; c->p = c->end; return; }
    c->p += n;
}

static u32 be32(const u8 *p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }
static u32 be16(const u8 *p) { return ((u32)p[0] << 8) | p[1]; }

/* ---- names ---- */

const char *sd_tag_name(unsigned tag) {
    switch (tag) {
    case SD_TAG_array_type: return "array_type";
    case SD_TAG_class_type: return "class_type";
    case SD_TAG_enumeration_type: return "enumeration_type";
    case SD_TAG_formal_parameter: return "formal_parameter";
    case SD_TAG_lexical_block: return "lexical_block";
    case SD_TAG_member: return "member";
    case SD_TAG_pointer_type: return "pointer_type";
    case SD_TAG_reference_type: return "reference_type";
    case SD_TAG_compile_unit: return "compile_unit";
    case SD_TAG_structure_type: return "structure_type";
    case SD_TAG_subroutine_type: return "subroutine_type";
    case SD_TAG_typedef: return "typedef";
    case SD_TAG_union_type: return "union_type";
    case SD_TAG_unspecified_parameters: return "unspecified_parameters";
    case SD_TAG_inheritance: return "inheritance";
    case SD_TAG_inlined_subroutine: return "inlined_subroutine";
    case SD_TAG_ptr_to_member_type: return "ptr_to_member_type";
    case SD_TAG_subrange_type: return "subrange_type";
    case SD_TAG_base_type: return "base_type";
    case SD_TAG_const_type: return "const_type";
    case SD_TAG_enumerator: return "enumerator";
    case SD_TAG_subprogram: return "subprogram";
    case SD_TAG_variable: return "variable";
    case SD_TAG_volatile_type: return "volatile_type";
    default: return NULL;
    }
}

bool sd_is_aggregate(unsigned tag) {
    return tag == SD_TAG_class_type || tag == SD_TAG_structure_type || tag == SD_TAG_union_type;
}
bool sd_is_named_type(unsigned tag) {
    return sd_is_aggregate(tag) || tag == SD_TAG_enumeration_type || tag == SD_TAG_typedef;
}

/* ---- ELF ---- */

bool sd_elf_sections(AvpStr elf, SdSections *out) {
    memset(out, 0, sizeof *out);
    const u8 *b = (const u8 *)elf.p;
    if (elf.n < 52 || memcmp(b, "\x7f" "ELF", 4) != 0 || b[4] != 1 || b[5] != 2) return false;
    u32 shoff = be32(b + 32), shentsize = be16(b + 46), shnum = be16(b + 48), shstrndx = be16(b + 50);
    if (shentsize < 40 || shoff > elf.n || (u64)shnum * shentsize > elf.n - shoff || shstrndx >= shnum) return false;
    const u8 *strh = b + shoff + (size_t)shstrndx * shentsize;
    u32 str_off = be32(strh + 16), str_size = be32(strh + 20);
    if (str_off > elf.n || str_size > elf.n - str_off) return false;
    for (u32 i = 0; i < shnum; i++) {
        const u8 *h = b + shoff + (size_t)i * shentsize;
        u32 name = be32(h), off = be32(h + 16), size = be32(h + 20);
        if (name >= str_size || off > elf.n || size > elf.n - off) continue;
        const char *nm = (const char *)b + str_off + name;
        size_t room = str_size - name;
        if (!memchr(nm, 0, room)) continue;
        SdSec s = {b + off, size};
        if (strcmp(nm, ".debug_abbrev") == 0) out->abbrev = s;
        else if (strcmp(nm, ".debug_info") == 0) out->info = s;
        else if (strcmp(nm, ".debug_line") == 0) out->line = s;
        else if (strcmp(nm, ".debug_loc") == 0) out->loc = s;
    }
    return out->info.n > 0 && out->abbrev.n > 0;
}

/* ---- abbreviations ---- */

typedef struct {
    u32 code, tag;
    bool children;
    u16 nattr;
    u16 *at, *form;
} Abbrev;
typedef struct {
    u32 off;
    Abbrev *v;
    size_t n;
    Abbrev **direct; /* code -> abbrev, codes below ndirect */
    size_t ndirect;
} AbbrevTable;

static AbbrevTable *read_abbrevs(AvpArena *a, const SdSec *sec, u32 off) {
    AbbrevTable *t = AVP_NEW(a, AbbrevTable);
    t->off = off;
    if (off >= sec->n) return t;
    Cur c = {sec->p + off, sec->p + sec->n, false};
    AVP_VEC(Abbrev) v = {0};
    u32 maxcode = 0;
    for (;;) {
        u64 code = rd_uleb(&c);
        if (c.bad || code == 0) break;
        Abbrev ab = {0};
        ab.code = (u32)code;
        ab.tag = (u32)rd_uleb(&c);
        ab.children = rd_u8(&c) != 0;
        AVP_VEC(u16) at = {0}, fm = {0};
        for (;;) {
            u64 x = rd_uleb(&c), f = rd_uleb(&c);
            if (c.bad || (x == 0 && f == 0)) break;
            avp_vec_push(&at, (u16)x);
            avp_vec_push(&fm, (u16)f);
        }
        ab.nattr = (u16)at.n;
        ab.at = AVP_NEWN(a, u16, at.n + 1);
        ab.form = AVP_NEWN(a, u16, at.n + 1);
        if (at.n) memcpy(ab.at, at.v, at.n * sizeof(u16)), memcpy(ab.form, fm.v, fm.n * sizeof(u16));
        avp_vec_free(&at);
        avp_vec_free(&fm);
        if (ab.code > maxcode) maxcode = ab.code;
        avp_vec_push(&v, ab);
        if (c.bad) break;
    }
    t->n = v.n;
    t->v = AVP_NEWN(a, Abbrev, v.n + 1);
    if (v.n) memcpy(t->v, v.v, v.n * sizeof(Abbrev));
    avp_vec_free(&v);
    if (maxcode < 65536) {
        t->ndirect = (size_t)maxcode + 1;
        t->direct = AVP_NEWN(a, Abbrev *, t->ndirect);
        for (size_t i = 0; i < t->n; i++) t->direct[t->v[i].code] = &t->v[i];
    }
    return t;
}

static const Abbrev *find_abbrev(const AbbrevTable *t, u64 code) {
    if (t->direct) return code < t->ndirect ? t->direct[code] : NULL;
    for (size_t i = 0; i < t->n; i++)
        if (t->v[i].code == code) return &t->v[i];
    return NULL;
}

/* ---- DIEs ---- */

typedef struct {
    u64 u;             /* constant, address or reference (made absolute) */
    i64 s;
    const u8 *block;   /* block forms */
    u32 block_n;
    const char *str;
    bool is_block, is_const, is_signed;
} Val;

/* Read one attribute value of form `form`; false when the form cannot be sized. */
static bool read_form(Cur *c, unsigned form, u32 cu_off, const SdSec *info, Val *v) {
    (void)info;
    memset(v, 0, sizeof *v);
    switch (form) {
    case SD_FORM_addr: v->u = rd_u32(c); return true;
    case SD_FORM_block1: v->block_n = rd_u8(c); goto block;
    case SD_FORM_block2: v->block_n = rd_u16(c); goto block;
    case SD_FORM_block4: v->block_n = rd_u32(c); goto block;
    case SD_FORM_block: v->block_n = (u32)rd_uleb(c); goto block;
    case SD_FORM_data1: v->u = rd_u8(c); v->is_const = true; return true;
    case SD_FORM_data2: v->u = rd_u16(c); v->is_const = true; return true;
    case SD_FORM_data4: v->u = rd_u32(c); v->is_const = true; return true;
    case SD_FORM_data8: { u64 hi = rd_u32(c); v->u = (hi << 32) | rd_u32(c); v->is_const = true; return true; }
    case SD_FORM_sdata: v->s = rd_sleb(c); v->u = (u64)v->s; v->is_const = v->is_signed = true; return true;
    case SD_FORM_udata: v->u = rd_uleb(c); v->is_const = true; return true;
    case SD_FORM_flag: v->u = rd_u8(c); return true;
    case SD_FORM_string: v->str = rd_str(c); return true;
    case SD_FORM_strp: v->u = rd_u32(c); return true; /* .debug_str is not read: no MWCC unit uses it */
    case SD_FORM_ref_addr: v->u = rd_u32(c); return true;
    case SD_FORM_ref1: v->u = cu_off + rd_u8(c); return true;
    case SD_FORM_ref2: v->u = cu_off + rd_u16(c); return true;
    case SD_FORM_ref4: v->u = cu_off + rd_u32(c); return true;
    case SD_FORM_ref8: { rd_u32(c); v->u = cu_off + rd_u32(c); return true; }
    case SD_FORM_ref_udata: v->u = cu_off + rd_uleb(c); return true;
    case SD_FORM_indirect: return read_form(c, (unsigned)rd_uleb(c), cu_off, info, v);
    default: return false;
    }
block:
    v->block = c->p;
    v->is_block = true;
    skip(c, v->block_n);
    return true;
}

/* DW_OP_plus_uconst N (or a plain constant) as a member offset */
static bool member_offset(const Val *v, u32 *out) {
    if (v->is_const) { *out = (u32)v->u; return true; }
    if (!v->is_block || v->block_n == 0) return false;
    Cur c = {v->block, v->block + v->block_n, false};
    u32 op = rd_u8(&c);
    if (op == 0x23) { *out = (u32)rd_uleb(&c); return !c.bad && c.p == c.end; }
    if (op >= 0x08 && op <= 0x11) { /* constN followed by plus */
        u64 k = 0;
        switch (op) {
        case 0x08: k = rd_u8(&c); break;
        case 0x0a: k = rd_u16(&c); break;
        case 0x0c: k = rd_u32(&c); break;
        case 0x10: k = rd_uleb(&c); break;
        default: return false;
        }
        if (rd_u8(&c) != 0x22 || c.bad) return false;
        *out = (u32)k;
        return true;
    }
    return false;
}

typedef AVP_VEC(SdDie) DieVec;
typedef AVP_VEC(SdCu) CuVec;

static void note(AvpArena *a, SdInfo *d, const char *text) {
    if (d->notes.n < 200) avp_vec_push(&d->notes, avp_str(avp_strdup(a, text)));
}

static void apply(SdDie *x, SdCu *cu, unsigned at, unsigned form, const Val *v) {
    switch (at) {
    case SD_AT_name: if (v->str) x->name = v->str; break;
    case SD_AT_type: x->type = (u32)v->u; break;
    case SD_AT_containing_type: x->containing_type = (u32)v->u; break;
    case SD_AT_byte_size: x->byte_size = (u32)v->u; x->flags |= SD_F_SIZE; break;
    case SD_AT_bit_size: x->bit_size = (u16)v->u; x->flags |= SD_F_BITS; break;
    case SD_AT_bit_offset: x->bit_offset = (u16)v->u; x->flags |= SD_F_BITS; break;
    case SD_AT_data_member_location: {
        u32 o;
        if (member_offset(v, &o)) { x->member_loc = o; x->flags |= SD_F_MEMBER_LOC; }
        break;
    }
    case SD_AT_low_pc: x->low = (u32)v->u; x->flags |= SD_F_LOW; break;
    case SD_AT_high_pc: x->high = (u32)v->u; x->flags |= SD_F_HIGH; break;
    case SD_AT_decl_line: x->decl_line = (u16)v->u; break;
    case SD_AT_decl_file: x->decl_file = (u16)v->u; break;
    case SD_AT_external: if (v->u) x->flags |= SD_F_EXTERNAL; break;
    case SD_AT_declaration: if (v->u) x->flags |= SD_F_DECLARATION; break;
    case SD_AT_accessibility: x->accessibility = (u8)v->u; break;
    case SD_AT_virtuality: x->virtuality = (u8)v->u; break;
    case SD_AT_encoding: x->encoding = (u8)v->u; break;
    case SD_AT_location:
        x->flags |= SD_F_LOC;
        if (v->is_block) { x->loc_expr = v->block; x->loc_n = v->block_n; }
        else { x->loc = (u32)v->u; x->flags |= SD_F_LOCLIST; } /* DWARF 2: data4 is a .debug_loc offset */
        break;
    case SD_AT_frame_base:
        if (!v->is_block) { x->frame_base = (u32)v->u; x->flags |= SD_F_FRAME; }
        break;
    case SD_AT_const_value: x->const_value = v->is_signed ? v->s : (form == SD_FORM_data4 ? (i64)(i32)(u32)v->u : (i64)v->u); x->flags |= SD_F_CONST; break;
    case SD_AT_upper_bound: x->upper_bound = (i64)v->u; x->flags |= SD_F_UPPER; break;
    case SD_AT_producer: if (v->str) cu->producer = v->str; break;
    case SD_AT_comp_dir: if (v->str) cu->comp_dir = v->str; break;
    case SD_AT_stmt_list: cu->stmt_list = (u32)v->u; cu->has_stmt = true; break;
    case SD_AT_language: cu->language = (u32)v->u; break;
    default: break;
    }
}

SdInfo *sd_parse(AvpArena *a, const SdSections *sec) {
    SdInfo *d = AVP_NEW(a, SdInfo);
    d->sec = *sec;
    DieVec dies = {0};
    CuVec cus = {0};
    AbbrevTable *tables[8] = {0};
    size_t ntables = 0;
    u32 off = 0;
    u32 stack[256];
    while ((size_t)off + 11 <= sec->info.n) {
        Cur h = {sec->info.p + off, sec->info.p + sec->info.n, false};
        u32 len = rd_u32(&h);
        u32 ver = rd_u16(&h), abbrev_off = rd_u32(&h), asize = rd_u8(&h);
        SdCu cu = {0};
        cu.off = off;
        cu.end = (u32)((u64)off + 4 + len > sec->info.n ? sec->info.n : (u64)off + 4 + len);
        cu.die = SD_NONE;
        if (len == 0xFFFFFFFFu || len < 7) {
            note(a, d, avp_sprintf(a, "unit at 0x%x: unit length 0x%x not supported; the rest of .debug_info is unread", off, len));
            break;
        }
        if (ver != 2 || asize != 4) {
            note(a, d, avp_sprintf(a, "unit at 0x%x: DWARF version %u, address size %u (only 2 and 4 are read); skipped", off, ver, asize));
            avp_vec_push(&cus, cu);
            off = cu.end;
            continue;
        }
        AbbrevTable *t = NULL;
        for (size_t k = 0; k < ntables; k++)
            if (tables[k]->off == abbrev_off) t = tables[k];
        if (!t) {
            t = read_abbrevs(a, &sec->abbrev, abbrev_off);
            if (ntables < 8) tables[ntables++] = t;
        }
        u32 cu_index = (u32)cus.n;
        avp_vec_push(&cus, cu);
        SdCu *pc = &cus.v[cu_index];
        Cur c = {sec->info.p + off + 11, sec->info.p + cu.end, false};
        int depth = 0;
        while (c.p < c.end) {
            u32 die_off = (u32)(c.p - sec->info.p);
            u64 code = rd_uleb(&c);
            if (c.bad) break;
            if (code == 0) {
                if (depth > 0) {
                    depth--;
                    SdDie *pd = &dies.v[stack[depth]];
                    pd->next = (u32)dies.n;
                }
                continue;
            }
            const Abbrev *ab = find_abbrev(t, code);
            if (!ab) {
                note(a, d, avp_sprintf(a, "DIE 0x%x: abbreviation %llu not in the table; rest of unit 0x%x skipped", die_off, (unsigned long long)code, off));
                break;
            }
            SdDie x = {0};
            x.off = die_off;
            x.tag = (u16)ab->tag;
            x.depth = (u8)(depth > 255 ? 255 : depth);
            x.parent = depth > 0 ? stack[depth - 1] : SD_NONE;
            x.cu = cu_index;
            x.next = (u32)dies.n + 1;
            bool ok = true;
            for (u16 k = 0; k < ab->nattr; k++) {
                Val v;
                if (!read_form(&c, ab->form[k], off, &sec->info, &v)) {
                    note(a, d, avp_sprintf(a, "DIE 0x%x (%s): form 0x%x of attribute 0x%x cannot be sized; rest of unit 0x%x skipped",
                                           die_off, sd_tag_name(ab->tag) ? sd_tag_name(ab->tag) : "?", ab->form[k], ab->at[k], off));
                    ok = false;
                    break;
                }
                apply(&x, pc, ab->at[k], ab->form[k], &v);
            }
            if (!ok || c.bad) {
                if (ok) note(a, d, avp_sprintf(a, "DIE 0x%x runs past the end of unit 0x%x", die_off, off));
                break;
            }
            if (ab->children) x.flags |= SD_F_CHILDREN;
            if (x.tag == SD_TAG_compile_unit && depth == 0) {
                pc->die = (u32)dies.n;
                pc->name = x.name;
                if ((x.flags & SD_F_LOW) && (x.flags & SD_F_HIGH)) { pc->has_pc = true; pc->low = x.low; pc->high = x.high; }
            }
            if (!sd_tag_name(x.tag) && d->notes.n < 200) {
                bool seen = false;
                for (size_t k = 0; k < d->notes.n; k++)
                    if (avp_str_contains(d->notes.v[k], avp_str(avp_sprintf(a, "unknown tag 0x%x", x.tag)))) seen = true;
                if (!seen) note(a, d, avp_sprintf(a, "unknown tag 0x%x (first at DIE 0x%x): kept, not interpreted", x.tag, die_off));
            }
            avp_vec_push(&dies, x);
            if (ab->children) {
                if (depth < 255) stack[depth++] = (u32)dies.n - 1;
                else { note(a, d, avp_sprintf(a, "unit 0x%x nests deeper than 255; rest skipped", off)); break; }
            }
        }
        /* close what the unit left open */
        while (depth > 0) {
            depth--;
            dies.v[stack[depth]].next = (u32)dies.n;
        }
        off = cu.end;
    }
    d->ndies = dies.n;
    d->dies = dies.n ? avp_memdup(a, dies.v, dies.n * sizeof(SdDie)) : NULL;
    d->ncus = cus.n;
    d->cus = cus.n ? avp_memdup(a, cus.v, cus.n * sizeof(SdCu)) : NULL;
    avp_vec_free(&dies);
    avp_vec_free(&cus);
    return d;
}

const SdDie *sd_die_at(const SdInfo *d, u32 off) {
    size_t lo = 0, hi = d->ndies;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (d->dies[mid].off < off) lo = mid + 1;
        else hi = mid;
    }
    return lo < d->ndies && d->dies[lo].off == off ? &d->dies[lo] : NULL;
}

/* ---- types ---- */

static u32 type_size_r(const SdInfo *d, u32 type, int depth) {
    const SdDie *x = sd_die_at(d, type);
    if (!x || depth > 32) return 0;
    switch (x->tag) {
    case SD_TAG_pointer_type:
    case SD_TAG_reference_type: return (x->flags & SD_F_SIZE) ? x->byte_size : 4;
    case SD_TAG_typedef:
    case SD_TAG_const_type:
    case SD_TAG_volatile_type: return type_size_r(d, x->type, depth + 1);
    case SD_TAG_array_type: {
        if (x->flags & SD_F_SIZE) return x->byte_size;
        u64 n = 1;
        for (u32 k = sd_index(d, x) + 1; k < x->next; k++)
            if (d->dies[k].tag == SD_TAG_subrange_type && d->dies[k].parent == sd_index(d, x))
                n *= (d->dies[k].flags & SD_F_UPPER) ? (u64)(d->dies[k].upper_bound + 1) : 0;
        return (u32)(n * type_size_r(d, x->type, depth + 1));
    }
    default: return (x->flags & SD_F_SIZE) ? x->byte_size : 0;
    }
}
u32 sd_type_size(const SdInfo *d, u32 type) { return type_size_r(d, type, 0); }

static const char *anon_name(AvpArena *a, const SdDie *x) {
    const char *kw = x->tag == SD_TAG_union_type ? "union" : x->tag == SD_TAG_enumeration_type ? "enum"
                     : x->tag == SD_TAG_class_type ? "class" : "struct";
    return avp_sprintf(a, "%s <anon@0x%x>", kw, x->off);
}

static const char *join_decl(AvpArena *a, const char *base, const char *inner) {
    return *inner ? avp_sprintf(a, "%s %s", base, inner) : base;
}

static const char *decl_r(AvpArena *a, const SdInfo *d, u32 type, const char *inner, int depth) {
    if (!type) return join_decl(a, "void", inner);
    const SdDie *x = sd_die_at(d, type);
    if (!x) return join_decl(a, avp_sprintf(a, "<type@0x%x?>", type), inner);
    if (depth > 24) return join_decl(a, "<...>", inner);
    bool wrap = inner[0] == '*' || inner[0] == '&' || strstr(inner, "::*") != NULL;
    switch (x->tag) {
    case SD_TAG_pointer_type: return decl_r(a, d, x->type, avp_sprintf(a, "*%s", inner), depth + 1);
    case SD_TAG_reference_type: return decl_r(a, d, x->type, avp_sprintf(a, "&%s", inner), depth + 1);
    case SD_TAG_const_type: return decl_r(a, d, x->type, *inner ? avp_sprintf(a, "const %s", inner) : "const", depth + 1);
    case SD_TAG_volatile_type: return decl_r(a, d, x->type, *inner ? avp_sprintf(a, "volatile %s", inner) : "volatile", depth + 1);
    case SD_TAG_ptr_to_member_type: {
        const SdDie *cls = sd_die_at(d, x->containing_type);
        const char *cn = cls && cls->name ? cls->name : "?";
        return decl_r(a, d, x->type, avp_sprintf(a, "%s::*%s", cn, inner), depth + 1);
    }
    case SD_TAG_array_type: {
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_putz(&b, wrap ? avp_sprintf(a, "(%s)", inner) : inner);
        bool any = false;
        u32 i = sd_index(d, x);
        for (u32 k = i + 1; k < x->next; k++) {
            const SdDie *s = &d->dies[k];
            if (s->parent != i || s->tag != SD_TAG_subrange_type) continue;
            any = true;
            if (s->flags & SD_F_UPPER) avp_buf_printf(&b, "[%lld]", (long long)(s->upper_bound + 1));
            else avp_buf_putz(&b, "[]");
        }
        if (!any) avp_buf_putz(&b, "[]");
        return decl_r(a, d, x->type, avp_buf_cz(&b), depth + 1);
    }
    case SD_TAG_subroutine_type: {
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_putz(&b, wrap ? avp_sprintf(a, "(%s)", inner) : inner);
        avp_buf_putc(&b, '(');
        u32 i = sd_index(d, x);
        int n = 0;
        for (u32 k = i + 1; k < x->next; k++) {
            const SdDie *s = &d->dies[k];
            if (s->parent != i) continue;
            if (s->tag == SD_TAG_formal_parameter) {
                avp_buf_printf(&b, "%s%s", n++ ? ", " : "", decl_r(a, d, s->type, "", depth + 1));
            } else if (s->tag == SD_TAG_unspecified_parameters) {
                avp_buf_printf(&b, "%s...", n++ ? ", " : "");
            }
        }
        if (!n) avp_buf_putz(&b, "void");
        avp_buf_putc(&b, ')');
        return decl_r(a, d, x->type, avp_buf_cz(&b), depth + 1);
    }
    default:
        if (x->name) return join_decl(a, x->name, inner);
        if (sd_is_named_type(x->tag)) return join_decl(a, anon_name(a, x), inner);
        return join_decl(a, avp_sprintf(a, "<%s@0x%x>", sd_tag_name(x->tag) ? sd_tag_name(x->tag) : "tag?", x->off), inner);
    }
}

const char *sd_decl(AvpArena *a, const SdInfo *d, u32 type, const char *inner) {
    return decl_r(a, d, type, inner ? inner : "", 0);
}

/* ---- locations ---- */

static const char *reg_name(AvpArena *a, u64 r) {
    if (r < 32) return avp_sprintf(a, "r%llu", (unsigned long long)r);
    if (r < 64) return avp_sprintf(a, "f%llu", (unsigned long long)(r - 32));
    return avp_sprintf(a, "reg%llu", (unsigned long long)r);
}

static const char *signed_hex(AvpArena *a, i64 v) {
    return v < 0 ? avp_sprintf(a, "-0x%llx", (unsigned long long)-v) : avp_sprintf(a, "+0x%llx", (unsigned long long)v);
}

const char *sd_expr_text(AvpArena *a, const u8 *p, size_t n) {
    Cur c = {p, p + n, false};
    AvpBuf b;
    avp_buf_init(&b, a);
    while (c.p < c.end && !c.bad) {
        if (b.n) avp_buf_putc(&b, ' ');
        u32 op = rd_u8(&c);
        if (op >= 0x50 && op <= 0x6f) avp_buf_putz(&b, reg_name(a, op - 0x50));
        else if (op >= 0x70 && op <= 0x8f) avp_buf_printf(&b, "[%s%s]", reg_name(a, op - 0x70), signed_hex(a, rd_sleb(&c)));
        else if (op >= 0x30 && op <= 0x4f) avp_buf_printf(&b, "lit%u", op - 0x30);
        else switch (op) {
        case 0x03: avp_buf_printf(&b, "@0x%08X", rd_u32(&c)); break;
        case 0x06: avp_buf_putz(&b, "deref"); break;
        case 0x08: avp_buf_printf(&b, "const %u", rd_u8(&c)); break;
        case 0x09: avp_buf_printf(&b, "const %d", (int)(i8)rd_u8(&c)); break;
        case 0x0a: avp_buf_printf(&b, "const %u", rd_u16(&c)); break;
        case 0x0b: avp_buf_printf(&b, "const %d", (int)(i16)rd_u16(&c)); break;
        case 0x0c: avp_buf_printf(&b, "const 0x%x", rd_u32(&c)); break;
        case 0x0d: avp_buf_printf(&b, "const %d", (int)(i32)rd_u32(&c)); break;
        case 0x10: avp_buf_printf(&b, "const %llu", (unsigned long long)rd_uleb(&c)); break;
        case 0x11: avp_buf_printf(&b, "const %lld", (long long)rd_sleb(&c)); break;
        case 0x22: avp_buf_putz(&b, "plus"); break;
        case 0x23: avp_buf_printf(&b, "plus 0x%llx", (unsigned long long)rd_uleb(&c)); break;
        case 0x90: avp_buf_putz(&b, reg_name(a, rd_uleb(&c))); break;
        case 0x91: avp_buf_printf(&b, "fb%s", signed_hex(a, rd_sleb(&c))); break;
        case 0x92: { u64 r = rd_uleb(&c); avp_buf_printf(&b, "[%s%s]", reg_name(a, r), signed_hex(a, rd_sleb(&c))); break; }
        case 0x93: avp_buf_printf(&b, "piece %llu", (unsigned long long)rd_uleb(&c)); break;
        default: avp_buf_printf(&b, "op0x%02x?", op); c.p = c.end; break; /* operands unknown: stop */
        }
    }
    if (c.bad) avp_buf_putz(&b, " <truncated>");
    return b.n ? avp_buf_cz(&b) : "<empty>";
}

SdLocVec sd_loclist(AvpArena *a, const SdInfo *d, u32 off, u32 base) {
    SdLocVec v = {0};
    if (off >= d->sec.loc.n) return v;
    Cur c = {d->sec.loc.p + off, d->sec.loc.p + d->sec.loc.n, false};
    for (int guard = 0; guard < 4096; guard++) {
        u32 lo = rd_u32(&c), hi = rd_u32(&c);
        if (c.bad || (lo == 0 && hi == 0)) break;
        if (lo == 0xFFFFFFFFu) { base = hi; continue; }
        u32 n = rd_u16(&c);
        if (c.bad || n > (u32)(c.end - c.p)) break;
        SdLocRange r;
        if (lo == 0 && hi == 0xFFFFFFFFu) { r.begin = 0; r.end = SD_NONE; }
        else { r.begin = base + lo; r.end = base + hi; }
        r.expr = sd_expr_text(a, c.p, n);
        c.p += n;
        avp_vec_push(&v, r);
    }
    return v;
}

const char *sd_location_text(AvpArena *a, const SdInfo *d, const SdDie *x, u32 (*map)(void *, u32), void *ctx) {
    if (!(x->flags & SD_F_LOC)) return "-";
    if (!(x->flags & SD_F_LOCLIST)) return sd_expr_text(a, x->loc_expr, x->loc_n);
    const SdCu *cu = &d->cus[x->cu];
    SdLocVec v = sd_loclist(a, d, x->loc, cu->has_pc ? cu->low : 0);
    if (!v.n) { avp_vec_free(&v); return "(no location)"; }
    const SdDie *fn = x;
    while (fn->parent != SD_NONE && fn->tag != SD_TAG_subprogram) fn = &d->dies[fn->parent];
    bool whole = v.n == 1 && (v.v[0].end == SD_NONE ||
                              (fn->tag == SD_TAG_subprogram && v.v[0].begin <= fn->low && v.v[0].end >= fn->high));
    if (whole) {
        const char *s = v.v[0].expr;
        avp_vec_free(&v);
        return s;
    }
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t k = 0; k < v.n; k++) {
        u32 lo = map ? map(ctx, v.v[k].begin) : v.v[k].begin, hi = map ? map(ctx, v.v[k].end) : v.v[k].end;
        avp_buf_printf(&b, "%s%s [%08X-%08X)", k ? ", " : "", v.v[k].expr, lo, hi);
    }
    avp_vec_free(&v);
    return avp_buf_cz(&b);
}

/* ---- line tables ---- */

const char *sd_file_name(const SdLines *l, u32 file) {
    return file >= 1 && file < l->nfiles ? l->files[file] : "?";
}

SdLines sd_lines(AvpArena *a, const SdInfo *d, u32 stmt_list) {
    SdLines l = {0};
    if (stmt_list >= d->sec.line.n) { l.error = "stmt_list outside .debug_line"; return l; }
    Cur c = {d->sec.line.p + stmt_list, d->sec.line.p + d->sec.line.n, false};
    u32 len = rd_u32(&c);
    const u8 *end = c.p + len;
    if (c.bad || len > (u32)(c.end - c.p)) { l.error = "line program runs past .debug_line"; return l; }
    c.end = end;
    u32 ver = rd_u16(&c);
    u32 hlen = rd_u32(&c);
    const u8 *prog = c.p + hlen;
    if (ver < 2 || ver > 3 || prog > end) { l.error = avp_sprintf(a, "line program version %u not read", ver); return l; }
    l.min_inst = (u8)rd_u8(&c);
    u32 default_stmt = rd_u8(&c);
    (void)default_stmt;
    i32 line_base = (i8)rd_u8(&c);
    u32 line_range = rd_u8(&c), opcode_base = rd_u8(&c);
    u8 std_len[256] = {0};
    for (u32 k = 1; k < opcode_base; k++) std_len[k] = (u8)rd_u8(&c);
    AVP_VEC(const char *) dirs = {0}, files = {0};
    avp_vec_push(&dirs, "");
    for (;;) {
        const char *s = rd_str(&c);
        if (!s || !*s) break;
        avp_vec_push(&dirs, s);
    }
    avp_vec_push(&files, "");
    for (;;) {
        const char *s = rd_str(&c);
        if (!s || !*s) break;
        u64 dir = rd_uleb(&c);
        rd_uleb(&c);
        rd_uleb(&c);
        avp_vec_push(&files, dir && dir < dirs.n ? avp_sprintf(a, "%s%s", dirs.v[dir], s) : s);
    }
    if (c.bad || line_range == 0) { l.error = "line program header unreadable"; avp_vec_free(&dirs); avp_vec_free(&files); return l; }
    l.nfiles = files.n;
    l.files = avp_memdup(a, files.v, files.n * sizeof(char *));
    avp_vec_free(&dirs);
    avp_vec_free(&files);
    c.p = prog;
    u32 addr = 0, file = 1;
    i64 line = 1;
    u32 mi = l.min_inst ? l.min_inst : 1;
#define EMIT(endseq) do { SdLineRow r_ = {addr, (u32)line, file, (endseq)}; avp_vec_push(&l.rows, r_); } while (0)
    while (c.p < c.end && !c.bad) {
        u32 op = rd_u8(&c);
        if (op >= opcode_base) {
            u32 adj = op - opcode_base;
            addr += (adj / line_range) * mi;
            line += line_base + (i32)(adj % line_range);
            EMIT(false);
            continue;
        }
        switch (op) {
        case 0: {
            u64 n = rd_uleb(&c);
            if (n == 0 || n > (u64)(c.end - c.p)) { c.bad = true; break; }
            const u8 *next = c.p + n;
            u32 sub = rd_u8(&c);
            if (sub == 1) { EMIT(true); addr = 0; file = 1; line = 1; }
            else if (sub == 2) addr = rd_u32(&c);
            c.p = next; /* define_file and vendor opcodes are skipped */
            break;
        }
        case 1: EMIT(false); break;
        case 2: addr += (u32)rd_uleb(&c) * mi; break;
        case 3: line += rd_sleb(&c); break;
        case 4: file = (u32)rd_uleb(&c); break;
        case 5: rd_uleb(&c); break;
        case 6: case 7: break;
        case 8: addr += ((255 - opcode_base) / line_range) * mi; break;
        case 9: addr += rd_u16(&c); break;
        default:
            for (u32 k = 0; k < std_len[op]; k++) rd_uleb(&c);
            break;
        }
    }
#undef EMIT
    l.ok = !c.bad;
    if (c.bad) l.error = "line program truncated";
    return l;
}

/* ---- a function's parameters and locals ---- */

size_t sd_param_count(const SdInfo *d, const SdDie *fn) {
    size_t n = 0;
    for (u32 k = sd_index(d, fn) + 1; k < fn->next; k++)
        n += d->dies[k].parent == sd_index(d, fn) && d->dies[k].tag == SD_TAG_formal_parameter;
    return n;
}

AvpStrVec sd_local_names(AvpArena *a, const SdInfo *d, const SdDie *fn) {
    AvpStrVec v = {0};
    for (u32 k = sd_index(d, fn) + 1; k < fn->next; k++)
        if (d->dies[k].tag == SD_TAG_variable && d->dies[k].name) avp_vec_push(&v, avp_str(avp_strdup(a, d->dies[k].name)));
    return v;
}

AvpStrVec sd_names_missing(const AvpStrVec *have, const AvpStrVec *want) {
    AvpStrVec out = {0};
    bool *used = avp_alloc(avp_arena(), (have->n + 1) * sizeof *used);
    for (size_t w = 0; w < want->n; w++) {
        bool found = false;
        for (size_t h = 0; h < have->n && !found; h++)
            if (!used[h] && avp_str_eq(have->v[h], want->v[w])) used[h] = found = true;
        if (!found) avp_vec_push(&out, want->v[w]);
    }
    return out;
}
