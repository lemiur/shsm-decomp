# shsm_dol: the DOL reader

Ports the two DOL readers of the Python tools: `gatecheck.py` `Dol`/`Segment` (lines 118-190) and `codecmp.py`
`Dol` (lines 109-131). Header: 18 section offsets, addresses and sizes (7 text, 11 data) at 0x00/0x48/0x90, bss
address and size at 0xD8/0xDC, entry point at 0xE0 (all big-endian `u32`).

| function | Python |
|---|---|
| `shsm_dol_parse(a, data, size)` | `gatecheck.Dol(data)`: raises `SHSM_EXC_VALUE` with GateError's text ("DOL too short (N bytes)", "DOL section I runs past the end of the file") |
| `shsm_dol_load(a, path)` | `gatecheck.Dol.load`: `SHSM_EXC_OS` "cannot read <path>: <str(OSError)>" |
| `ShsmDol.segs[0..nsegs)` | `Dol.segments` (non-empty sections in header order); `shsm_dol_seg_name` (`T<i>`/`D<i>`), `shsm_dol_seg_end`, `shsm_dol_seg_is_text` |
| `shsm_dol_segment_at(d, addr)` | `Dol.segment_at` |
| `shsm_dol_file_offset(d, addr)` | address-to-file-offset mapping (-1 outside every segment) |
| `shsm_dol_read(a, d, addr, n, &out)` | `gatecheck.Dol.read`: gathers across adjacent segments, false when a byte is outside them |
| `shsm_dol_read_within(d, addr, n, &out)` | `codecmp.Dol.read`: only within one segment (a view, no copy) |
| `shsm_dol_in_bss(d, addr)` | `Dol.in_bss` (both tools) |

The retail image's path is `ShsmRepo.retail_dol` (from `config/R5WEA4/config.yml`). `cc_codecmp`'s own `CcDol`
(`cc_project_dol`, `cc_dol_read`) predates this module; folding it onto `shsm_dol` is a later coordinated change.

Tests: `tests/shsm_dol_test.c` (a synthetic image; the retail DOL against `gatecheck.Dol` through Python).
