/* check_docs pins: the Toolchain list of docs/toolchain.md against doctor's pin table (check_pins). The table is a parameter
 * (the tests give their own); the tool passes the C doctor's (doctor.h), where the Python imported tools/doctor.py
 * from the checkout (D-check_docs-1). */
#include "check_docs.h"
#include "shsm_cli.h"
#include "shsm_re.h"

static AvpArena *A(void) { return avp_arena(); }

CdPins cd_doctor_pins(void) {
    return (CdPins){doctor_pins, doctor_npins, doctor_compilers_required, doctor_compilers_optional};
}

CdEntries cd_toolchain_entries(AvpArena *a, const char *root) {
    CdEntries entries = {0};
    AvpStrVec lines = cd_read_lines(a, avp_path_join(a, root, CD_PINS_DOC));
    bool inside = false;
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr line = lines.v[i];
        if (avp_str_startswith(line, AVP_S("**Toolchain**"))) {
            inside = true;
            continue;
        }
        if (!inside) continue;
        if (avp_str_startswith(line, AVP_S("**")) || avp_str_startswith(line, AVP_S("## "))) break;
        if (avp_str_startswith(line, AVP_S("- "))) {
            CdEntry e = {(i64)i + 1, avp_str_slice(line, 2, AVP_END)};
            avp_vec_push(&entries, e);
        } else if (entries.n && avp_str_strip(line).n) {
            CdEntry *e = &entries.v[entries.n - 1];
            e->text = avp_str(avp_sprintf(a, "%.*s %.*s", AVP_SF(e->text), AVP_SF(avp_str_strip(line))));
        }
    }
    return entries;
}

static const CdEntry *entry_with(const CdEntries *entries, const char *value) {
    for (size_t i = 0; i < entries->n; i++)
        if (avp_str_contains(entries->v[i].text, avp_str(value))) return &entries->v[i];
    return NULL;
}

static const char *pin_field(const DoctorPin *p, const char *field) {
    if (!strcmp(field, "sha256")) return p->sha256;
    if (!strcmp(field, "zip_sha256")) return p->zip_sha256;
    if (!strcmp(field, "sha1")) return p->sha1;
    if (!strcmp(field, "commit")) return p->commit;
    if (!strcmp(field, "url")) return p->url;
    if (!strcmp(field, "version")) return p->version;
    if (!strcmp(field, "asset")) return p->asset;
    return NULL;
}

static const char *first12(const char *v) { return avp_cz(A(), avp_u8_slice(avp_str(v), 0, 12)); }

