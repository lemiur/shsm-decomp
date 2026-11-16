/* pre_commit.exe: the front end of tools/c/tools/pre_commit (a port of python tools/hooks/pre_commit.py). */
#include "pre_commit.h"

static int run(int argc, char **argv) { return pre_commit_main(argc, argv); }

AVP_MAIN(run, "pre_commit")
