/* names.exe attr: change the attributes of one symbols.txt line (scope, size, align), and move the unit's section end
 * in splits.txt when the last symbol of that section shrinks. A plan like rename's: the edited files are planned with
 * their line edits, conflicts refuse the whole thing, names_write_plan writes it. */
#include "names.h"
#include "names_priv.h"

#include "avp/fmt.h"

#include <ctype.h>
#include <string.h>

static const char *rule_tag(AvpArena *a, const char *rule, const char *lift) {
    if (lift) return avp_sprintf(a, "[rule %s; lifted by %s]", rule, lift);
    return avp_sprintf(a, "[rule %s; no flag lifts it]", rule);
}

static void push(AvpArena *a, AvpStrVec *v, const char *s) { avp_vec_push(v, avp_str_dup(a, avp_str(s))); }

static AvpStr rstrip_cr(AvpStr s) {
    while (s.n && s.p[s.n - 1] == '\r') s.n--;
    return s;
}

static AvpStr join_lines(AvpArena *a, const AvpStrVec *lines) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < lines->n; i++) {
        if (i) avp_buf_putc(&b, '\n');
        avp_buf_put(&b, lines->v[i]);
    }
    return avp_str_dup(a, avp_buf_str(&b));
}

/* ---- the line rewrite ---- */

#define MAX_TOK 64

/* raw with `key:value` set among the comment tokens: an existing key:... token is replaced in place; else the new token
 * goes after the first anchor prefix that is present (symbols.txt order: type size scope align data hidden), else at
 * the end. */
static const char *set_token(AvpArena *a, const char *raw, const char *key, const char *value,
                             const char *const *anchors) {
    size_t klen = strlen(key);
    const char *cm = strstr(raw, "//");
    const char *tok = avp_sprintf(a, "%s:%s", key, value);
    if (!cm) return avp_sprintf(a, "%s // %s", raw, tok);
    const char *ts[MAX_TOK];
    size_t tl[MAX_TOK], n = 0;
    for (const char *p = cm + 2; *p && n < MAX_TOK;) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        ts[n] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        tl[n] = (size_t)(p - ts[n]);
        n++;
    }
    for (size_t i = 0; i < n; i++)
        if (tl[i] > klen && strncmp(ts[i], key, klen) == 0 && ts[i][klen] == ':')
            return avp_sprintf(a, "%.*s%s%s", (int)(ts[i] - raw), raw, tok, ts[i] + tl[i]);
    for (size_t k = 0; anchors && anchors[k]; k++) {
        size_t al = strlen(anchors[k]);
        for (size_t i = 0; i < n; i++)
            if (tl[i] >= al && strncmp(ts[i], anchors[k], al) == 0) {
                const char *end = ts[i] + tl[i];
                return avp_sprintf(a, "%.*s %s%s", (int)(end - raw), raw, tok, end);
            }
    }
    size_t len = strlen(raw);
    while (len && (raw[len - 1] == ' ' || raw[len - 1] == '\t')) len--;
    return avp_sprintf(a, "%.*s %s", (int)len, raw, tok);
}

/* ---- splits.txt ---- */

/* The `section` line in the block of unit `file`, rewritten with end:0x<new_end> in *lines; false when absent. */
static bool splits_end_edit(AvpArena *a, AvpStrVec *lines, const char *file, const char *section, u64 new_end,
                            NamesEdit *edit) {
    size_t fl = strlen(file);
    bool in_unit = false;
    for (size_t i = 0; i < lines->n; i++) {
        AvpStr l = rstrip_cr(lines->v[i]);
        bool indented = l.n && (l.p[0] == '\t' || l.p[0] == ' ');
        if (!indented) {
            in_unit = l.n > fl && memcmp(l.p, file, fl) == 0 && l.p[fl] == ':';
            continue;
        }
        if (!in_unit) continue;
        const char *z = avp_cz(a, l);
        const char *p = z;
        while (*p == ' ' || *p == '\t') p++;
        size_t sl = strlen(section);
        if (strncmp(p, section, sl) != 0 || (p[sl] != ' ' && p[sl] != '\t')) continue;
        const char *e = strstr(p, "end:0x");
        if (!e) return false;
        const char *q = e + 6;
        while (isxdigit((unsigned char)*q)) q++;
        const char *nl = avp_sprintf(a, "%.*send:0x%08llX%s", (int)(e - z), z, (unsigned long long)new_end, q);
        bool cr = avp_str_endswith(lines->v[i], AVP_S("\r"));
        edit->line = i + 1;
        edit->old = avp_str_dup(a, l);
        edit->new_ = avp_str_dup(a, avp_str(nl));
        lines->v[i] = avp_str_dup(a, avp_str(cr ? avp_sprintf(a, "%s\r", nl) : nl));
        return true;
    }
    return false;
}

