# elf_symbols: accepted differences from tools/scripts/elf_symbols.py

Proven equal to the retired Python tools/scripts/elf_symbols.py by the differential harness, removed at retirement, commit 66ee84d3. Last run: 21 cases, 0 differ.

On the real Ghost Rider boot ELF `sections.txt`, `symbols.txt`, `source_files.txt` and `classes.txt` are
byte-identical to the Python's and to the committed `ref/ghostrider_ps2/*.txt`.

- Uncaught Python exceptions (`struct.error`, `ValueError`, `IndexError`, `FileNotFoundError`) become a two-line
  `elf_symbols.exe: error:` message on stderr with exit 1; stderr is compared by class.
- The demangler is the Python's class `V2` ported statement by statement; its `DemangleError` (tried at the next `__`
  candidate) and `ValueError`/`IndexError` (the whole symbol gives `-`) are `avp_throw` codes `AVP_ERR_FORMAT` and
  `AVP_ERR_OTHER`. `str.isdigit()` on latin-1 characters is reproduced for the superscripts 1 2 3.
- The path pattern keeps the Python's accident: `BS.encode() + rb'~+-]'` puts a lone backslash before `~`, so the
  class holds `~` and not a backslash; `PATH_RE` therefore never sees a backslash and the `.replace(BS, '/')` is dead.
- Output files are written with the ANSI code page (the Python's locale default) and `\n` becomes `\r\n`; symbol
  and section names are latin-1 bytes decoded to text first, as in the Python.

## Traceback emulation dropped
- The emulated Python traceback text was dropped after the Python tools were retired: an internal error is one line on stderr, `elf_symbols.exe: error: <Class>: <message>`, with the same exit code as before.
