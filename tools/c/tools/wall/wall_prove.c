/* The prover (wall.py prove, wants_proof, apply_proof, proof_line). prove() calls the eqcheck port's
 * prove_functions in process, as the Python imports tools.eqcheck. */
#include "wall.h"

#include "eq_prove.h"

static AvpJson *proof(AvpArena *a, AvpJson *verdict, AvpJson *reason, AvpJson *line) {
    AvpJson *p = avp_json_obj(a);
    avp_json_set(a, p, AVP_S("verdict"), verdict);
    avp_json_set(a, p, AVP_S("reason"), reason);
    avp_json_set(a, p, AVP_S("line"), line);
    return p;
}

static AvpJson *prover_error(AvpArena *a, AvpStr why) {
    AvpStr full = avp_pyfmt(a, "prover error: %S", &why);
    AvpStr line = avp_pyfmt(a, "NOT PROVEN: %S", &full);
    return proof(a, avp_json_strz(a, "NOT PROVEN"), avp_json_str(a, full), avp_json_str(a, line));
}

AvpJson *wall_prove(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const CcCompareResult *res, EqLive *live) {
    EqLive *own = live ? NULL : eq_live_new(r, 4);
    AvpJson *rr = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        CcRepo cr = {r->root, r->version};
        CcContext *ctx = cc_repo_context(&cr, avp_str(u->name));
        rr = eq_prove_functions(a, res->retail_fn, res->compiled_fn, ctx, EQ_RET_DEFAULT, false, res,
                                live ? live : own);
        avp_try_end(&t);
    } else {
        /* "prover error: <type(e).__name__>[: <first line of str(e)>]" */
        eq_live_free(own);
        const EqExc *e = eq_exc_caught(&t);
        const char *cls = eq_exc_class(e->code);
        const char *s = eq_exc_str(a, e);
        if (!*s) return prover_error(a, avp_str(cls));
        AvpStrVec ls = avp_str_splitlines(avp_str(s), false);
        AvpStr first = ls.n ? ls.v[0] : AVP_S("");
        AvpStr why = avp_pyfmt(a, "%s: %S", cls, &first);
        avp_vec_free(&ls);
        return prover_error(a, why);
    }
    eq_live_free(own);
    AvpJson *verdict = avp_json_getz(rr, "verdict");
    AvpJson *reason = avp_json_has(rr, AVP_S("reason")) ? avp_json_getz(rr, "reason") : avp_json_strz(a, "");
    AvpJson *line = avp_json_getz(rr, "line");
    return proof(a, verdict, reason, avp_json_truthy(line) ? line : verdict);
}

bool wall_wants_proof(const AvpJson *d, bool prove_all) {
    if (avp_json_truthy(avp_json_getz(d, "exact")) || avp_json_truthy(avp_json_getz(d, "missing"))) return false;
    return prove_all || avp_str_startswith(wall_jstr(d, "verdict"), AVP_S("EQUIVALENT-LOOKING"));
}

void wall_apply_proof(AvpArena *a, AvpJson *d, AvpJson *p) {
    avp_json_set(a, d, AVP_S("proof"), p);
    const AvpJson *pv = avp_json_getz(p, "verdict");
    AvpStr verdict = wall_jstr(d, "verdict");
    if (avp_json_is_str(pv) && avp_str_eqz(pv->u.str, "EQUIVALENT") &&
        avp_str_startswith(verdict, AVP_S("EQUIVALENT-LOOKING"))) {
        avp_json_set(a, d, AVP_S("looking"), avp_json_getz(d, "verdict"));
        const AvpJson *lv = avp_json_getz(p, "line");
        AvpStr line = avp_json_truthy(lv) ? wall_py_str(a, lv) : AVP_S("");
        if (avp_str_startswith(line, AVP_S("EQUIVALENT (proved"))) {
            avp_json_set(a, d, AVP_S("verdict"), avp_json_str(a, line));
        } else {
            const AvpJson *rv = avp_json_getz(p, "reason");
            AvpStr reason = avp_json_truthy(rv) ? wall_py_str(a, rv) : AVP_S("eqcheck");
            avp_json_set(a, d, AVP_S("verdict"), avp_json_str(a, avp_pyfmt(a, "EQUIVALENT (proved; %S)", &reason)));
        }
    }
}

AvpStr wall_proof_line(AvpArena *a, const AvpJson *d) {
    const AvpJson *p = avp_json_getz(d, "proof");
    if (!avp_json_truthy(p) || avp_str_startswith(wall_jstr(d, "verdict"), AVP_S("EQUIVALENT (proved")))
        return (AvpStr){0};
    const AvpJson *lv = avp_json_getz(p, "line");
    AvpStr line = wall_py_str(a, avp_json_truthy(lv) ? lv : avp_json_getz(p, "verdict"));
    return avp_pyfmt(a, "eqcheck: %S", &line);
}
