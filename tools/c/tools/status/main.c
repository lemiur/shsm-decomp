/* status.exe: python tools/status.py in C (status_cli.c). */
#include "status.h"

static int run(int argc, char **argv) { return status_main(argc, argv); }

AVP_MAIN(run, "status")
