/* unit_stub: tools/unit.py lines 1234-1473 and the splits.txt insertion of cmd_new. */
#include "unit_stub.h"

#include "shsm_placeholder.h"
#include "shsm_re.h"

#include <string.h>

#define BANNER "===================="

const char *unit_header_guard(AvpArena *a, const char *path) {
    AvpStr stem = avp_str(unit_stem(a, path));
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "");
    /* re.sub(r"[^A-Za-z0-9]", "_", stem).upper(): one "_" per code point */
    for (size_t i = 0; i < stem.n; i++) {
        unsigned char c = (unsigned char)stem.p[i];
        if ((c >= 'a' && c <= 'z')) avp_buf_putc(&b, (char)(c - 32));
        else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) avp_buf_putc(&b, (char)c);
        else if ((c & 0xC0) != 0x80) avp_buf_putc(&b, '_');
    }
    avp_buf_putz(&b, "_H");
    return avp_buf_cz(&b);
}

static bool has(AvpSet *s, const char *n) { return s && avp_set_has(s, avp_str(n)); }

static bool in_list(const AvpStrVec *v, const char *s) {
    for (size_t i = 0; i < v->n; i++)
        if (avp_str_eqz(v->v[i], s)) return true;
    return false;
}

static bool stub_ok(const UnitDemangled *d, const AvpStrVec *own_classes) {
    static const char *const plain[] = {"unsigned int", "u8", "u16", "u32", "u64", "s8", "s16", "s32", "s64", "float",
                                        "double", "bool", "char", "int", "wchar_t"};
    if (d->mangled && !d->has_args) return false;
    if (d->complex) return false;
    if (!d->has_args) return true;
    ShsmRe *re = shsm_re("(?:const )?[A-Za-z_]\\w*", 0);
    ShsmReMatch m;
    for (size_t i = 0; i < d->args.n; i++) {
        AvpStr arg = d->args.v[i];
        if (!shsm_re_fullmatch(re, arg, 0, AVP_END, &m)) continue;
        char buf[256];
        snprintf(buf, sizeof buf, "%.*s", (int)arg.n, arg.p);
        bool known = unit_is_base_type(buf);
        for (size_t k = 0; k < sizeof plain / sizeof *plain && !known; k++) known = !strcmp(plain[k], buf);
        if (!known && !in_list(own_classes, buf)) return false; /* a class passed by value, defined elsewhere */
    }
    return true;
}

static void line(AvpStrVec *v, const char *s) { avp_vec_push(v, avp_str(s)); }

bool unit_has_source_ext(const char *path) {
    static const char *const exts[] = {".c", ".cpp", ".cp", ".s"};
    size_t n = strlen(path);
    for (size_t i = 0; i < sizeof exts / sizeof *exts; i++) {
        size_t e = strlen(exts[i]);
        if (n > e && !strcmp(path + n - e, exts[i])) return true;
    }
    return false;
}

/* The unit-name comment: the default "unit name unconfirmed" pair, the --name-evidence text (each line re-prefixed
 * with the language's comment leader), or nothing when the name is confirmed with no evidence text. */
static void name_comment(AvpArena *a, AvpStrVec *L, bool confirmed, const char *evidence, const char *leader) {
    if (evidence && *evidence) {
        AvpStrVec parts = avp_str_split(avp_str(evidence), AVP_S("\n"), -1);
        for (size_t i = 0; i < parts.n; i++) {
            AvpStr s = avp_str_strip(parts.v[i]);
            if (avp_str_startswith(s, AVP_S("//"))) s = avp_str_lstrip(avp_str_slice(s, 2, (i64)s.n));
            else if (avp_str_startswith(s, AVP_S("#"))) s = avp_str_lstrip(avp_str_slice(s, 1, (i64)s.n));
            line(L, avp_sprintf(a, "%s%s%.*s", leader, s.n ? " " : "", (int)s.n, s.p));
        }
        avp_vec_free(&parts);
        line(L, "");
    } else if (!confirmed) {
        line(L, avp_sprintf(a, "%s unit name unconfirmed: no string or class-name evidence names this file; rename it when",
                            leader));
        line(L, avp_sprintf(a, "%s evidence appears and keep the open item in docs/exceptions.md.", leader));
        line(L, "");
    }
}

/* An assembly unit stub (`.s`): the shape of MetroTRK's __exception.s and RenderWare's skinmatrixblend.s. One
 * `.fn`/`.endfn` pair per function symbol of the range, each under a `# <name>   // 0x8XXXXXXX` header comment. */
