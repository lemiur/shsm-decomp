# shsm_libraries: the address-to-library table

Ports `tools/common/libraries.py`: `config/<ver>/libraries.tsv` (start, end exclusive, library, note; hex addresses;
`#` comments, blank lines and a `start ...` header skipped; a missing file is an empty table). Header
`shsm_libraries.h`.

| Python | C |
|---|---|
| `Libraries.parse(text, path)` | `shsm_libraries_parse(a, text, path)` (raises `SHSM_EXC_VALUE` with the Python's `file:line` messages) |
| `Libraries.load(repo)` | `shsm_libraries_load(a, repo)` |
| `load_libraries(repo)` (cached per file stamp) | `shsm_load_libraries(repo)` (cached per thread and stamp) |
| `lookup`, `row_of`, `names`, `problems(start, end)` | `shsm_libraries_lookup`, `shsm_libraries_row_of`, `shsm_libraries_names`, `shsm_libraries_problems(a, l, have_range, start, end)` |
| `library_of(address, repo)`, `group_of(lib)` | `shsm_library_of`, `shsm_library_group_of` |

Rows are sorted as Python's `sorted()` of the tuples (start, end, library, note; strings by code point). Tests:
`tests/shsm_asmindex_test.c` (`libraries`, the cases of `TestLibrariesAndPaths.test_libraries`). Deviations:
`shsm_libraries.DEVIATIONS.md`.
