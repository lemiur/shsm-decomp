/* analysis_test.c: classes_analysis part 1 (tools/classes.py lines 926-1427) on synthetic listings.
 *
 * With CLASSES_ANALYSIS_DUMP=<file> in the environment the test `dump_real_tree` runs part 1 on the real tree
 * (build/R5WEA4/asm, config/R5WEA4/symbols.txt, config/R5WEA4/splits.txt; run from the repository root) and writes
 * the canonical dump the proof script compares with the Python's; without it the test does nothing. */
#include "avp/test.h"
#include "classes_analysis.h"

#include <stdlib.h>

static AvpStr S(const char *z) { return avp_str(z); }

/* ---- the canonical dump (shared by the synthetic tests and the real-tree proof) ------------------------------- */

static void put_s(AvpBuf *b, AvpStr s) {
    if (CLASSES_NONE(s)) avp_buf_putz(b, "None");
    else avp_buf_put(b, s);
}
static void put_on(AvpBuf *b, bool has, i64 v) {
    if (has) avp_buf_printf(b, "%lld", (long long)v);
    else avp_buf_putz(b, "None");
}

/* the end of a `" TAG " + " ".join(items)` line */
static const char *k_nl(size_t n) { return n ? "\n" : " \n"; }

static int cmp_mo(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const ClassesMemberObj *p = x, *q = y;
    int c = avp_cmp_i64(p->off, q->off);
    return c ? c : avp_cmp_str(p->vt, q->vt);
}
static int cmp_mn(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const ClassesMemberNamed *p = x, *q = y;
    int c = avp_cmp_i64(p->off, q->off);
    if (c) return c;
    c = avp_cmp_str(p->cls, q->cls);
    return c ? c : avp_cmp_str(p->ev, q->ev);
}
typedef struct {
    AvpStr f, k, o;
} CtorOf;
static int cmp_co(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const CtorOf *p = x, *q = y;
    int c = avp_cmp_str(p->f, q->f);
    return c ? c : avp_cmp_str(p->k, q->k);
}