static const char *build_asm_source(AvpArena *a, const UnitSymPtrVec *funcs, bool confirmed, const char *name_evidence,
                                    const AvpStrVec *data_note) {
    AvpStrVec L = {0};
    name_comment(a, &L, confirmed, name_evidence, "#");
    line(&L, "#");
    line(&L, "# ORIGINAL-ASM: TODO state the evidence that these functions were assembly in the original (list them in");
    line(&L, "# docs/exceptions.md); the stubs below are placeholders, one per function symbol of the unit's range.");
    line(&L, "");
    line(&L, ".include \"macros.inc\"");
    line(&L, "");
    line(&L, ".section .text, \"ax\"");
    line(&L, "");
    if (data_note->n) {
        line(&L, "# Data owned by this unit (the names are the symbols.txt names):");
        for (size_t i = 0; i < data_note->n && i < 40; i++)
            line(&L, avp_sprintf(a, "#   %.*s", (int)data_note->v[i].n, data_note->v[i].p));
        if (data_note->n > 40) line(&L, avp_sprintf(a, "#   ... %zu more", data_note->n - 40));
        line(&L, "");
    }
    for (size_t i = 0; i < funcs->n; i++) {
        const UnitSym *f = funcs->v[i];
        UnitDemangled d = unit_demangle(a, f->name);
        const char *title = d.mangled && d.has_args ? unit_dm_qualified(a, &d) : f->name;
        const char *scope = f->scope && *f->scope ? f->scope : "global";
        /* dtk quotes the names the assembler would split (the \ of a static initialiser's name) */
        bool plain = true;
        for (const char *s = f->name; *s; s++)
            plain &= (*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_' ||
                     *s == '$' || *s == '.';
        const char *sym = plain ? f->name : avp_sprintf(a, "\"%s\"", f->name);
        line(&L, avp_sprintf(a, "# %s   // 0x%08llX", title, f->addr));
        line(&L, avp_sprintf(a, ".fn %s, %s", sym, scope));
        line(&L, "    # TODO: the hand-written body");
        line(&L, "    blr");
        line(&L, avp_sprintf(a, ".endfn %s", sym));
        line(&L, "");
    }
    AvpStr src = avp_str_join(a, AVP_S("\n"), L.v, L.n);
    src = avp_str_rstrip_chars(src, AVP_S("\n"));
    const char *res = avp_sprintf(a, "%.*s\n", (int)src.n, src.p);
    avp_vec_free(&L);
    return res;
}

const char *unit_build_source(AvpArena *a, const char *path, const UnitSymPtrVec *funcs, bool confirmed,
                              const char *name_evidence, const AvpStrVec *data_note, AvpSet *factory_names,
                              AvpSet *vtable_names, AvpSet *sinit_names, const UnitDecls *decls, const char **header) {
    if (unit_is_asm_source(path)) {
        *header = NULL;
        return build_asm_source(a, funcs, confirmed, name_evidence, data_note);
    }
    bool game = unit_is_game_path(path);
    AvpStr ps = avp_str(path);
    bool cpp = avp_str_endswith(ps, AVP_S(".cpp")) || avp_str_endswith(ps, AVP_S(".cp"));
    size_t n = funcs->n;
    UnitDemangled *dems = AVP_NEWN(a, UnitDemangled, n ? n : 1);
    bool *gen = AVP_NEWN(a, bool, n ? n : 1);
    for (size_t i = 0; i < n; i++) {
        dems[i] = unit_demangle(a, funcs->v[i]->name);
        gen[i] = unit_dm_generated(&dems[i]) || has(sinit_names, funcs->v[i]->name);
    }
    AvpStrVec own_classes = {0};
    for (size_t i = 0; i < n; i++) {
        const UnitDemangled *d = &dems[i];
        if (gen[i]) continue;
        if (*d->cls && d->mangled && !in_list(&own_classes, d->cls) && !d->complex) line(&own_classes, d->cls);
    }
    AvpStrVec L = {0};
    name_comment(a, &L, confirmed, name_evidence, "//");
    if (game && cpp) line(&L, avp_sprintf(a, "#include <%s.h>", unit_stem(a, path)));
    else line(&L, "// TODO: includes");
    line(&L, "");
    if (data_note->n) {
        line(&L, "// Data owned by this unit (declare each once in the header, define it here; names are the");
        line(&L, "// symbols.txt names):");
        for (size_t i = 0; i < data_note->n && i < 40; i++)
            line(&L, avp_sprintf(a, "//   %.*s", (int)data_note->v[i].n, data_note->v[i].p));
        if (data_note->n > 40) line(&L, avp_sprintf(a, "//   ... %zu more", data_note->n - 40));
        line(&L, "");
    }
    const char *open_ns = NULL; /* the `namespace X {` block the stubs below are in */
    for (size_t i = 0; i < n; i++) {
        if (gen[i]) continue;
        const UnitSym *f = funcs->v[i];
        const UnitDemangled *d = &dems[i];
        const char *title = d->mangled ? unit_dm_qualified(a, d) : d->base;
        if (game) {
            line(&L, "/*");
            line(&L, BANNER);
            line(&L, title);
            if (has(factory_names, f->name)) {
                line(&L, "");
                line(&L, "Address taken by the static initialiser: probably an inline factory that belongs in the "
                         "header.");
            }
            line(&L, BANNER);
            line(&L, "*/");
            line(&L, avp_sprintf(a, "// 0x%08llX", f->addr));
        }
        /* A unit with no generated header gets no class declaration: the one name a function is qualified with must
         * be a namespace (the stub goes inside it) or a class some header defines, else the stub cannot compile. */
        bool ns_fn = false, qualified_ok = true;
        if (d->mangled && *d->cls && !(game && cpp)) {
            ns_fn = !d->is_ctor && !d->is_dtor && unit_decls_has_namespace(decls, d->cls) &&
                    !unit_decls_has_class(decls, d->cls);
            qualified_ok = ns_fn || unit_decls_has_class(decls, d->cls);
        }
        bool stub = d->mangled && qualified_ok && stub_ok(d, &own_classes);
        const char *want_ns = stub && ns_fn ? d->cls : NULL;
        if (open_ns && (!want_ns || strcmp(open_ns, want_ns))) {
            line(&L, avp_sprintf(a, "} // namespace %s", open_ns));
            line(&L, "");
            open_ns = NULL;
        }
        if (want_ns && !open_ns) {
            line(&L, avp_sprintf(a, "namespace %s {", want_ns));
            line(&L, "");
            open_ns = want_ns;
        }
        if (stub) {
            line(&L, avp_sprintf(a, "%s {", unit_sig_text(a, d, !ns_fn)));
            line(&L, SHSM_PLACEHOLDER_BODY_LINE);
            line(&L, game ? "}" : avp_sprintf(a, "} // 0x%08llX", f->addr));
        } else if (d->mangled) {
            line(&L, avp_sprintf(a, "// TODO: signature not expanded or needs a complete type: %s", f->name));
            line(&L, avp_sprintf(a, "// %s { }%s", d->has_args ? unit_sig_text(a, d, true) : f->name,
                                 game ? "" : avp_sprintf(a, " // 0x%08llX", f->addr)));
        } else {
            if (cpp && game) line(&L, avp_sprintf(a, "extern \"C\" void %s() {", d->base));
            else line(&L, avp_sprintf(a, "void %s() {", d->base));
            line(&L, SHSM_PLACEHOLDER_BODY_LINE);
            line(&L, game ? "}" : avp_sprintf(a, "} // 0x%08llX", f->addr));
        }
        line(&L, "");
    }
    if (open_ns) {
        line(&L, avp_sprintf(a, "} // namespace %s", open_ns));
        line(&L, "");
    }
    AvpBuf g;
    avp_buf_init(&g, a);
    avp_buf_putz(&g, "");
    bool any_gen = false;
    for (size_t i = 0; i < n; i++)
        if (gen[i]) {
            if (any_gen) avp_buf_putz(&g, ", ");
            avp_buf_printf(&g, "%s (0x%08llX)", funcs->v[i]->name, funcs->v[i]->addr);
            any_gen = true;
        }
    if (any_gen) {
        line(&L, avp_sprintf(a, "// Compiler-generated, no source of their own: %s.", avp_buf_cz(&g)));
        line(&L, "");
    }
    AvpStr src = avp_str_join(a, AVP_S("\n"), L.v, L.n);
    src = avp_str_rstrip_chars(src, AVP_S("\n"));
    const char *source = avp_sprintf(a, "%.*s\n", (int)src.n, src.p);
    avp_vec_free(&L);
    *header = NULL;
    if (!(game && cpp)) return source;
    AvpStrVec h = {0};
    const char *guard = unit_header_guard(a, path);
    line(&h, avp_sprintf(a, "#ifndef %s", guard));
    line(&h, avp_sprintf(a, "#define %s", guard));
    line(&h, "");
    line(&h, "#include <revolution/types.h>");
    line(&h, "");
    AvpStrVec used = {0};
    for (size_t i = 0; i < n; i++) {
        if (gen[i]) continue;
        for (size_t k = 0; k < dems[i].used.n; k++)
            if (!in_list(&used, avp_cz(a, dems[i].used.v[k])) && !in_list(&own_classes, avp_cz(a, dems[i].used.v[k])))
                avp_vec_push(&used, dems[i].used.v[k]);
    }
    avp_sort_strs(used.v, used.n);
    for (size_t i = 0; i < used.n; i++) line(&h, avp_sprintf(a, "class %.*s;", (int)used.v[i].n, used.v[i].p));
    if (used.n) line(&h, "");
    for (size_t c = 0; c < own_classes.n; c++) {
        const char *cname = avp_cz(a, own_classes.v[c]);
        line(&h, "/*");
        line(&h, avp_sprintf(a, " * TODO: class %s: base class, members and vtable are not recovered yet. Declared from",
                             cname));
        line(&h, " * the mangled names of the unit's functions (return types are unknown and shown as void).");
        line(&h, " */");
        line(&h, avp_sprintf(a, "class %s {", cname));
        line(&h, "public:");
        for (size_t i = 0; i < n; i++) {
            const UnitDemangled *d = &dems[i];
            if (gen[i] || strcmp(d->cls, cname) || !d->mangled) continue;
            const char *tag = has(vtable_names, funcs->v[i]->name) ? "  // in a vtable-like data object" : "";
            if (!d->has_args || d->complex) line(&h, avp_sprintf(a, "    // TODO: %s", funcs->v[i]->name));
            else line(&h, avp_sprintf(a, "    %s;%s", unit_sig_text(a, d, false), tag));
        }
        line(&h, "};");
        line(&h, "");
    }
    bool any_free = false;
    for (size_t i = 0; i < n; i++) {
        const UnitDemangled *d = &dems[i];
        if (gen[i] || !d->mangled || *d->cls || !d->has_args || d->complex) continue;
        line(&h, avp_sprintf(a, "%s;", unit_sig_text(a, d, true)));
        any_free = true;
    }
    if (any_free) line(&h, "");
    bool any_ext = false;
    for (size_t i = 0; i < n; i++)
        if (!gen[i] && !dems[i].mangled) {
            if (!any_ext) line(&h, "extern \"C\" {");
            line(&h, avp_sprintf(a, "void %s();", dems[i].base));
            any_ext = true;
        }
    if (any_ext) {
        line(&h, "}");
        line(&h, "");
    }
    line(&h, "#endif");
    AvpStr ht = avp_str_join(a, AVP_S("\n"), h.v, h.n);
    *header = avp_sprintf(a, "%.*s\n", (int)ht.n, ht.p);
    avp_vec_free(&h);
    return source;
}

const char *unit_split_entry_text(AvpArena *a, const char *path, const UnitRanges *ranges) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "%s:", path);
    for (int i = 0; i < 10; i++) {
        const UnitRange *r = unit_ranges_get(ranges, UNIT_SPLIT_SECTIONS[i]);
        if (r) avp_buf_printf(&b, "\n\t%-11s start:0x%08llX end:0x%08llX", UNIT_SPLIT_SECTIONS[i], r->lo, r->hi);
    }
    avp_buf_putc(&b, '\n');
    return avp_buf_cz(&b);
}

