/* sibling: pair retail functions with a symbol-bearing sibling binary (front end). */
#include "sibling.h"

#include "avp/base.h"

static int run(int argc, char **argv) { return sibling_main(argc, argv); }

AVP_MAIN(run, "sibling")
