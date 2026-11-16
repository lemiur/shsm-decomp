/* classes_baserule.c: base rule 1 (`scan --base-rule ctor`), base subobjects by constructor evidence.
 *
 * The reference (tools/classes.py, base rule 0, the default) keys a base by vtable images: a constructor call on
 * this+off becomes a candidate (the callee's owner vtable at off), kept only when that table's image lands on one of
 * the derived vtable's groups (_image_at, through the base's complete_off); any group no candidate covers is then
 * given the vtable with the best slot overlap. For a base subobject at a non-zero offset whose class keeps its vptr
 * at a non-zero offset of its own (CGhostPhotoTrigger: its constructor calls fn_80039E40 on this+0x34, and
 * fn_80039E40 installs lbl_803E2548 at +0x5C and a member's table lbl_803E2474 at +0x4), the callee's owner is the
 * member's table (lowest offset), its image does not land, and slot overlap names an unrelated class.
 *
 * The rule here reads what the compiler does to a subobject instead of matching tables:
 *
 *   base     The derived constructor calls the subobject's constructor F with r3 = this+off and afterwards stores
 *            its own vtable into the subobject's vptr: for a vptr that F installs at +p (F's last store of a table
 *            at a3+p), the derived constructor's last store at this+off+p is its own table, after the call.
 *            The derived destructor calls the subobject's destructor with r4 = 0 (destroy, never delete).
 *            Thunks in the derived table's group for the subobject adjust `this` by -off (corroboration only).
 *   member   A member object's constructor is called the same way, but its vptr is never overwritten by the
 *            derived constructor, and its destructor is called with r4 = -1.
 *
 * Evidence tiers, strongest first (CLASSES_BR_*):
 *   VPTR  a constructor call whose callee F installs at least one vtable through a3. Overwritten: a base; the base
 *         vtable is the table F installs at the lowest overwritten +p, and its table sits at word
 *         (A_derived - A_F) / 4 of the derived vtable, where A_derived and A_F are the two stores' addends (the
 *         derived table's group for that vptr is the base table's image; complete_off is not consulted). Nothing
 *         overwritten: a member (a verdict at off; each table F installs is recorded as a member object at off+p,
 *         as the reference records a member's vptr, unless the derived constructor stores a table there itself:
 *         an inlined constructor of the member's own class, which the site already records). F installs only the
 *         derived class's own table: no evidence.
 *         An inlined constructor's vptr store that the owner's store overwrites (the reference's "inlined-ctor
 *         store" candidate) is VPTR evidence too: with a parent constructor inlined, the call above is the
 *         grandparent's and the inlined store names the parent; both stand, and _prune_ancestors keeps the parent
 *         (noting the grandparent "reached directly"), as in the reference.
 *   DTOR  a destructor call with r4 an immediate. r4 = 0: a base; its vtable is the table the sub-destructor
 *         resets at a3+p where this+off+p is one of the derived vptrs (word as above, from that group's addend),
 *         else the reference's choice (the callee's dtor owner), else (a `__dt__` of a class without a vtable)
 *         that class by name. r4 = -1: a member (a verdict at off; a destructor of a class without a vtable is
 *         recorded under its mangled class, as the reference records it).
 *   REF   everything else: the reference's candidates (constructor calls of functions that install no table,
 *         destructor calls without an immediate r4 of 0 or -1) and slot overlap, unchanged.
 *
 * Conflicts and missing evidence: at one offset the strongest tier decides. A candidate is dropped when a member
 * verdict of a stronger tier stands at its offset, or when base candidates of a stronger tier stand there and none
 * names the same vtable (or class); a member verdict at the offset of an admitted base is kept as evidence on that
 * base ("conflict: ..."), so between a base and a member of the same tier the base is kept, the conflict noted.
 * Where a tier has no evidence, the next applies; slot overlap still covers the groups no candidate placed (a group
 * whose base constructor is inlined and leaves no store, or a class constructed only by functions the scan cannot
 * read).
 * Evidence for a base already placed at the same offset is added to it without re-imaging the table (a
 * destructor call naming the reference's vtable carries no addend). A placed table must still fit the derived
 * vtable and be compatible with it (_compatible), else it is dropped, as the reference drops an image that does
 * not fit.
 *
 * Recording (the JSON schema is unchanged): a base keeps the reference's form {base, offset, word, evidence}; offset
 * is off (the subobject's start), word the image's word, and "primary" (part 2) is word == 0. Evidence strings:
 * "ctor-call F" (as the reference) and "vptr-overwrite this+0xO (F installs V there)" for VPTR; "dtor-call F (r4=0)"
 * for DTOR; "thunks this-0xOFF (N slots)" when N > 0 slots of the image are thunks adjusting by off; "conflict: E"
 * for a weaker member verdict E at the same offset. Members: "ctor-call F (vptr not overwritten)" and
 * "dtor-call F (r4=-1)" verdicts; member objects through the existing member_objects / member_named records. */