size_t unit_infer_lib(AvpArena *a, const UnitConfig *cfg, const char *path, const char *forced) {
    if (forced) {
        for (size_t k = 0; k < cfg->libs.n; k++)
            if (!strcmp(cfg->libs.v[k].name, forced)) return k;
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_putz(&b, "");
        for (size_t k = 0; k < cfg->libs.n; k++) avp_buf_printf(&b, "%s%s", k ? ", " : "", cfg->libs.v[k].name);
        unit_fail("--lib %s: no such library in configure.py (have: %s)", forced, avp_buf_cz(&b));
    }
    AvpStrVec parts = avp_str_split(avp_str(path), AVP_S("/"), -1);
    size_t ndir = parts.n ? parts.n - 1 : 0;
    /* A library's root is the directory prefix all its objects share; the new path must lie
       under it. The longest root wins (first library on a tie). */
    size_t best = SIZE_MAX, best_len = 0;
    for (size_t lib = 0; lib < cfg->libs.n; lib++) {
        AvpStrVec root = {0};
        bool have = false;
        size_t rlen = 0;
        for (size_t i = 0; i < cfg->objects.n; i++) {
            if (cfg->objects.v[i].lib != lib) continue;
            AvpStrVec op = avp_str_split(avp_str(cfg->objects.v[i].path), AVP_S("/"), -1);
            size_t odir = op.n ? op.n - 1 : 0;
            if (!have) {
                avp_vec_free(&root);
                root = op;
                rlen = odir;
                have = true;
                continue;
            }
            size_t n = 0;
            while (n < rlen && n < odir && avp_str_eq(root.v[n], op.v[n])) n++;
            rlen = n;
            avp_vec_free(&op);
        }
        if (have && rlen > 0 && rlen <= ndir && rlen > best_len) {
            size_t k = 0;
            while (k < rlen && avp_str_eq(parts.v[k], root.v[k])) k++;
            if (k == rlen) {
                best = lib;
                best_len = rlen;
            }
        }
        if (have) avp_vec_free(&root);
    }
    if (best == SIZE_MAX) {
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_putz(&b, "");
        if (ndir >= 2 && avp_str_eq(parts.v[0], AVP_S("revolution")))
            avp_buf_printf(&b, "; for a new SDK module add an RVLSDKLib(\"%.*s\") entry to configure.py and pass --lib %.*s",
                           (int)parts.v[1].n, parts.v[1].p, (int)parts.v[1].n, parts.v[1].p);
        avp_vec_free(&parts);
        unit_fail("cannot infer the library of %s: no library covers its directory%s; pass --lib NAME", path, avp_buf_cz(&b));
    }
    avp_vec_free(&parts);
    return best;
}

