# shsm_archive: `tarfile` over `git archive` output

`#include "shsm_archive.h"`. Reads a tar held in memory and writes selected members to a folder.

What the Python tools use (grep of `tools/` for `tarfile`, `zipfile`, `shutil.make_archive` / `unpack_archive`,
`git archive`):

| Python site | use | C |
|---|---|---|
| `merge_lane.py` 934-947 `extract_git_tree` | `git -C <slot> archive --format=tar <rev>` into memory; `tarfile.open(fileobj=BytesIO(...))`; every regular file under `src/`, `include/`, plus `docs/exceptions.md` and `configure.py`, written to a temporary folder for the whole-tree rules check | run git with `avp_run`, then `shsm_tar_extract_files(p, n, dest, select, ctx, &written, &err)` with a select callback |
| `neutral.py` 95-114 `materialise_rev` | `git archive --format=tar <sha> -- include src`; every regular file written under `build/neutral/tree-<sha16>/` | `shsm_tar_extract_files(p, n, dest, NULL, NULL, ...)` |
| `download_tool.py` 103-105 | `zipfile.ZipFile(BytesIO(download)).extractall(output)` of toolchain zips | not provided (no zip reader; the download itself needs HTTP) |

No tool writes a tar or zip archive.

API:
- `shsm_tar_open` / `shsm_tar_next`: the members `tarfile` yields, with the same `TarInfo.name` (ustar prefix, pax
  `path`, GNU long names, the git global pax header read and not yielded), kind, size, mode, mtime, link name; data
  points into the caller's buffer.
- `shsm_tar_extract_files`: the tools' loop (regular files only, optional name filter, parents made, files
  overwritten, mtime left at the write time, as `open(.., "wb").write(...)` leaves it), refusing a name that leaves
  the folder.
- `shsm_tar_extract`: `TarFile.extractall(folder, filter="data")` (directories too, mtimes from the archive; links
  skipped and counted). No tool needs it yet; kept from another project of the author's with its tests.

Provenance: another project of the author's `core/include/avp/zip.h` and `core/src/zip_tar.c` (their tar part) at commit
`49e811503d7ba5c9cff92160557df057c50a8ab3`, names `avp_` -> `shsm_`; `shsm_tar_extract_files` is this project's.

## Tests

`tests/shsm_archive_test.c` (CTest `shsm_archive_test`): tars written by Python's `tarfile` in the pax, GNU and
ustar formats with the member lists `tarfile` reads back (that project's corpus, `tests/shsm_archive_corpus.h`), the
behaviour on a tar cut at every offset and on damaged headers, `extractall`, and this project's: the member list of
`git archive --format=tar 3fce933 -- tools/common` of this repository as `tarfile` gives it
(`tests/shsm_archive_git.h`, from `tests/gen_shsm_zlib_archive.py`), its extraction like `neutral.py` (every file's
sha256), the merge_lane-style selection (only the selected regular files are written), and the refusal of a member
that would land outside the folder.