static AvpStr dump_analysis(const ClassesAnalysis *an) {
    AvpBuf b = {0};
    for (size_t i = 0; i < an->nvt; i++) {
        const ClassesVtable *r = an->vts[i];
        avp_buf_printf(&b, "VT %.*s ", AVP_SF(r->name));
        for (size_t k = 0; k < r->words->n; k++) {
            if (k) avp_buf_putc(&b, ' ');
            put_s(&b, r->words->v[k]);
        }
        avp_buf_putc(&b, '\n');
        if (r->rtti) {
            avp_buf_putz(&b, " VTR ");
            for (size_t k = 0; k < r->rtti->n; k++)
                avp_buf_printf(&b, "%s%lld=%.*s:%lld", k ? " " : "", (long long)r->rtti->v[k].k,
                               AVP_SF(r->rtti->v[k].sym), (long long)r->rtti->v[k].off);
            avp_buf_putc(&b, '\n');
        }
    }
    for (size_t i = 0; i < an->embedded.n; i++)
        avp_buf_printf(&b, "EMB %.*s %lld %.*s\n", AVP_SF(an->embedded.v[i].sym), (long long)an->embedded.v[i].base,
                       AVP_SF(an->embedded.v[i].name));
    AvpMapIter it = {0};
    while (avp_map_next(an->embedded_by_sym, &it)) {
        const ClassesEmbeddedAtList *l = it.val->p;
        avp_buf_printf(&b, "EBS %.*s ", AVP_SF(it.key));
        for (size_t k = 0; k < l->n; k++)
            avp_buf_printf(&b, "%s%lld:%.*s", k ? " " : "", (long long)l->v[k].base, AVP_SF(l->v[k].name));
        avp_buf_putc(&b, '\n');
    }
    for (size_t i = 0; i < an->sites.n; i++) {
        const ClassesSite *s = &an->sites.v[i];
        avp_buf_printf(&b, "SITE %.*s %.*s %s ", AVP_SF(s->func), AVP_SF(s->token), classes_kind_names[s->kind]);
        put_s(&b, s->owner);
        avp_buf_putz(&b, s->owner.p ? " True\n" : " False\n");
        for (size_t k = 0; k < s->stores.n; k++)
            avp_buf_printf(&b, " ST %lld %lld %.*s %lld\n", (long long)s->stores.v[k].idx, (long long)s->stores.v[k].off,
                           AVP_SF(s->stores.v[k].vt), (long long)s->stores.v[k].add);
        avp_buf_putz(&b, " FIN");
        for (size_t k = 0; k < s->final.n; k++)
            avp_buf_printf(&b, "%s%lld:%zu", k ? " " : " ", (long long)s->final.v[k].off, s->final.v[k].st);
        avp_buf_putz(&b, k_nl(s->final.n));
        avp_buf_putz(&b, " OV");
        for (size_t k = 0; k < s->over.n; k++) avp_buf_printf(&b, " %zu", s->over.v[k]);
        avp_buf_putz(&b, k_nl(s->over.n));
        avp_buf_putz(&b, " VTS");
        for (size_t k = 0; k < s->vts.n; k++) avp_buf_printf(&b, " %.*s", AVP_SF(s->vts.v[k]));
        avp_buf_putz(&b, k_nl(s->vts.n));
        avp_buf_putz(&b, " MEM");
        for (size_t k = 0; k < s->members.n; k++)
            avp_buf_printf(&b, " %.*s:%lld", AVP_SF(s->members.v[k].vt), (long long)s->members.v[k].lo);
        avp_buf_putz(&b, k_nl(s->members.n));
    }
    for (size_t i = 0; i < an->nvt; i++) {
        ClassesVtable *r = an->vts[i];
        avp_buf_printf(&b, "G %.*s %.*s |", AVP_SF(r->name), AVP_SF(r->group_ev));
        for (size_t k = 0; k < r->group.n; k++) {
            avp_buf_printf(&b, " %lld=", (long long)r->group.v[k].a);
            put_on(&b, r->group.v[k].has_o, r->group.v[k].o);
        }
        avp_buf_putc(&b, '\n');
        if (r->conflicts.n) {
            avp_buf_putz(&b, " CONF ");
            for (size_t k = 0; k < r->conflicts.n; k++) avp_buf_printf(&b, "%s%.*s", k ? "; " : "", AVP_SF(r->conflicts.v[k]));
            avp_buf_putc(&b, '\n');
        }
        if (r->has_inst) {
            avp_buf_putz(&b, " INST");
            for (int k = 0; k < 5; k++) {
                avp_buf_printf(&b, " %s:", classes_kind_names[k]);
                for (size_t j = 0; j < r->inst.k[k].n; j++)
                    avp_buf_printf(&b, "%s%.*s", j ? "," : "", AVP_SF(r->inst.k[k].v[j]));
            }
            avp_buf_putc(&b, '\n');
        }
        if (r->member_objects.n) {
            ClassesMemberObj *mo = avp_memdup(avp_scratch(), r->member_objects.v, r->member_objects.n * sizeof *mo);
            avp_sort(mo, r->member_objects.n, sizeof *mo, cmp_mo, NULL, false);
            avp_buf_putz(&b, " MO");
            for (size_t k = 0; k < r->member_objects.n; k++)
                avp_buf_printf(&b, " %lld:%.*s", (long long)mo[k].off, AVP_SF(mo[k].vt));
            avp_buf_putc(&b, '\n');
        }
        for (size_t k = 0; k < r->cand.n; k++) {
            const ClassesCand *c = &r->cand.v[k];
            avp_buf_printf(&b, " CAND %s%.*s %s%lld %.*s\n", c->b_is_name ? "name:" : "", AVP_SF(c->b),
                           c->off_is_store ? "store-at:" : "", (long long)c->off, AVP_SF(c->ev));
        }
        if (r->member_named.n) {
            ClassesMemberNamed *mn = avp_memdup(avp_scratch(), r->member_named.v, r->member_named.n * sizeof *mn);
            avp_sort(mn, r->member_named.n, sizeof *mn, cmp_mn, NULL, false);
            avp_buf_putz(&b, " MN ");
            for (size_t k = 0; k < r->member_named.n; k++)
                avp_buf_printf(&b, "%s%lld:%.*s:%.*s", k ? " | " : "", (long long)mn[k].off, AVP_SF(mn[k].cls),
                               AVP_SF(mn[k].ev));
            avp_buf_putc(&b, '\n');
        }
        if (r->dtor_funcs) {
            AvpStrVec d = avp_set_sorted(r->dtor_funcs);
            avp_buf_putz(&b, " DTOR");
            for (size_t k = 0; k < d.n; k++) avp_buf_printf(&b, " %.*s", AVP_SF(d.v[k]));
            avp_buf_putc(&b, '\n');
            avp_vec_free(&d);
        }
        for (size_t k = 0; k < r->bases.n; k++) {
            const ClassesBase *x = r->bases.v[k];
            avp_buf_printf(&b, " BASE %s%.*s ", x->base_is_name ? "name:" : "", AVP_SF(x->base));
            put_on(&b, x->has_offset, x->offset);
            avp_buf_putc(&b, ' ');
            put_on(&b, x->has_word, x->word);
            avp_buf_putc(&b, ' ');
            for (size_t j = 0; j < x->evidence.n; j++)
                avp_buf_printf(&b, "%s%.*s", j ? " | " : "", AVP_SF(x->evidence.v[j]));
            avp_buf_putc(&b, '\n');
        }
        avp_buf_printf(&b, " EXCL %lld\n", (long long)r->exclusive);
        for (size_t k = 0; k < r->slots.n; k++) {
            const ClassesSlot *e = &r->slots.v[k];
            avp_buf_printf(&b, " SL index=%lld offset=%lld target=", (long long)e->index, (long long)e->index * 4);
            put_s(&b, e->target);
            if (avp_str_eqz(e->status, "header")) {
                avp_buf_putz(&b, " status=header");
                if (e->has_rtti)
                    avp_buf_printf(&b, " rtti=%.*s rtti_offset=%lld", AVP_SF(e->rtti), (long long)e->rtti_offset);
                avp_buf_putc(&b, '\n');
                continue;
            }
            if (e->has_thunk)
                avp_buf_printf(&b, " thunk=%lld:%.*s", (long long)e->thunk_adjust, AVP_SF(e->thunk_target));
            avp_buf_printf(&b, " status=%.*s", AVP_SF(e->status));
            if (e->has_base) {
                avp_buf_printf(&b, " base=%.*s base_target=", AVP_SF(e->base));
                put_s(&b, e->base_target);
                avp_buf_printf(&b, " base_index=%lld", (long long)e->base_index);
            }
            avp_buf_printf(&b, " group=%.*s\n", AVP_SF(e->group));
        }
    }
    avp_buf_putz(&b, "DTORORDER");
    for (size_t k = 0; k < an->dtor_order.n; k++) avp_buf_printf(&b, " %.*s", AVP_SF(an->dtor_order.v[k]));
    avp_buf_putc(&b, '\n');
    AVP_VEC(CtorOf) co = {0};
    AvpMap *maps[2] = {an->ctor_of_ctor, an->ctor_of_dtor};
    for (int m = 0; m < 2; m++) {
        memset(&it, 0, sizeof it);
        while (avp_map_next(maps[m], &it)) {
            CtorOf x = {it.key, m ? S("dtor") : S("ctor"), *(AvpStr *)it.val->p};
            avp_vec_push(&co, x);
        }
    }
    avp_sort(co.v, co.n, sizeof(CtorOf), cmp_co, NULL, false);
    for (size_t k = 0; k < co.n; k++)
        avp_buf_printf(&b, "CTOROF %.*s %.*s %.*s\n", AVP_SF(co.v[k].f), AVP_SF(co.v[k].k), AVP_SF(co.v[k].o));
    avp_vec_free(&co);
    return avp_buf_str(&b);
}

