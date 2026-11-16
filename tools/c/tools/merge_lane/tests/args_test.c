/* TestArgs of tools/test_merge_lane.py: argument errors exit 2 before anything runs.
 *   TestArgs.test_merge_needs_reviewed       -> merge_needs_reviewed
 *   TestArgs.test_merge_needs_update         -> merge_needs_update
 *   TestArgs.test_update_needs_all_core_steps -> update_needs_all_core_steps
 *   TestArgs.test_unknown_step               -> unknown_step
 * differs: the Python calls ml.parse_args and expects SystemExit(2); the port's parser is inside ml_main, which
 * returns 2 (argparse's exit code) with the usage on stderr instead of exiting. The Python's LaneTest fixture is not
 * needed by these tests and is not built. test_unknown_step only expected SystemExit; argparse's code is 2. */
#include "ml_test_fixtures.h"

static int parse(const char *const *args) {
    size_t n = 0;
    while (args[n]) n++;
    char **argv = AVP_NEWN(TA, char *, n + 2);
    argv[0] = "merge_lane";
    for (size_t i = 0; i < n; i++) argv[i + 1] = (char *)args[i];
    argv[n + 1] = NULL;
    AvpBuf out;
    avp_buf_init(&out, TA);
    MlHooks h = {.out = &out};
    int rc = ml_main((int)n + 1, argv, &h);
    CHECK_MSG(!tx_has(avp_buf_str(&out), "== summary =="), "a step ran: %.*s", AVP_SF(avp_buf_str(&out)));
    return rc;
}

AVP_TEST(merge_needs_reviewed) {
    static const char *const args[] = {"--slot", "s", "--branch", "b", "--update", "--merge", NULL};
    CHECK_EQ_I64(parse(args), 2);
}

AVP_TEST(merge_needs_update) {
    static const char *const args[] = {"--slot", "s", "--branch", "b", "--merge", "--reviewed", NULL};
    CHECK_EQ_I64(parse(args), 2);
}

AVP_TEST(update_needs_all_core_steps) {
    static const char *const args[] = {"--slot", "s", "--branch", "b", "--update", "--steps", "rebase,gate", NULL};
    CHECK_EQ_I64(parse(args), 2);
}

AVP_TEST(unknown_step) {
    static const char *const args[] = {"--slot", "s", "--branch", "b", "--steps", "gate,bogus", NULL};
    CHECK_EQ_I64(parse(args), 2);
}

AVP_TEST_MAIN("merge_lane_args")
