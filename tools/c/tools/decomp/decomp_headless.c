/* decomp_headless: analyzeHeadless command lines and runs (decomp.py headless_argv, run_headless) and the ELF's
 * small-data bases (elf_symbols, sda_bases). */
#include "decomp.h"

#include <string.h>

DcHeadlessFn dc_headless_hook = NULL;
void *dc_headless_hook_ctx = NULL;
double dc_lock_wait_seconds = 10.0;

const char *dc_scripts_dir(AvpArena *a) {
    /* SCRIPTS_DIR: tools/ghidra of the checkout this program belongs to (<root>/build/tools/decomp.exe) */
    return avp_path_normpath(a, avp_join(a, avp_exe_dir(), "..", "..", "tools", "ghidra"));
}

const char **dc_headless_argv(AvpArena *a, const char *install, const char *project_dir, const DcHeadlessOpts *o,
                              size_t *argc) {
    AvpPtrVec v = {0};
#define ADD(x) avp_vec_push(&v, (void *)(x))
    ADD(dc_headless_script(a, install));
    ADD(project_dir);
    ADD(DECOMP_PROJECT_NAME);
    if (o->import_path && *o->import_path) ADD("-import"), ADD(o->import_path);
    if (o->process && *o->process) ADD("-process"), ADD(o->process);
    if (o->loader && *o->loader) ADD("-loader"), ADD(o->loader);
    if (o->processor && *o->processor) ADD("-processor"), ADD(o->processor);
    if (o->cspec && *o->cspec) ADD("-cspec"), ADD(o->cspec);
    ADD("-scriptPath");
    ADD(o->script_path ? o->script_path : dc_scripts_dir(a));
    for (size_t i = 0; i < o->npre; i++) {
        ADD("-preScript");
        ADD(o->pre[i].script);
        for (size_t j = 0; j < o->pre[i].nargs; j++) ADD(o->pre[i].args[j]);
    }
    for (size_t i = 0; i < o->npost; i++) {
        ADD("-postScript");
        ADD(o->post[i].script);
        for (size_t j = 0; j < o->post[i].nargs; j++) ADD(o->post[i].args[j]);
    }
    if (o->no_analysis) ADD("-noanalysis");
    if (o->read_only) ADD("-readOnly");
    if (o->overwrite) ADD("-overwrite");
#undef ADD
    const char **argv = AVP_NEWN(a, const char *, v.n + 1);
    for (size_t i = 0; i < v.n; i++) {
        argv[i] = v.v[i];
        if (!*argv[i] || strchr(argv[i], '!'))
            shsm_raise(SHSM_EXC_USAGE, "headless argument %s cannot pass through analyzeHeadless.bat",
                       avp_pyfmt(a, "%r", argv[i]).p);
    }
    *argc = v.n;
    avp_vec_free(&v);
    return argv;
}

static bool has_lock_error(AvpStr text) { return avp_str_contains(text, AVP_S("Unable to lock project")); }

double dc_run_headless(AvpArena *a, const DecompPaths *p, const char *const *argv, size_t argc, const char *label,
                       AvpStr *log_text) {
    if (dc_headless_hook) return dc_headless_hook(dc_headless_hook_ctx, p, argv, argc, label, log_text);
    if (!avp_makedirs(p->logs, true)) shsm_raise_io();
    const char *log = avp_join(a, p->logs, avp_sprintf(a, "%s.log", label));
    double t0 = avp_time();
    i64 rc = 0;
    AvpStr text = {0};
    for (int attempt = 0; attempt < 3; attempt++) {
        AvpProcSpec spec = {.argv = argv, .cwd = p->repo->root, .in = AVP_IO_NULL, .out = AVP_IO_FILE,
                            .out_path = log, .err = AVP_IO_STDOUT, .arena = a};
        AvpProcResult r = avp_run(&spec);
        if (r.status == AVP_PROC_START_FAILED)
            shsm_raise(r.winerror == 2 || r.winerror == 3 ? SHSM_EXC_FILE_NOT_FOUND : SHSM_EXC_OS, "%s",
                       r.message ? r.message : "cannot start analyzeHeadless");
        rc = r.returncode;
        if (!avp_read_text(a, log, AVP_TM_UTF8_REPLACE, &text)) shsm_raise_io();
        if (!has_lock_error(text)) break;
        avp_sleep(dc_lock_wait_seconds); /* a run that just finished can hold the lock for a few seconds */
    }
    double seconds = avp_time() - t0;
    if (has_lock_error(text))
        shsm_raise(SHSM_EXC_USAGE, "the Ghidra project is in use by another run (log: %s)",
                   shsm_repo_rel(a, p->repo, log));
    if (rc != 0 || avp_str_contains(text, AVP_S("SCRIPT ERROR"))) {
        AvpStrVec lines = avp_str_splitlines(text, false);
        AvpBuf tail;
        avp_buf_init(&tail, a);
        int n = 0;
        for (size_t i = 0; i < lines.n && n < 3; i++) {
            if (!avp_str_contains(lines.v[i], AVP_S("ERROR")) && !avp_str_contains(lines.v[i], AVP_S("FAILED")))
                continue;
            if (n++) avp_buf_putz(&tail, "; ");
            avp_buf_put(&tail, lines.v[i]);
        }
        avp_vec_free(&lines);
        shsm_raise(SHSM_EXC_USAGE, "analyzeHeadless failed (exit %lld): %s (log: %s)", (long long)rc,
                   n ? avp_buf_cz(&tail) : "see the log", shsm_repo_rel(a, p->repo, log));
    }
    (void)argc;
    *log_text = text;
    return seconds;
}

