/* mwt_build: the per-build address tables (mwt_build.h). Each address is the first instruction of the code the trace
 * reads at: a function entry, or the first instruction of the epilogue block one return path of that function takes
 * (each return path of the inliner's decision function has its own block, so the block names the rule). The names
 * follow docs/compiler_inline.md, where every address is listed with the code it stands for. */
#include "mwt_build.h"

#include "avp/hash.h"

#define A(k, v) {k, v}

static const MwtAddr ADDRS_10[] = {
    /* CInline_CheckInlineability (named), its CExpr_IRTreeWalk callback (+0xc0), its auto-inline size test (+0xe0) */
    A("CHECKINL", 0x5615c0), A("CHECKINL_RESULT", 0x561646), A("CHECKINL_QUAL4", 0x561651),
    A("BODYWALK", 0x561680), A("AUTO_FAILCOUNT", 0x561701), A("AUTO_FAILLOCALS", 0x56170d), A("AUTO_OK", 0x561712),
    /* the inlinability classifier (static, called by CheckInlineability and ComputeInlineability): its returns */
    A("KIND_NEVERINLINE", 0x5641e7), A("KIND_RESULTDTOR", 0x564224), A("KIND_VARARGS", 0x564248),
    A("KIND_DOBJSTACK", 0x564276), A("KIND_END", 0x5642b9), A("KIND_RESULTCLASS", 0x5642c0),
    A("KIND_PARAMDTOR", 0x5642dc), A("KIND_EXPR3E", 0x5642f6),
    /* the pass driver (ExpandStatements), the two statement-level calls of the expression walker */
    A("EXPSTMTS", 0x55cf90), A("STMT1", 0x55d1b8), A("STMT2", 0x55d372),
    /* the decision for one call (static), after its CMid_GetFuncData of the callee, and its return blocks */
    A("DECIDE", 0x561e80), A("DECIDE_FD", 0x561f1a), A("R_NOTCAND", 0x5622d0), A("R_NOFUNCDATA", 0x562110),
    A("R_ALWAYS", 0x561f07), A("R_BULAST", 0x561f7f), A("R_BUSIZE", 0x56214b), A("BU_CMP", 0x56213f), A("R_LVL3", 0x561fcd),
    A("R_LVL1", 0x562155), A("R_LVL2", 0x562169), A("R_PASS", 0x5622c4), A("R_OPTFIT", 0x562102),
    A("R_ANALYSIS", 0x56224f), A("R_NOCUR", 0x562173), A("R_NOCURFD", 0x562180),
    /* the walker after the expansion call; the statement-inline queue test; the queued statement expansion */
    A("XP", 0x55df73), A("QUEUE", 0x55e335), A("XS", 0x55d472),
    /* CMangler_GetLinkName (named): the two stores of a freshly made link name */
    A("LN1", 0x56ea77), A("LN2", 0x56ea87),
    /* CInline_ComputeInlineability (named): entry and return; the cost function of a call node (kind 0x39);
     * the call-graph visitor (static): the growth revert and the kept IR; CInline_Expand (named) */
    A("COMPUTE", 0x55f260), A("COMPUTE_RET", 0x55f661), A("CALLCOST", 0x564800),
    A("REVERT", 0x55cdbd), A("KEEP", 0x55ceb0), A("EXPAND", 0x55cee0),
    /* data: the inliner's state and the inline options (copts) */
    A("G_CURFN", 0x72b060), A("G_LEVEL", 0x72b192), A("G_MAXSIZE", 0x72b066), A("G_QUEUE", 0x72b088), A("G_AUTOINLINE", 0x779086),
    A("G_MAXAUTO", 0x779056), A("G_DEPTH", 0x779066), A("G_BOTTOMUP", 0x77906b), A("G_ALWAYS", 0x779100),
    A("G_DONTINLINE", 0x7790ff), A("G_X112", 0x779112),
    /* constants: the object datatype of an alias (Object +2), the inlinability kind's offset in the function data */
    A("C_DALIAS", 5), A("C_FD_KIND", 0xf0),
    /* regalloc (docs/compiler_regalloc.md): CodeGen_Generator (named); InterferenceGraph.obj: buildinterferencegraph
     * (named), its coalescing refusals and merge, the variables' walk, the inheritance, the rematerialisation walk;
     * Coloring.obj: simplify (static), select (static); data: the class byte, the attempt counter, the physical
     * and used register counts, the alias array, the VLE byte, the opcode table's name pointers (22-byte entries) */
    A("RA_CODEGEN", 0x46bcb0), A("RA_BUILD", 0x63d720), A("RA_CO_INTERF", 0x63dd7a), A("RA_CO_RESTR", 0x63dd91),
    A("RA_CO_SET", 0x63ddc3), A("RA_CO_MERGE", 0x63ddd5), A("RA_ARG", 0x63d97a), A("RA_LOC", 0x63da02),
    A("RA_INHERIT", 0x63dc43), A("RA_REMAT", 0x63d7e0), A("RA_REMAT_INSN", 0x63d820), A("RA_REMAT_DEF", 0x63d850),
    A("RA_NODES", 0x63d8e9), A("RA_SIMP_K", 0x5b8dd5), A("RA_SWEEP1", 0x5b8de6), A("RA_SWEEP2", 0x5b9010),
    A("RA_PUSH_LOW", 0x5b8e90), A("RA_EST", 0x5b8ee5), A("RA_CAND1", 0x5b8ef8), A("RA_CAND", 0x5b8f41),
    A("RA_PUSH_OPT", 0x5b8ffa), A("RA_PUSH_AFTER", 0x5b90c9), A("RA_SEL_VOL", 0x5b8c69), A("RA_SEL_AVAIL", 0x5b8c7a),
    A("RA_SEL_NODE", 0x5b8ca0), A("RA_SEL_EDGE", 0x5b8cbf), A("RA_SEL_FREE", 0x5b8d0f), A("RA_SEL_PICK", 0x5b8d29),
    A("RA_SEL_NV", 0x5b8d84), A("RA_SEL_SPILL", 0x5b8db0), A("RA_SEL_RET", 0x5b8d53), A("RA_REWRITE_VAR", 0x5b8b93), A("RA_G_CLASS", 0x794146),
    A("RA_G_ITER", 0x770970), A("RA_G_NREAL", 0x770b68), A("RA_G_USED", 0x778924), A("RA_G_ALIAS", 0x7378f0),
    A("RA_G_VLE", 0x778e9b), A("RA_OPNAMES", 0x6f06c2),
    /* the inliner (CInline.obj, statics): the argument binder entry and return, its renames of a callee's locals and
     * parameter objects to @NNN (CParser_GetUniqueName's result stored at Object +0xc) */
    A("RA_INL_BIND", 0x55eab0), A("RA_INL_BIND_RET", 0x55ed5c), A("RA_INL_LOCAL", 0x55ed18), A("RA_INL_PARAM", 0x55eef4),
    /* sched (docs/compiler_regalloc.md, "Scheduler"): Scheduler.obj: scheduleinstructions (named) and its block visit,
     * the block scheduler (static) at its node creation, slack loop, select call, issue, successor update and cycle
     * end; the dependence builder's register operand, the edge adder and its new-edge and raised-latency stores, the
     * three may_alias answers and their prechecks; the selection (static): can-issue answers, incumbent, successor
     * counts, a candidate taken; PCodeAssembly.obj: the assemblepcode call per emitted instruction; data: the
     * machine model pointer, the block's greatest height, coloring, the opcode priority bytes, the schedule mode, the
     * processor, the precheck flag */
    A("SC_DRIVER", 0x5bb420), A("SC_VISIT", 0x5bb4f1), A("SC_BLOCK", 0x5bb600), A("SC_NODE", 0x5bb66d),
    A("SC_SLACK", 0x5bb6a6), A("SC_SELECT", 0x5bb718), A("SC_ISSUE", 0x5bb72b), A("SC_UPD", 0x5bb74e),
    A("SC_CYCLE", 0x5bb7a8), A("SC_END", 0x5bb7c3), A("SC_OPND", 0x5bb9e7), A("SC_EDGE", 0x5bc0b0),
    A("SC_EDGE_NEW", 0x5bc169), A("SC_EDGE_UPD", 0x5bc130), A("SC_ALIAS_LS", 0x5bbb5c), A("SC_ALIAS_SL", 0x5bbc0c),
    A("SC_ALIAS_SS", 0x5bbc5c), A("SC_PRE_LS", 0x5bbb48), A("SC_PRE_SL", 0x5bbbf8), A("SC_PRE_SS", 0x5bbc48),
    A("SC_CI1", 0x5bb80f), A("SC_INC", 0x5bb81a), A("SC_CI2", 0x5bb881), A("SC_SUCC", 0x5bb903), A("SC_TAKE", 0x5bb95f),
    A("SC_ASM", 0x5a0cf8), A("SC_G_MACHINE", 0x731548), A("SC_G_MAXH", 0x73154c), A("SC_G_COLORING", 0x770c84),
    A("SC_PRIO", 0x6f06cb), A("SC_G_MODE", 0x778ea5), A("SC_G_PROC", 0x778ea4), A("SC_G_PRE", 0x778f2b),
};

