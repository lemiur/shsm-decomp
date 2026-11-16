# decompctx.exe: allowed differences from `python tools/decompctx.py`

Proven equal to the retired Python tools/decompctx.py by the differential harness, removed at retirement, commit 66ee84d3. Last run: 191 cases, 0 differ.

`build/tools/decompctx.exe` (tools/c/tools/decompctx) gives the Python's stdout (progress lines), exit codes, argparse
usage and errors, and writes the same output and depfile bytes (UTF-8, CRLF line ends as a Windows text-mode `open`
writes them). It repeats the Python's behaviour, including its quirks: the file is read as UTF-8 and, when that or its
processing raises, read again with the locale's encoding and processed again (progress lines and the guard set are not
rolled back, so a failing nested include repeats "Processing file" lines once per enclosing level); `#include` lines
are matched after `str.strip()` with the lazy `(.+?)[>"]` group; a header guard or `#pragma once` is recognised on
line 0 only; `.s` includes are kept as plain lines; `exit("No include directories specified")` is exit 1; an
include that names a directory ends in a PermissionError traceback.

Exceptions end like the Python's uncaught ones (traceback on stderr, exit 1).


## D-decompctx-1. An include cycle without guards stops at a fixed depth
- Behaviour: nesting deeper than 300 files ends with `RecursionError: maximum recursion depth exceeded` (traceback, exit
  1) immediately.
- Reason: the Python recurses until its own limit, and its `except Exception` retry at every level then re-runs each
  subtree, so it takes exponentially long (no practical run ends). No source in this tree has such a cycle; a cycle with
  `#ifndef` guards or `#pragma once` is processed equally (cases `cycle`, `self`).

## D-decompctx-2. Root directory
- Behaviour: the output and depfile names are joined onto the repository root found by `avp_root()` (the current
  checkout, resolved with realpath), not onto the directory above the script.
- Reason: the executable lives in `build/tools/`, not in `tools/`. Both are the checkout the command is run from.
