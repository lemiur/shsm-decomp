# shsm_zlib: Python's `zlib` (and `binascii.crc32`)

`#include "shsm_zlib.h"`. zlib-ng 2.2.4 in zlib-compatible mode (`third_party/zlib-ng/`), the library CPython 3.14
links, driven the way CPython drives it, so compressed bytes are identical at every level.

What the Python tools use (grep of `tools/`, `zlib`, `gzip`, `binascii`):

| Python site | use | C |
|---|---|---|
| `objcache.py` 485, 505 | `zlib.compress(marshal.dumps(header))`: cache result headers and manifests in `%LOCALAPPDATA%\shsm-cache\objcache` (default level, wbits 15) | `shsm_zlib_compress(a, p, n, -1, 15, &out_n)` |
| `objcache.py` 495, 512 | `zlib.decompress(...)` of those files | `shsm_zlib_decompress(a, p, n, 15, 0, &out_n, &err)` |
| `eqcheck.py` 2365 | `zlib.crc32(name.encode())`: deterministic `--sample` | `shsm_zlib_crc32(p, n, 0)` |
| `hashes.py` 131 | `binascii.crc32(b) & 0xFFFFFFFF`: the game's CRC-32 hash function | `shsm_zlib_crc32(p, n, 0)` |

Also available: the streaming objects (`zlib.compressobj` / `decompressobj`: `shsm_zcomp_*`, `shsm_zdecomp_*`) and
`shsm_zlib_adler32` (`zlib.adler32`). No tool uses `gzip`. core's `avp_crc32(value, p, n)` (hash.h, a table CRC-32)
gives the same values as `shsm_zlib_crc32`.

Errors carry `str(zlib.error)` (`"Error -3 while decompressing data: incorrect header check"`) in `ShsmZErr`.

The objcache files also hold `marshal` data, which has no C reader yet (USAGE-python-stdlib.md).

Provenance: another project of the author's `core/include/avp/zip.h` and `core/src/zip.c` (their zlib part) at commit
`49e811503d7ba5c9cff92160557df057c50a8ab3`, names `avp_` -> `shsm_`; `shsm_zlib_crc32` and `shsm_zlib_adler32` added.

## Tests

`tests/shsm_zlib_test.c` (CTest `shsm_zlib_test`): that project's corpus (`tests/shsm_zlib_corpus.h`: compressed bytes
of CPython at levels -1, 0, 1, 6, 9 over five data kinds and many sizes, the streaming objects, the error texts of
truncated and damaged streams), plus `tests/shsm_zlib_py.h` from `tests/gen_shsm_zlib_archive.py`: an objcache
result header and manifest as the Python writes them (decompressed here to the same marshal bytes, and compressed
here to the same zlib bytes), and crc32 / adler32 values (with a start value, and in two pieces).
