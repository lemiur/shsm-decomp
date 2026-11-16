/* sibling: read-only lookups of the stored pairing, for brief. */
#include "sibling_lookup.h"

#include "avp/fs.h"
#include "shsm_mangle.h"

#include <string.h>

struct SibLookup {
    AvpArena *a;
    const ShsmRepo *repo;
    Sibling *sb;
    bool loaded; /* the span list is built on first use: it loads both sides */
    SibFnVec s, g;
    SibLookupSpan *spans;
    size_t nspans;
};

SibLookup *sibling_lookup_open(AvpArena *a, const ShsmRepo *repo) {
    const char *path = avp_path_join(a, avp_path_join(a, avp_path_join(a, repo->index_dir, "sibling"),
                                                      SIBLING_DEFAULT_NAME),
                                     "sibling.sqlite");
    if (!avp_isfile(path)) return NULL;
    SibLookup *lk = avp_alloc(a, sizeof *lk);
    lk->a = a;
    lk->repo = repo;
    AvpTry t;
    if (AVP_TRY(&t)) {
        lk->sb = sibling_open(repo, SIBLING_DEFAULT_NAME);
        if (!sibling_has_pairs(lk->sb)) lk->sb = NULL;
        avp_try_end(&t);
    } else {
        lk->sb = NULL; /* an unreadable database is an absent one */
    }
    return lk->sb ? lk : NULL;
}

bool sibling_lookup_fn(SibLookup *lk, i64 address, SibLookupFn *out) {
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(lk->sb->db,
                           "select s.tier, f.name, f.lib, f.object, f.ord from shsm s join functions f on"
                           " f.address = s.peer where s.address = ? and s.peer is not null",
                           -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(st, 1, address);
    bool found = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        AvpArena *a = lk->a;
        int tier = (int)sqlite3_column_int64(st, 0);
        const char *name = avp_strdup(a, (const char *)sqlite3_column_text(st, 1));
        out->name = name;
        const char *d = shsm_demangle_or_name(a, name);
        out->demangled = d ? d : name;
        out->tier = tier > 0 && tier < SIB_NTIERS ? SIBLING_TIERS[tier] : "?";
        out->lib = sqlite3_column_type(st, 2) == SQLITE_NULL ? "" : avp_strdup(a, (const char *)sqlite3_column_text(st, 2));
        out->object = sqlite3_column_type(st, 3) == SQLITE_NULL ? "" : avp_strdup(a, (const char *)sqlite3_column_text(st, 3));
        out->order = sqlite3_column_int64(st, 4);
        found = tier > 0;
    }
    sqlite3_finalize(st);
    return found;
}

bool sibling_lookup_same_named(SibLookup *lk, const char *name, SibLookupSame *out) {
    if (!name || sibling_is_placeholder(name)) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(lk->sb->db, "select size, lib, object, ord from functions where name = ? order by address", -1,
                           &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
    AvpArena *a = lk->a;
    size_t n = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (!n) {
            out->name = avp_strdup(a, name);
            const char *d = shsm_demangle_or_name(a, name);
            out->demangled = d ? d : out->name;
            out->size = sqlite3_column_int64(st, 0);
            out->lib = sqlite3_column_type(st, 1) == SQLITE_NULL ? "" : avp_strdup(a, (const char *)sqlite3_column_text(st, 1));
            out->object = sqlite3_column_type(st, 2) == SQLITE_NULL ? "" : avp_strdup(a, (const char *)sqlite3_column_text(st, 2));
            out->order = sqlite3_column_int64(st, 3);
        }
        n++;
    }
    sqlite3_finalize(st);
    out->count = n;
    return n > 0;
}

static void load_spans(SibLookup *lk) {
    if (lk->loaded) return;
    lk->loaded = true;
    AvpTry t;
    if (AVP_TRY(&t)) {
        lk->g = sibling_load_sibling(lk->a, lk->sb);
        lk->s = sibling_load_shsm(lk->a, lk->sb);
        sibling_link_peers(&lk->s, &lk->g);
        SibArgs args = {0};
        args.tiers = 1u << SIB_EXACT | 1u << SIB_EXACT_SEQ; /* the tiers `units` and `apply` default to */
        lk->spans = sibling_span_list(lk->a, lk->sb, &lk->s, &lk->g, &args, &lk->nspans);
        avp_try_end(&t);
    } else {
        lk->nspans = 0;
    }
}

SibLookupSpanVec sibling_lookup_range(SibLookup *lk, i64 start, i64 end) {
    SibLookupSpanVec v = {0};
    load_spans(lk);
    for (size_t k = 0; k < lk->nspans; k++)
        if (lk->spans[k].start < end && lk->spans[k].end > start) avp_vec_push(&v, lk->spans[k]);
    return v;
}
