# fdiff: accepted differences from tools/mwcc_trace/fdiff.py

Proven equal to the retired Python tools/mwcc_trace/fdiff.py by the differential harness, removed at retirement, commit 66ee84d3.

## D-fdiff-1. A tool that cannot start
- Behaviour: when ninja or objdiff-cli is found (tools/bin or PATH) but cannot be started, fdiff.exe prints
  `FileNotFoundError: <reason>` on stderr and exits 1, after whatever stdout it had written; the Python dies with a
  FileNotFoundError traceback (exit 1).
- Reason: the traceback text is the interpreter's; stdout, exit code and stderr class are the same.

## D-fdiff-2. The target check's error text when dtk fails
- Behaviour: when `dtk elf disasm` of the compiled object fails, the message is `target check could not run
  (CalledProcessError: Command '<dtk path>' returned non-zero exit status N.); --no-targets skips it`; the Python's
  str(CalledProcessError) shows the whole command list, and a dtk that cannot start is a FileNotFoundError there.
  The class of a failed retail-listing read is the C file layer's (FileNotFoundError, PermissionError, ...), as in
  Python.
- Reason: the text comes from the shared `cc_load_object` (lib/cc_listing.c), which reports only the dtk path; dtk
  is present in every configured checkout and does not fail on objects mwcceppc wrote. Exit code 2 and the
  `error: target check could not run (` prefix are the same.

## D-fdiff-3. Subprocess output decoding
- Behaviour: the output of ninja and objdiff-cli is decoded as UTF-8 with replacement; the Python's `text=True` uses
  the locale code page, strictly.
- Reason: both tools write ASCII here (objdiff's JSON escapes nothing outside ASCII for these symbols); the harness
  sample is identical. A non-ASCII byte in a ninja error tail could differ.
