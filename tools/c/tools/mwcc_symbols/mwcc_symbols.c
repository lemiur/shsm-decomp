/* mwcc_symbols: the CLI of tools/mwcc_trace/symbols.py (main, check_all, the --dump file). */
#include "mwcc_symbols.h"

#include <stdio.h>
#include <string.h>

const char *mwcc_symbols_root(AvpArena *a) { return avp_path_abspath(a, avp_root()); }

const char *mwcc_symbols_default_exe(AvpArena *a, const char *root) {
    return avp_join(a, root, "tools", "compilers", "Wii", "1.0", "mwcceppc.exe");
}

char *mwcc_symbols_hex(AvpArena *a, i64 v) {
    if (v < 0) return avp_sprintf(a, "0x-%llx", (unsigned long long)(0 - (u64)v));
    return avp_sprintf(a, "0x%llx", (unsigned long long)v);
}

/* '%-28s' % s: padded to 28 code points */
static void pad28(AvpBuf *out, AvpStr s) {
    avp_buf_put(out, s);
    for (size_t n = avp_u8_len(s); n < 28; n++) avp_buf_putc(out, ' ');
}

static int cmp_str(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_str_cmp(*(const AvpStr *)x, *(const AvpStr *)y);
}

void mwcc_symbols_check_all(AvpArena *a, const char *root, AvpBuf *out) {
    const char *comp = avp_join(a, root, "tools", "compilers");
    AvpWalk *w = avp_walk_open(comp, true);
    AvpWalkItem item = {0};
    while (w && avp_walk_next(w, &item)) {
        char *dp = avp_sprintf(a, "%.*s", AVP_SF(item.dirpath));
        size_t nf = item.files.n;
        AvpStr *files = AVP_NEWN(a, AvpStr, nf ? nf : 1);
        for (size_t i = 0; i < nf; i++) files[i] = avp_str(avp_sprintf(a, "%.*s", AVP_SF(item.files.v[i])));
        avp_sort(files, nf, sizeof *files, cmp_str, NULL, false);
        for (size_t i = 0; i < nf; i++) {
            if (!avp_str_eq(avp_str_lower(a, files[i]), AVP_S("mwcceppc.exe"))) continue;
            char *path = avp_path_join(a, dp, avp_sprintf(a, "%.*s", AVP_SF(files[i])));
            char *rel;
            if (!avp_path_relpath(a, dp, comp, &rel)) rel = dp;
            AvpTry t;
            if (AVP_TRY(&t)) {
                ShsmPeSymbols *tab = shsm_pe_symbols_load(a, path);
                avp_try_end(&t);
                pad28(out, avp_str(rel));
                avp_buf_printf(out, " NB11: %zu functions, %zu module ranges\n", tab->nfuncs, tab->nranges);
            } else if (t.code == SHSM_PE_EXC_NO_DEBUG_INFO) {
                pad28(out, avp_str(rel));
                avp_buf_printf(out, " no NB11 (%s)\n", t.msg);
            } else {
                avp_walk_close(w);
                avp_throw(t.code, "%s", t.msg);
            }
        }
    }
    if (w) avp_walk_close(w);
}

AvpStr mwcc_symbols_dump_text(AvpArena *a, const ShsmPeSymbols *t) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_put(&b, AVP_S("va\tsize\tname\tmodule\n"));
    for (size_t i = 0; i < t->nfuncs; i++) {
        const ShsmPeFunc *f = &t->funcs[i];
        /* '0x%08x' of a non-negative va; size can be negative only for a malformed table */
        avp_buf_printf(&b, "0x%08llx\t%lld\t%.*s\t%.*s\n", (unsigned long long)f->va, (long long)f->size,
                     AVP_SF(f->name), AVP_SF(f->module));
    }
    return avp_buf_str(&b);
}

AvpStr mwcc_symbols_addr_line(AvpArena *a, const ShsmPeSymbols *t, i64 va) {
    if (!t->nfuncs) avp_throw(SHSM_PE_EXC_INDEX, "list index out of range");
    const ShsmPeFunc *first = &t->funcs[0], *last = &t->funcs[t->nfuncs - 1];
    AvpStr r = {0};
    if (first->va <= va && va < last->va + last->size) r = shsm_pe_symbols_describe(a, t, va);
    if (!r.n) r = shsm_pe_symbols_describe_data(t, va);
    if (!r.n) r = AVP_S("no symbol");
    return avp_str(avp_sprintf(a, "%s  %.*s", mwcc_symbols_hex(a, va), AVP_SF(r)));
}

void mwcc_symbols_name_lines(AvpArena *a, const ShsmPeSymbols *t, AvpStr name, AvpBuf *out) {
    const ShsmPeFunc **hits;
    size_t n = shsm_pe_symbols_find(a, t, name, &hits);
    if (!n) avp_buf_printf(out, "%.*s  not found\n", AVP_SF(name));
    for (size_t i = 0; i < n; i++)
        avp_buf_printf(out, "%s  %.*s (%.*s)\n", mwcc_symbols_hex(a, hits[i]->va), AVP_SF(hits[i]->name),
                     AVP_SF(hits[i]->module));
}

/* ---- main ------------------------------------------------------------------------------------------------------ */

static void flush_buf(AvpBuf *b) {
    AvpStr s = avp_buf_str(b);
    fwrite(s.p, 1, s.n, stdout);
    avp_buf_clear(b);
}

