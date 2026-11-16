/* Ports tools/test_witness.py TestLookup: wt_lookup, the light read-only query brief uses. */
#include "avp/fs.h"
#include "avp/test.h"
#include "witness_db.h"
#include "witness_lookup.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

static void sql(sqlite3 *db, const char *s) {
    char *err = NULL;
    if (sqlite3_exec(db, s, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "sql: %s\n", err ? err : "?");
        sqlite3_free(err);
    }
}

AVP_TEST(lookup) {
    AvpArena *a = avp_arena();
    const char *d = "build/tmp/witness_lookup_test";
    avp_rmtree(d, AVP_RMTREE_IGNORE_ERRORS);
    REQUIRE(avp_makedirs(d, true));
    ShsmRepo r;
    memset(&r, 0, sizeof r);
    r.index_dir = d;
    WtLookup got;
    CHECK(!wt_lookup(a, &r, 0x80001000, 3, &got)); /* no database */
    sqlite3 *db = NULL;
    REQUIRE(sqlite3_open(wt_db_path(a, &r), &db) == SQLITE_OK);
    sql(db, "PRAGMA synchronous=OFF; PRAGMA journal_mode=MEMORY; BEGIN");
    sql(db, WT_TABLES_SQL);
    sql(db, "insert into fn values ('ps2', 1048576, 'p1', NULL, NULL, 64, 16, 0, 0),"
            " ('ps2', 1048832, 'p2', NULL, NULL, 128, 32, 0, 0),"
            " ('wii', 2147491840, 'fn_80002000', NULL, NULL, 32, 8, 0, 0)");
    sql(db, "insert into pairs values (2147487744, 1049088, 'string', '\"abc\"')");
    sql(db, "insert into pairs values (2147491840, 1048576, 'call', '')");
    sql(db, "insert into inl values (2147487744, 1048576, 1049088, 'call set'), (2147487744, 1048832, 1049088, 'call set')");
    sql(db, "insert into inl values (2147500000, 1048576, 1049088, 'call set')"); /* no pair */
    sql(db, "COMMIT");
    sqlite3_close(db);
    REQUIRE(wt_lookup(a, &r, 0x80001000, 1, &got));
    CHECK_EQ_I64(got.ps2, 0x100200);
    CHECK_CZ(got.tier, "string");
    CHECK_CZ(got.evidence, "\"abc\"");
    CHECK_EQ_I64(got.more, 1);
    REQUIRE(got.helpers.n == 1);
    CHECK_EQ_I64(got.helpers.v[0].helper, 0x100100);
    CHECK_EQ_I64(got.helpers.v[0].size, 0x80);
    CHECK(got.helpers.v[0].wii_copy == NULL);
    REQUIRE(wt_lookup(a, &r, 0x80001000, 3, &got));
    REQUIRE(got.helpers.n == 2);
    CHECK_CZ(got.helpers.v[1].wii_copy, "fn_80002000");
    CHECK_EQ_I64(got.more, 0);
    CHECK(!wt_lookup(a, &r, 0x80009999, 3, &got));
    /* a helper row without a pair: ps2, tier and evidence are None */
    REQUIRE(wt_lookup(a, &r, 2147500000, 3, &got));
    CHECK_EQ_I64(got.ps2, WT_NONE);
    CHECK(got.tier == NULL && got.evidence == NULL);
    CHECK_EQ_I64(got.helpers.v[0].size, 0x40);
    avp_rmtree(d, AVP_RMTREE_IGNORE_ERRORS);
}

AVP_TEST(lookup_order) {
    AvpArena *a = avp_arena();
    const char *d = "build/tmp/witness_lookup_order_test";
    avp_rmtree(d, AVP_RMTREE_IGNORE_ERRORS);
    REQUIRE(avp_makedirs(d, true));
    ShsmRepo r;
    memset(&r, 0, sizeof r);
    r.index_dir = d;
    WtOrderRank *rows = NULL;
    size_t n = 7;
    CHECK(!wt_lookup_order(a, &r, &rows, &n)); /* no database: no evidence */
    CHECK_EQ_I64((i64)n, 0);
    sqlite3 *db = NULL;
    REQUIRE(sqlite3_open(wt_db_path(a, &r), &db) == SQLITE_OK);
    sql(db, WT_TABLES_SQL);
    sql(db, "insert into fn values ('ps2', 4096, 'p0', NULL, NULL, 4, 1, 0, 0), ('ps2', 8192, 'p1', NULL, NULL, 4, 1, 0, 0),"
            " ('ps2', 12288, 'p2', NULL, NULL, 4, 1, 0, 0)");
    sql(db, "insert into pairs values (2147491840, 4096, 'call', ''), (2147487744, 12288, 'call', '')");
    sqlite3_close(db);
    REQUIRE(wt_lookup_order(a, &r, &rows, &n));
    REQUIRE(n == 2);
    CHECK_EQ_I64(rows[0].wii, 0x80001000); /* Wii address order */
    CHECK_EQ_I64(rows[0].rank, 2);
    CHECK_EQ_I64(rows[1].wii, 0x80002000);
    CHECK_EQ_I64(rows[1].rank, 0);
    avp_rmtree(d, AVP_RMTREE_IGNORE_ERRORS);
}

AVP_TEST_MAIN("witness")