/* ---- synthetic listings ---------------------------------------------------------------------------------------- */

#define ROW(addr, text) "/* " addr " 00000000  00 00 00 00 */\t" text "\n"

/* CBase (ctor + dtor), CDer : CBase (ctor calling CBase's ctor and a member's, dtor calling CBase's dtor and a
 * member's), COver (no ctor; two stores of its table at one addend: a mixed site; its base found by slot overlap),
 * CInl (an inlined CBase constructor: CBase's vptr stored, then overwritten by CInl's), CNew (stored through
 * lbl+addend into a __nw__ result: vt_at and a factory site). */
static const char *ASM_AN =
    ".section .text\n"
    ".fn __ct__5CBaseFv, global\n"
    ROW("80001000", "lis r4, __vt__5CBase@ha")
    ROW("80001004", "addi r4, r4, __vt__5CBase@l")
    ROW("80001008", "stw r4, 0x0(r3)")
    ROW("8000100C", "blr")
    ".endfn __ct__5CBaseFv\n"
    ".fn __dt__5CBaseFv, global\n"
    ROW("80001010", "cmpwi r3, 0x0")
    ROW("80001014", "lis r4, __vt__5CBase@ha")
    ROW("80001018", "addi r4, r4, __vt__5CBase@l")
    ROW("8000101C", "stw r4, 0x0(r3)")
    ROW("80001020", "blr")
    ".endfn __dt__5CBaseFv\n"
    ".fn fn_a, global\n" ROW("80001030", "blr") ".endfn fn_a\n"
    ".fn fn_b, global\n" ROW("80001034", "blr") ".endfn fn_b\n"
    ".fn fn_x, global\n" ROW("80001038", "blr") ".endfn fn_x\n"
    ".fn __ct__4CDerFv, global\n"
    ROW("80001100", "mr r31, r3")
    ROW("80001104", "bl __ct__5CBaseFv")
    ROW("80001108", "addi r3, r31, 0x8")
    ROW("8000110C", "bl __ct__4CMemFv")
    ROW("80001110", "lis r4, __vt__4CDer@ha")
    ROW("80001114", "addi r4, r4, __vt__4CDer@l")
    ROW("80001118", "stw r4, 0x0(r31)")
    ROW("8000111C", "addi r3, r31, 0xC")
    ROW("80001120", "bl __ct__5CMem2Fv")
    ROW("80001124", "mr r3, r31")
    ROW("80001128", "blr")
    ".endfn __ct__4CDerFv\n"
    ".fn __dt__4CDerFv, global\n"
    ROW("80001200", "mr r31, r3")
    ROW("80001204", "lis r4, __vt__4CDer@ha")
    ROW("80001208", "addi r4, r4, __vt__4CDer@l")
    ROW("8000120C", "stw r4, 0x0(r31)")
    ROW("80001210", "addi r3, r31, 0x10")
    ROW("80001214", "bl __dt__4CMemFv")
    ROW("80001218", "mr r3, r31")
    ROW("8000121C", "bl __dt__5CBaseFv")
    ROW("80001220", "blr")
    ".endfn __dt__4CDerFv\n"
    ".fn fn_mixed, global\n"
    ROW("80001300", "lis r4, __vt__5COver@ha")
    ROW("80001304", "addi r4, r4, __vt__5COver@l")
    ROW("80001308", "stw r4, 0x10(r3)")
    ROW("8000130C", "stw r4, 0x14(r3)")
    ROW("80001310", "blr")
    ".endfn fn_mixed\n"
    ".fn __ct__4CInlFv, global\n"
    ROW("80001400", "lis r4, __vt__5CBase@ha")
    ROW("80001404", "addi r4, r4, __vt__5CBase@l")
    ROW("80001408", "stw r4, 0x0(r3)")
    ROW("8000140C", "lis r4, __vt__4CInl@ha")
    ROW("80001410", "addi r4, r4, __vt__4CInl@l")
    ROW("80001414", "stw r4, 0x0(r3)")
    ROW("80001418", "blr")
    ".endfn __ct__4CInlFv\n"
    ".fn fn_new, global\n"
    ROW("80001500", "li r3, 0x20")
    ROW("80001504", "bl __nw__FUl")
    ROW("80001508", "lis r4, lbl_80400100@ha")
    ROW("8000150C", "addi r4, r4, lbl_80400100@l")
    ROW("80001510", "addi r4, r4, 0x8")
    ROW("80001514", "stw r4, 0x0(r3)")
    ROW("80001518", "blr")
    ".endfn fn_new\n"
    ".section .data\n"
    "# .data:0x0 | 0x80400000 | size: 0x14\n"
    ".obj __vt__5CBase, global\n"
    "\t.4byte 0\n\t.4byte 0\n\t.4byte __dt__5CBaseFv\n\t.4byte fn_a\n\t.4byte 0\n"
    ".endobj __vt__5CBase\n"
    "# .data:0x14 | 0x80400014 | size: 0x14\n"
    ".obj __vt__4CDer, global\n"
    "\t.4byte 0\n\t.4byte 0\n\t.4byte __dt__4CDerFv\n\t.4byte fn_a\n\t.4byte fn_b\n"
    ".endobj __vt__4CDer\n"
    "# .data:0x28 | 0x80400028 | size: 0x10\n"
    ".obj __vt__5COver, global\n"
    "\t.4byte 0\n\t.4byte 0\n\t.4byte fn_x\n\t.4byte fn_a\n"
    ".endobj __vt__5COver\n"
    "# .data:0x38 | 0x80400038 | size: 0x10\n"
    ".obj __vt__4CInl, global\n"
    "\t.4byte 0\n\t.4byte 0\n\t.4byte fn_b\n\t.4byte fn_a\n"
    ".endobj __vt__4CInl\n"
    "# .data:0x100 | 0x80400100 | size: 0x8\n"
    ".obj lbl_80400100, global\n"
    "\t.4byte 0x12345678\n\t.4byte 0x9ABCDEF0\n"
    ".endobj lbl_80400100\n"
    "# .data:0x108 | 0x80400108 | size: 0x10\n"
    ".obj __vt__4CNew, global\n"
    "\t.4byte 0\n\t.4byte 0\n\t.4byte fn_b\n\t.4byte fn_x\n"
    ".endobj __vt__4CNew\n";

