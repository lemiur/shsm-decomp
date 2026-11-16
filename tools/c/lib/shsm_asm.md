# shsm_asm: the retail asm listing parser

Ports `tools/common/asm.py`: dtk's listings `build/<ver>/asm/**/*.s` into functions with instruction rows, data
objects with typed items, references, strings, floats and section runs, and `listings()`, the current listings
the last dtk split names (`build/<ver>/config.json`). Header `shsm_asm.h`.

Python `None` is `SHSM_NONE` (`INT64_MIN`) in integer fields and `p == NULL` in `AvpStr` fields. Every string of a
parse is a view into the file's own arena (`ShsmAsmFile.arena`), freed with `shsm_asm_free`.

| Python | C |
|---|---|
| `parse_text(text, path, unit)` | `shsm_asm_parse_text(text, path, unit, flags)` (text copied; `SHSM_ASM_NO_ROWS` counts rows without keeping them, as the index needs) |
| `parse_file(path, unit)` | `shsm_asm_parse_file(path, unit, flags)` (UTF-8, newlines untranslated; a read or decode failure raises as `shsm_raise_io`) |
| `read_block(path, start, end, section)` | `shsm_asm_read_block(path, start, end, section, &file)` |
| `AsmFile.function/object/block(name)` | `shsm_asm_function` / `shsm_asm_object` / `shsm_asm_block` |
| `Row`, `Item`, `Block`, `Ref` | `ShsmAsmRow`, `ShsmAsmItem` (value tagged: none, int, float, str), `ShsmAsmBlock` (labels as an ordered vector), `ShsmAsmRef` |
| `af.strings`, `af.floats`, `af.sections` | `ShsmAsmString`, `ShsmAsmFloat` (`has_value` false for `None`), `ShsmAsmSection` |
| `to_dict()` of Row, Item, Block | `shsm_asm_row_json`, `shsm_asm_item_json`, `shsm_asm_block_json(rows)`; a ref as a list: `shsm_asm_ref_json` |
| `unquote`, `decode_string`, `bytes_to_text`, `parse_operands` | `shsm_asm_unquote`, `shsm_asm_decode_string`, `shsm_asm_bytes_to_text`, `shsm_asm_parse_operands` |
| `CALL ... JUMPTABLE` | `SHSM_REF_CALL ... SHSM_REF_JUMPTABLE` (static strings; parsed refs compare by pointer, refs read from the index are interned to them) |
| `listing_rel`, `split_record`, `stale_note`, `listings` | `shsm_asm_listing_rel`, `shsm_asm_split_record`, `shsm_asm_stale_note`, `shsm_asm_listings` (`ShsmListings`: parallel `rels`/`paths`, `stale`, `record`, `note`) |

The Python's regular expressions (`_ROW`, `_BLOCK_HEAD`, `_RUN_HEAD`, `_START`, `_SYMREF` with `match`, `finditer` and
`fullmatch`, `_LABEL`, `_NOT_SYM`) are hand-written scanners that reproduce the regex semantics, backtracking
included (the lazy `\S+?` of a block header, the alternatives of a symbol reference's addend and relocation).
`str.strip()` and `\s` use Python's whitespace set (`shsm_ws_len`); `int()` and `float()` are `avp_parse_int` /
`avp_parse_float`. `bytes_to_text` decodes cp932 as Python's codec does: the single bytes Python maps itself (0x80,
0xA0, 0xFD-0xFF) are handled here, double bytes go through Windows' code page 932.

Proof: `tests/shsm_asm_real_test.c` parses every current listing of the real tree in both languages and compares a
hash of the full dump (blocks with rows, labels, items and line/byte ranges; references; strings; floats; section
runs): 978 of 978 listings equal. `tests/shsm_asm_test.c` ports `TestAsmParser` and the listings cases of
`TestStaleListings`. Deviations: `shsm_asm.DEVIATIONS.md`.