/* ---- the plan ---- */

static bool is_local_sym(const ShsmSymbol *s) { return s->scope && strcmp(s->scope, "local") == 0; }

static bool is_label(const ShsmSymbol *s) { return s->type && strcmp(s->type, "label") == 0; }

static bool parse_address(const char *t, u64 *out) {
    if (t[0] != '0' || (t[1] != 'x' && t[1] != 'X') || !t[2]) return false;
    u64 v = 0;
    for (const char *p = t + 2; *p; p++) {
        if (!isxdigit((unsigned char)*p)) return false;
        v = v * 16 + (u64)(isdigit((unsigned char)*p) ? *p - '0' : (tolower((unsigned char)*p) - 'a' + 10));
    }
    *out = v;
    return true;
}

static const char *unit_of(const NamesCtx *cx, const ShsmSymbol *s) {
    const char *u = shsm_sym_unit_of(cx->st, s->address);
    return u ? u : "(no unit)";
}

/* The symbol `ref` names: a name, or an address 0x8XXXXXXX; a name that several symbols carry is a conflict. */
static const ShsmSymbol *pick_symbol(NamesCtx *cx, NamesPlan *p, const char *ref) {
    AvpArena *a = cx->a;
    AvpPtrVec cands = {0};
    u64 addr;
    if (parse_address(ref, &addr)) {
        AvpPtrVec at = {0};
        shsm_sym_at(cx->st, addr, NULL, &at);
        bool real = false;
        for (size_t i = 0; i < at.n; i++)
            if (!is_label(at.v[i])) real = true;
        for (size_t i = 0; i < at.n; i++)
            if (!(real && is_label(at.v[i]))) avp_vec_push(&cands, at.v[i]);
        avp_vec_free(&at);
    } else {
        names_all_by_name(cx, ref, &cands);
    }
    if (!cands.n)
        shsm_raise(SHSM_EXC_USAGE, "no symbol named %s in symbols.txt", avp_cz(a, avp_repr_str(a, avp_str(ref))));
    const ShsmSymbol *s = cands.v[0];
    if (cands.n > 1) {
        AvpBuf b;
        avp_buf_init(&b, a);
        for (size_t i = 0; i < cands.n; i++) {
            const ShsmSymbol *c = cands.v[i];
            avp_buf_putz(&b, avp_sprintf(a, "%s0x%08llX in %s", i ? ", " : "", (unsigned long long)c->address,
                                         unit_of(cx, c)));
        }
        push(a, &p->conflicts,
             avp_sprintf(a, "%zu symbols are named %s (%s): name the one to change by its address %s", cands.n, ref,
                         avp_buf_cz(&b), rule_tag(a, "ambiguous-name", "the address form 0x8XXXXXXX")));
        s = NULL;
    }
    avp_vec_free(&cands);
    return s;
}