static ClassesAnalysis *an_synth(AvpArena *a) {
    AvpStr unit = S("game/An"), text = S(ASM_AN);
    ClassesProgram *p = classes_program_from_texts(a, &unit, &text, 1, S(""));
    return classes_analysis_part1(a, p, NULL);
}

static const ClassesSite *site_of(const ClassesAnalysis *an, const char *func) {
    for (size_t i = 0; i < an->sites.n; i++)
        if (avp_str_eqz(an->sites.v[i].func, func)) return &an->sites.v[i];
    return NULL;
}

AVP_TEST(sites) {
    ClassesAnalysis *an = an_synth(avp_arena());
    REQUIRE(an->nvt == 5);
    REQUIRE(an->sites.n == 7);
    const ClassesSite *s = site_of(an, "__ct__4CDerFv");
    REQUIRE(s != NULL);
    CHECK_STR(s->owner, "__vt__4CDer");
    CHECK(s->kind == CLASSES_K_CTOR);
    CHECK(site_of(an, "__dt__4CDerFv")->kind == CLASSES_K_DTOR);
    s = site_of(an, "fn_mixed"); /* one table stored twice with one addend: no owner */
    CHECK(CLASSES_NONE(s->owner));
    REQUIRE(s->members.n == 1);
    CHECK_EQ_I64(s->members.v[0].lo, 0x10);
    REQUIRE(s->final.n == 2);
    s = site_of(an, "__ct__4CInlFv"); /* the CBase vptr overwritten by the CInl one */
    CHECK_STR(s->owner, "__vt__4CInl");
    REQUIRE(s->over.n == 1);
    CHECK_EQ_U64(s->over.v[0], 0);
    REQUIRE(s->final.n == 1);
    CHECK_EQ_U64(s->final.v[0].st, 1);
    s = site_of(an, "fn_new"); /* lbl+8 is __vt__4CNew (vt_at), stored into a __nw__ result */
    CHECK(s->kind == CLASSES_K_FACTORY);
    CHECK_STR(s->token, "ret@80001504");
    CHECK_STR(s->owner, "__vt__4CNew");
    CHECK_EQ_I64(s->stores.v[0].add, 0);
    CHECK_STR(classes_an_vt_of_ctor(an, S("__ct__5CBaseFv"), CLASSES_KIND_BIT(CLASSES_K_CTOR)), "__vt__5CBase");
    CHECK(CLASSES_NONE(classes_an_vt_of_ctor(an, S("__dt__5CBaseFv"), CLASSES_KIND_BIT(CLASSES_K_CTOR))));
    CHECK_STR(classes_an_vt_of_ctor(an, S("__dt__5CBaseFv"), CLASSES_KIND_BIT(CLASSES_K_DTOR)), "__vt__5CBase");
    CHECK(CLASSES_NONE(classes_an_vt_of_ctor(an, S("fn_mixed"), 0x1F)));
    i64 addr;
    REQUIRE(classes_an_vt_addr(an, S("__vt__4CDer+0x8"), &addr));
    CHECK_EQ_I64(addr, 0x8040001C);
    CHECK(!classes_an_vt_addr(an, S("__vt__4CDer+0x8g"), &addr));
    CHECK(!classes_an_vt_addr(an, S("nothing"), &addr));
    i64 off;
    ClassesVtable *r = classes_an_vt_at(an, 0x80400000, 0x18, &off);
    REQUIRE(r != NULL);
    CHECK_STR(r->name, "__vt__4CDer");
    CHECK_EQ_I64(off, 4);
    CHECK(classes_an_vt_at(an, 0x80400000, -4, &off) == NULL);
    CHECK(classes_an_vt_at(an, 0x80400038, 0x10, &off) == NULL); /* past the 4 words of CInl */
}

