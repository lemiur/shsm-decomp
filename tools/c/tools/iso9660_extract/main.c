/* iso9660_extract.exe: the front end of tools/c/tools/iso9660_extract (a port of python tools/scripts/iso9660_extract.py). */
#include "iso9660_extract.h"

static int run(int argc, char **argv) { return iso9660_main(argc, argv); }

AVP_MAIN(run, "iso9660_extract")