static const MwtAddr ADDRS_10A[] = {
    A("CHECKINL", 0x55d410), A("CHECKINL_RESULT", 0x55d496), A("CHECKINL_QUAL4", 0x55d4a1),
    A("BODYWALK", 0x55d4d0), A("AUTO_FAILCOUNT", 0x55d551), A("AUTO_FAILLOCALS", 0x55d55d), A("AUTO_OK", 0x55d562),
    A("KIND_NEVERINLINE", 0x560027), A("KIND_RESULTDTOR", 0x560064), A("KIND_VARARGS", 0x560088),
    A("KIND_DOBJSTACK", 0x5600b6), A("KIND_END", 0x5600ea), A("KIND_RESULTCLASS", 0x5600f0),
    A("KIND_PARAMDTOR", 0x56010c), A("KIND_EXPR3E", 0x560126),
    A("EXPSTMTS", 0x558e60), A("STMT1", 0x559088), A("STMT2", 0x559242),
    A("DECIDE", 0x55dcd0), A("DECIDE_FD", 0x55dd6a), A("R_NOTCAND", 0x55e110), A("R_NOFUNCDATA", 0x55df50),
    A("R_ALWAYS", 0x55dd57), A("R_BULAST", 0x55ddc3), A("R_BUSIZE", 0x55df8b), A("BU_CMP", 0x55df7f), A("R_LVL3", 0x55de0d),
    A("R_LVL1", 0x55df95), A("R_LVL2", 0x55dfa9), A("R_PASS", 0x55e104), A("R_OPTFIT", 0x55df42),
    A("R_ANALYSIS", 0x55e08f), A("R_NOCUR", 0x55dfb3), A("R_NOCURFD", 0x55dfc0),
    A("XP", 0x559e43), A("QUEUE", 0x55a1c5), A("XS", 0x559342),
    A("LN1", 0x56a64c), A("LN2", 0x56a658),
    A("COMPUTE", 0x55b0b0), A("COMPUTE_RET", 0x55b4b1), A("CALLCOST", 0x560630),
    A("REVERT", 0x558c9c), A("KEEP", 0x558d80), A("EXPAND", 0x558db0),
    A("G_CURFN", 0x719050), A("G_LEVEL", 0x719182), A("G_MAXSIZE", 0x719056), A("G_QUEUE", 0x719078), A("G_AUTOINLINE", 0x74f516),
    A("G_MAXAUTO", 0x74f4f2), A("G_DEPTH", 0x74f4fe), A("G_BOTTOMUP", 0x74f502), A("G_ALWAYS", 0x74f599),
    A("G_DONTINLINE", 0x74f598), A("G_X112", 0x74f5aa),
    A("C_DALIAS", 6), A("C_FD_KIND", 0xda),
    /* regalloc (docs/compiler_regalloc.md): CodeGen_Generator (named); InterferenceGraph.obj: buildinterferencegraph
     * (named), its coalescing refusals and merge, the variables' walk, the inheritance, the rematerialisation walk;
     * Coloring.obj: simplify (static), select (static); data: the class byte, the attempt counter, the physical
     * and used register counts, the alias array, the VLE byte, the opcode table's name pointers (22-byte entries) */
    A("RA_CODEGEN", 0x46a410), A("RA_BUILD", 0x6386f0), A("RA_CO_INTERF", 0x638d4a), A("RA_CO_RESTR", 0x638d61),
    A("RA_CO_SET", 0x638d93), A("RA_CO_MERGE", 0x638da5), A("RA_ARG", 0x63894a), A("RA_LOC", 0x6389d2),
    A("RA_INHERIT", 0x638c13), A("RA_REMAT", 0x6387b0), A("RA_REMAT_INSN", 0x6387f0), A("RA_REMAT_DEF", 0x638820),
    A("RA_NODES", 0x6388b9), A("RA_SIMP_K", 0x5b4485), A("RA_SWEEP1", 0x5b4496), A("RA_SWEEP2", 0x5b46c0),
    A("RA_PUSH_LOW", 0x5b4540), A("RA_EST", 0x5b4595), A("RA_CAND1", 0x5b45a8), A("RA_CAND", 0x5b45f1),
    A("RA_PUSH_OPT", 0x5b46aa), A("RA_PUSH_AFTER", 0x5b4779), A("RA_SEL_VOL", 0x5b4319), A("RA_SEL_AVAIL", 0x5b432a),
    A("RA_SEL_NODE", 0x5b4350), A("RA_SEL_EDGE", 0x5b436f), A("RA_SEL_FREE", 0x5b43bf), A("RA_SEL_PICK", 0x5b43d9),
    A("RA_SEL_NV", 0x5b4434), A("RA_SEL_SPILL", 0x5b4460), A("RA_SEL_RET", 0x5b4403), A("RA_REWRITE_VAR", 0x5b4243), A("RA_G_CLASS", 0x76c395),
    A("RA_G_ITER", 0x7503f8), A("RA_G_NREAL", 0x7505d8), A("RA_G_USED", 0x758348), A("RA_G_ALIAS", 0x724560),
    A("RA_G_VLE", 0x74f339), A("RA_OPNAMES", 0x6e3d78),
    A("RA_INL_BIND", 0x55a940), A("RA_INL_BIND_RET", 0x55abec), A("RA_INL_LOCAL", 0x55aba8), A("RA_INL_PARAM", 0x55ad43),
    /* sched: the same code as Wii/1.0's, the block scheduler on at -0x4980, the driver's own layout (fewer
     * processors), PCodeAssembly.obj at -0x4580; data read from the 1.0a code */
    A("SC_DRIVER", 0x5b6ad0), A("SC_VISIT", 0x5b6b90), A("SC_BLOCK", 0x5b6c80), A("SC_NODE", 0x5b6ced),
    A("SC_SLACK", 0x5b6d26), A("SC_SELECT", 0x5b6d98), A("SC_ISSUE", 0x5b6dab), A("SC_UPD", 0x5b6dce),
    A("SC_CYCLE", 0x5b6e28), A("SC_END", 0x5b6e43), A("SC_OPND", 0x5b7067), A("SC_EDGE", 0x5b7730),
    A("SC_EDGE_NEW", 0x5b77e9), A("SC_EDGE_UPD", 0x5b77b0), A("SC_ALIAS_LS", 0x5b71dc), A("SC_ALIAS_SL", 0x5b728c),
    A("SC_ALIAS_SS", 0x5b72dc), A("SC_PRE_LS", 0x5b71c8), A("SC_PRE_SL", 0x5b7278), A("SC_PRE_SS", 0x5b72c8),
    A("SC_CI1", 0x5b6e8f), A("SC_INC", 0x5b6e9a), A("SC_CI2", 0x5b6f01), A("SC_SUCC", 0x5b6f83), A("SC_TAKE", 0x5b6fdf),
    A("SC_ASM", 0x59c778), A("SC_G_MACHINE", 0x71e1b8), A("SC_G_MAXH", 0x71e1bc), A("SC_G_COLORING", 0x7506f4),
    A("SC_PRIO", 0x6e3d81), A("SC_G_MODE", 0x74f343), A("SC_G_PROC", 0x74f342), A("SC_G_PRE", 0x74f3c8),
};

