# wdbg: accepted differences from tools/mwcc_trace/wdbg.py

Proven equal to the retired Python tools/mwcc_trace/wdbg.py by the differential harness, removed at retirement, commit 66ee84d3.

## D-wdbg-1. Specs are written in the wdbg spec language, not Python
- Behaviour: the `<spec>` argument is a file in the language of `SPEC.md` (statements `bp ... end`, `out`, `wr`,
  `snap`, `if/elif/else`, assignments; the functions `u8 u16 u32 rd hits sym env int str repr hex len fmt le8 le16
  le32`; thread-context fields as names), not a Python file defining `BPS`. A Python spec given to the C tool is a
  syntax error. The four shipped specs are translated in `specs/` with the same names (`.wdbg`); `first_fn_patch`
  keeps the `FIXMODE` (read once at load, default `fix`) and `FIXFN` (read at each hit, default `1`) environment
  variables.
- Reason: the C tool cannot run Python handlers, and the README forbids bridging to Python. The specs only read and
  write debuggee memory, read registers, count hits, format log lines and read the environment, which the language
  covers. Evidence: `diff/cases.txt` (21 cases: the three specs over game/PadRumble, MSL mem, RVL SDK OSAlarm and zlib
  adler32, extra flag words, the harness-only `diff/probe.wdbg` with snapshots, `sym`, `rd`, % conversions and
  if/elif/else, error cases), `diff/cases_env.txt` (`FIXMODE=break FIXFN=2`, 4 cases) and `diff/cases_break1.txt`
  (`FIXMODE=break`, 3 cases): every log.txt, o.o and snapshot equal (`diff/ref.py` gives the Python side the Python
  form of each spec).

## D-wdbg-2. One-line errors instead of tracebacks
- Behaviour: where the Python dies with an uncaught exception (too few arguments, unreadable `build.ninja` or spec,
  `CreateProcessW` failure, an error in a spec's top level) the C tool prints one line `<Class>: <message>` on stderr
  and exits 1 (a spec syntax error: `SyntaxError: <spec>:<line>: <message>`); the Python prints a traceback, also exit
  1. A handler error writes one log line `HANDLER ERR <Class>: <message>`; the Python writes `HANDLER ERR` followed by
  the multi-line traceback and a blank line. The messages are Python's where the language has the same error
  (`NameError: name 'x' is not defined`, `KeyError: 4635052`, `ZeroDivisionError: ...`).
- Reason: a traceback names Python source lines that do not exist in the C tool. Exit codes and the stderr class are
  equal (harness); the shipped specs raise no handler errors.

## D-wdbg-3. The compiler inherits wdbg.exe's error mode
- Behaviour: `AVP_MAIN` sets `SetErrorMode(FAILCRITICALERRORS | NOGPFAULTERRORBOX | NOOPENFILEERRORBOX)`, which the
  debugged mwcceppc.exe inherits; under the Python a crashing compiler could show a Windows error dialog.
- Reason: the project's crash and hang policy (`core/include/avp/base.h`): no tool may wait on a UI. The creation
  flags otherwise equal the Python's (`DEBUG_PROCESS`, handles not inherited, cwd = the checkout root).

## D-wdbg-4. `cstr` in the spec language; `may_alias` writes `name` out twice
- Behaviour: the spec language has one function the Python specs get from the language, `cstr(bytes)` (the bytes up to
  the first NUL as a string, bytes above 0x7f taken as Latin-1 and logged as UTF-8). `specs/may_alias.wdbg` uses it to
  read an object's name; the Python spec's helper `name(d, al)` is written out once for each operand because the
  language has no user functions, and its `KIND` dict is an if/elif chain.
- Reason: reading a C string from the debuggee is the only thing `may_alias.py` needs that the language lacked. Names
  are ASCII in practice, so the Latin-1 handling is not exercised. Evidence: `diff/cases.txt` runs `may_alias` over
  game/PadRumble and MSL mem; logs equal.

## Compiler crash log
- The Python's `EXCEPTION ... eip=... (<sym>) ...` line and `  stack:` line (32 words) for any exception other than
  breakpoint and single-step are ported in `wdbg_debug.c` (`log_crash`) with Python's `%#x` form (`0x0` for zero); the
  exception is then passed on (`DBG_EXCEPTION_NOT_HANDLED`), as before. No harness case crashes the compiler.

## Harness notes (not behaviour differences)
- `diff/cases.py` compares a snapshot named `s2.bin` (probe.wdbg's `snap "s2"` over the default range
  0x687000-0x7c8000) by size only: two runs of the Python side give different contents (the compiler's heap pointers
  and run state). `s1.bin` (0x687000-0x689000), every log and every o.o are compared byte for
  byte.
- Debuggee stack contents depend on the command line (the output directory differs between the sides), so the probe
  spec reads no stack words.

## Post-retirement changes (after the Python tools were retired)

## D-wdbg-5. Compiler builds: `build`, `compiler()`, `addr()`; handler errors stop their handler
- Behaviour: a spec can name the compiler builds its addresses are valid for (`build "Wii/1.0", "Wii/1.0a"`; another build
  stops the load with a `BuildError` line) and take addresses by NB11 name (`addr("IRO_Optimizer")`); the first line of
  `log.txt` is `compiler: <build> (...)`; `sym()` reads the names of the run's compiler, where the Python always read
  `Wii/1.0`. A handler that raises writes one `HANDLER ERR <Class>: <message> (<spec>:<line>, bp <addr>; this handler is stopped)`
  line and is not run again (hits are still counted); a top-level error names the spec line too.
- Reason: `iro_gate` was written for `Wii/1.0` and run on FMOD units, which build with `Wii/1.0a`: its breakpoints sat in
  unrelated code, the handler that sets `first` never ran, and every other handler raised `NameError: name 'first' is not
  defined` at each event. The build line turns that into one message before anything runs; the one-line-per-handler stop
  keeps a spec error from burying the log.

## D-wdbg-6. `iro-scan`
- Behaviour: `wdbg.exe iro-scan ...` (SPEC.md does not cover it; `--help`) runs the single-run path once per unit with the
  `iro_gate` spec and prints one line per non-exact function: IR optimiser on or off, the first refusal, and a flag when
  it is off in a function whose siblings in the unit have it on.
- Reason: the wall passes asked for the check as a command (a pointer of an aligned typedef compared with NULL turns the
  IR optimiser off for the whole function).