NamesPlan *names_plan_attr(NamesCtx *cx, const NamesAttrOpts *o) {
    AvpArena *a = cx->a;
    NamesPlan *p = names_plan_new(cx);
    if (!o->scope && !o->has_size && !o->has_align)
        shsm_raise(SHSM_EXC_USAGE, "attr: give at least one of --scope, --size, --align");
    if (o->scope && strcmp(o->scope, "global") && strcmp(o->scope, "local") && strcmp(o->scope, "weak"))
        shsm_raise(SHSM_EXC_USAGE, "attr: --scope is global, local or weak, not %s", o->scope);
    if (o->has_size && o->size <= 0) shsm_raise(SHSM_EXC_USAGE, "attr: --size must be positive");
    if (o->has_align && (o->align <= 0 || (o->align & (o->align - 1))))
        shsm_raise(SHSM_EXC_USAGE, "attr: --align must be a power of two");
    const ShsmSymbol *s = pick_symbol(cx, p, o->symbol);
    if (!s) return p;
    const char *line = s->raw;
    u64 old_end = shsm_symbol_end(s);
    const ShsmSpan *span = cx->st->splits ? shsm_splits_range_of(cx->st->splits, s->address) : NULL;
    if (span && strcmp(span->section, s->section) != 0) span = NULL;
    const ShsmSplitUnit *su = span ? shsm_splits_unit(cx->st->splits, span->unit) : NULL;
    AvpStr splits_text = {0};
    NamesEdit splits_edit = {0};

    if (o->scope) {
        const char *cur = s->scope ? s->scope : "global";
        if (strcmp(cur, o->scope) != 0) {
            if (strcmp(o->scope, "local") == 0) {
                AvpPtrVec same = {0};
                names_all_by_name(cx, s->name, &same);
                const char *us = shsm_sym_unit_of(cx->st, s->address);
                for (size_t i = 0; i < same.n; i++) {
                    const ShsmSymbol *x = same.v[i];
                    if (x == s) continue;
                    const char *ux = shsm_sym_unit_of(cx->st, x->address);
                    if (is_local_sym(x) && us && ux && strcmp(us, ux) == 0)
                        push(a, &p->conflicts,
                             avp_sprintf(a, "two scope:local symbols named %s would sit in unit %s (0x%08llX and "
                                            "0x%08llX) %s",
                                         s->name, us, (unsigned long long)s->address, (unsigned long long)x->address,
                                         rule_tag(a, "local-in-unit", NULL)));
                    else if (!is_local_sym(x))
                        push(a, &p->notes,
                             avp_sprintf(a, "%s also exists as a %s symbol at 0x%08llX (%s): a reference by name is "
                                            "ambiguous once this one is local",
                                         s->name, x->scope ? x->scope : "global", (unsigned long long)x->address,
                                         unit_of(cx, x)));
                }
                avp_vec_free(&same);
            }
            line = set_token(a, line, "scope", o->scope, (const char *const[]){"size:", "type:", NULL});
        }
    }
    if (o->has_size && (!s->has_size || (u64)o->size != s->size)) {
        u64 new_end = s->address + (u64)o->size;
        size_t n = 0;
        bool move_end = false; /* --resplit: the unit's section end moves to new_end */
        if (new_end > old_end) {
            u64 from = old_end > s->address ? old_end : s->address + 1;
            ShsmSymbol *const *hit = shsm_sym_in_range(cx->st, from, new_end, &n);
            for (size_t i = 0; i < n; i++)
                push(a, &p->conflicts,
                     avp_sprintf(a, "growing %s to 0x%llX would run into %s at 0x%08llX %s", s->name,
                                 (unsigned long long)o->size, hit[i]->name, (unsigned long long)hit[i]->address,
                                 rule_tag(a, "size-grows-into-next", NULL)));
            if (span && new_end > span->end) {
                bool crosses = false; /* another unit's range in the way */
                for (size_t k = 0; k < cx->st->splits->nspans; k++) {
                    const ShsmSpan *o2 = &cx->st->splits->spans[k];
                    if (o2 != span && o2->start < new_end && o2->end > span->end && o2->start >= span->end) crosses = true;
                }
                if (!o->resplit || crosses || !su)
                    push(a, &p->conflicts,
                         avp_sprintf(a, "growing %s to 0x%llX ends at 0x%08llX, past the end of its %s range in unit "
                                        "%s (0x%08llX-0x%08llX); the range would need end 0x%08llX: use unit.exe "
                                        "resplit%s %s",
                                     s->name, (unsigned long long)o->size, (unsigned long long)new_end, span->section,
                                     span->unit, (unsigned long long)span->start, (unsigned long long)span->end,
                                     (unsigned long long)new_end,
                                     crosses ? " (another unit's range follows it)" : ", or pass --resplit to move the "
                                                                                       "section end",
                                     rule_tag(a, "size-past-unit-end", crosses ? NULL : "--resplit")));
                else
                    move_end = true;
            }
        } else {
            ShsmSymbol *const *hit = shsm_sym_in_range(cx->st, new_end, old_end, &n);
            for (size_t i = 0; i < n; i++)
                push(a, &p->conflicts,
                     avp_sprintf(a, "shrinking %s to 0x%llX frees 0x%08llX-0x%08llX, which holds %s at 0x%08llX %s",
                                 s->name, (unsigned long long)o->size, (unsigned long long)new_end,
                                 (unsigned long long)old_end, hit[i]->name, (unsigned long long)hit[i]->address,
                                 rule_tag(a, "freed-range-not-empty", NULL)));
            if (span && su && span->end == old_end && !p->conflicts.n) {
                /* the last symbol of the unit's section: the section end would fall past the symbol */
                if (!o->resplit)
                    push(a, &p->conflicts,
                         avp_sprintf(a, "shrinking %s to 0x%llX leaves the %s range of unit %s (0x%08llX-0x%08llX) "
                                        "ending 0x%llX past the symbol; the range would need end 0x%08llX: use "
                                        "unit.exe resplit, or pass --resplit to move the section end %s",
                                     s->name, (unsigned long long)o->size, span->section, span->unit,
                                     (unsigned long long)span->start, (unsigned long long)span->end,
                                     (unsigned long long)(span->end - new_end), (unsigned long long)new_end,
                                     rule_tag(a, "section-end-moves", "--resplit")));
                else
                    move_end = true;
            }
        }
        if (move_end && !p->conflicts.n) {
            AvpStrVec sl = names_split_nl(names_read_text(a, cx->repo->splits_txt));
            if (splits_end_edit(a, &sl, su->file, span->section, new_end, &splits_edit)) {
                splits_text = join_lines(a, &sl);
                push(a, &p->notes,
                     avp_sprintf(a, "--resplit: the %s range of unit %s moves its end from 0x%08llX to 0x%08llX",
                                 span->section, span->unit, (unsigned long long)span->end,
                                 (unsigned long long)new_end));
            } else {
                push(a, &p->conflicts,
                     avp_sprintf(a, "splits.txt has no %s line with end: in the block of %s %s", span->section,
                                 su->file, rule_tag(a, "splits-line-missing", NULL)));
            }
            avp_vec_free(&sl);
        }
        line = set_token(a, line, "size", avp_sprintf(a, "0x%llX", (unsigned long long)o->size),
                         (const char *const[]){"type:", NULL});
    }
    if (o->has_align && (!s->has_align || s->align != o->align))
        line = set_token(a, line, "align", avp_sprintf(a, "%lld", (long long)o->align),
                         (const char *const[]){"scope:", "size:", "type:", NULL});
    if (p->conflicts.n) return p;
    /* symbols.txt, then splits.txt */
    if (strcmp(line, s->raw) != 0) {
        AvpStrVec lines = names_split_nl(names_read_text(a, cx->repo->symbols_txt));
        size_t i = s->lineno - 1;
        if (s->lineno >= 1 && i < lines.n && avp_str_eqz(rstrip_cr(lines.v[i]), s->raw)) {
            bool cr = avp_str_endswith(lines.v[i], AVP_S("\r"));
            NamesEdit e = {s->lineno, avp_str_dup(a, avp_str(s->raw)), avp_str_dup(a, avp_str(line))};
            lines.v[i] = avp_str_dup(a, avp_str(cr ? avp_sprintf(a, "%s\r", line) : line));
            NamesFile *f = names_plan_file_add(p, shsm_repo_rel(a, cx->repo, cx->repo->symbols_txt));
            avp_vec_push(&f->edits, e);
            f->text = join_lines(a, &lines);
        } else {
            push(a, &p->conflicts,
                 avp_sprintf(a, "symbols.txt line %zu is not the line of %s (file changed?)", s->lineno, s->name));
        }
        avp_vec_free(&lines);
    }
    if (splits_text.p && !p->conflicts.n) {
        NamesFile *f = names_plan_file_add(p, shsm_repo_rel(a, cx->repo, cx->repo->splits_txt));
        avp_vec_push(&f->edits, splits_edit);
        f->text = splits_text;
    }
    if (p->conflicts.n) p->files.n = 0;
    return p;
}

