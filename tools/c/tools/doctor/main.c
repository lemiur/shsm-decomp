/* doctor.exe: the front end of tools/c/tools/doctor (a port of python tools/doctor.py). */
#include "doctor.h"

static int run(int argc, char **argv) { return doctor_main(argc, argv); }

AVP_MAIN(run, "doctor")
