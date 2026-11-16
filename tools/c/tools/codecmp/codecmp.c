/* codecmp: see codecmp.h. */
#include "codecmp.h"

static void put_pair(AvpBuf *b, size_t i, AvpStr retail, AvpStr compiled, const char *tag)
{
    /* "  %3d %-32s | %-32s [tag]" or, without a tag, "  %3d %-32s | %s" */
    avp_buf_printf(b, "\n  %3zu ", i);
    avp_buf_put(b, retail);
    for (size_t k = avp_u8_len(retail); k < 32; k++) avp_buf_putc(b, ' ');
    avp_buf_putz(b, " | ");
    avp_buf_put(b, compiled);
    if (tag) {
        for (size_t k = avp_u8_len(compiled); k < 32; k++) avp_buf_putc(b, ' ');
        avp_buf_printf(b, " [%s]", tag);
    }
}

AvpStr codecmp_describe_result(AvpArena *a, AvpStr name, const CcFnResult *r, bool show_unresolved, size_t limit)
{
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_put(&b, name);
    avp_buf_printf(&b, ": %zu/%zu rows differ", r->npositional, r->rows);
    if (r->targets.n) avp_buf_printf(&b, ", %zu target mismatches", r->targets.n);
    if (r->unresolved.n) avp_buf_printf(&b, ", %zu unresolved targets", r->unresolved.n);
    for (size_t k = 0; k < r->npositional && k < limit; k++)
        put_pair(&b, r->positional[k].index, r->positional[k].retail, r->positional[k].compiled, NULL);
    for (size_t k = 0; k < r->targets.n && k < limit; k++) {
        const CcTargetDiff *t = &r->targets.v[k];
        put_pair(&b, t->i, t->retail, t->compiled, "target");
        avp_buf_putz(&b, "\n        -> ");
        avp_buf_put(&b, t->detail);
    }
    if (show_unresolved)
        for (size_t k = 0; k < r->unresolved.n && k < limit; k++) {
            const CcTargetDiff *t = &r->unresolved.v[k];
            put_pair(&b, t->i, t->retail, t->compiled, "unresolved");
            avp_buf_putz(&b, "\n        -> ");
            avp_buf_put(&b, t->detail);
        }
    return avp_buf_str(&b);
}

int codecmp_main(int argc, char **argv)
{
    AvpArena *a = avp_arena();
    AvpArgParser *ap = avp_args_parser(a, &(AvpArgParserDesc){
        .prog = "codecmp.exe", .description = "Compare a compiled object with the unit's retail listing."});
    avp_arg(ap, &(AvpArgDesc){.names = "unit"});
    avp_arg(ap, &(AvpArgDesc){.names = "object"});
    avp_arg(ap, &(AvpArgDesc){.names = "--asm"});
    static const char *const none[] = {NULL};
    avp_arg(ap, &(AvpArgDesc){.names = "-s", .action = AVP_ARG_APPEND, .dest = "symbols", .dflt_list = none});
    avp_arg(ap, &(AvpArgDesc){.names = "--no-targets", .action = AVP_ARG_STORE_TRUE});
    avp_arg(ap, &(AvpArgDesc){.names = "--unresolved", .action = AVP_ARG_STORE_TRUE,
                              .help = "list every unresolved target"});
    AvpArgs *r = avp_args_parse(ap, argc, argv);

    AvpStr unit = avp_args_str(r, "unit");
    if (avp_str_startswith(unit, AVP_S("main/"))) unit = avp_str_slice(unit, 5, (i64)unit.n);
    const char *root = avp_root();
    AvpStr asm_opt = avp_args_str(r, "asm");
    const char *asm_path = asm_opt.n ? avp_cz(a, asm_opt)
                                     : avp_path_join(a, root, avp_sprintf(a, "build/R5WEA4/asm/%.*s.s", AVP_SF(unit)));
    AvpStr err = {0};
    AvpMap *retail = cc_load_asm(a, asm_path, &err);
    if (!retail) avp_fatal("%.*s", AVP_SF(err));
    AvpMap *compiled = cc_load_object(a, avp_cz(a, avp_args_str(r, "object")), NULL, &err);
    if (!compiled) avp_fatal("%.*s", AVP_SF(err));
    CcProject *proj = cc_project_new(root);
    CcContext *ctx = cc_context_new(proj, unit);
    AvpMap *res = cc_codecmp_object(a, retail, compiled, ctx, !avp_args_bool(r, "no_targets"), NULL);

    AvpStrVec symbols = avp_args_strs(r, "symbols");
    bool show_unres = avp_args_bool(r, "unresolved");
    size_t bad = 0, tot_unres = 0;
    AvpMapIter it = {0};
    while (avp_map_next(res, &it)) {
        bool listed = symbols.n == 0;
        for (size_t k = 0; k < symbols.n && !listed; k++) listed = avp_str_eq(symbols.v[k], it.key);
        if (!listed) continue;
        const CcFnResult *fr = it.val->p;
        tot_unres += fr->unresolved.n;
        if (!cc_fn_result_exact(fr)) bad++;
        AvpStr text = codecmp_describe_result(a, it.key, fr, show_unres, 40);
        fwrite(text.p, 1, text.n, stdout);
        fputc('\n', stdout);
    }
    printf("%zu functions, %zu not exact, %zu unresolved targets\n", avp_map_len(res), bad, tot_unres);
    return bad ? 1 : 0;
}