static const MwtBuild BUILDS[] = {
    {"Wii/1.0", "0c3f0b23a0deac4c48b123e558e690bea167fa17", ADDRS_10, sizeof ADDRS_10 / sizeof ADDRS_10[0]},
    {"Wii/1.0a", "fb0820a3f519b4e7c03a2b7a3e810007ff3c5c81", ADDRS_10A, sizeof ADDRS_10A / sizeof ADDRS_10A[0]},
};

const MwtBuild *mwt_build_find(const char *build) {
    if (!build) return NULL;
    for (size_t i = 0; i < sizeof BUILDS / sizeof BUILDS[0]; i++)
        if (!strcmp(BUILDS[i].build, build)) return &BUILDS[i];
    return NULL;
}

bool mwt_build_value(const MwtBuild *b, const char *key, u32 *out) {
    for (size_t i = 0; i < b->naddrs; i++)
        if (!strcmp(b->addrs[i].key, key)) {
            *out = b->addrs[i].addr;
            return true;
        }
    return false;
}

bool mwt_build_verify(AvpArena *a, const char *root, const MwtBuild *b, const char **why) {
    const char *exe = avp_sprintf(a, "%s/tools/compilers/%s/mwcceppc.exe", root, b->build);
    AvpBytes data = {0};
    if (!avp_read_all(a, exe, &data)) {
        *why = avp_sprintf(a, "cannot read %s", exe);
        return false;
    }
    const char *h = avp_sha1_hex(a, data.v, data.n);
    if (strcmp(h, b->sha1) != 0) {
        *why = avp_sprintf(a, "%s has SHA-1 %s; mwtrace's addresses for %s are for %s", exe, h, b->build, b->sha1);
        return false;
    }
    return true;
}

bool mwt_build_expand(AvpArena *a, const MwtBuild *b, AvpStr tmpl, AvpStr *out, const char **why) {
    AvpBuf buf;
    avp_buf_init(&buf, a);
    size_t i = 0;
    while (i < tmpl.n) {
        char c = tmpl.p[i];
        if (c == '{') {
            size_t j = i + 1;
            while (j < tmpl.n && tmpl.p[j] != '}') j++;
            if (j < tmpl.n) {
                const char *key = avp_cz(a, avp_strn(tmpl.p + i + 1, j - i - 1));
                u32 v;
                if (!mwt_build_value(b, key, &v)) {
                    *why = avp_sprintf(a, "build %s has no address {%s}", b->build, key);
                    return false;
                }
                avp_buf_printf(&buf, "%#x", (unsigned)v);
                i = j + 1;
                continue;
            }
        }
        avp_buf_putc(&buf, tmpl.p[i]);
        i++;
    }
    *out = avp_buf_str(&buf);
    return true;
}