/* An unexpected internal error: one line on stderr, exit 1. */
static int traceback(const char *cls, const char *msg) {
    fflush(stdout);
    fprintf(stderr, "mwcc_symbols.exe: error: %s: %s\n", cls, msg);
    return 1;
}

static AvpArgParser *make_parser(const char *default_exe) {
    AvpArgParser *ap = avp_args_parser(
        NULL, &(AvpArgParserDesc){.prog = "mwcc_symbols.exe",
                                  .description = "Function names for mwcceppc.exe, read from the compiler's own "
                                                 "CodeView debug info."});
    avp_arg(ap, &(AvpArgDesc){.names = "--exe", .dflt = AVP_DEF_STR(default_exe)});
    avp_arg(ap, &(AvpArgDesc){.names = "--dump", .action = AVP_ARG_STORE_TRUE,
                              .help = "write build/mwcc_symbols/<version>.tsv"});
    avp_arg(ap, &(AvpArgDesc){.names = "--addr", .nargs = AVP_NARGS_PLUS, .metavar = "VA"});
    avp_arg(ap, &(AvpArgDesc){.names = "--name", .nargs = AVP_NARGS_PLUS, .metavar = "NAME"});
    avp_arg(ap, &(AvpArgDesc){.names = "--check-all", .action = AVP_ARG_STORE_TRUE});
    return ap;
}

int mwcc_symbols_main(int argc, char **argv) {
    AvpArena *a = avp_arena();
    const char *root = mwcc_symbols_root(a);
    AvpArgs *args = avp_args_parse(make_parser(mwcc_symbols_default_exe(a, root)), argc, argv);
    AvpBuf out;
    avp_buf_init(&out, a);
    AvpTry t;
    if (avp_args_bool(args, "check_all")) {
        if (AVP_TRY(&t)) {
            mwcc_symbols_check_all(a, root, &out);
            avp_try_end(&t);
        } else {
            flush_buf(&out);
            return traceback(shsm_pe_exc_class(t.code), t.msg);
        }
        flush_buf(&out);
        return 0;
    }
    AvpStr exe_s = avp_args_str(args, "exe");
    const char *exe = avp_sprintf(a, "%.*s", AVP_SF(exe_s));
    ShsmPeSymbols *tab = NULL;
    if (AVP_TRY(&t)) {
        tab = shsm_pe_symbols_load(a, exe);
        avp_try_end(&t);
    } else if (t.code == SHSM_PE_EXC_NO_DEBUG_INFO || t.code == AVP_ERR_IO) {
        fflush(stdout);
        fprintf(stderr, "mwcc_symbols.exe: %s: %s\n", exe, t.msg);
        return 2;
    } else {
        return traceback(shsm_pe_exc_class(t.code), t.msg);
    }

    if (avp_args_bool(args, "dump")) {
        const char *outdir = avp_join(a, root, "build", "mwcc_symbols");
        if (!avp_makedirs(outdir, true)) {
            AvpStr m = avp_io_err_str(a);
            return traceback(avp_io_err_class(), avp_sprintf(a, "%.*s", AVP_SF(m)));
        }
        const char *path = avp_path_join(a, outdir, avp_sprintf(a, "%s.tsv", shsm_pe_version_of(a, root, exe)));
        /* open(out, 'w'): the locale's ANSI code page, '\n' written as '\r\n' */
        if (!avp_write_text(path, mwcc_symbols_dump_text(a, tab), AVP_TEXT(AVP_ENC_MBCS, AVP_ERR_STRICT, AVP_NL_DEFAULT))) {
            AvpStr m = avp_io_err_str(a);
            return traceback(avp_io_err_class(), avp_sprintf(a, "%.*s", AVP_SF(m)));
        }
        avp_buf_printf(&out, "wrote %s (%zu functions)\n", path, tab->nfuncs);
    }
    size_t naddr = avp_args_is_none(args, "addr") ? 0 : avp_args_len(args, "addr");
    AvpStrVec addrs = naddr ? avp_args_strs(args, "addr") : (AvpStrVec){0};
    for (size_t i = 0; i < naddr; i++) {
        i64 va;
        bool overflow = false;
        if (!avp_parse_int(addrs.v[i], 0, &va, &overflow)) {
            flush_buf(&out);
            if (overflow)
                return traceback("OverflowError", "address does not fit in 64 bits");
            return traceback("ValueError", avp_sprintf(a, "invalid literal for int() with base 0: %s",
                                                       avp_repr_str(a, addrs.v[i]).p));
        }
        if (AVP_TRY(&t)) {
            AvpStr line = mwcc_symbols_addr_line(a, tab, va);
            avp_try_end(&t);
            avp_buf_put(&out, line);
            avp_buf_putc(&out, '\n');
        } else {
            flush_buf(&out);
            return traceback(shsm_pe_exc_class(t.code), t.msg);
        }
    }
    size_t nname = avp_args_is_none(args, "name") ? 0 : avp_args_len(args, "name");
    AvpStrVec names = nname ? avp_args_strs(args, "name") : (AvpStrVec){0};
    for (size_t i = 0; i < nname; i++) mwcc_symbols_name_lines(a, tab, names.v[i], &out);
    flush_buf(&out);
    return 0;
}
