/* tools/mcp/server.py in C: the operations of the dolphin tool (tools/c/tools/dolphin, linked as tool_dolphin) as Model
 * Context Protocol tools over stdio, one JSON-RPC 2.0 message per line.
 *
 *     McpServer s = {.root = root};                       // env_factory NULL: dolphin_env_new(root)
 *     AvpJson *tools = mcp_list_tools(a);                 // list_tools()
 *     AvpJson *res = mcp_call_tool(&s, a, name, args);    // Server.call_tool; NULL for an unknown tool (KeyError)
 *     AvpJson *resp = mcp_handle(&s, a, msg);             // Server.handle; NULL for a notification
 *     int code = mcp_serve(&s, in_handle, out);           // Server.serve over a Win32 handle and a FILE (binary)
 *
 * mcp_handle raises MCP_EXC_ATTRIBUTE (Python's AttributeError, message = str(e)) where the Python's handle raised
 * one (params that are not an object); mcp_serve answers it with -32603 for a single message and stops with exit 1
 * inside a batch, as the Python's uncaught exception did. */
#pragma once
#include "dolphin.h"

#include <stdio.h>

#define MCP_SERVER_NAME "shsm-dolphin"
#define MCP_SERVER_VERSION "1.0"
#define MCP_DEFAULT_PROTOCOL "2024-11-05"
#define MCP_TEXT_LIMIT 200

enum { MCP_PARSE_ERROR = -32700, MCP_INVALID_REQUEST = -32600, MCP_METHOD_NOT_FOUND = -32601,
       MCP_INVALID_PARAMS = -32602, MCP_INTERNAL_ERROR = -32603 };

/* exceptions the Python raised that the shared SHSM_EXC_* set has no code for */
enum { MCP_EXC_ATTRIBUTE = 160, MCP_EXC_OVERFLOW, MCP_EXC_UNICODE_DECODE };

typedef DolphinEnv *(*McpEnvFactory)(void *ctx, const char *root);
typedef struct {
    const char *root;          /* the checkout (Server(root)) */
    McpEnvFactory env_factory; /* NULL: dolphin_env_new(root) */
    void *env_ctx;
    bool initialized;          /* notifications/initialized seen */
} McpServer;

AvpJson *mcp_input_schema(AvpArena *a, const DolphinOp *op);
AvpJson *mcp_list_tools(AvpArena *a);
/* text_result(text, error) */
AvpJson *mcp_text_result(AvpArena *a, AvpStr text, bool error);
AvpJson *mcp_call_tool(McpServer *s, AvpArena *a, AvpStr name, const AvpJson *arguments);
AvpJson *mcp_handle(McpServer *s, AvpArena *a, const AvpJson *msg);
int mcp_serve(McpServer *s, void *in_handle, FILE *out);
/* type(e).__name__ for the codes a tool call can raise (SHSM_EXC_*, the Gdb* codes, MCP_EXC_*) */
const char *mcp_exc_class(int exc);
/* str(value) of a JSON value as Python prints it ("%s" % value) */
AvpStr mcp_py_str(AvpArena *a, const AvpJson *v);
/* the command line: server.py [--root DIR] */
int mcp_main(int argc, char **argv);