static AvpStr line_for(AvpStr text, size_t start) {
    i64 nl = avp_str_rfind(text, AVP_S("\n"), 0, (i64)start);
    return avp_str_slice(text, nl + 1, (i64)start);
}

AvpStr unit_insert_object(AvpArena *a, const UnitConfig *cfg, const UnitSplits *splits, size_t lib, const char *path,
                          i64 start) {
    AvpStr text = cfg->text;
    AVP_VEC(const UnitObj *) members = {0};
    for (size_t i = 0; i < cfg->objects.n; i++)
        if (cfg->objects.v[i].lib == lib) avp_vec_push(&members, &cfg->objects.v[i]);
    if (!members.n) unit_fail("library %s has no Object entry to place the new one next to", cfg->libs.v[lib].name);
    i64 *addr = AVP_NEWN(a, i64, members.n);
    bool *has_addr = AVP_NEWN(a, bool, members.n);
    for (size_t i = 0; i < members.n; i++) {
        const UnitSplit *s = NULL;
        for (size_t k = 0; k < splits->n; k++) /* {s.path: s}: the last split of that path */
            if (!strcmp(splits->v[k].path, members.v[i]->path)) s = &splits->v[k];
        const UnitRange *t = s ? unit_ranges_get(&s->ranges, ".text") : NULL;
        if (t) {
            addr[i] = t->lo;
            has_addr[i] = true;
        }
    }
    /* Python keys addr by path: a path listed twice shares one address */
    for (size_t i = 0; i < members.n; i++)
        for (size_t k = 0; k < members.n; k++)
            if (has_addr[k] && !strcmp(members.v[i]->path, members.v[k]->path)) {
                addr[i] = addr[k];
                has_addr[i] = true;
            }
    const UnitObj *ref = NULL;
    for (size_t i = 0; i < members.n; i++)
        if (has_addr[i] && addr[i] < start) {
            if (!ref) ref = members.v[i];
            else {
                size_t r = 0;
                while (members.v[r] != ref) r++;
                if (addr[i] > addr[r]) ref = members.v[i];
            }
        }
    size_t pos;
    AvpStr indent;
    if (ref) {
        indent = line_for(text, ref->start);
        if (avp_str_strip(indent).n) indent = AVP_S("            ");
        pos = ref->end;
    } else {
        bool after = false;
        for (size_t i = 0; i < members.n; i++)
            if (has_addr[i]) {
                if (!after) {
                    ref = members.v[i];
                    after = true;
                } else {
                    size_t r = 0;
                    while (members.v[r] != ref) r++;
                    if (addr[i] < addr[r]) ref = members.v[i];
                }
            }
        if (!after) ref = members.v[members.n - 1];
        indent = line_for(text, ref->start);
        if (avp_str_strip(indent).n) indent = AVP_S("            ");
        pos = after ? (size_t)(avp_str_rfind(text, AVP_S("\n"), 0, (i64)ref->start) + 1) : ref->end;
        while (after) { /* keep comments attached to the object below */
            if (pos == 0) break;
            size_t pl = (size_t)(avp_str_rfind(text, AVP_S("\n"), 0, (i64)pos - 1) + 1);
            if (avp_str_startswith(avp_str_strip(avp_str_slice(text, (i64)pl, (i64)pos)), AVP_S("#"))) pos = pl;
            else break;
        }
    }
    avp_vec_free(&members);
    return avp_str(avp_sprintf(a, "%.*s%.*sObject(NonMatching, \"%s\"),\n%.*s", (int)pos, text.p, (int)indent.n,
                               indent.p, path, (int)(text.n - pos), text.p + pos));
}