#include "classes_analysis.h"

static bool eq(AvpStr x, AvpStr y) { return avp_str_eq(x, y); }

/* F's last vtable store at each offset through a3 (the first a3 site of F); NULL when F stores no table */
static const ClassesSite *a3_site(const ClassesAnalysis *an, AvpStr f) {
    AvpVal *v = avp_map_get(an->sites_of_func, f);
    if (!v) return NULL;
    size_t first = (size_t)(v->u >> 32), n = (size_t)(v->u & 0xFFFFFFFFu);
    for (size_t i = first; i < first + n; i++)
        if (avp_str_eqz(an->sites.v[i].token, "a3")) return &an->sites.v[i];
    return NULL;
}

static void push_cand(ClassesVtable *r, bool is_name, AvpStr b, i64 off, AvpStr ev, int tier, bool has_word,
                      i64 word) {
    ClassesCand c = {is_name, b, false, off, ev, tier, has_word, word};
    avp_vec_push(&r->cand, c);
}

static void push_verdict(ClassesVtable *r, i64 off, int tier, AvpStr ev) {
    ClassesMemberVerdict m = {off, tier, ev};
    avp_vec_push(&r->verdicts, m);
}

static void push_member_object(ClassesVtable *r, i64 off, AvpStr vt) {
    for (size_t q = 0; q < r->member_objects.n; q++)
        if (r->member_objects.v[q].off == off && eq(r->member_objects.v[q].vt, vt)) return;
    ClassesMemberObj mo = {off, vt};
    avp_vec_push(&r->member_objects, mo);
}

static void push_member_named(ClassesVtable *r, i64 off, AvpStr cls, AvpStr ev) {
    for (size_t i = 0; i < r->member_named.n; i++) {
        const ClassesMemberNamed *m = &r->member_named.v[i];
        if (m->off == off && eq(m->cls, cls) && eq(m->ev, ev)) return;
    }
    ClassesMemberNamed m = {off, cls, ev};
    avp_vec_push(&r->member_named, m);
}

bool classes_br_ctor_call(ClassesAnalysis *an, ClassesVtable *r, const ClassesSite *s, const ClassesCall *call) {
    AvpArena *a = an->a;
    const ClassesSite *f = a3_site(an, call->target);
    if (!f || !f->final.n) return false;
    i64 off = call->args[0].v;
    bool foreign = false; /* F installs a table other than the derived class's own */
    const ClassesSt *best_f = NULL, *best_o = NULL;
    for (size_t k = 0; k < f->final.n; k++) {
        const ClassesSt *fs = &f->stores.v[f->final.v[k].st];
        if (eq(fs->vt, r->name)) continue;
        foreign = true;
        for (size_t j = 0; j < s->final.n; j++) {
            const ClassesSt *os = &s->stores.v[s->final.v[j].st];
            if (os->off == off + fs->off && os->idx > call->index && eq(os->vt, r->name) &&
                (!best_f || fs->off < best_f->off)) {
                best_f = fs;
                best_o = os;
            }
        }
    }
    if (!foreign) return false;
    AvpStr ctor_ev = avp_str_concat(a, AVP_S("ctor-call "), call->target);
    if (!best_f) {
        push_verdict(r, off, CLASSES_BR_VPTR,
                     avp_str(avp_sprintf(a, "ctor-call %.*s (vptr not overwritten)", AVP_SF(call->target))));
        for (size_t k = 0; k < f->final.n; k++) { /* F's tables the derived constructor leaves in place */
            const ClassesSt *fs = &f->stores.v[f->final.v[k].st];
            bool stored = false; /* an inlined member constructor's own table there: the site already records it */
            for (size_t j = 0; j < s->final.n && !stored; j++)
                stored = s->stores.v[s->final.v[j].st].off == off + fs->off;
            if (!eq(fs->vt, r->name) && !stored) push_member_object(r, off + fs->off, fs->vt);
        }
        return true;
    }
    i64 d = best_o->add - best_f->add;
    bool has_word = d >= 0 && classes_mod4(d) == 0;
    push_cand(r, false, best_f->vt, off, ctor_ev, CLASSES_BR_VPTR, has_word, has_word ? classes_floordiv4(d) : 0);
    push_cand(r, false, best_f->vt, off,
              avp_str(avp_sprintf(a, "vptr-overwrite this+0x%llX (%.*s installs %.*s there)",
                                  (unsigned long long)best_o->off, AVP_SF(call->target), AVP_SF(best_f->vt))),
              CLASSES_BR_VPTR, has_word, has_word ? classes_floordiv4(d) : 0);
    return true;
}

