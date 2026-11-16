/* tools/common/__main__.py `index`: open the index (writing only when it must change), optionally clear it, refresh,
 * and print the refresh line, the listings note and (with --stats) the row counts. */
#include "asmindex_cli.h"

#include "avp/clock.h"
#include "avp/fmt.h"
#include "avp/sort.h"
#include "shsm_asmindex.h"
#include "shsm_cli.h"

static int cmp_kind(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_str_cmp(((const ShsmKindCount *)x)->kind, ((const ShsmKindCount *)y)->kind);
}

static AvpStr kinds_line(AvpArena *a, const char *head, ShsmKindCount *v, size_t n) {
    avp_sort(v, n, sizeof *v, cmp_kind, NULL, false); /* sorted(dict.items()) */
    AvpBuf b = {0};
    avp_buf_putz(&b, head);
    for (size_t i = 0; i < n; i++) {
        if (i) avp_buf_putz(&b, ", ");
        avp_buf_printf(&b, "%.*s %lld", AVP_SF(v[i].kind), (long long)v[i].count);
    }
    AvpStr s = avp_str_dup(a, avp_buf_str(&b));
    avp_buf_free(&b);
    return s;
}

static int run(void *ctx, AvpArgs *args) {
    (void)ctx;
    AvpArena *a = avp_arena();
    AvpStr root = avp_args_str(args, "root");
    const ShsmRepo *repo = shsm_repo_find(root.p ? avp_cz(a, root) : NULL);
    double t0 = avp_perf_counter();
    ShsmAsmIndex *ix = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = repo, .no_refresh = true});
    double t_open = avp_perf_counter() - t0;
    if (avp_args_bool(args, "rebuild")) shsm_asmindex_clear(ix);
    int jobs = avp_args_is_none(args, "jobs") ? 0 : (int)avp_args_int(args, "jobs");
    const ShsmRefresh *info = shsm_asmindex_refresh(ix, jobs);
    ShsmOutput *out = shsm_output_open(args, -1, 1, -1);
    shsm_output_data(out, "refresh", shsm_refresh_json(a, info));
    shsm_output_data(out, "open_seconds", avp_json_float(a, avp_py_round(t_open, 3)));
    shsm_output_data(out, "db", avp_json_str(a, avp_str(ix->db_path)));
    shsm_output_line(out, avp_pyfmt(a, "index %s: %d files scanned, %d parsed, %d removed in %.2fs (parse %.2fs)",
                                    shsm_repo_rel(a, repo, ix->db_path), info->scanned, info->parsed, info->removed,
                                    info->seconds, info->parse_seconds));
    if (info->note && *info->note) shsm_output_line(out, avp_pyfmt(a, "note: %s", info->note));
    if (avp_args_bool(args, "stats")) {
        ShsmIndexStats st = shsm_asmindex_stats(a, ix);
        shsm_output_data(out, "stats", shsm_stats_json(a, &st));
        const char *names[] = {"files", "symbols", "refs", "strings", "floats"};
        i64 vals[] = {st.files, st.symbols, st.refs, st.strings, st.floats};
        for (size_t i = 0; i < 5; i++) shsm_output_line(out, avp_pyfmt(a, "  %-8s %8d", names[i], vals[i]));
        shsm_output_line(out, kinds_line(a, "  symbols by kind: ", st.symbols_by_kind.v, st.symbols_by_kind.n));
        shsm_output_line(out, kinds_line(a, "  refs by kind:    ", st.refs_by_kind.v, st.refs_by_kind.n));
        avp_vec_free(&st.symbols_by_kind);
        avp_vec_free(&st.refs_by_kind);
    }
    shsm_output_close(out);
    shsm_asmindex_close(ix);
    return SHSM_EXIT_OK;
}

int asmindex_main(int argc, char **argv) {
    AvpArgParser *ap = shsm_cli_parser("asmindex", "Shared tool library commands.", false, false, false);
    avp_arg(ap, &(AvpArgDesc){.names = "--rebuild", .action = AVP_ARG_STORE_TRUE, .help = "drop the index first"});
    avp_arg(ap, &(AvpArgDesc){.names = "--stats", .action = AVP_ARG_STORE_TRUE, .help = "print row counts"});
    avp_arg(ap, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_NONE, .help = "parser processes"});
    avp_arg(ap, &(AvpArgDesc){.names = "--root", .metavar = "DIR", .dflt = AVP_DEF_NONE, .help = "checkout to work on"});
    avp_arg(ap, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE, .help = "print JSON"});
    return shsm_cli_main(ap, argc, argv, run, NULL);
}