const UnitSplit *unit_data_reader(const UnitIndex *x, const UnitSplits *splits, const AvpStrVec *includes) {
    AvpMap *score = avp_map_new();
    for (size_t i = 0; i < includes->n; i++) {
        const UnitBlockVec *r = unit_index_referrers(x, avp_cz(avp_scratch(), includes->v[i]));
        for (size_t k = 0; k < r->n; k++) {
            if (strcmp(r->v[k]->kind, "fn")) continue;
            for (size_t s = 0; s < splits->n; s++) {
                const UnitRange *t = unit_ranges_get(&splits->v[s].ranges, ".text");
                if (t && t->lo <= r->v[k]->addr && r->v[k]->addr < t->hi)
                    avp_map_put(score, avp_str(splits->v[s].path), NULL)->i += 1;
            }
        }
    }
    AvpMapIter it = {0};
    AvpStr best = {0};
    i64 bs = 0;
    bool any = false;
    while (avp_map_next(score, &it))
        if (!any || it.val->i > bs) {
            best = it.key;
            bs = it.val->i;
            any = true;
        }
    const UnitSplit *res = NULL;
    for (size_t s = 0; any && s < splits->n && !res; s++)
        if (avp_str_eqz(best, splits->v[s].path)) res = &splits->v[s];
    avp_map_free(score);
    return res;
}