bool classes_br_dtor_call(ClassesAnalysis *an, ClassesVtable *r, const ClassesCall *call, AvpStr ref_bvt) {
    AvpArena *a = an->a;
    const ClassesVal *r4 = &call->args[1];
    if (r4->kind != CLASSES_V_IMM || (r4->v != 0 && r4->v != -1)) return false;
    i64 off = call->args[0].v;
    AvpStr tgt = call->target;
    if (r4->v == -1) {
        push_verdict(r, off, CLASSES_BR_DTOR, avp_str(avp_sprintf(a, "dtor-call %.*s (r4=-1)", AVP_SF(tgt))));
        if (!ref_bvt.p && avp_str_startswith(tgt, AVP_S("__dt__"))) {
            AvpStr cl = classes_mangled_class(a, tgt);
            if (cl.p && cl.n) push_member_named(r, off, cl, avp_str_concat(a, AVP_S("dtor-call "), tgt));
        }
        return true;
    }
    AvpStr ev = avp_str(avp_sprintf(a, "dtor-call %.*s (r4=0)", AVP_SF(tgt)));
    /* the sub-destructor's table at a3+p where the derived class keeps a vptr at this+off+p */
    const ClassesSite *f = a3_site(an, tgt);
    const ClassesSt *best_f = NULL;
    i64 best_a = 0;
    for (size_t k = 0; f && k < f->final.n; k++) {
        const ClassesSt *fs = &f->stores.v[f->final.v[k].st];
        if (eq(fs->vt, r->name) || (best_f && fs->off >= best_f->off)) continue;
        for (size_t g = 0; g < r->group.n; g++) {
            const ClassesGroupEnt *e = &r->group.v[g];
            if (e->has_o && e->o == off + fs->off) {
                best_f = fs;
                best_a = e->a;
                break;
            }
        }
    }
    if (best_f) {
        i64 d = best_a - best_f->add;
        bool has_word = d >= 0 && classes_mod4(d) == 0;
        push_cand(r, false, best_f->vt, off, ev, CLASSES_BR_DTOR, has_word, has_word ? classes_floordiv4(d) : 0);
        return true;
    }
    if (ref_bvt.p) {
        push_cand(r, false, ref_bvt, off, ev, CLASSES_BR_DTOR, false, 0);
        return true;
    }
    if (avp_str_startswith(tgt, AVP_S("__dt__"))) { /* a base without a vtable */
        AvpStr cl = classes_mangled_class(a, tgt);
        if (cl.p && cl.n) {
            push_cand(r, true, cl, off, ev, CLASSES_BR_DTOR, false, 0);
            return true;
        }
    }
    return false;
}

bool classes_br_admit(const ClassesVtable *r, size_t ci, const i64 *offs, const bool *ok) {
    const ClassesCand *c = &r->cand.v[ci];
    if (!ok[ci]) return true; /* the reference drops it later */
    i64 off = offs[ci];
    for (size_t i = 0; i < r->verdicts.n; i++)
        if (r->verdicts.v[i].off == off && r->verdicts.v[i].tier > c->tier) return false;
    bool stronger = false; /* a stronger candidate at off; admitted only when one of them names the same base */
    for (size_t j = 0; j < r->cand.n; j++) {
        const ClassesCand *o = &r->cand.v[j];
        if (j == ci || !ok[j] || offs[j] != off || o->tier <= c->tier) continue;
        if (o->b_is_name == c->b_is_name && eq(o->b, c->b)) return true;
        stronger = true;
    }
    return !stronger;
}

void classes_br_finish(ClassesAnalysis *an, ClassesVtable *r) {
    AvpArena *a = an->a;
    for (size_t i = 0; i < r->bases.n; i++) {
        ClassesBase *b = r->bases.v[i];
        if (!b->has_offset) continue;
        for (size_t k = 0; k < r->verdicts.n; k++)
            if (r->verdicts.v[k].off == b->offset)
                avp_vec_push(&b->evidence, avp_str_concat(a, AVP_S("conflict: "), r->verdicts.v[k].ev));
        if (b->base_is_name || !b->has_word || b->offset <= 0) continue;
        const ClassesVtable *br = classes_an_vt(an, b->base);
        i64 n = 0, end = b->word + (i64)(br ? br->words->n : 0);
        for (i64 k = b->word; k < end && k < (i64)r->words->n; k++) {
            AvpStr w = r->words->v[k];
            i64 adj = 0;
            if (w.p) classes_resolve_thunk(an->prog, w, &adj);
            if (adj == b->offset) n++;
        }
        if (n)
            avp_vec_push(&b->evidence, avp_str(avp_sprintf(a, "thunks this-0x%llX (%lld slots)",
                                                           (unsigned long long)b->offset, (long long)n)));
    }
}
