# changes_fmt.exe: allowed differences from `python tools/changes_fmt.py`

Proven equal to the retired Python tools/changes_fmt.py by the differential harness, removed at retirement, commit 66ee84d3. Last run: 406 cases.

`build/tools/changes_fmt.exe` (tools/c/tools/changes_fmt) gives the Python's stdout bytes (CRLF line ends through a pipe,
the output text in the locale's code page), the `-o` markdown file (UTF-8, CRLF line ends, written only after the whole
report was formatted), the exit codes and the argparse usage and errors (exit 2). Failures the Python reports as an
uncaught exception (a missing or malformed file, a document of the wrong shape, a file on another drive, a name the
stdout code page cannot hold) exit 1 with a traceback on stderr.

Two Python behaviours the port keeps on purpose: the file is read with the locale's default encoding, not UTF-8 (a
UTF-8 name is mojibake of that many characters, which changes the column widths, and a byte cp1252 does not define,
0x81 0x8D 0x8F 0x90 0x9D, fails); and `os.path.relpath(file, root)` is opened relative to the working directory, so the
tool only finds its input when it is run from the repository root (or with an absolute path inside it).

## Deviations

## D-changes_fmt-1. Traceback text
- Behaviour: an uncaught Python exception is reported as `changes_fmt.exe: error:`, one `File` line and
  `<Class>: <message>`; CPython prints every frame with source lines. The exit code is 1 in both.
- Reason: the harness compares the stderr class (empty or not), not the frames; the class and message of the
  exception are kept.

## D-changes_fmt-2. Code pages other than cp1252
- Behaviour: for the default encoding the port decodes cp1252 as Python's codec does (five undefined bytes are errors,
  a character outside the table cannot be printed) and UTF-8 when `PYTHONUTF8=1` or the ANSI code page is 65001. Any
  other ANSI code page goes through `MultiByteToWideChar`/`WideCharToMultiByte` (an invalid sequence is replaced,
  where Python raises). `PYTHONIOENCODING`, `-X utf8` and `PYTHONLEGACYWINDOWSSTDIO` are not honoured, and
  `UnicodeEncodeError` for a lone surrogate is raised for every encoding but the text of the message differs.
- Reason: only this machine's code page (1252) is exercised by the project; the report documents are ASCII.

## D-changes_fmt-3. Integers beyond 64 bits
- Behaviour: an integer too big for 64 bits (`"fuzzy_match_percent": 100000000000000000000`) is read as a double.
  Equal-looking big values compare as the nearest doubles, where Python compares exactly (`10**20 > 1e20` is false in
  both, but `12345678901234567890 > 1.2345678901234567e19` is true only in Python).
- Reason: objdiff-cli writes percentages as floats (and counts as strings); the case `big_int_equal_float` agrees.

## D-changes_fmt-4. Containers where numbers are expected
- Behaviour: a `list` or `dict` in a value or name position raises TypeError. Python orders two lists
  (`[1] > [0]`) and prints a list or dict name through `str()` in the markdown table (its length is taken first, which
  works for them), so it carries on.
- Reason: objdiff-cli never writes these; a document that does is not a report.

## D-changes_fmt-5. Repository root
- Behaviour: the root `relpath` is taken from is the checkout the tool runs in (`avp_root()`: `AVP3_ROOT`, the
  working directory's checkout, or the one holding the executable); the Python uses the directory of its own script.
- Reason: the same directory when the tool is run in the checkout it belongs to.

## D-changes_fmt-6. JSON nesting limit
- Behaviour: a document nested deeper than the C parser's limit raises `RecursionError` where Python's limit differs
  (core/json.h).
- Reason: shared-library behaviour; a report is a few levels deep.

## Traceback emulation dropped
- The emulated Python traceback text was dropped after the Python tools were retired: an internal error is one line on stderr, `changes_fmt.exe: error: <Class>: <message>`, with the same exit code as before.

## D-changes_fmt-1. The report file is opened as given
- Behaviour: `changes_fmt.exe <file>` opens `<file>` relative to the working directory (or as an absolute path). The Python
  opened `os.path.relpath(<file>, <root of the tool>)`, which is the same file only when the working directory is that root.
- Reason: a lane slot's `build.ninja` (written by the main checkout's `configure.exe` when `merge_lane.exe --slot` runs) calls
  the main checkout's `changes_fmt.exe` with the slot's report from the slot as working directory; the root-relative path
  named a file that does not exist there and the `changes` step failed.
