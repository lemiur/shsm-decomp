/* sibling: `apply`, the placeholder renames of `names` in bulk.
 *
 * Each candidate is planned with the names tool's own planner (names_plan_rename, the code behind `names.exe rename`),
 * so symbols.txt, the source, the headers, the docs and the config lists change together; the plans merge into one
 * batch and the sources are read once. A candidate is a retail function whose symbol is still a placeholder
 * (fn_<address>, with or without a mangled suffix), whose pair lies in a clean object span, and whose sibling name is
 * free. Everything else is counted and listed by class; nothing is guessed. */
#include "sibling.h"

#include "names.h"
#include "shsm_mangle.h"
#include "shsm_git.h"
#include "twins.h"

#include <ctype.h>
#include <string.h>

enum {
    CL_RENAME,    /* planned */
    CL_SAME,      /* already named like the sibling */
    CL_REAL_NAME, /* a different real name: the project named it (often a static function it invented) */
    CL_OTHER_PH,  /* a placeholder of another form (lbl_, unk_, ...) */
    CL_UNCLEAN,   /* the pair lies in a span that is not clean */
    CL_TAKEN,     /* the sibling name is already another retail symbol */
    CL_DUPLICATE, /* two retail functions would get the same name */
    CL_SIGNATURE, /* the sibling's signature differs from the placeholder's: a rename changes only the name */
    CL_SPECIAL,   /* constructor, destructor, operator, template: the source rewrite needs a human */
    CL_REFUSED,   /* the planner refuses it (reason printed) */
    CL_CONFLICT,  /* the plan has a conflict (reason printed) */
    CL_SOURCE,    /* an unmangled placeholder the source names would gain a mangled name: the source site is a human's */
    CL_CTORDTOR,  /* a constructor or destructor: the CodeWarrior special name, symbols.txt only (planned) */
    CL_NCLASS
};
static const char *const CLASS_NAMES[CL_NCLASS] = {"rename",  "same",     "real-name", "other-placeholder", "unclean-span",
                                                   "taken",   "duplicate", "signature", "special",           "refused",
                                                   "conflict", "needs-source", "ctor-dtor"};

/* "@36@__dt__Q25Audio11MaterialMapFv": the compiler's this-adjusting thunk, its offset and the function it jumps to;
 * NULL for any other name. */
const char *sibling_thunk_note(AvpArena *a, const char *name) {
    if (!name || name[0] != '@') return NULL;
    const char *end = strchr(name + 1, '@');
    if (!end || end == name + 1) return NULL;
    const char *target = end + 1;
    const char *dem = shsm_demangle_or_name(a, target);
    return avp_sprintf(a, "a this-adjusting thunk (offset %.*s) to %s", (int)(end - name - 1), name + 1,
                       dem ? dem : target);
}

/* A constructor or destructor by its CodeWarrior name, whose class the mangled name gives (__ct__Q25Audio3FooFv). */
bool sibling_is_ctor_dtor(AvpArena *a, const char *sibling_name) {
    if (strncmp(sibling_name, "__ct__", 6) != 0 && strncmp(sibling_name, "__dt__", 6) != 0) return false;
    ShsmDemangled *dn = shsm_mangle_parse(a, sibling_name);
    return dn && dn->nscope && !*dn->thunk;
}

typedef struct {
    size_t i; /* retail index */
    int cls;
    const char *why; /* reason for the refused classes */
    NamesPlan *plan;
    bool added_sig; /* an unmangled placeholder renamed to a mangled name: the rename adds a signature */
    bool mentioned; /* the source mentions the old plain name */
    const char *old_plain;
} Item;

bool sibling_is_fn_placeholder(const char *name) {
    if (strncmp(name, "fn_", 3) != 0) return false;
    for (int k = 3; k < 11; k++)
        if (!isxdigit((unsigned char)name[k])) return false;
    return name[11] == 0 || strncmp(name + 11, "__", 2) == 0;
}