bool unit_data_only_position(const UnitSplits *splits, const UnitRanges *ranges, const UnitSplit *reader, size_t *line,
                             const char **why) {
    size_t n = splits->n;
    size_t first = reader ? (size_t)(reader - splits->v) + 1 : n;
    *why = NULL;
    UnitSizeVec order = {0};
    for (size_t i = first; i <= n; i++) avp_vec_push(&order, i);
    for (size_t i = first; i-- > 0;) avp_vec_push(&order, i);
    for (size_t oi = 0; oi < order.n; oi++) {
        size_t i = order.v[oi];
        bool ok = true;
        for (size_t r = 0; r < ranges->n && ok; r++) {
            const UnitRange *rg = &ranges->v[r];
            for (size_t s = 0; s < n && ok; s++) {
                const UnitRange *o = unit_ranges_get(&splits->v[s].ranges, rg->sec);
                if (!o) continue;
                if (s < i ? o->hi > rg->lo : o->lo < rg->hi) ok = false;
            }
        }
        if (ok) {
            avp_vec_free(&order);
            if (i < n) {
                *line = splits->v[i].first_line;
                return true;
            }
            return false;
        }
    }
    avp_vec_free(&order);
    *why = "no place in splits.txt keeps every section's ranges ascending: the entry was put after the reader, move "
           "it by hand";
    if (first < n) {
        *line = splits->v[first].first_line;
        return true;
    }
    return false;
}

AvpStr unit_splits_with_entry(AvpArena *a, AvpStr stext, const char *entry, bool has_pos, size_t pos) {
    if (!has_pos) {
        AvpStr body = avp_str_rstrip_chars(stext, AVP_S("\n"));
        return avp_str(avp_sprintf(a, "%.*s\n\n%s\n", (int)body.n, body.p, entry));
    }
    AvpStrVec lines = avp_str_split(stext, AVP_S("\n"), -1);
    size_t p = pos < lines.n ? pos : lines.n;
    AvpStr head = avp_str_join(a, AVP_S("\n"), lines.v, p);
    AvpStr tail = avp_str_join(a, AVP_S("\n"), lines.v + p, lines.n - p);
    AvpStr res = avp_str(avp_sprintf(a, "%.*s%s%s\n%.*s", (int)head.n, head.p, pos ? "\n" : "", entry, (int)tail.n,
                                     tail.p));
    avp_vec_free(&lines);
    return res;
}
