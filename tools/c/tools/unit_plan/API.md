# unit_plan: the API other tools link

`unit_plan.h`, static library `tool_unit_plan` (in a tool's `CMakeLists.txt`: `shsm_add_tool(<name> ... LIBS
tool_unit_plan)`; the directory `tools/c/tools/unit_plan` goes on the include path with it). Ports the importable part
of `tools/unit_plan.py`. Exceptions are `shsm_raise` codes (`SHSM_EXC_USAGE` where the Python raised
`cli.UsageError`); allocation goes into the arena passed first.

## plan_for

    UpPlanDoc *up_plan_for(AvpArena *a, i64 start, i64 end, const ShsmRepo *r, const char *min_conf,
                           ShsmAsmIndex *ix, bool build, bool refresh);

`plan_for(start, end, repo=None, min_conf="low", ix=None, build=True, refresh=True)`: the proposed units of
`[start, end)`. `r` NULL is the cwd's checkout; `ix` NULL opens the asm index read-only (`refresh` as given) and closes
it again, else the caller's open index is used. `min_conf` is `"low"`, `"medium"` or `"high"` (NULL: low). Returns
NULL when `build` is false and the cache is not current (never spends the model build); raises `SHSM_EXC_USAGE`
`no functions in 0x........-0x........` when the range holds no function. Writes one stderr warning per process when
the class evidence is missing or stale, as the Python does.

`UpPlanDoc`: `start`, `end`, `accuracy` (the calibration, `UpCalib`), `header` (the accuracy line), `units`
(`UpUnits`), `class_note` (NULL when the class evidence is current). `up_plan_doc_json` gives the Python's dict
`{range, accuracy, header, units, class_note}`; `up_unit_json` one unit as `unit_plan.py --json` prints it.

`UpUnit`: `start`, `end` (exclusive), `functions`, `end_tags` (bit mask of `UP_T_*`; `up_tag_names` gives the sorted
names), `has_conf`/`confidence` (the cut's precision on known units; none for the range's last unit), `names` (name
strings, at most six), `known_units` (sorted unit paths of split source units inside it), `sinit`, `command`
(`build/tools/unit.exe new <path> --text START-END --dry-run`, or `# already split: <unit>`).

## Lower levels

- `up_load_cached(a, r, ix, build)`: `UpState` (model, evidence, labels, calibration) through the cache
  `build/index/unit_plan.cbin` (DEVIATIONS D-unit_plan-3).
- `up_load_model`, `up_model_new`: the model of `.text` (`UpFunc`, `UpDatum`, class groups).
- `up_gap_evidence`, `up_predict`, `up_combine`, `up_truth`, `up_evaluate`, `up_bands`, `up_plan`: the evidence,
  the predictor and its calibration (`check`).
- `up_find_library`, `up_parse_range`: the `--lib` and `START-END` arguments.

The classes.json reader (`unit_plan_classes.h`) is private to this tool, to be replaced by the classes port's.
