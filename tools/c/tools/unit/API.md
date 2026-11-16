# unit: the C API of tools/c/tools/unit (library `tool_unit`)

The port of `tools/unit.py`. Another tool links it with `LIBS tool_unit` (a `tools/<name>/CMakeLists.txt` calling
`shsm_add_tool(... LIBS tool_unit INCLUDES ${SHSM_REPO}/tools/c/tools/unit)`), for example draft's promote step,
which calls `new` and the data inference in-process instead of running `unit.py`. Everything allocates from the
`UnitProject`'s arena; a `ToolError` of the Python is `avp_throw(UNIT_TOOL_ERROR, ...)` with the full message in
`unit_error_message()` (catch it with `AVP_TRY`).

## Headers

| header | contents | unit.py |
|---|---|---|
| `unit.h` | the model: `UnitProject` (`unit_project_new(a, root)`, `unit_syms`, `unit_sym`, `unit_text_funcs`, `unit_sinit_funcs`, `unit_ctor_hits`, `unit_find_split`, `unit_project_splits`, `unit_project_config`, `unit_project_index`), symbols.txt (`UnitSym`), splits.txt (`UnitSplit`, `UnitRanges` = an insertion-ordered section dict, `unit_load_splits`), configure.py (`UnitConfig`, `unit_parse_configure`, `unit_config_find`), `unit_load_force_active`, the asm index (`UnitIndex`, `UnitBlock`, `unit_index_load`, `unit_index_from_texts`, `unit_index_block`, `unit_index_referrers`, `unit_index_in_range`, `unit_is_gap_name`), the demangler (`unit_demangle`, `unit_dm_fname`, `unit_dm_qualified`, `unit_sig_text`), text files with their own line endings (`unit_read_text`, `unit_write_text`, `unit_guess_eol`), output (`unit_out`, `unit_capture`) | lines 116-903 |
| `unit_infer.h` | `unit_text_range_findings` (the .text range rules), `unit_infer_data` (`UnitDataResult`: ranges, notes, warnings, owned blocks, objects left to other units, shared names), `unit_find_ctors`, `unit_xref`, `unit_registration_object`, `unit_claimed_by` | 909-1227, 1808-1838 |
| `unit_stub.h` | `unit_build_source` (source and header stubs), `unit_header_guard`, `unit_split_entry_text`, `unit_splits_with_entry`, `unit_infer_lib`, `unit_insert_object`, `unit_data_reader`, `unit_data_only_position` | 1234-1473 |
| `unit_cmd.h` | the subcommands `unit_cmd_new(p, &UnitNewArgs)`, `unit_cmd_check`, `unit_cmd_park`, `unit_cmd_flip(p, &UnitFlipArgs)`, `unit_regen`, `unit_remove_stale_listings`, `unit_gate_pending_ok`, `unit_main` | 1476-2193 |

## Calling `new` in-process

    UnitProject *p = unit_project_new(avp_arena(), root);
    UnitNewArgs n = {.path = "game/CFoo.cpp", .text = "0x800C2C48-0x800C2FF0", .data = "auto", .dry_run = true};
    AvpBuf out = {0};
    unit_capture(&out);              /* the lines `new` prints, instead of stdout */
    AvpTry t;
    int rc;
    if (AVP_TRY(&t)) { rc = unit_cmd_new(p, &n); avp_try_end(&t); }
    else if (t.code == UNIT_TOOL_ERROR) rc = 2;   /* unit_error_message() */
    unit_capture(NULL);

The data inference alone: `unit_infer_data(p, unit_project_index(p), true, lo, hi, &splits, &includes, &excludes)`
with `splits = unit_project_splits(p)`; `includes`/`excludes` are `AvpStrVec`s (empty for none).

## Notes

- The asm index is parsed from the current listings on every run (about 0.9 s for the 60 MB of listings); there
  is no cache file (DEVIATIONS.md, D-unit-1).
- `UnitIndex` keys by symbol name exactly as unit.py's `AsmIndex` does (`by_name` keeps the last block of a name;
  `referrers` lists blocks in listing order).