void cd_check_pins(const char *root, CdReport *rep, const CdPins *pins) {
    CdEntries entries = cd_toolchain_entries(A(), root);
    if (!entries.n) {
        cd_report_add(rep, "pins", CD_PINS_DOC, 0, false, "no **Toolchain** list found");
        return;
    }
    /* The text the values are looked up in: the Toolchain list, and the guide, which keeps the DOL hash (its "Byte gate"
     * paragraph). */
    AvpStrVec agents_lines = cd_read_lines(A(), avp_path_join(A(), root, CD_GUIDE_DOC));
    AvpStrVec pin_lines = cd_read_lines(A(), avp_path_join(A(), root, CD_PINS_DOC));
    for (size_t i = 0; i < pin_lines.n; i++) avp_vec_push(&agents_lines, pin_lines.v[i]);
    AvpStr agents = avp_str_join(A(), AVP_S("\n"), agents_lines.v, agents_lines.n);

    static const char *const fields[] = {"sha256", "zip_sha256", "sha1", "commit", "url"};
    static const char *const others[] = {"version", "asset"};
    AvpSet *pinned = avp_set_new();
    for (size_t k = 0; k < pins->npins; k++) {
        const DoctorPin *pin = &pins->pins[k];
        for (size_t f = 0; f < 5; f++) {
            const char *value = pin_field(pin, fields[f]);
            if (!value) continue;
            avp_set_add(pinned, avp_str(value));
            const CdEntry *e = entry_with(&entries, value);
            if (!e) {
                if (avp_str_contains(agents, avp_str(value))) continue; /* the DOL hash lives in the "Byte gate" paragraph */
                cd_report_add(rep, "pins", CD_PINS_DOC, 0, false, "doctor pin %s.%s %s is not in the Toolchain list",
                              pin->key, fields[f], value);
                continue;
            }
            for (size_t o = 0; o < 2; o++) {
                const char *ov = pin_field(pin, others[o]);
                if (strcmp(fields[f], "url") != 0 && ov && !avp_str_contains(e->text, avp_str(ov)))
                    cd_report_add(rep, "pins", CD_PINS_DOC, e->lineno, false, "%s: the entry with %s %s does not name %s %s",
                                  pin->key, fields[f], first12(value), others[o], ov);
            }
            if (!strcmp(fields[f], "commit") && pin->url && !avp_str_contains(e->text, avp_str(pin->url)))
                cd_report_add(rep, "pins", CD_PINS_DOC, e->lineno, false, "%s: the entry with commit %s does not name %s",
                              pin->key, first12(value), pin->url);
        }
        if (pin->packages) {
            const CdEntry *e = entry_with(&entries, "pip install");
            for (const char *const *p = pin->packages; *p; p++) {
                bool ok = false;
                if (e) {
                    AvpStr esc = shsm_re_escape(A(), avp_str(*p));
                    ok = shsm_re_search(shsm_re_try(avp_sprintf(A(), "(?<![\\w-])%.*s(?![\\w-])", AVP_SF(esc)), 0), e->text,
                                        0, AVP_END, NULL);
                }
                if (!ok)
                    cd_report_add(rep, "pins", CD_PINS_DOC, e ? e->lineno : 0, false,
                                  "%s: package %s is not in the pip install line", pin->key, *p);
            }
        }
    }
    for (size_t i = 0; i < entries.n; i++) {
        ShsmReFindall fa = shsm_re_findall(shsm_re("\\b[0-9a-f]{64}\\b", 0), entries.v[i].text, 0, AVP_END);
        for (size_t k = 0; k < fa.count; k++) {
            AvpStr h = shsm_re_findall_at(&fa, k, 0);
            if (!avp_set_has(pinned, h))
                cd_report_add(rep, "pins", CD_PINS_DOC, entries.v[i].lineno, false,
                              "sha256 %.*s is not pinned in doctor's pin table", AVP_SF(h));
        }
        shsm_re_findall_free(&fa);
        fa = shsm_re_findall(shsm_re("\\b[0-9a-f]{40}\\b", 0), entries.v[i].text, 0, AVP_END);
        for (size_t k = 0; k < fa.count; k++) {
            AvpStr h = shsm_re_findall_at(&fa, k, 0);
            if (!avp_set_has(pinned, h))
                cd_report_add(rep, "pins", CD_PINS_DOC, entries.v[i].lineno, true,
                              "40-hex value %.*s (commit or sha1) is not pinned in doctor's pin table", AVP_SF(h));
        }
        shsm_re_findall_free(&fa);
    }
    const char *const *lists[] = {pins->compilers_required, pins->compilers_optional};
    for (size_t l = 0; l < 2; l++) {
        for (const char *const *c = lists[l]; c && *c; c++) {
            AvpStr cs = avp_str(*c), before, after;
            AvpStr tail = avp_str_partition(cs, AVP_S("/"), &before, &after) ? after : cs;
            if (!avp_str_contains(agents, cs) && !avp_str_contains(agents, tail))
                cd_report_add(rep, "pins", CD_PINS_DOC, 0, false, "compiler %s (doctor's compiler list) is not named", *c);
        }
    }
    const char *dol = NULL;
    for (size_t k = 0; k < pins->npins; k++)
        if (!strcmp(pins->pins[k].key, "dol")) dol = pins->pins[k].sha1;
    if (dol && dol[0]) {
        const char *cfg = avp_path_join(A(), root, "config");
        AvpStrVec vers = {0};
        if (avp_isdir(cfg)) {
            if (!avp_listdir(A(), cfg, &vers)) shsm_raise_io();
            avp_sort_strs(vers.v, vers.n);
        }
        for (size_t i = 0; i < vers.n; i++) {
            const char *ver = avp_cz(A(), vers.v[i]);
            const char *sha_file = avp_join(A(), cfg, ver, "build.sha1");
            if (avp_isfile(sha_file)) {
                AvpStr text;
                if (!avp_read_text(A(), sha_file, AVP_TM_UTF8, &text)) shsm_raise_io();
                AvpStrVec words = avp_str_split(text, AVP_S(""), -1);
                AvpStr first = words.n ? words.v[0] : AVP_S("");
                if (!avp_str_eqz(first, dol))
                    cd_report_add(rep, "pins", avp_sprintf(A(), "config/%s/build.sha1", ver), 1, false,
                                  "sha1 %.*s differs from PINS dol %s", AVP_SF(first), dol);
            }
            const char *yml = avp_join(A(), cfg, ver, "config.yml");
            if (avp_isfile(yml)) {
                AvpStr text;
                if (!avp_read_text(A(), yml, AVP_TM_UTF8_REPLACE, &text)) shsm_raise_io();
                if (!avp_str_contains(text, avp_str(dol)))
                    cd_report_add(rep, "pins", avp_sprintf(A(), "config/%s/config.yml", ver), 0, false,
                                  "does not carry the DOL sha1 %s", dol);
            }
        }
    }
    const char *configure = avp_path_join(A(), root, "configure.py");
    if (avp_isfile(configure)) {
        AvpStrVec lines = cd_read_lines(A(), configure);
        for (size_t i = 0; i < lines.n; i++) {
            ShsmReMatch m;
            if (!shsm_re_match(shsm_re("\\s*config\\.(\\w+)_tag\\s*=\\s*[\"']([^\"']+)[\"']", 0), lines.v[i], 0, AVP_END,
                               &m))
                continue;
            AvpStr name = shsm_re_group(&m, 1), val = shsm_re_group(&m, 2);
            const char *key = avp_str_eqz(name, "objdiff") ? "objdiff-cli" : avp_cz(A(), name);
            const DoctorPin *pin = NULL;
            for (size_t k = 0; k < pins->npins; k++)
                if (!strcmp(pins->pins[k].key, key)) pin = &pins->pins[k];
            if (pin && pin->version && !avp_str_eqz(val, pin->version))
                cd_report_add(rep, "pins", "configure.py", (i64)i + 1, false, "config.%.*s_tag %.*s differs from PINS %s version %s",
                              AVP_SF(name), AVP_SF(val), key, pin->version);
        }
    }
}
