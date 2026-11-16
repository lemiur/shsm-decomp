# shsm_links: NTFS junctions and symbolic links

Ports the link helpers of `tools/slot.py` (`is_link`, `link_target`, `make_junction`, `remove_link`, `scan_links`).
Built on `core/include/avp/fs.h` (`avp_lstat`, `avp_readlink`, `avp_junction_create`, `avp_symlink`, `avp_rmdir`,
`avp_remove`, `avp_scandir`); nothing here follows a link. The link-first rule is AGENTS.md's: "Never run `git
worktree remove` on a nested worktree whose directories are NTFS junctions ... Remove each junction first". Header:
`shsm_links.h`; tests: `tests/shsm_links_test.c`; deviations: `shsm_links.DEVIATIONS.md`.

Provenance: the `is_link` / `remove_link` structure follows `lib/worktree.c of another project of the author's` at commit
`49e811503d7ba5c9cff92160557df057c50a8ab3` (no code copied: that project shells out to `cmd /c rmdir` and `mklink`; this
module calls the Win32 wrappers of core).

## Errors

Functions return `false` (or `NULL` for a string) and record an OSError in fs.h's thread-local record
(`avp_io_err()`, `avp_io_is(errno)`). A caller that wants the Python exception calls `shsm_raise_io()`
(`shsm_cli.h`) right after. Refusals are recorded the same way: `EEXIST` for a link path that exists, winerror 4390
("not a reparse point") for removing or reading the target of a non-link, `ENOENT` for a missing path.

## API

Types: `ShsmLinkKind` (`SHSM_LINK_NONE`, `_JUNCTION`, `_SYMLINK_FILE`, `_SYMLINK_DIR`, `_OTHER`), `ShsmLink`
(`rel`, `kind`, `target`), `ShsmLinkVec` (heap vector of `ShsmLink`; `avp_vec_free`).

- `shsm_link_kind_name(kind)` text: `none`, `junction`, `symlink-file`, `symlink-dir`, `other`.
- `shsm_link_kind_from_attrs(attrs, tag)` kind from a dwFileAttributes / reparse tag pair (scandir entries, stat).
- `shsm_link_stat(path, &kind)` lstat-style kind; `false` with the error when the path cannot be examined.
- `shsm_link_kind(path)` the kind, `NONE` for ordinary things and unexaminable paths (`is_link` is `!= NONE`).
- `shsm_link_target(a, path)` target text or `NULL` (`link_target`). Junction: substitute name without `\??\`.
  Symbolic link: substitute name as `os.readlink` gives it, leading `\?\` stripped (`\?\UNC\` becomes `\`);
  a relative target is returned as stored. Never follows.
- `shsm_link_create_junction(link, target)` `make_junction`: refuses an existing `link` (dangling counts); target made
  absolute; missing parent folders of the link created; the target need not exist.
- `shsm_link_create_symlink(link, target, is_dir)` the same for a symbolic link (target stored as given); `false`
  when the process may not create symbolic links.
- `shsm_link_remove(path)` removes the link itself (`remove_link`). Not a reparse point, or missing: refused,
  nothing deleted. A reparse point with the directory attribute (junction, directory symlink, dangling or not):
  `RemoveDirectoryW` on the link, which cannot delete a non-empty real folder and never enters the target. Any other
  reparse point: `DeleteFileW`.
- `shsm_links_walk(a, root, &out)` every reparse point under `root` (`scan_links`) without descending through any
  link; ordinary folders are descended; per folder the entries are visited in name order (code point order) with a
  folder's contents right after its name; `rel` uses forward slashes; a root that is itself a link gives one entry
  `"."`. `false` when the root cannot be examined or listed; unreadable subfolders are skipped.

Typical teardown of a worktree: `shsm_links_walk` the checkout, `shsm_link_remove` each entry (deepest paths do not
occur: no entry lies inside another because links are not entered), then remove the worktree.