static u32 be32(const u8 *d, size_t n, size_t off) {
    if (off + 4 > n) shsm_raise(SHSM_EXC_VALUE, "unpack_from requires a buffer of at least %zu bytes", off + 4);
    return (u32)d[off] << 24 | (u32)d[off + 1] << 16 | (u32)d[off + 2] << 8 | d[off + 3];
}
static u32 be16(const u8 *d, size_t n, size_t off) {
    if (off + 2 > n) shsm_raise(SHSM_EXC_VALUE, "unpack_from requires a buffer of at least %zu bytes", off + 2);
    return (u32)d[off] << 8 | d[off + 1];
}

void dc_elf_symbols(AvpArena *a, const char *path, const char *const *names, size_t nnames, bool *has, u64 *val) {
    AvpBytes data = {0};
    if (!avp_read_all(a, path, &data)) shsm_raise_io();
    const u8 *d = data.v;
    size_t n = data.n;
    for (size_t i = 0; i < nnames; i++) has[i] = false;
    if (n < 6 || memcmp(d, "\x7f" "ELF", 4) != 0 || d[4] != 1 || d[5] != 2)
        shsm_raise(SHSM_EXC_USAGE, "%s is not a 32-bit big-endian ELF", path);
    u32 shoff = be32(d, n, 0x20), shentsize = be16(d, n, 0x2E), shnum = be16(d, n, 0x30);
    for (u32 s = 0; s < shnum; s++) {
        size_t sh = (size_t)shoff + (size_t)s * shentsize;
        if (be32(d, n, sh + 4) != 2) continue; /* SHT_SYMTAB */
        u32 link = be32(d, n, sh + 24);
        size_t strtab = be32(d, n, (size_t)shoff + (size_t)link * shentsize + 16);
        u32 off0 = be32(d, n, sh + 16), size = be32(d, n, sh + 20);
        for (size_t off = off0; off < (size_t)off0 + size; off += 16) {
            u32 st_name = be32(d, n, off), st_value = be32(d, n, off + 4);
            size_t start = strtab + st_name;
            if (start > n) continue;
            const u8 *end = memchr(d + start, 0, n - start);
            if (!end) shsm_raise(SHSM_EXC_VALUE, "subsection not found");
            size_t len = (size_t)(end - (d + start));
            for (size_t i = 0; i < nnames; i++) {
                if (strlen(names[i]) == len && memcmp(names[i], d + start, len) == 0) {
                    has[i] = true;
                    val[i] = st_value;
                }
            }
        }
    }
}

void dc_sda_bases(AvpArena *a, const char *elf_path, const ShsmSymbolTable *t, u64 *sda, u64 *sda2) {
    static const char *const names[2] = {"_SDA_BASE_", "_SDA2_BASE_"};
    static const char *const secs[2] = {".sdata", ".sdata2"};
    bool has[2] = {false, false};
    u64 val[2] = {0, 0};
    if (elf_path && avp_str_endswith(avp_str_lower(a, avp_str(elf_path)), AVP_S(".elf")))
        dc_elf_symbols(a, elf_path, names, 2, has, val);
    u64 out[2];
    for (int i = 0; i < 2; i++) {
        if (has[i]) {
            out[i] = val[i];
            continue;
        }
        AvpPtrVec in = {0};
        shsm_sym_in_section(t, secs[i], &in);
        if (!in.n)
            shsm_raise(SHSM_EXC_USAGE, "cannot find %s: no ELF symbol and no %s symbols", names[i], secs[i]);
        u64 lo = UINT64_MAX;
        for (size_t j = 0; j < in.n; j++) {
            const ShsmSymbol *s = in.v[j];
            if (s->address < lo) lo = s->address;
        }
        avp_vec_free(&in);
        out[i] = lo + 0x8000;
    }
    *sda = out[0];
    *sda2 = out[1];
}
