# names: accepted differences from tools/names.py

The unit-table cache `build/index/units.json` that `check` makes the Python write is not written by the C (shared
library deviation `D-B1` in `tools/c/DEVIATIONS.md`); `diff/rename_apply.py` leaves `build/index/` out of its tree
comparison for that reason.

## D-names-1. Files that are not valid UTF-8
- Behaviour: the Python reads every source, header, doc and config file with `surrogateescape`, so a file holding
  bytes that are not UTF-8 still goes through its regular expressions (the constructor/destructor rewrite of
  `special_line`, the class-body search of `add_member_decls`, `def_name` and the address comments of `check`). The C
  runs those expressions in str mode, which needs valid UTF-8: on such a file it stops with exit 2 and
  `names.py: error: <decode message>`. The whole-identifier rewrite itself (`ident_rx`) works on bytes in the C and
  gives the Python's result on any input.
- Reason: no file the tool reads is affected: all 755 files under src/, include/ and docs/ and configure.py,
  config.yml, symbols.txt and AGENTS.md decode as UTF-8 (checked when the port was made). The Python would in any case
  fail on such a line when it prints it (a lone surrogate cannot be encoded to stdout), so neither tool gives a useful
  result there.

## D-names-2. Malformed object files in `check`
- Behaviour: an object that starts with `\x7fELF` but is shorter than 6 bytes, or a symbol table whose `sh_link`
  names a section index past the section table, is counted as not built (`missing_objects`) by the C. The Python
  raises IndexError there (outside its `except (OSError, ValueError, struct.error)`) and `check` ends with exit 2.
- Reason: MWCC never writes such an object; every other malformed case (bad magic, wrong class or byte order,
  truncated section or symbol table, a name without its NUL) is counted as not built by both, as the Python intends.

## D-names-3. Behaviour beyond the port (the Python is retired; these are the C tool's own contract)
- `rename --signature`, `rename --family`, the unit-aware collision rule (a name collides only as an exact non-local
  name or as two `scope:local` symbols inside one unit's `splits.txt` ranges), rule tags on refusals, the final status
  line that survives the output cap (`DRY RUN: ...`, `APPLIED: ...`, `REFUSED: ...`), and the "mentions in comments, not
  rewritten" notes have no counterpart in `tools/names.py`. `names_plan_rename` keeps its signature; the switches are in
  `NamesRenameOpts` / `names_plan_rename_opts`. `attr` (`names_plan_attr`, `names_attr.c`) is
  new as well: one `symbols.txt` line's scope, size or align, with the unit's `splits.txt` section end moved when the last
  symbol of that section shrinks.
