# eqcheck: accepted differences from tools/eqcheck.py

Proven equal to the retired Python tools/eqcheck.py by the differential harness, removed at retirement, commit 66ee84d3.

Entries for the term language and callee liveness (eq_terms.c, eq_live.c) come first; node ids, simplification and
show() text are otherwise the Python's, checked by tests/terms_test.c and tests/live_test.c against values printed by
the Python on the same inputs. Later modules add their entries below.

## D-eqcheck-1. Impossible shift and extension arguments abort instead of raising
- Behaviour: `Terms.sra(x, n)` with a negative n on a constant raises `ValueError: negative shift count` in the
  Python, and `Terms.sext(x, bits)` accepts any bits; `eq_sra` asserts `n >= 0` and `eq_sext` asserts
  `1 <= bits <= 62` (an `AVP_ASSERT` failure ends the process).
- Reason: no caller can pass such values (the Side gives sra the `srawi` field masked to 0..31 or a constant `sraw`
  count clamped to 31, and sext 8 or 16), so the Python's exception path is unreachable; a loud stop is safer than a
  silently wrong term in a prover.

Entries for the instruction model (eq_side.c) follow. State, BlockRun and Side are otherwise the Python's, checked by
tests/side_test.c against the Python's Side.run_block on the same listings (effects, registers, slots, memory and
every node made, by id).

## D-eqcheck-2. The Python's own errors become thrown exceptions of the same class and text
- Behaviour: where the Python's instruction model fails with an error of Python itself on a malformed listing (a
  KeyError for a register name the state lacks, e.g. `r40`; an IndexError for a missing operand; a ValueError from
  `int()` or tuple unpacking, e.g. a three-field `rlwinm` with two fields; the ValueError "negative shift count" of
  `mask_of`; the TypeError of `subfic` with a relocation operand), eq_side throws the same class (`SHSM_EXC_KEY`,
  `EQ_EXC_INDEX`, `SHSM_EXC_VALUE`, `SHSM_EXC_TYPE`) with the same `str(e)` through the eqcheck exception mechanism
  (eq_side.h). The Python lets these escape the prover (a traceback for one function, "<Class>: <text>" for the unit
  in a scan); the C caller decides what to print. `run_block` on a block id outside the graph asserts (the Python's
  IndexError; the prover only passes ids of the graph).
- Reason: no traceback exists in C; the class and text are what reach the scan report.

## D-eqcheck-3. Integer operands beyond 64 bits wrap
- Behaviour: `_int(tok)` (Python `int(tok, 0)`, unbounded) and `int(tok)` read into 64 bits, wrapping modulo 2^64.
- Reason: every use reduces the value to at most 32 bits (`& 0xFFFF` / `_s16` of immediates, `& MASK` in mulc,
  `& 31` of `srawi`, bit tests of the `mtcrf` mask) except the rotate and mask fields and the `lmw`/`stmw` register
  number, which are below 32 in any listing; dtk never writes a 20-digit immediate.

## D-eqcheck-4. Condition-bit and register-number digits are ASCII
- Behaviour: `crbit`'s `tok.isdigit()`, `branch_cond`'s `ops[0][2:].isdigit()` and the `int()` they guard accept ASCII
  digits only; Python's accept every Unicode decimal digit.
- Reason: listings are ASCII (the listing parser, cc_listing.c, makes the same assumption).

Entries for the prover and the relocation resolver (eq_prove.c) follow. Prover, make_resolver, target_hints and
prove_functions are otherwise the Python's, checked by tests/prove_test.c against the Python's whole result dicts
(json.dumps without "seconds") on hand listings, on real functions and their mutants, on an identity sweep of real
retail functions and on the functions of compiled objects (with their compare results).

## D-eqcheck-5. An attribute error on a non-term operand is thrown as TypeError
- Behaviour: where the Python's `mismatch` reads `.op` of a subterm pair element that is not a Node (an int or str
  argument at a position where the other side has a node), it fails with `AttributeError: 'int' object has no
  attribute 'op'`; eq_prove throws `SHSM_EXC_TYPE` with the same text (eq_exc_class gives "TypeError").
- Reason: no exception code exists for AttributeError, and the case is unreachable: diff_path / first_diff only
  descend into nodes of the same op and arity, whose argument kinds agree position by position.

## D-eqcheck-6. An unmapped reachable block asserts
- Behaviour: `self.mapping[p]` (edges_into, fixpoint, check) raises KeyError in the Python when a reachable retail
  block has no partner; eq_prove asserts.
- Reason: wall.match_cfgs maps every block reachable from the entry, so the lookup cannot fail.

## D-eqcheck-7. "seconds" is the C prover's own time
- Behaviour: prove_functions' "seconds" (round(elapsed, 3)) measures the C prover, typically several times faster
  than the Python's.
- Reason: a timing (the porting rules' D1); eq_prove_functions_ex without seconds is what the tests compare.

Entries for the command line (eq_cli.c) follow. object_for, cmd_one, scan, selfcheck and main are otherwise the
Python's, checked by the differential harness (diff/cases.txt: help and usage errors, single runs of every verdict
with --explain, --json and --ret, --file variants, scan with build/eqcheck.json, selfcheck in both modes).

## D-eqcheck-8. scan and selfcheck run their units on threads, not in a process pool
- Behaviour: the Python's `cli.pmap(_scan_unit, ..., processes=True)` runs each unit (compile or listing, comparison,
  proofs) in a worker process; the C tool runs the same per-unit job on a thread pool (min(jobs, units) threads; one
  unit at a time on the calling thread for -j 1 or a single unit, as pmap). Each job has its own codecmp context,
  callee-liveness memo (one per unit, as the Python's worker made) and work arena, and compiles into the unit's own
  build/tmp/eqcheck/obj/<unit path>/ as the Python did. The worker's `Repo.find(root)` and `units.get_unit(name)`
  are the main thread's (already resolved by _select_units, so they cannot fail in the worker). The worker's
  `except RecursionError` ("term too deep (recursion)") has no C counterpart: the C prover and term code do not
  recurse on term depth, so a function the Python gave up on that way would get the prover's verdict (no such
  function exists in this tree: the full identity and built selfchecks agree with the Python on all 2983 / 2982
  functions). Output, build/eqcheck.json and exit codes are the same; timings ("seconds", "in %.1f s") are the C
  run's (D-eqcheck-7).
- Reason: the prover is thread-safe per proof (eq_prove.h) and processes buy nothing in C.

## Post-retirement changes (after the Python tools were retired)

## D-eqcheck-asm-1. Assembly source units are left out
- Behaviour: a unit whose source is `src/<unit>.s` (`shsm_unit_is_asm`) is left out of the units the tool selects by status, library or change; named directly it fails with `unit <name> is an assembly source unit (...): assembled by binutils as, not compiled` (lib `D-shsm_units-1`).
- Reason: the user's ruling of 2026-11-14 allows `.s` source units (assembled by configure's binutils `as` rule); MWCC never compiles such a unit, so there is nothing to compile and compare.
