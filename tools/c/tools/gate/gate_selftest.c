/* gate_selftest: --selftest, the proof that the gate can fail (run_selftest and dtk_check of tools/gate.py). */
#include "gate.h"
#include "shsm_dol.h"
#include "shsm_repo.h"
#include "shsm_symbols.h"

static void print_line(AvpBuf *out, const char *text) {
    if (out) {
        avp_buf_putz(out, text);
        avp_buf_putc(out, '\n');
    } else {
        fputs(text, stdout);
        fputc('\n', stdout);
    }
}

/* dtk shasum -c on a scratch contract naming path: true when dtk accepts it. */
static bool dtk_check(AvpArena *a, GateSelftest *s, const char *dtk, const char *path, const char *sha) {
    const char *d = avp_cz(a, avp_path_dirname(path));
    AvpStr base = avp_path_basename(path);
    const char *contract_name = avp_sprintf(a, "%.*s.sha1", AVP_SF(base));
    const char *contract = avp_path_join(a, d, contract_name);
    if (!avp_write_text(contract, avp_str(avp_sprintf(a, "%s  %.*s\n", sha, AVP_SF(base))), AVP_TM_UTF8_NL_LF))
        shsm_raise_io();
    GateRun r = s->runner(s->runner_ctx, a, (const char *const[]){dtk, "shasum", "-c", contract_name}, 4, d, NULL, 0);
    AvpStr want = avp_str(avp_sprintf(a, "%.*s: OK", AVP_SF(base)));
    return gate_run_ok(&r) && avp_str_contains(gate_plain(a, gate_run_output(a, &r)), want);
}

typedef struct {
    AvpArena *a;
    AvpBuf *out;
    size_t n, failed;
    const char *first_failed;
} Checks;

static void check(Checks *c, const char *name, bool cond, const char *detail) {
    c->n++;
    if (!cond && !c->failed++) c->first_failed = name;
    print_line(c->out, avp_sprintf(c->a, "[%s] %s%s%s", cond ? "ok  " : "FAIL", name, (detail && *detail) ? ": " : "",
                                   (detail && *detail) ? detail : ""));
}

/* the scratch contract of the preflight checks */
static bool scratch(AvpArena *a, const char *cfg, const char *cd, const char *root, const char *want,
                    const char *retail, bool edit_sha, bool edit_yml) {
    AvpStr flip = avp_str(avp_sprintf(a, "%c%s", want[0] != '0' ? '0' : '1', want + 1));
    const char *bs = avp_path_join(a, cd, "build.sha1"), *cy = avp_path_join(a, cd, "config.yml");
    for (int k = 0; k < 2; k++) {
        AvpStr t;
        if (!avp_read_text(a, avp_path_join(a, cfg, k == 0 ? "build.sha1" : "config.yml"), AVP_TM_UTF8, &t))
            shsm_raise_io();
        bool edit = k == 0 ? edit_sha : edit_yml;
        if (edit) t = avp_str_replace(a, t, avp_str(want), flip, -1);
        if (!avp_write_text(k == 0 ? bs : cy, t, AVP_TM_UTF8)) shsm_raise_io();
    }
    const char *why = NULL, *sha = NULL;
    return gate_verify_contract(a, bs, cy, root, retail, &why, &sha);
}

