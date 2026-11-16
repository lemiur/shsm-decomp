# mcp_server.exe: allowed differences from `python tools/mcp/server.py`

Proven equal to the retired Python `tools/mcp/server.py` by the differential harness, removed at retirement, commit 66ee84d3.

`build/tools/mcp_server.exe` (tools/c/tools/mcp_server) gives the Python's stdout bytes (one compact JSON-RPC response
per line, `\n` line ends, ensure_ascii escapes), exit codes and stderr class, with the same 19 `dolphin_<op>` tools
and input schemas. The operations are the dolphin tool's C library (`tool_dolphin`), so the dolphin port's own
deviations (`tools/c/tools/dolphin/DEVIATIONS.md`, e.g. D-dolphin-1: `dolphin_launch` starts the C broker) apply to the
tool results unchanged.

Provenance: the stdin loop of `mcp_serve` (ReadFile chunks, `\n` / `\r\n` / lone `\r` line breaks) follows
`avp_mcp_serve_handles` in another project of the author's tools/c/core/src/mcp.c at commit
49e811503d7ba5c9cff92160557df057c50a8ab3; nothing else is taken from it (its MCP semantics are the Python SDK's, not
this server's).

## D-mcp_server-1. Traceback text on stderr
- Behaviour: where the Python prints a traceback (`traceback.print_exc()` after a tool failure that is not a
  UsageError, or the uncaught exception that ends the server: a batch whose message has non-object params, nesting
  too deep for json.loads, stdin that is not UTF-8), the C prints `mcp_server.exe: error:` and the
  exception's last line (`<class>: <message>`) only, without the frames.
- Reason: there are no Python frames to show. stdout, the exit code (0, or 1 for the uncaught exception) and the
  presence of stderr output are the same (`% stderr: class`); the error text in the tool result is identical.

## D-mcp_server-2. Integer arguments beyond 64 bits
- Behaviour: an `integer` argument whose value does not fit in an i64 (`"count": 1e30`, or a 30-digit literal) is
  accepted by the coercion as in the Python, but the dolphin operation then sees its default for that parameter
  (`dolphin_call` reads ints as i64), where the Python passed the unbounded int on (and failed later, or asked the
  stub for an absurd range).
- Reason: no operation has a meaningful value there (lengths, counts, depths, sizes are small); the C dolphin tool has
  the same i64 limit on its command line (argparse ints, D-B entries of the shared parser).

## D-mcp_server-3. The UTF-8 error position on stdin
- Behaviour: stdin is decoded in 8192-byte chunks as Python's TextIOWrapper does, and invalid UTF-8 ends the server
  before any line of the failing chunk is answered, as in the Python; the chunk boundaries follow the reads of the
  pipe, so for input that arrives in pieces (a live client) the position in the `UnicodeDecodeError` message on
  stderr and the lines answered before it may differ from the Python's.
- Reason: both read what the pipe has ready, up to 8192 bytes; for a script written at once (the harness) they agree
  (`badutf8*.jsonl`, `truncated_utf8.jsonl` cases). Only stderr text and the timing of the stop can differ.

## Traceback emulation dropped
- The emulated Python traceback text was dropped after the Python tools were retired: an internal error is one line on stderr, `mcp_server.exe: error: <Class>: <message>`, with the same exit code as before.