AVP_TEST(groups) {
    ClassesAnalysis *an = an_synth(avp_arena());
    ClassesVtable *over = classes_an_vt(an, S("__vt__5COver"));
    REQUIRE(over != NULL);
    CHECK_STR(over->group_ev, "stores (mixed site, offsets normalised)");
    REQUIRE(over->group.n == 1);
    CHECK(over->group.v[0].has_o && over->group.v[0].o == 0);
    CHECK(over->has_inst && over->inst.k[CLASSES_K_OTHER].n == 1);
    ClassesVtable *der = classes_an_vt(an, S("__vt__4CDer"));
    CHECK_STR(der->group_ev, "stores");
    CHECK_EQ_U64(der->inst.k[CLASSES_K_CTOR].n, 1);
    CHECK_EQ_U64(der->inst.k[CLASSES_K_DTOR].n, 1);
    ClassesGroup g = {0};
    ClassesGroupEnt e = {8, true, 4};
    avp_vec_push(&g, e);
    CHECK(classes_group_get(&g, 8) != NULL);
    CHECK(classes_group_get(&g, 0) == NULL);
    i64 co;
    CHECK(classes_an_complete_off(an, der, &co) && co == 0);
    CHECK_EQ_U64(classes_an_fit_len(classes_an_vt(an, S("__vt__5CBase"))), 4); /* trailing null dropped */
    CHECK_EQ_U64(classes_an_fit_len(der), 5);
    CHECK_EQ_I64(classes_floordiv4(-1), -1);
    CHECK_EQ_I64(classes_mod4(-1), 3);
}