static bool lib_selected(AvpArena *a, const SibFn *f, const AvpStrVec *libs) {
    if (!libs->n) return true;
    TwinsFn t = {0};
    t.lib = f->lib;
    for (size_t k = 0; k < libs->n; k++)
        if (twins_lib_matches(a, &t, avp_cz(a, libs->v[k]))) return true;
    return false;
}

static const char *owner_of(AvpArena *a, const SibFn *p) {
    if (!p->object) return "(no map object)";
    return p->lib && *p->lib ? avp_sprintf(a, "%s %s", p->lib, p->object) : p->object;
}

/* plan one rename: class CL_RENAME with a plan, or the class and reason of the refusal */
static void plan_item(NamesCtx *cx, const SibFnVec *s, const SibFnVec *g, Item *it) {
    AvpArena *a = cx->a;
    const SibFn *f = &s->v[it->i], *p = &g->v[f->peer];
    AvpTry t;
    NamesPlan *plan = NULL;
    if (AVP_TRY(&t)) {
        plan = names_plan_rename(cx, f->name, p->name, false, false);
        avp_try_end(&t);
    } else {
        const char *msg = t.msg ? avp_strdup(a, t.msg) : "refused";
        it->why = msg;
        if (strstr(msg, "does not have the signature")) it->cls = CL_SIGNATURE;
        else if (strstr(msg, "special member") || strstr(msg, "not an identifier")) it->cls = CL_SPECIAL;
        else it->cls = CL_REFUSED;
        if (it->cls == CL_REFUSED && sibling_thunk_note(a, p->name)) it->why = sibling_thunk_note(a, p->name);
        if (it->cls == CL_SPECIAL && sibling_is_ctor_dtor(a, p->name)) {
            NamesPlan *sym = names_plan_rename(cx, f->name, p->name, false, true);
            if (!sym->conflicts.n) {
                it->cls = CL_CTORDTOR;
                it->plan = sym;
                it->why = "a constructor or destructor: symbols.txt only, the class body needs a human";
            }
        }
        return;
    }
    if (plan->conflicts.n) {
        it->cls = CL_CONFLICT;
        it->why = avp_cz(a, plan->conflicts.v[0]);
        return;
    }
    if (plan->special) {
        /* a constructor or destructor: the symbol takes its CodeWarrior special name; the class body is a human's */
        if (sibling_is_ctor_dtor(a, p->name)) {
            NamesPlan *sym = names_plan_rename(cx, f->name, p->name, false, true);
            if (!sym->conflicts.n) {
                it->cls = CL_CTORDTOR;
                it->plan = sym;
                it->why = "a constructor or destructor: symbols.txt only, the class body needs a human";
                return;
            }
        }
        it->cls = CL_SPECIAL;
        it->why = "a constructor or destructor stub: its class body needs a human";
        return;
    }
    it->cls = CL_RENAME;
    it->plan = plan;
}

static const char *describe(AvpArena *a, const SibFnVec *s, const SibFnVec *g, const Item *it) {
    const SibFn *f = &s->v[it->i], *p = &g->v[f->peer];
    const char *line = avp_sprintf(a, "0x%08llX %-9s %s -> %s  [%s]", (long long)f->address, SIBLING_TIERS[f->tier],
                                   f->name, p->name, owner_of(a, p));
    if (it->why) line = avp_sprintf(a, "%s  (%s)", line, it->why);
    if (it->mentioned) line = avp_sprintf(a, "%s  [source mentions %s]", line, it->old_plain);
    return line;
}

