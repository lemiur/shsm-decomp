# mcp_server: the dolphin tool's MCP server, over the dolphin tool's library (cmake/shsm_tool.cmake documents shsm_add_tool).
shsm_add_tool(mcp_server LIBS tool_dolphin)

# The pipe tests run build/tools/mcp_server.exe.
add_dependencies(test_mcp_server_pipes_test mcp_server)