AVP_TEST(bases_and_images) {
    ClassesAnalysis *an = an_synth(avp_arena());
    ClassesVtable *der = classes_an_vt(an, S("__vt__4CDer"));
    /* ctor-call CBase (base), ctor-call CMem before the vptr store (a named candidate), dtor-call CBase */
    REQUIRE(der->cand.n == 3);
    CHECK_STR(der->cand.v[0].b, "__vt__5CBase");
    CHECK(der->cand.v[1].b_is_name);
    CHECK_STR(der->cand.v[1].b, "CMem");
    CHECK_STR(der->cand.v[2].ev, "dtor-call __dt__5CBaseFv");
    REQUIRE(der->member_named.n == 2); /* CMem2 constructed after the store, CMem destroyed at this+0x10 */
    CHECK_EQ_I64(der->member_named.v[0].off, 0xC);
    CHECK_STR(der->member_named.v[1].ev, "dtor-call __dt__4CMemFv");
    REQUIRE(der->dtor_funcs != NULL);
    CHECK(avp_set_has(der->dtor_funcs, S("__dt__4CDerFv")));
    REQUIRE(an->dtor_order.n == 2);
    CHECK_STR(an->dtor_order.v[0], "__vt__5CBase");
    REQUIRE(der->bases.n == 2);
    CHECK_STR(der->bases.v[0]->base, "__vt__5CBase");
    CHECK_EQ_I64(der->bases.v[0]->word, 0);
    CHECK_EQ_U64(der->bases.v[0]->evidence.n, 2);
    CHECK(der->bases.v[1]->base_is_name && !der->bases.v[1]->has_word);
    CHECK(classes_an_is_descendant(an, S("__vt__4CDer"), S("__vt__5CBase"), 0));
    CHECK(!classes_an_is_descendant(an, S("__vt__5CBase"), S("__vt__4CDer"), 0));
    CHECK(classes_an_is_descendant(an, S("__vt__5CBase"), S("x"), 33));
    ClassesVtable *inl = classes_an_vt(an, S("__vt__4CInl"));
    REQUIRE(inl->cand.n == 1);
    CHECK(inl->cand.v[0].off_is_store);
    REQUIRE(inl->bases.n == 1);
    CHECK_STR(inl->bases.v[0]->evidence.v[0], "inlined-ctor store in __ct__4CInlFv");
    ClassesVtable *over = classes_an_vt(an, S("__vt__5COver"));
    REQUIRE(over->bases.n == 1);
    CHECK_STR(over->bases.v[0]->evidence.v[0], "slot-overlap (1 slots)");
    CHECK_EQ_I64(over->exclusive, 0);
    CHECK_EQ_I64(der->exclusive, 1);
    /* CNew overlaps CInl (fn_b) and CDer (fn_b at another index): CInl, the only fit */
    ClassesVtable *nw = classes_an_vt(an, S("__vt__4CNew"));
    REQUIRE(nw->bases.n == 1);
    CHECK_STR(nw->bases.v[0]->base, "__vt__4CInl");
    CHECK(classes_an_looks_dtor(an, S("__dt__5CBaseFv")));
    CHECK(!classes_an_looks_dtor(an, S("fn_a")));
}

AVP_TEST(slots) {
    ClassesAnalysis *an = an_synth(avp_arena());
    ClassesVtable *der = classes_an_vt(an, S("__vt__4CDer"));
    REQUIRE(der->slots.n == 5);
    CHECK_STR(der->slots.v[0].status, "header");
    CHECK_STR(der->slots.v[2].status, "override");
    CHECK_STR(der->slots.v[2].base_target, "__dt__5CBaseFv");
    CHECK_STR(der->slots.v[3].status, "inherited");
    CHECK_STR(der->slots.v[4].status, "override"); /* over the trailing null of CBase */
    CHECK(der->slots.v[4].has_base && CLASSES_NONE(der->slots.v[4].base_target));
    CHECK_EQ_I64(der->slots.v[4].base_index, 4);
    CHECK_STR(der->slots.v[4].group, "complete");
    ClassesVtable *base = classes_an_vt(an, S("__vt__5CBase"));
    CHECK_STR(base->slots.v[4].status, "null");
    CHECK_STR(base->slots.v[2].status, "new");
    CHECK(!base->slots.v[2].has_base);
    ClassesSpans sp = classes_an_spans(an, base, 0);
    CHECK_EQ_U64(sp.n, 0);
}

