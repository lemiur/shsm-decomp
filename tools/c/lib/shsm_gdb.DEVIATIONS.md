# shsm_gdb deviations

## D-shsm_gdb-1. Hex integers are non-negative and below 2**64
- Behaviour: `int(x, 16)` on stub data (stop-reply fields, register and GPR values, checksums) accepts what Python
  accepts (surrounding whitespace, `+`, `0x`, underscores) but not a `-` sign or a value of 2**64 or more: such a
  stop-reply field is skipped (Python keeps it), such a register value raises `ValueError` "invalid literal for int()
  with base 16: '...'" (Python returns the int).
- Reason: values are u64 in C; a GDB stub sends unsigned hex of at most 16 digits (Dolphin: 8 for GPRs, 16 for FPRs).

## D-shsm_gdb-2. Broker arguments of the wrong JSON type
- Behaviour: integer parameters (`num`, `addr`, `length`, `kind`, `count`) accept JSON ints and booleans; anything
  else raises TypeError "%x format: an integer is required, not <type>" (kind `error`). Python accepts a float for
  `kind` (`"Z%d"` truncates it) and `count`, raises different TypeError texts for some other mistyped arguments, and
  reports some of them later (a mistyped address with a zero length is never formatted). Argument counts give
  Python's exact TypeError text.
- Reason: the tools send well-typed arguments; the reply kind (`error`) is the same.

## D-shsm_gdb-3. A request line that is JSON but not an object
- Behaviour: answered `{"ok": false, "error": "bad request", "kind": "usage"}`; the Python broker's serve loop dies
  with AttributeError (`req.get`) and the broker process exits. A request in UTF-16/32 (which `json.loads(bytes)`
  detects) is also "bad request".
- Reason: no client sends either; the C broker stays up instead of crashing.

## D-shsm_gdb-4. Malformed broker replies
- Behaviour: a reply that is not a JSON object raises GdbError "broker error" (Python: AttributeError); a typed
  `shsm_target_*` result of the wrong shape raises GdbError "malformed reply from the session broker" (Python returns
  the value as it is).
- Reason: both brokers send well-formed replies; the typed C API needs typed values.

## D-shsm_gdb-5. Using a client after close()
- Behaviour: a command on a closed client raises GdbDisconnected "connection to the GDB stub lost: [WinError 10038]
  ..." (Python raises a bare OSError from `settimeout` before its try block).
- Reason: the tools never use a client after closing it; the C keeps to the GdbError family.

## D-shsm_gdb-6. Exception class names in shsm_cli
- Behaviour: `shsm_exc_class` (shsm_cli.c) returns "Exception" for codes 96-98; `shsm_gdb_exc_class` gives "GdbError",
  "GdbTimeout", "GdbDisconnected".
- Reason: shsm_cli is a shared module this port does not edit; the class name is printed only for an empty message,
  and every GDB message is non-empty, so `<prog>: error: <message>` is unaffected.
