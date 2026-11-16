# transform_dep.exe: allowed differences from `python tools/transform_dep.py`

Proven equal to the retired Python tools/transform_dep.py by the differential harness, removed at retirement, commit 66ee84d3. Last run: 817 cases, 0 differ.

`build/tools/transform_dep.exe` (tools/c/tools/transform_dep) writes the Python's output file byte for byte (UTF-8,
CRLF line ends) and gives its exit codes and argparse usage and errors. It repeats the Python's Windows behaviour:
`$HOME` is read before the arguments (`KeyError: 'HOME'`, exit 1, even for `-h`; `$WINEPREFIX` replaces
`$HOME/.wine`), the input is read with the locale's default encoding (cp1252: a UTF-8 file is mojibake, the bytes
0x81 0x8D 0x8F 0x90 0x9D fail) in universal-newline mode, `in_wsl()` is false, a path that does not start with `z`
goes through `os.path.realpath(os.path.join(winedevices, path))` (an absolute drive path discards the prefix and is
canonicalised: drive letter, separators, the file system's case), and an empty path line is an `IndexError`
(exit 1, no output file written).

The tool prints nothing; the file each side writes is what was compared.

## Deviations

## D-transform_dep-1. Traceback text
- Behaviour: an uncaught Python exception is reported as `transform_dep.exe: error:`, one `File` line and
  `<Class>: <message>`; CPython prints every frame. The exit code is 1 in both.
- Reason: the harness compares the stderr class (empty or not), not the frames.

## D-transform_dep-2. Code pages other than cp1252
- Behaviour: cp1252 is decoded as Python's codec does; UTF-8 when `PYTHONUTF8=1` or the ANSI code page is 65001; any
  other ANSI code page through `MultiByteToWideChar` (an invalid multi-byte sequence is replaced, where Python raises
  `UnicodeDecodeError`).
- Reason: only this machine's code page (1252) is exercised; dependency files are ASCII paths.

## D-transform_dep-3. NUL in a path
- Behaviour: a NUL character in the path of a line that goes through `realpath` (a 0x00 byte in the file) ends the
  path there in the C tool (`E:\x\0y\a.h` gives `E:\x`); Python keeps the whole path, normalised but not
  canonicalised (`e:\x\0y\a.h`). `diff/cases_known.txt` shows it.
- Reason: a dependency file written by the compiler never holds a NUL; the paths are C strings in `fs.h`.

## D-transform_dep-4. Reserved device names under an existing directory
- Behaviour: when `$WINEPREFIX/dosdevices` exists and a relative line names a reserved device (`NUL`, as the last
  component), `avp_path_realpath` returns the device path `\\.\nUL` where Python keeps `...\dosdevices\nUL`
  (`CON` agrees). Only `diff/cases_wine.txt` case `edge/dos_device` shows it; with the default prefix
  (`~/.wine` does not exist) both agree.
- Reason: behaviour of the shared `avp_path_realpath` (core, not changed by this port); not a path a dependency file holds.

## Traceback emulation dropped
- The emulated Python traceback text was dropped after the Python tools were retired: an internal error is one line on stderr, `transform_dep.exe: error: <Class>: <message>`, with the same exit code as before.
