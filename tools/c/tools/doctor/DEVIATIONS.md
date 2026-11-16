# doctor: deviations from tools/doctor.py

Proven equal to the retired Python tools/doctor.py by the differential harness, removed at retirement, commit 66ee84d3.

## D-doctor-1. Default root
- Behaviour: without `--root`, the C doctor checks the checkout it is run from (`shsm_find_root`); doctor.py checks the
  checkout that holds the script (`dirname(dirname(__file__))`).
- Reason: the C tool has no script path; run from a checkout (as every caller and the harness do) both name the same
  root. With `--root` the two agree exactly (harness cases `--root ...`).

## D-doctor-3. The asm-index line runs asmindex.exe
- Behaviour: the `asm-index` line runs `asmindex.exe --stats --json --root <root>` from the folder of doctor.exe
  (`build/tools/`); the fix hints are `build/tools/asmindex.exe [--stats]`. When asmindex.exe is absent the line is
  `WARN asm-index: index --stats exited -1 without JSON`.
- Reason: asmindex.exe is built with doctor.exe by the same build.

## D-doctor-4. The ghidra line runs decomp.exe
- Behaviour: the `ghidra` line runs `decomp.exe --root <root> status` from the folder of doctor.exe (`build/tools/`);
  the hints are `build/tools/decomp.exe status|setup`. When decomp.exe is absent the line is `MISSING ghidra:
  decomp.exe not built` (hint `tools/build_c.cmd`).
- Reason: decomp.exe is built with doctor.exe by the same build.

## D-doctor-5. `--debug` on an error
- Behaviour: with `--debug`, an error raised by the checks prints one line `<ExceptionClass>: <message>` on stderr and
  exits 1; doctor.py re-raises, printing a traceback (also exit 1).
- Reason: a C tool has no traceback; the exit code and the stderr class are the same.

## D-doctor-7. `--install-hooks` installs the C pre-commit shim
- Behaviour: `--install-hooks` copies `tools/hooks/pre-commit-c` (the shim that runs `build/tools/pre_commit.exe` of
  the checkout being committed, and says so and exits 0 when the executable is not built) into the hooks directory,
  LF only. A checkout without `pre-commit-c` is an error (exit 2). A hook carrying the `shsm-hook` marker line counts
  as doctor's own for the `hooks` line, for reinstalling over it and for `--uninstall-hooks`; this includes an older
  shim that fell back to a Python script.
- Reason: the owner's ruling of 2026-11-13: a checkout whose C tools are built commits through the C hook.
