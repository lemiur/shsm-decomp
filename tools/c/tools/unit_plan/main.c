/* unit_plan.exe: tools/unit_plan.py (proposed translation-unit boundaries with their evidence). */
#include "unit_plan.h"

static int run(int argc, char **argv) { return unit_plan_main(argc, argv); }

AVP_MAIN(run, "unit_plan")
