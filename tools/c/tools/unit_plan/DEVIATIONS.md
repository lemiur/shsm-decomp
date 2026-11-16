# unit_plan: accepted differences from tools/unit_plan.py

Proven equal to the retired Python tools/unit_plan.py by the differential harness, removed at retirement, commit 66ee84d3.

The text output, exit codes and stderr (the stale-classes warning included) are the same; the `--json` documents are the same after the two normalisations below.

## D-unit_plan-1. Order of the calibration's tag keys in JSON
- Behaviour: the `accuracy` object (`START-END --json`, `--lib L --json`, `plan_for`) and the `tags` object
  (`check --json`) list each tag where it is first predicted on a labelled gap; within one gap the C takes the tags in
  name order, the Python in the iteration order of a `set` of strings. The keys and values are equal; only the order
  of keys first seen on the same gap can differ.
- Reason: the Python's order depends on the string hash seed of the process that built `unit_plan.pickle`
  (`PYTHONHASHSEED` is random), so it changes whenever the Python rebuilds its cache: the Python is not
  deterministic here and no fixed order can reproduce it. The harness re-dumps these two objects with sorted keys on
  both sides (`diff/cases.py` normalize). Defect in the Python tool: the order should be fixed (e.g. sorted).

## D-unit_plan-2. Last-bit differences in a combined confidence
- Behaviour: a cut whose tag combination has fewer than five calibrated cases gets `1 - prod(1 - p_t)` over its tags;
  the C multiplies in tag-name order, the Python in `set` order. With three or more tags the product can differ in
  the last bits, so a `confidence` in `--json` can differ by about 1e-16 (the text shows only `high`/`medium`/`low`,
  which equal unless a value sits exactly on 0.6 or 0.9, which no calibration produces).
- Reason: the Python's own value changes from run to run (the set order follows the per-process hash seed even with
  a warm cache), so it is not reproducible. The harness rounds `confidence` to 12 significant digits on both sides.

## D-unit_plan-3. The cache file
- Behaviour: the C caches in `build/index/unit_plan.cbin` (the `avp/cache.h` format: checksummed, written
  atomically), never reads or writes `build/index/unit_plan.pickle`, and stores only the model of `.text`; the
  evidence, the ground truth and the calibration are recomputed from it on every run (a few milliseconds). Its stamp
  covers the same inputs as the Python's `_stamp` (the asm index's `files` summary; size and mtime of symbols.txt,
  splits.txt, libraries.tsv, objdiff.json, configure.py, build.ninja and classes.json) with `avp_tools_stamp()` (the
  C sources) in place of the stat of `unit_plan.py`.
- Reason: pickle is Python-only. The observable behaviour is the same: a current cache is used, a stale or damaged one
  is rebuilt and rewritten, a failed write is ignored, and `up_plan_for(..., build=false)` returns NULL exactly when
  the cache is not current (the Python's `plan_for(build=False)` returns None). The two languages keep separate
  caches, so running one never invalidates the other's.

## D-unit_plan-4. classes.json that the Python cannot read either
- Behaviour: a classes.json whose top level is not an object, or whose `classes` is not a list, gives no class
  evidence in the C; the Python fails with AttributeError/TypeError (exit 2). An unreadable file's note carries the
  OSError text of `avp_io_err_str`, which follows Python's `str(OSError)` but is not proven for every Windows error.
- Reason: inputs `python tools/classes.py scan` never writes; not reachable from the harness.

## Internal differences that cannot be seen from outside
- `classes.json` is read through this tool's own reader (`unit_plan_classes.h`: `load_classes`,
  `stale_reason`, `mangled_class`), to consolidate with the classes port's reader once that lands.
