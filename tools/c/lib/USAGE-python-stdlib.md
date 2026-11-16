# Python standard library used by the Python tools, and its C coverage

Frozen at retirement (generator removed with the Python tools); do not edit.
Imports are read with `ast` from every tracked `.py` under `tools/` (not `tools/c/`) and `configure.py`, at any
depth. Status: covered, partly, another lane (a porting lane in progress builds it), missing, not needed (a
language feature in C).

## By module

| module | tools | status | C module |
|---|---|---|---|
| `os` | 106 | covered | core avp/fs.h |
| `sys` | 90 | covered | core avp/base.h |
| `json` | 62 | covered | core avp/json.h |
| `re` | 51 | covered | lib/shsm_re.h (PCRE2, Python semantics) |
| `shutil` | 48 | covered | core avp/fs.h |
| `unittest` | 43 | not needed | core avp/test.h |
| `subprocess` | 41 | covered | core avp/proc.h |
| `io` | 39 | partly | core avp/str.h (AvpBuf), avp/fs.h |
| `tempfile` | 39 | covered | core avp/fs.h |
| `time` | 31 | covered | core avp/clock.h |
| `argparse` | 29 | covered | core avp/args.h |
| `contextlib` | 29 | not needed | - |
| `struct` | 29 | not needed | plain C byte access (no module needed) |
| `hashlib` | 18 | covered | core avp/hash.h |
| `bisect` | 14 | covered | core avp/sort.h |
| `collections` | 12 | covered | core avp/map.h, avp/vec.h |
| `sqlite3` | 10 | another lane | asm/sqlite lane (shsm_asmindex, sqlite vendored there) |
| `shlex` | 9 | covered | lib/shsm_text.h (split); avp_list2cmdline |
| `concurrent` | 8 | covered | core avp/pool.h |
| `types` | 8 | not needed | - |
| `difflib` | 6 | covered | core avp/difflib.h |
| `importlib` | 6 | not needed | linking |
| `pathlib` | 6 | covered | core avp/fs.h |
| `typing` | 6 | not needed | - |
| `pickle` | 4 | partly | core avp/cache.h (binary cache format, not pickle's) |
| `stat` | 4 | covered | core avp/fs.h |
| `threading` | 4 | covered | core avp/pool.h |
| `ctypes` | 3 | partly | core avp/win.h (the Windows calls the tools make) |
| `fnmatch` | 3 | covered | core avp/fs.h |
| `itertools` | 3 | not needed | loops |
| `platform` | 3 | partly | core avp/win.h |
| `random` | 3 | missing | that project's pyrandom (Mersenne Twister) not taken |
| `socket` | 3 | another lane | sockets / JSON-RPC lane |
| `traceback` | 3 | not needed | - |
| `atexit` | 2 | not needed | atexit() / AVP_MAIN |
| `datetime` | 2 | covered | core avp/clock.h |
| `inspect` | 2 | not needed | - |
| `math` | 2 | covered | C <math.h>; avp/fmt.h (fsum, round) |
| `signal` | 2 | partly | core avp/proc.h (kill), avp/base.h |
| `tarfile` | 2 | covered | lib/shsm_archive.h (reading, selective extraction) |
| `textwrap` | 2 | partly | avp/args.h (help wrapping only) |
| `urllib` | 2 | missing | download_tool.py only |
| `xml` | 2 | missing | - |
| `zlib` | 2 | covered | lib/shsm_zlib.h (zlib-ng) |
| `__future__` | 1 | not needed | - |
| `_sha2` | 1 | covered | core avp/hash.h |
| `_winapi` | 1 | partly | core avp/win.h, avp/proc.h |
| `ast` | 1 | missing | Python source parsing (generators only) |
| `binascii` | 1 | partly | crc32: lib/shsm_zlib.h; hexlify: avp/str.h |
| `copy` | 1 | not needed | - |
| `dataclasses` | 1 | not needed | structs |
| `fcntl` | 1 | not needed | POSIX-only branch (Windows tools) |
| `functools` | 1 | not needed | loops, avp/sort.h comparators |
| `gc` | 1 | not needed | - |
| `glob` | 1 | covered | core avp/fs.h |
| `marshal` | 1 | missing | objcache.py's cache headers: no C reader yet |
| `msvcrt` | 1 | partly | core avp/win.h |
| `runpy` | 1 | not needed | - |
| `secrets` | 1 | missing | - |
| `ssl` | 1 | missing | - |
| `tokenize` | 1 | missing | - |
| `tomllib` | 1 | missing | - |
| `warnings` | 1 | not needed | - |
| `zipfile` | 1 | missing | that project has a zip reader (core zip*.c), not taken: only download_tool.py uses it |

## By tool

Modules in **bold** have no C counterpart yet (missing or another lane's).

| tool | standard-library imports |
|---|---|
| `configure.py` | argparse, pathlib, sys, typing |
| `tools/__init__.py` | - |
| `tools/brief.py` | concurrent, json, os, re, struct, subprocess, sys, time |
| `tools/build_c.py` | argparse, os, shutil, subprocess, sys |
| `tools/changes_fmt.py` | argparse, json, os, pathlib, typing |
| `tools/check_docs.py` | importlib, io, os, re, subprocess, sys, **tokenize** |
| `tools/classes.py` | argparse, bisect, collections, hashlib, json, os, re, sys |
| `tools/codecmp.py` | argparse, bisect, difflib, os, re, struct, subprocess, sys |
| `tools/common/__init__.py` | - |
| `tools/common/__main__.py` | sys, time |
| `tools/common/_fixture.py` | json, os, tempfile |
| `tools/common/asm.py` | json, os, re |
| `tools/common/asmindex.py` | collections, concurrent, os, **sqlite3**, time, **urllib** |
| `tools/common/cache.py` | hashlib, os, struct, tempfile, time |
| `tools/common/cli.py` | argparse, concurrent, json, os, re, subprocess, sys, time, traceback |
| `tools/common/compare.py` | difflib, os, re, shutil, tempfile |
| `tools/common/libraries.py` | bisect, os |
| `tools/common/mangle.py` | re |
| `tools/common/repo.py` | **ast**, os, re |
| `tools/common/report.py` | json, os |
| `tools/common/symbols.py` | bisect, os, re |
| `tools/common/test_common.py` | argparse, contextlib, io, json, os, shutil, **sqlite3**, subprocess, sys, tempfile, time, unittest |
| `tools/common/test_common_real.py` | os, shutil, tempfile, time, unittest |
| `tools/common/testing.py` | os, re, unittest |
| `tools/common/units.py` | json, os, re, shlex |
| `tools/decomp.py` | bisect, hashlib, json, os, re, shutil, struct, subprocess, sys, time |
| `tools/decompctx.py` | argparse, fnmatch, os, re, typing |
| `tools/doctor.py` | argparse, glob, hashlib, json, os, re, subprocess, sys |
| `tools/dolphin.py` | ctypes, inspect, json, os, re, **secrets**, signal, **socket**, struct, subprocess, sys, time |
| `tools/download_tool.py` | argparse, io, os, pathlib, platform, shutil, **ssl**, stat, typing, **urllib**, **zipfile** |
| `tools/draft.py` | argparse, bisect, hashlib, json, os, re, shutil, sys, threading |
| `tools/eqcheck.py` | collections, json, os, re, sys, time, zlib |
| `tools/flagsweep.py` | argparse, concurrent, itertools, json, os, re, shlex, shutil, subprocess, sys, time |
| `tools/fragments.py` | collections, hashlib, json, os, re, **sqlite3**, sys, time |
| `tools/gate.py` | argparse, fnmatch, hashlib, importlib, json, os, re, shutil, stat, sys, time |
| `tools/gatecheck.py` | argparse, bisect, os, re, struct, sys |
| `tools/hashes.py` | binascii, os, re, **sqlite3**, struct, sys |
| `tools/hooks/pre_commit.py` | argparse, os, subprocess, sys |
| `tools/ledger.py` | argparse, difflib, json, os, re, subprocess, sys |
| `tools/levers.py` | collections, concurrent, difflib, hashlib, itertools, json, os, re, shutil, sys, time |
| `tools/match.py` | json, os, shutil, sys, tempfile, threading, time |
| `tools/mcp/server.py` | argparse, inspect, json, os, sys, traceback |
| `tools/merge_lane.py` | argparse, collections, importlib, io, json, os, re, shutil, stat, subprocess, sys, tarfile, tempfile, time |
| `tools/mwcc_trace/cc.py` | argparse, os, re, shlex, subprocess, sys |
| `tools/mwcc_trace/fdiff.py` | argparse, json, os, shutil, subprocess, sys |
| `tools/mwcc_trace/pe_disasm.py` | bisect, os, pickle, re, struct, sys |
| `tools/mwcc_trace/permuter_run.py` | os, sys |
| `tools/mwcc_trace/specs/cse_quals.py` | - |
| `tools/mwcc_trace/specs/first_fn_copyprop.py` | - |
| `tools/mwcc_trace/specs/first_fn_patch.py` | os |
| `tools/mwcc_trace/symbols.py` | argparse, bisect, os, re, struct, sys |
| `tools/mwcc_trace/test_fdiff.py` | contextlib, io, os, shutil, subprocess, sys, tempfile, unittest |
| `tools/mwcc_trace/test_symbols.py` | os, sys, unittest |
| `tools/mwcc_trace/wdbg.py` | ctypes, os, runpy, shlex, struct, subprocess, sys, traceback |
| `tools/names.py` | functools, os, re, struct, sys |
| `tools/neutral.py` | hashlib, importlib, io, json, os, re, shutil, subprocess, sys, tarfile, time |
| `tools/ninja_syntax.py` | io, os, pathlib, re, textwrap, typing |
| `tools/objcache.py` | _sha2, argparse, concurrent, fcntl, hashlib, **marshal**, msvcrt, os, **random**, re, shutil, struct, subprocess, sys, tempfile, time, zlib |
| `tools/permute.py` | argparse, hashlib, os, re, shlex, subprocess, sys |
| `tools/pick.py` | json, os, re, sys |
| `tools/project.py` | io, json, math, os, pathlib, platform, shlex, subprocess, sys, typing |
| `tools/query.py` | bisect, json, math, os, re, struct, sys, types |
| `tools/rules/check_rules.py` | argparse, hashlib, json, os, pickle, re, struct, subprocess, sys |
| `tools/rules/test_check_rules.py` | atexit, contextlib, importlib, io, json, os, re, shutil, subprocess, sys, tempfile, unittest |
| `tools/run_tests.py` | argparse, concurrent, fnmatch, json, os, re, signal, subprocess, sys, threading, time |
| `tools/score_history.py` | argparse, collections, datetime, hashlib, importlib, json, os, re, **sqlite3**, subprocess, sys, time |
| `tools/scripts/elf_symbols.py` | collections, os, re, struct, sys |
| `tools/scripts/iso9660_extract.py` | os, re, struct, sys |
| `tools/slot.py` | _winapi, argparse, datetime, itertools, json, os, **socket**, stat, subprocess, sys, time |
| `tools/status.py` | os, re, sys, tempfile, **xml** |
| `tools/test_brief.py` | concurrent, contextlib, io, json, os, shutil, **sqlite3**, sys, types, unittest |
| `tools/test_c_tools.py` | os, shutil, subprocess, sys, unittest |
| `tools/test_check_docs.py` | contextlib, io, json, os, shutil, sys, tempfile, types, unittest |
| `tools/test_classes.py` | atexit, contextlib, io, json, os, shutil, subprocess, sys, tempfile, unittest |
| `tools/test_codecmp.py` | gc, os, shutil, struct, sys, tempfile, unittest, warnings |
| `tools/test_decomp.py` | contextlib, io, json, os, shutil, struct, sys, tempfile, unittest |
| `tools/test_doctor.py` | contextlib, hashlib, io, json, os, shutil, subprocess, sys, tempfile, unittest |
| `tools/test_dolphin.py` | contextlib, io, json, os, shutil, **socket**, struct, subprocess, sys, threading, time, unittest |
| `tools/test_draft.py` | json, os, shutil, sys, tempfile, types, unittest |
| `tools/test_eqcheck.py` | contextlib, io, os, **random**, re, sys, unittest |
| `tools/test_flagsweep.py` | os, shlex, shutil, sys, tempfile, unittest |
| `tools/test_fragments.py` | contextlib, io, json, os, shutil, sys, tempfile, unittest |
| `tools/test_gate.py` | hashlib, io, json, os, shutil, sys, tempfile, unittest |
| `tools/test_gatecheck.py` | contextlib, io, os, **random**, struct, sys, tempfile, unittest |
| `tools/test_hashes.py` | contextlib, io, json, os, struct, sys, tempfile, unittest |
| `tools/test_hooks.py` | contextlib, io, os, shutil, subprocess, sys, tempfile, unittest |
| `tools/test_ledger.py` | contextlib, copy, io, json, os, re, shutil, sys, tempfile, unittest |
| `tools/test_levers.py` | collections, os, shutil, subprocess, sys, tempfile, types, unittest |
| `tools/test_match.py` | contextlib, io, json, os, shutil, sys, tempfile, types, unittest |
| `tools/test_mcp_server.py` | json, os, shutil, struct, subprocess, sys, unittest |
| `tools/test_merge_lane.py` | contextlib, io, json, os, shutil, subprocess, sys, tempfile, unittest |
| `tools/test_names.py` | contextlib, io, json, os, shutil, struct, sys, unittest |
| `tools/test_neutral.py` | os, shutil, sys, tempfile, unittest |
| `tools/test_objcache.py` | io, os, shutil, subprocess, sys, tempfile, time, unittest |
| `tools/test_permute.py` | contextlib, io, os, shlex, shutil, sys, tempfile, **tomllib**, unittest |
| `tools/test_pick.py` | contextlib, io, json, os, shutil, sys, time, unittest |
| `tools/test_project.py` | ctypes, io, json, os, shlex, shutil, subprocess, sys, tempfile, unittest |
| `tools/test_query.py` | contextlib, io, json, os, shutil, struct, sys, unittest |
| `tools/test_run_tests.py` | contextlib, io, json, os, shutil, subprocess, sys, tempfile, textwrap, time, unittest |
| `tools/test_score_history.py` | io, json, os, **sqlite3**, subprocess, sys, tempfile, time, unittest |
| `tools/test_slot.py` | contextlib, io, json, os, shutil, subprocess, sys, tempfile, time, unittest |
| `tools/test_status.py` | json, os, shutil, sys, tempfile, unittest, **xml** |
| `tools/test_twins.py` | contextlib, io, json, os, sys, unittest |
| `tools/test_unit.py` | contextlib, io, json, os, struct, subprocess, sys, tempfile, unittest |
| `tools/test_unit_plan.py` | contextlib, io, os, sys, tempfile, time, unittest |
| `tools/test_wall.py` | contextlib, io, json, os, re, shutil, sys, tempfile, types, unittest |
| `tools/test_witness.py` | contextlib, io, json, os, shutil, **sqlite3**, struct, sys, tempfile, types, unittest |
| `tools/transform_dep.py` | argparse, os, platform |
| `tools/twins.py` | hashlib, os, re, **sqlite3**, sys |
| `tools/unit.py` | __future__, argparse, bisect, contextlib, dataclasses, hashlib, io, json, os, pathlib, pickle, re, shutil, struct, subprocess, sys, time |
| `tools/unit_plan.py` | bisect, collections, json, os, pickle, re, sys |
| `tools/wall.py` | bisect, collections, difflib, json, os, re, struct, sys, time |
| `tools/witness.py` | bisect, collections, difflib, hashlib, json, os, re, **sqlite3**, struct, sys, time |