/* an embedded group: a table with a second group (addend 0x10 stored at this+0x8) */
static const char *ASM_EMB2 =
    ".section .text\n"
    ".fn __ct__2CMFv, global\n"
    ROW("80002000", "lis r4, __vt__2CM@ha")
    ROW("80002004", "addi r4, r4, __vt__2CM@l")
    ROW("80002008", "stw r4, 0x0(r3)")
    ROW("8000200C", "addi r5, r4, 0x10")
    ROW("80002010", "stw r5, 0x8(r3)")
    ROW("80002014", "blr")
    ".endfn __ct__2CMFv\n"
    ".fn m1, global\n" ROW("80002020", "blr") ".endfn m1\n"
    ".fn \"@8@m2\", global\n" ROW("80002024", "subi r3, r3, 0x8") ROW("80002028", "b m2") ".endfn \"@8@m2\"\n"
    ".fn m2, global\n" ROW("8000202C", "blr") ".endfn m2\n"
    ".section .data\n"
    "# .data:0x0 | 0x80410000 | size: 0x1C\n"
    ".obj __vt__2CM, global\n"
    "\t.4byte 0\n\t.4byte 0\n\t.4byte m1\n\t.4byte m2\n\t.4byte 0\n\t.4byte 0\n\t.4byte \"@8@m2\"\n"
    ".endobj __vt__2CM\n";

static ClassesAnalysis *an_emb(AvpArena *a) {
    AvpStr unit = S("game/M"), text = S(ASM_EMB2);
    ClassesProgram *p = classes_program_from_texts(a, &unit, &text, 1, S(""));
    return classes_analysis_part1(a, p, NULL);
}

/* an embedded vtable (inside an object that starts with a non-zero word), with RTTI headers */
static const char *ASM_EMB3 = ".section .data\n"
                              "# .data:0x0 | 0x80500000 | size: 0x20\n"
                              ".obj lbl_80500000, local\n"
                              "\t.4byte 0x12345678\n"
                              "\t.4byte 0x0\n"
                              "\t.4byte __RTTI__4CBaz\n"
                              "\t.4byte 0\n"
                              "\t.4byte m1\n"
                              "\t.4byte 0\n"
                              "\t.4byte m3\n"
                              "\t.4byte __RTTI__4CBaz\n"
                              ".endobj lbl_80500000\n"
                              ".section .text\n"
                              ".fn ctor3, global\n"
                              ROW("80003000", "lis r4, lbl_80500000@ha")
                              ROW("80003004", "addi r4, r4, lbl_80500000@l")
                              ROW("80003008", "addi r5, r4, 0x8")
                              ROW("8000300C", "stw r5, 0x0(r3)")
                              ROW("80003010", "addi r5, r4, 0x10")
                              ROW("80003014", "stw r5, 0x4(r3)")
                              ROW("80003018", "blr")
                              ".endfn ctor3\n"
                              ".fn m3, global\n" ROW("80003020", "blr") ".endfn m3\n";

static ClassesAnalysis *an_all(AvpArena *a) {
    AvpStr units[3] = {S("an"), S("emb"), S("emb3")}, texts[3] = {S(ASM_AN), S(ASM_EMB2), S(ASM_EMB3)};
    ClassesProgram *p = classes_program_from_texts(a, units, texts, 3, S(""));
    return classes_analysis_part1(a, p, NULL);
}

AVP_TEST(write_asm) {
    const char *dir = getenv("CLASSES_ANALYSIS_ASM_DIR");
    if (!dir || !*dir) return;
    const char *texts[3] = {ASM_AN, ASM_EMB2, ASM_EMB3}, *names[3] = {"an.s", "emb.s", "emb3.s"};
    for (int i = 0; i < 3; i++) {
        FILE *fp = fopen(avp_sprintf(avp_arena(), "%s/%s", dir, names[i]), "wb");
        REQUIRE(fp != NULL);
        fputs(texts[i], fp);
        fclose(fp);
    }
    FILE *fp = fopen(avp_sprintf(avp_arena(), "%s.c.txt", dir), "wb");
    REQUIRE(fp != NULL);
    AvpStr d = dump_analysis(an_all(avp_arena()));
    fwrite(d.p, 1, d.n, fp);
    fclose(fp);
}

