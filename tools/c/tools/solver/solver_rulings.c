/* solver_rulings: config/<ver>/solver.tsv, the solver's rulings table (a tool input).
 *
 *     kind<TAB>args...<TAB>note
 *     surviving-source  <unit prefix>                    original source survives: no respelling candidates
 *     lever-flag        <flag>                           a per-file flag ruled a lever
 *     probe-flag        <flag>                           a flag compiled as a probe
 *     const-accessor    <plain spelling> <const spelling> a const accessor the module's own API supplies
 *
 * Lines starting with '#' and blank lines are skipped. The note is optional. */
#include "solver.h"

static const struct {
    const char *name;
    SvRuleKind kind;
    int nargs;
} KINDS[] = {
    {"surviving-source", SV_RULE_SURVIVING_SOURCE, 1},
    {"lever-flag", SV_RULE_LEVER_FLAG, 1},
    {"probe-flag", SV_RULE_PROBE_FLAG, 1},
    {"const-accessor", SV_RULE_CONST_ACCESSOR, 2},
};

bool sv_rulings_parse(AvpArena *a, AvpStr text, SvRulings *out, const char **err) {
    *out = (SvRulings){0};
    *err = NULL;
    AvpStrVec lines = avp_str_splitlines(text, false);
    bool ok = true;
    for (size_t i = 0; i < lines.n && ok; i++) {
        AvpStr line = avp_str_rstrip(lines.v[i]);
        AvpStr s = avp_str_strip(line);
        if (!s.n || s.p[0] == '#') continue;
        AvpStrVec f = avp_str_split(line, AVP_S("\t"), -1);
        AvpStr kind = avp_str_strip(f.v[0]);
        int k = -1;
        for (int j = 0; j < (int)AVP_ARRAY_LEN(KINDS); j++)
            if (avp_str_eqz(kind, KINDS[j].name)) k = j;
        if (k < 0) {
            *err = avp_sprintf(a, "solver.tsv:%zu: unknown kind '%.*s'", i + 1, AVP_SF(kind));
            ok = false;
        } else if (f.n < (size_t)KINDS[k].nargs + 1) {
            *err = avp_sprintf(a, "solver.tsv:%zu: %s takes %d argument(s) separated by tabs", i + 1, KINDS[k].name,
                               KINDS[k].nargs);
            ok = false;
        } else {
            SvRuling ru = {0};
            ru.kind = KINDS[k].kind;
            ru.line = i + 1;
            ru.arg1 = avp_cz(a, avp_str_strip(f.v[1]));
            ru.arg2 = KINDS[k].nargs > 1 ? avp_cz(a, avp_str_strip(f.v[2])) : NULL;
            size_t first_note = (size_t)KINDS[k].nargs + 1;
            ru.note = first_note < f.n
                          ? avp_cz(a, avp_str_strip(avp_str_join(a, AVP_S("\t"), f.v + first_note, f.n - first_note)))
                          : "";
            if (!*ru.arg1 || (ru.arg2 && !*ru.arg2)) {
                *err = avp_sprintf(a, "solver.tsv:%zu: empty argument", i + 1);
                ok = false;
            } else {
                avp_vec_push(out, ru);
            }
        }
        avp_vec_free(&f);
    }
    avp_vec_free(&lines);
    return ok;
}

SvRulings sv_rulings_load(AvpArena *a, const ShsmRepo *r) {
    SvRulings t = {0};
    const char *path = avp_join(a, r->config_dir, "solver.tsv");
    if (!avp_exists(path)) return t;
    AvpBytes b = {0};
    if (!avp_read_all(a, path, &b)) shsm_raise_io();
    const char *err = NULL;
    if (!sv_rulings_parse(a, avp_strn((const char *)b.v, b.n), &t, &err)) shsm_raise(SHSM_EXC_VALUE, "%s", err);
    return t;
}

const SvRuling *sv_rulings_surviving(const SvRulings *t, const char *unit_name, const char *unit_path) {
    for (size_t i = 0; i < t->n; i++) {
        const SvRuling *ru = &t->v[i];
        if (ru->kind != SV_RULE_SURVIVING_SOURCE) continue;
        size_t n = strlen(ru->arg1);
        if ((unit_name && strncmp(unit_name, ru->arg1, n) == 0) || (unit_path && strncmp(unit_path, ru->arg1, n) == 0))
            return ru;
    }
    return NULL;
}

bool sv_rulings_is_lever_flag(const SvRulings *t, const char *flag) {
    for (size_t i = 0; i < t->n; i++)
        if (t->v[i].kind == SV_RULE_LEVER_FLAG && strcmp(t->v[i].arg1, flag) == 0) return true;
    return false;
}
