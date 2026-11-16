/* tools/mcp/server.py's main(): the parser (help text with the tool list as epilog) and the stdio loop. */
#include "mcp_server.h"

#include "avp/avp.h"

#include <fcntl.h>
#include <io.h>
#include <windows.h>

static const char *epilog(AvpArena *a) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putz(&b, "tools:");
    AvpJson *tools = mcp_list_tools(a);
    for (size_t i = 0; i < avp_json_len(tools); i++) {
        AvpJson *t = avp_json_at(tools, i);
        AvpStr desc = avp_json_get_str(t, "description", AVP_S(""));
        i64 nl = avp_str_findc(desc, '\n');
        if (nl >= 0) desc.n = (size_t)nl;
        desc = avp_u8_slice(desc, 0, 96);
        AvpStr name = avp_json_get_str(t, "name", AVP_S(""));
        avp_buf_printf(&b, "\n  %-18.*s %.*s", AVP_SF(name), AVP_SF(desc));
    }
    return avp_cz(a, avp_buf_str(&b));
}

/* ROOT: the checkout around this program (<repo>/build/tools/mcp_server.exe) */
static const char *default_root(AvpArena *a) {
    return avp_path_normpath(a, avp_join(a, avp_exe_dir(), "..", ".."));
}

int mcp_main(int argc, char **argv) {
    AvpArena *a = avp_arena();
    AvpArgParser *ap = avp_args_parser(
        a, &(AvpArgParserDesc){
               .prog = "mcp_server.exe",
               .formatter = AVP_FORMAT_RAW_DESCRIPTION,
               .description = "MCP server (stdio, JSON-RPC 2.0, one message per line) over dolphin.exe: observe "
                              "the running game in Dolphin. Enabled by the user copying the entry of "
                              "tools/mcp/mcp.example.json into .mcp.json.",
               .epilog = epilog(a)});
    avp_arg(ap, &(AvpArgDesc){.names = "--root", .help = "checkout to work on (default: the one around this file)"});
    AvpArgs *args = avp_args_parse(ap, argc, argv);
    AvpStr root = avp_args_str(args, "root");
    /* sys.stdout.reconfigure(encoding="utf-8", newline="\n"): no \r\n translation from here on */
    fflush(stdout);
    _setmode(_fileno(stdout), _O_BINARY);
    McpServer s = {.root = root.n ? avp_cz(a, root) : default_root(a)};
    return mcp_serve(&s, GetStdHandle(STD_INPUT_HANDLE), stdout);
}
