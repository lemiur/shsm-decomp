/* merge_lane: the lane merge procedure (tools/merge_lane.py). */
#include "merge_lane.h"

static int run(int argc, char **argv) { return ml_main(argc, argv, NULL); }

AVP_MAIN(run, "merge_lane")
