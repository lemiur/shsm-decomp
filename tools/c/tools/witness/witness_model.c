/* The shared model of witness.h: FnInfo and the {address: FnInfo} dict. */
#include "witness.h"

#include <string.h>

const char *const WT_TIERS[WT_NTIERS] = {"string", "table", "constant", "call", "callees", "callers"};

WtFn *wt_fn_new(i64 addr) {
    WtFn *f = avp_xcalloc(1, sizeof *f);
    f->addr = addr;
    f->strs = avp_set_new();
    f->floats = avp_pyintset_new();
    f->ints = avp_pyintset_new();
    f->offs = avp_pyintset_new();
    return f;
}

void wt_side_init(WtSide *s) {
    memset(s, 0, sizeof *s);
    s->idx = avp_imap_new();
}

WtFn *wt_side_get(const WtSide *s, i64 addr) {
    AvpVal *v = avp_imap_get(s->idx, (u64)addr);
    return v ? s->v.v[v->i] : NULL;
}

void wt_side_put(WtSide *s, WtFn *f) {
    bool created = false;
    AvpVal *v = avp_imap_put(s->idx, (u64)f->addr, &created);
    if (created) {
        v->i = (i64)s->v.n;
        avp_vec_push(&s->v, f);
    } else {
        s->v.v[v->i] = f;
    }
}
