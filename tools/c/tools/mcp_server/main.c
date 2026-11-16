/* mcp_server.exe: tools/mcp/server.py in C (mcp_cli.c). */
#include "mcp_server.h"

#include "avp/base.h"

static int run(int argc, char **argv) { return mcp_main(argc, argv); }

AVP_MAIN(run, "mcp_server")
