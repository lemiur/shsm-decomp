# Deviations of shsm_links from tools/slot.py

## D-shsm_links-1. Walk order and path form
- Behaviour: `shsm_links_walk` yields the links in per-folder name order, each folder's contents directly after its
  name, with paths relative to the root and forward slashes. `slot.scan_links` returns absolute paths in the order of
  `sorted()` over whole paths (a stack walk, backslash-separated), which differs where a name is a prefix of a
  sibling's name (`a/z` against `a-c`: `-` sorts before `\`).
- Reason: the order requested for the port is `sorted(os.scandir)` per directory; callers that need slot.py's
  order sort the absolute paths themselves. Relative paths cannot be confused by the checkout's location.

## D-shsm_links-2. A walk root that cannot be listed
- Behaviour: a missing or unlistable root makes `shsm_links_walk` return `false` with the OSError; `scan_links`
  silently returned `[]` for it (its `except OSError: continue` applies to the root too). Unlistable subfolders are
  skipped silently, as in Python.
- Reason: a mistyped root should not look like "no links". The caller may treat `false` as Python's empty list.

## D-shsm_links-3. Junction creation
- Behaviour: `shsm_link_create_junction` refuses an existing link path with `EEXIST` before creating anything,
  and has no `os.symlink` fallback for platforms without junctions (Windows only). `make_junction` checked nothing
  itself and left the failure to `_winapi.CreateJunction`.
- Reason: the refusal was asked for, and a junction must never replace anything.

## D-shsm_links-4. Error text of a refused removal
- Behaviour: removing a path that is not a reparse point records `[WinError 4390] The file or directory is not a
  reparse point: '<path>'` (OSError); Python raised `Refusal("not a link: <path>")`.
- Reason: the C library has no `Refusal` class; the caller maps the refusal (winerror 4390) to its own message.

## D-shsm_links-5. Removing other reparse points
- Behaviour: as `remove_link`, a reparse point with an unknown tag and without the directory attribute is deleted with
  `DeleteFileW` (kind `other`), one with the directory attribute with `RemoveDirectoryW` (fails if non-empty).
- Reason: parity with Python; the removal only ever deletes the entry itself.
