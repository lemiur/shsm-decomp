/* The tool libraries the draft port calls link together into one image (this test is linked
 * with /WHOLEARCHIVE of each: CMakeLists.txt), and their entry points resolve. */
#include "avp/test.h"
#include "classes.h"
#include "draft.h"
#include "hashes.h"
#include "twins.h"
#include "unit_plan.h"

AVP_TEST(entry_points_resolve) {
    const void *fns[] = {
        (const void *)twins_load,       (const void *)twins_resolve,     (const void *)twins_of,
        (const void *)up_plan_for,      (const void *)unit_plan_main,    (const void *)hashes_var_names,
        (const void *)hashes_main,      (const void *)classes_db_load,   (const void *)classes_stale_reason,
        (const void *)classes_run_scan, (const void *)draft_main,
    };
    for (size_t i = 0; i < sizeof fns / sizeof *fns; i++) CHECK(fns[i] != NULL);
}

AVP_TEST_MAIN("draft")
