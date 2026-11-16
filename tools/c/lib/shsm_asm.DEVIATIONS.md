# shsm_asm: accepted differences from tools/common/asm.py

## D-shsm_asm-1. Malformed directives that crash the Python are skipped
- Behaviour: the Python raises (and the caller's whole parse fails) on a `.L_` label line whose eight characters after
  `.L_` are not hexadecimal, a bare `.section` without a name (IndexError), `.balign 0` (ZeroDivisionError) or a
  `.balign` / `.skip` argument `int()` rejects. The C parser skips the label, takes an empty section name, leaves the
  cursor unchanged on such a `.balign`, and counts such a `.skip` as length 0.
- Reason: dtk never writes these forms (978 of 978 real listings parse identically, `tests/shsm_asm_real_test.c`); a
  crash is not a behaviour to reproduce.

## D-shsm_asm-2. Unicode digits and integers beyond 64 bits
- Behaviour: Python's `int()` and the regex `\d` accept any Unicode decimal digit and integers of any size; the C
  scanners accept ASCII digits in `\d` positions, and an integer that does not fit in 64 bits is not an integer
  (a `.4byte` item of such a value becomes a string item, as for any other non-integer word).
- Reason: listings are ASCII except inside string literals, and every value is a 32-bit word or address.