int sibling_cmd_apply(Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibApplyArgs *args, ShsmOutput *out) {
    AvpArena *a = avp_arena();
    const ShsmRepo *repo = sb->repo;
    if (!args->dry_run && shsm_git_dirty(a, repo->root))
        shsm_raise(SHSM_EXC_USAGE, "the tree is not clean; commit or stash first (--dry-run lists the renames)");
    bool *clean = sibling_clean_flags(a, sb, s, g, &args->base);
    NamesCtx *cx = names_ctx_new(repo, NULL);
    AVP_VEC(Item) items = {0};
    i64 count[CL_NCLASS] = {0};
    /* classify by what can be decided without the planner */
    AvpMap *targets = avp_map_new();
    for (size_t i = 0; i < s->n; i++) {
        const SibFn *f = &s->v[i];
        if (f->peer < 0 || !f->tier || !(args->base.tiers & (1u << f->tier)) || !lib_selected(a, f, &args->libs))
            continue;
        const SibFn *p = &g->v[f->peer];
        Item it = {.i = i, .cls = CL_RENAME};
        if (strcmp(f->name, p->name) == 0) it.cls = CL_SAME;
        else if (!sibling_is_placeholder(f->name)) it.cls = CL_REAL_NAME;
        else if (!sibling_is_fn_placeholder(f->name)) it.cls = CL_OTHER_PH;
        else if (!clean[i]) it.cls = CL_UNCLEAN;
        else if (names_has(cx, p->name)) it.cls = CL_TAKEN;
        else {
            bool created;
            AvpVal *v = avp_map_put(targets, avp_str(p->name), &created);
            if (created) v->i = (i64)items.n;
            else {
                it.cls = CL_DUPLICATE;
                items.v[v->i].cls = CL_DUPLICATE; /* both claimants stay as they are */
            }
        }
        avp_vec_push(&items, it);
    }
    avp_map_free(targets);
    for (size_t k = 0; k < items.n; k++)
        if (items.v[k].cls == CL_RENAME) plan_item(cx, s, g, &items.v[k]);
    /* the source mentions of the old plain names of the functions left alone for a human */
    AvpStrVec words = {0};
    for (size_t k = 0; k < items.n; k++) {
        Item *it = &items.v[k];
        const SibFn *f = &s->v[it->i];
        it->added_sig = it->cls == CL_RENAME && strcmp(names_plain_of(a, f->name).plain, f->name) == 0 &&
                        strcmp(names_plain_of(a, g->v[f->peer].name).plain, g->v[f->peer].name) != 0;
        if (!it->added_sig && it->cls != CL_SIGNATURE && it->cls != CL_SPECIAL && it->cls != CL_REFUSED &&
            it->cls != CL_CONFLICT)
            continue;
        it->old_plain = names_plain_of(a, s->v[it->i].name).plain;
        avp_vec_push(&words, avp_str(it->old_plain));
    }
    AvpStrVec mentioned = names_code_mentions(cx, &words);
    for (size_t k = 0; k < items.n; k++) {
        Item *it = &items.v[k];
        if (!it->old_plain) continue;
        for (size_t m = 0; m < mentioned.n; m++)
            if (avp_str_eqz(mentioned.v[m], it->old_plain)) it->mentioned = true;
        if (it->added_sig && it->mentioned) {
            it->cls = CL_SOURCE;
            it->why = "the source names it; adding the signature needs its class or namespace";
            it->plan = NULL;
        }
    }
    /* the batch */
    NamesPlan *batch = names_plan_new(cx);
    for (size_t k = 0; k < items.n; k++) {
        count[items.v[k].cls]++;
        if (items.v[k].cls == CL_RENAME || items.v[k].cls == CL_CTORDTOR) names_plan_merge(batch, items.v[k].plan);
    }
    names_compute_edits(batch);
    i64 edits = 0;
    for (size_t k = 0; k < batch->files.n; k++) edits += (i64)batch->files.v[k]->edits.n;
    /* report */
    shsm_output_linez(out, avp_sprintf(a, "%s%lld renames planned (%zu files, %lld line edits) and %lld constructor/destructor symbols (symbols.txt only); skipped: %lld already"
                                          " the same, %lld different real name, %lld other placeholder, %lld in"
                                          " unclean spans, %lld sibling name taken, %lld duplicate target,"
                                          " %lld other signature, %lld special member, %lld refused, %lld conflict,"
                                          " %lld unmangled but named in source",
                                       args->dry_run ? "dry run: " : "", (long long)count[CL_RENAME], batch->files.n,
                                       (long long)edits, (long long)count[CL_CTORDTOR], (long long)count[CL_SAME], (long long)count[CL_REAL_NAME],
                                       (long long)count[CL_OTHER_PH], (long long)count[CL_UNCLEAN],
                                       (long long)count[CL_TAKEN], (long long)count[CL_DUPLICATE],
                                       (long long)count[CL_SIGNATURE], (long long)count[CL_SPECIAL],
                                       (long long)count[CL_REFUSED], (long long)count[CL_CONFLICT],
                                       (long long)count[CL_SOURCE]));
    AvpJson *filesj = avp_json_arr(a);
    for (size_t k = 0; k < batch->files.n; k++) {
        const NamesFile *nf = batch->files.v[k];
        avp_json_arr_push(a, filesj, avp_json_strz(a, nf->rel));
        if (k < 12)
            shsm_output_linez(out, avp_sprintf(a, "  edits %s: %zu lines", nf->rel, nf->edits.n));
    }
    shsm_output_data(out, "files", filesj);
    AvpJson *classes = avp_json_obj(a);
    static const int order[] = {CL_REAL_NAME, CL_TAKEN,    CL_DUPLICATE, CL_SIGNATURE, CL_SPECIAL, CL_REFUSED,
                                CL_CONFLICT,  CL_SOURCE,   CL_OTHER_PH,  CL_UNCLEAN, CL_RENAME, CL_CTORDTOR};
    for (size_t oi = 0; oi < sizeof order / sizeof *order; oi++) {
        int cls = order[oi];
        AvpJson *arr = avp_json_arr(a);
        i64 shown = 0;
        for (size_t k = 0; k < items.n; k++) {
            const Item *it = &items.v[k];
            if (it->cls != cls) continue;
            const SibFn *f = &s->v[it->i];
            AvpJson *o = avp_json_obj(a);
            avp_json_set(a, o, AVP_S("address"), avp_json_strz(a, avp_sprintf(a, "0x%08llX", (long long)f->address)));
            avp_json_set(a, o, AVP_S("current"), avp_json_strz(a, f->name));
            avp_json_set(a, o, AVP_S("sibling"), avp_json_strz(a, g->v[f->peer].name));
            avp_json_set(a, o, AVP_S("tier"), avp_json_strz(a, SIBLING_TIERS[f->tier]));
            avp_json_set(a, o, AVP_S("reason"), it->why ? avp_json_strz(a, it->why) : avp_json_null());
            avp_json_set(a, o, AVP_S("source_mentions"), avp_json_bool(it->mentioned));
            avp_json_arr_push(a, arr, o);
            if ((cls == CL_RENAME || cls == CL_CTORDTOR) && !args->dry_run) continue;
            if (shown == 0)
                shsm_output_linez(out, avp_sprintf(a, "%s (%lld)%s:", CLASS_NAMES[cls], (long long)count[cls],
                                                   count[cls] > SIBLING_APPLY_EXAMPLES && !args->all
                                                       ? ", first examples"
                                                       : ""));
            if (args->all || shown < SIBLING_APPLY_EXAMPLES)
                shsm_output_linez(out, avp_sprintf(a, "  %s", describe(a, s, g, it)));
            shown++;
        }
        avp_json_set(a, classes, avp_str(CLASS_NAMES[cls]), arr);
    }
    shsm_output_data(out, "classes", classes);
    if (batch->conflicts.n) {
        for (size_t k = 0; k < batch->conflicts.n && k < 20; k++)
            shsm_output_linez(out, avp_sprintf(a, "BATCH CONFLICT: %s", avp_cz(a, batch->conflicts.v[k])));
        shsm_raise(SHSM_EXC_USAGE, "the batch has %zu conflicts; nothing written", batch->conflicts.n);
    }
    if (args->dry_run) {
        shsm_output_linez(out, "nothing applied (--dry-run)");
        avp_vec_free(&items);
        return 0;
    }
    if (count[CL_RENAME] + count[CL_CTORDTOR]) names_write_plan(batch);
    shsm_output_linez(out, avp_sprintf(a, "applied: %lld symbols renamed, %zu files written; check with names.exe check"
                                          " and ninja",
                                       (long long)(count[CL_RENAME] + count[CL_CTORDTOR]), batch->files.n));
    avp_vec_free(&items);
    return 0;
}
