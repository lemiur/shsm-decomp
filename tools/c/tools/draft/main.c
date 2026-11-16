/* draft.exe: tools/draft.py in C (draft.h). */
#include "draft.h"

static int run(int argc, char **argv) { return draft_main(argc, argv); }

AVP_MAIN(run, "draft")