bool gate_run_selftest(AvpArena *a, const GateOpts *opts, const char *root, GateSelftest *s, const char **line) {
    (void)opts;
    const char *version = shsm_read_version(root);
    const char *cfg = avp_path_joinv(a, 3, (const char *const[]){root, "config", version});
    const char *why = NULL, *want = NULL;
    if (!gate_verify_contract(a, avp_path_join(a, cfg, "build.sha1"), avp_path_join(a, cfg, "config.yml"), root, NULL,
                              &why, &want))
        shsm_raise(SHSM_EXC_USAGE, "the real contract does not verify: %s", why);
    const char *base = NULL, *dol = NULL, *elf = NULL;
    const char *bases[2] = {avp_path_joinv(a, 3, (const char *const[]){root, "build", "gate"}), root};
    bases[0] = avp_path_join(a, bases[0], "tree");
    for (int i = 0; i < 2 && !base; i++) {
        const char *d = avp_path_joinv(a, 4, (const char *const[]){bases[i], "build", version, "main.dol"});
        const char *e = avp_path_joinv(a, 4, (const char *const[]){bases[i], "build", version, "main.elf"});
        if (avp_isfile(d) && avp_isfile(e) && strcmp(gate_sha1_file(a, d), want) == 0) {
            base = bases[i];
            dol = d;
            elf = e;
        }
    }
    if (!base) shsm_raise(SHSM_EXC_USAGE, "no green link (build/gate/tree or build/%s): run the gate first", version);
    ShsmRepo *rp = shsm_repo_new(base, version);
    GateUnits units = {0};
    gate_load_units_real(NULL, a, base, version, &units);
    ShsmSplits *splits = shsm_load_splits(rp);
    ShsmSymbolTable *st = shsm_load_symbols(rp, false);
    /* Matching units, game/PadRumble first, then by path */
    const GateUnit **order = AVP_NEWN(a, const GateUnit *, units.n + 1);
    size_t no = 0;
    for (size_t i = 0; i < units.n; i++)
        if (units.v[i].status && strcmp(units.v[i].status, "Matching") == 0 && strcmp(units.v[i].path, "game/PadRumble") == 0)
            order[no++] = &units.v[i];
    size_t rest = no;
    for (size_t i = 0; i < units.n; i++)
        if (units.v[i].status && strcmp(units.v[i].status, "Matching") == 0 && strcmp(units.v[i].path, "game/PadRumble") != 0)
            order[no++] = &units.v[i];
    for (size_t i = rest + 1; i < no; i++) /* insertion sort by path (stable) */
        for (size_t j = i; j > rest && strcmp(order[j - 1]->path, order[j]->path) > 0; j--) {
            const GateUnit *t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    const GateUnit *unit = NULL;
    const ShsmSymbol *sym = NULL;
    for (size_t i = 0; i < no && !sym; i++) {
        const ShsmSplitUnit *su = shsm_splits_unit(splits, order[i]->path);
        const ShsmSplitRange *rng = NULL;
        for (size_t k = 0; su && k < su->nranges && !rng; k++)
            if (strcmp(su->ranges[k].section, ".text") == 0) rng = &su->ranges[k];
        if (!rng) continue;
        size_t cnt = 0;
        ShsmSymbol *const *in = shsm_sym_in_range(st, rng->start, rng->end, &cnt);
        for (size_t k = 0; k < cnt; k++) {
            const ShsmSymbol *x = in[k];
            if (x->type && strcmp(x->type, "function") == 0 && x->has_size && x->size >= 32) {
                unit = order[i];
                sym = x;
                break;
            }
        }
    }
    if (!sym) shsm_raise(SHSM_EXC_USAGE, "no function of a Matching unit found in symbols.txt and splits.txt");
    i64 addr = (i64)sym->address + ((i64)(sym->size / 2) & ~(i64)3) + 1;
    const char *d = avp_path_joinv(a, 4, (const char *const[]){root, "build", "gate", "selftest"});
    if (!avp_makedirs(d, true)) shsm_raise_io();
    const char *clean = avp_path_join(a, d, "clean.dol"), *flipped = avp_path_join(a, d, "flipped.dol");
    if (!avp_copyfile(dol, clean)) shsm_raise_io();
    AvpBytes data = {0};
    if (!avp_read_all(a, dol, &data)) shsm_raise_io();
    ShsmDol *dl = shsm_dol_parse(a, data.v, data.n);
    const ShsmDolSegment *seg = shsm_dol_segment_at(dl, addr);
    if (!seg) shsm_raise(SHSM_EXC_USAGE, "address 0x%08llX is not in a DOL segment", (unsigned long long)addr);
    i64 off = seg->off + (addr - seg->addr);
    data.v[off] ^= 0xFF;
    if (!avp_write_file(flipped, data.v, data.n)) shsm_raise_io();
    char *rel = NULL;
    if (!avp_path_relpath(a, dol, root, &rel)) shsm_raise_io();
    print_line(s->out, avp_sprintf(a, "link: %s; flipped byte at 0x%08llX (file offset 0x%llX) in %s, unit %s", rel,
                                   (unsigned long long)addr, (unsigned long long)off, sym->name, unit->name));
    Checks c = {a, s->out, 0, 0, NULL};
    const char *dtk = avp_path_joinv(a, 4, (const char *const[]){root, "tools", "bin", "dtk.exe"});
    check(&c, "checksum accepts an unmodified copy", dtk_check(a, s, dtk, clean, want), "");
    check(&c, "checksum rejects the flipped copy", !dtk_check(a, s, dtk, flipped, want), "");
    check(&c, "hashlib agrees", strcmp(gate_sha1_file(a, clean), want) == 0 && strcmp(gate_sha1_file(a, flipped), want) != 0,
          "");
    const char *gc = avp_path_join(a, s->exe_dir ? s->exe_dir : avp_exe_dir(), "gatecheck.exe");
    GateRun r = s->runner(s->runner_ctx, a,
                          (const char *const[]){gc, "--root", base, "attribute", "--built", flipped, "--elf", elf,
                                                "--all-symbols"},
                          9, base, NULL, 0);
    AvpStr text = r.out;
    check(&c, "gatecheck attribute exits 1", !r.timed_out && r.returncode == 1,
          avp_sprintf(a, "exit %s", gate_rc_str(a, &r)));
    check(&c, "gatecheck attribute names the unit", avp_str_contains(text, avp_str(unit->path)), unit->path);
    check(&c, "gatecheck attribute names the symbol", avp_str_contains(text, avp_str(sym->name)), sym->name);
    AvpStrVec ls = avp_str_splitlines(text, false), others = {0};
    for (size_t i = 0; i < ls.n; i++)
        if (avp_str_startswith(ls.v[i], AVP_S("==")) && !avp_str_contains(ls.v[i], avp_str(unit->path)))
            avp_vec_push(&others, ls.v[i]);
    AvpStr oj = avp_str_join(a, AVP_S("; "), others.v, others.n < 2 ? others.n : 2);
    check(&c, "gatecheck attribute names no other unit", others.n == 0, avp_cz(a, oj));
    const char *cd = avp_path_join(a, d, "contract");
    if (!avp_makedirs(cd, true)) shsm_raise_io();
    GateContract ct;
    const char *err = NULL;
    if (!gate_read_contract(a, avp_path_join(a, cfg, "build.sha1"), avp_path_join(a, cfg, "config.yml"), root, &ct, &err))
        shsm_raise(SHSM_EXC_VALUE, "%s", err);
    check(&c, "preflight accepts the unedited contract", scratch(a, cfg, cd, root, want, ct.retail, false, false), "");
    check(&c, "preflight rejects an edited build.sha1", !scratch(a, cfg, cd, root, want, ct.retail, true, false), "");
    check(&c, "preflight rejects an edited config.yml hash", !scratch(a, cfg, cd, root, want, ct.retail, false, true), "");
    check(&c, "preflight rejects both edited (retail DOL differs)", !scratch(a, cfg, cd, root, want, ct.retail, true, true),
          "");
    if (c.failed) {
        *line = avp_sprintf(a, "SELFTEST RED: %zu of %zu checks failed (%s)", c.failed, c.n, c.first_failed);
        return false;
    }
    *line = avp_sprintf(a, "SELFTEST OK: %zu checks; the gate rejects a one-byte change in %s (%s)", c.n, sym->name,
                        unit->name);
    return true;
}