AVP_TEST(embedded_groups_and_tables) {
    ClassesAnalysis *an = an_all(avp_arena());
    ClassesVtable *m = classes_an_vt(an, S("__vt__2CM"));
    REQUIRE(m != NULL);
    REQUIRE(m->group.n == 2);
    CHECK_EQ_I64(m->group.v[1].a, 0x10);
    CHECK_EQ_I64(m->group.v[1].o, 8);
    ClassesSpans sp = classes_an_spans(an, m, 0);
    REQUIRE(sp.n == 1);
    CHECK_EQ_I64(sp.v[0].start, 4);
    CHECK_EQ_I64(sp.v[0].end, 7); /* the thunk with adjust 8 = this+0x8 continues the span */
    CHECK(sp.v[0].uncertain && CLASSES_NONE(sp.v[0].base));
    CHECK_STR(m->slots.v[4].status, "header");
    CHECK_STR(m->slots.v[6].status, "unresolved");
    CHECK(m->slots.v[6].has_thunk);
    CHECK_EQ_I64(m->slots.v[6].thunk_adjust, 8);
    CHECK_STR(m->slots.v[6].thunk_target, "m2");
    CHECK_STR(m->slots.v[6].group, "embedded");
    CHECK_EQ_I64(m->exclusive, 2);
    /* the embedded table of lbl_80500000 */
    REQUIRE(an->embedded.n == 1);
    CHECK_STR(an->embedded.v[0].name, "lbl_80500000+0x8");
    ClassesVtable *e = classes_an_vt(an, S("lbl_80500000+0x8"));
    REQUIRE(e != NULL && e->rtti != NULL);
    CHECK_EQ_U64(e->index, an->nvt - 1);
    CHECK_STR(e->slots.v[0].rtti, "__RTTI__4CBaz");
    CHECK_STR(e->slots.v[2].status, "header"); /* the group at addend 8 */
    AvpVal *v = avp_map_get(an->embedded_by_sym, S("lbl_80500000"));
    REQUIRE(v != NULL);
    CHECK_EQ_I64(((ClassesEmbeddedAtList *)v->p)->v[0].base, 8);
    i64 addr;
    REQUIRE(classes_an_vt_addr(an, S("lbl_80500000+0x8"), &addr));
    CHECK_EQ_I64(addr, 0x80500008);
    const ClassesSite *s = site_of(an, "ctor3"); /* lbl+8 and lbl+0x10 resolve into the embedded table */
    REQUIRE(s != NULL);
    CHECK_STR(s->owner, "lbl_80500000+0x8");
    CHECK_EQ_I64(s->stores.v[1].add, 8);
}

AVP_TEST(dump_synthetic) {
    AvpStr d = dump_analysis(an_synth(avp_arena()));
    CHECK(avp_str_contains(d, S("SITE __ct__4CDerFv a3 ctor __vt__4CDer True\n")));
    CHECK(avp_str_contains(d, S("\nCTOROF __dt__4CDerFv dtor __vt__4CDer\n")));
    CHECK(avp_str_contains(d, S("\nSITE fn_mixed a3 ctor None False\n ST 2 16 __vt__5COver 0\n ST 3 20 __vt__5COver 0\n"
                                " FIN 16:0 20:1\n OV \n")));
}

/* ---- the real-tree dump (proof against the Python) ------------------------------------------------------------ */

static void dump(const char *out) {
    AvpArena *a = avp_arena();
    ClassesProgram *p = classes_program_from_dir(a, "build/R5WEA4/asm", "config/R5WEA4/symbols.txt");
    AvpStr splits;
    REQUIRE(avp_read_text(a, "config/R5WEA4/splits.txt", AVP_TM_UTF8, &splits));
    ClassesRegions regions = classes_parse_data_regions(a, splits);
    double t0 = avp_perf_counter();
    ClassesAnalysis *an = classes_analysis_part1(a, p, &regions);
    double t1 = avp_perf_counter();
    AvpStr d = dump_analysis(an);
    FILE *fp = fopen(out, "wb");
    REQUIRE(fp != NULL);
    fwrite(d.p, 1, d.n, fp);
    fclose(fp);
    fprintf(stderr, "analysis part 1 %.2fs\n", t1 - t0);
}

AVP_TEST(dump_real_tree) {
    const char *out = getenv("CLASSES_ANALYSIS_DUMP");
    if (out && *out) dump(out);
}

AVP_TEST_MAIN("classes_analysis")