/* ---- names.exe add ---- */

NamesPlan *names_plan_add(NamesCtx *cx, const NamesAddOpts *o) {
    AvpArena *a = cx->a;
    NamesPlan *p = names_plan_new(cx);
    u64 addr, size;
    if (!o->name || !*o->name) shsm_raise(SHSM_EXC_USAGE, "add: give the symbol name");
    if (!o->section || o->section[0] != '.' || !o->section[1])
        shsm_raise(SHSM_EXC_USAGE, "add: --section is a section name such as .bss or .data");
    if (!o->address || !parse_address(o->address, &addr))
        shsm_raise(SHSM_EXC_USAGE, "add: --address is a hexadecimal address 0x8XXXXXXX");
    if (!o->size || !parse_address(o->size, &size) || !size)
        shsm_raise(SHSM_EXC_USAGE, "add: --size is a positive hexadecimal size 0xN");
    const char *type = o->type ? o->type : "object";
    if (strcmp(type, "object") && strcmp(type, "function"))
        shsm_raise(SHSM_EXC_USAGE, "add: --type is object or function, not %s", type);
    if (o->scope && strcmp(o->scope, "global") && strcmp(o->scope, "local") && strcmp(o->scope, "weak"))
        shsm_raise(SHSM_EXC_USAGE, "add: --scope is global, local or weak, not %s", o->scope);
    if (o->has_align && (o->align <= 0 || (o->align & (o->align - 1))))
        shsm_raise(SHSM_EXC_USAGE, "add: --align must be a power of two");
    u64 end = addr + size;

    AvpPtrVec at = {0};
    shsm_sym_at(cx->st, addr, NULL, &at);
    for (size_t i = 0; i < at.n; i++) {
        const ShsmSymbol *x = at.v[i];
        push(a, &p->conflicts,
             avp_sprintf(a, "address 0x%08llX is already named %s (%s:0x%08llX, size 0x%llX) %s",
                         (unsigned long long)addr, x->name, x->section, (unsigned long long)x->address,
                         (unsigned long long)(x->has_size ? x->size : 0), rule_tag(a, "address-named", NULL)));
    }
    avp_vec_free(&at);
    const ShsmSymbol *under = shsm_sym_containing(cx->st, addr);
    if (under && under->address < addr)
        push(a, &p->conflicts,
             avp_sprintf(a, "0x%08llX lies inside %s (0x%08llX-0x%08llX): shrink it first with names.exe attr %s "
                            "--size 0x%llX %s",
                         (unsigned long long)addr, under->name, (unsigned long long)under->address,
                         (unsigned long long)shsm_symbol_end(under), under->name,
                         (unsigned long long)(addr - under->address), rule_tag(a, "overlaps-neighbour", NULL)));
    size_t nin = 0;
    ShsmSymbol *const *inside = shsm_sym_in_range(cx->st, addr + 1, end, &nin);
    for (size_t i = 0; i < nin; i++)
        push(a, &p->conflicts,
             avp_sprintf(a, "the range 0x%08llX-0x%08llX runs into %s at 0x%08llX: give --size 0x%llX at most %s",
                         (unsigned long long)addr, (unsigned long long)end, inside[i]->name,
                         (unsigned long long)inside[i]->address, (unsigned long long)(inside[i]->address - addr),
                         rule_tag(a, "overlaps-neighbour", NULL)));
    const ShsmSpan *span = cx->st->splits ? shsm_splits_range_of(cx->st->splits, addr) : NULL;
    if (!span || strcmp(span->section, o->section) != 0 || end > span->end)
        push(a, &p->conflicts,
             avp_sprintf(a, "0x%08llX-0x%08llX is outside the %s ranges of splits.txt%s %s", (unsigned long long)addr,
                         (unsigned long long)end, o->section,
                         span ? avp_sprintf(a, " (the range there is %s in unit %s, 0x%08llX-0x%08llX)", span->section,
                                            span->unit, (unsigned long long)span->start,
                                            (unsigned long long)span->end)
                              : "",
                         rule_tag(a, "outside-section", NULL)));
    AvpPtrVec same = {0};
    names_all_by_name(cx, o->name, &same);
    bool local = o->scope && strcmp(o->scope, "local") == 0;
    for (size_t i = 0; i < same.n; i++)
        if (!local || !is_local_sym(same.v[i]))
            push(a, &p->conflicts,
                 avp_sprintf(a, "a symbol named %s already exists at 0x%08llX %s", o->name,
                             (unsigned long long)((const ShsmSymbol *)same.v[i])->address,
                             rule_tag(a, "name-exists", "--scope local for a per-unit static")));
    avp_vec_free(&same);

    /* the line goes after the symbol of the section just below the address (else before the one just above) and takes
     * that neighbour's line terminator */
    const ShsmSymbol *prev = NULL, *next = NULL;
    for (size_t i = 0; i < cx->st->n; i++) {
        const ShsmSymbol *s = &cx->st->symbols[i];
        if (strcmp(s->section, o->section) != 0) continue;
        if (s->address < addr) {
            if (!prev || s->address > prev->address || (s->address == prev->address && s->lineno > prev->lineno)) prev = s;
        } else if (s->address > addr) {
            if (!next || s->address < next->address || (s->address == next->address && s->lineno < next->lineno)) next = s;
        }
    }
    if (!prev && !next && !p->conflicts.n)
        push(a, &p->conflicts,
             avp_sprintf(a, "symbols.txt has no symbol in %s to place the line by %s", o->section,
                         rule_tag(a, "empty-section", NULL)));
    if (p->conflicts.n) return p;

    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "%s = %s:0x%08llX; // type:%s size:0x%llX", o->name, o->section, (unsigned long long)addr, type,
                   (unsigned long long)size);
    if (o->scope) avp_buf_printf(&b, " scope:%s", o->scope);
    if (o->has_align) avp_buf_printf(&b, " align:%lld", (long long)o->align);
    if (o->data && *o->data) avp_buf_printf(&b, " data:%s", o->data);
    const char *line = avp_buf_cz(&b);

    AvpStrVec lines = names_split_nl(names_read_text(a, cx->repo->symbols_txt));
    size_t at_idx = prev ? prev->lineno : next->lineno - 1; /* the new line's 0-based index */
    const ShsmSymbol *form = prev ? prev : next;
    if (at_idx > lines.n || form->lineno < 1 || form->lineno > lines.n ||
        !avp_str_eqz(rstrip_cr(lines.v[form->lineno - 1]), form->raw)) {
        push(a, &p->conflicts, avp_sprintf(a, "symbols.txt line %zu is not the line of %s (file changed?)",
                                           form->lineno, form->name));
        return p;
    }
    bool cr = avp_str_endswith(lines.v[form->lineno - 1], AVP_S("\r"));
    AvpStrVec out = {0};
    for (size_t i = 0; i < lines.n; i++) {
        if (i == at_idx) avp_vec_push(&out, avp_str_dup(a, avp_str(cr ? avp_sprintf(a, "%s\r", line) : line)));
        avp_vec_push(&out, lines.v[i]);
    }
    if (at_idx == lines.n) avp_vec_push(&out, avp_str_dup(a, avp_str(cr ? avp_sprintf(a, "%s\r", line) : line)));
    NamesEdit e = {at_idx + 1, AVP_S(""), avp_str_dup(a, avp_str(line))};
    NamesFile *f = names_plan_file_add(p, shsm_repo_rel(a, cx->repo, cx->repo->symbols_txt));
    avp_vec_push(&f->edits, e);
    f->text = join_lines(a, &out);
    avp_vec_free(&lines);
    avp_vec_free(&out);
    push(a, &p->notes, avp_sprintf(a, "the line goes %s %s (line %zu)", prev ? "after" : "before", form->name, e.line));
    return p;
}
