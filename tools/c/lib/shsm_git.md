# shsm_git: git, once

`lib/shsm_git.h` / `lib/shsm_git.c`. Every `git` call of the Python tools (`slot.py`, `merge_lane.py`, `gate.py`,
`neutral.py`, `run_tests.py`, `score_history.py`, `hooks/pre_commit.py`, `doctor.py`, and the units/draft status
checks) goes through this module; the C tools do not spawn `git` any other way. Tests: `tests/shsm_git_test.c`.
Accepted differences: [`shsm_git.DEVIATIONS.md`](shsm_git.DEVIATIONS.md).

## Model

- Process spawn is `avp_run`: an argument vector, no shell, so a path with spaces needs no quoting.
  `checkout` is the folder git runs in (`git -C checkout ...`); `NULL` runs in this process's folder.
- The first layer is the runner: `shsm_git_run` returns the exit code and both streams and never raises for a
  non-zero exit (`subprocess.run(["git", ...], capture_output=True, text=True)`). `ShsmGitOpts` carries the timeout,
  stdin bytes, environment additions (`"NAME=VALUE"`), `quotepath_off` (`-c core.quotepath=off`), `binary` (raw bytes)
  and `soft_start`.
- The second layer is typed queries. Each documents in `shsm_git.h` which Python call it replaces. They all set
  `core.quotepath=off`.
- Failure convention: a scalar query whose Python form tested `rc != 0` and went on returns `NULL` / `false`
  (`rev_parse`, `merge_base`, `blob`, `object_exists`, `ref_exists`, ...). A query filling an output vector takes a last
  parameter `AvpStr *err`: `NULL` raises `SHSM_EXC_USAGE` `git <args> failed: <stderr>`; non-NULL returns `false` with
  the stripped stderr (else stdout) in `*err`. A timeout is a failure (`rc == -1`, `timed_out`).
- Text mode is `text=True, encoding="utf-8", errors="replace"` (U+FFFD for invalid bytes, `\r\n` and lone `\r` become
  `\n`). Binary mode (`opts.binary`, and every `-z` or blob call here) returns the bytes unchanged.
- Strings and views live in the arena `a`; vectors are heap vectors (`avp_vec_free` optional).

## API

Runner
- `shsm_git_run(a, checkout, args, n, opts)` -> `ShsmGitRun {rc, started, timed_out, out, err}`; `shsm_git_ok(&r)`.
- `shsm_git_failure_text(&r)` `(stderr or stdout).strip()`, `"timed out"`; `shsm_git_require(a, &r, args, n)` raises
  `git <args> failed: <text>` unless ok; `shsm_git_check(...)` = run + require (slot's `git(..., check=True)`).
- `shsm_git_available(a)`; macros `SHSM_GIT(a, dir, "rev-parse", "HEAD")`, `SHSM_GIT_OPTS(a, dir, &opts, ...)`,
  `SHSM_GIT_CHECK(a, dir, ...)` for literal argument lists. The generic runner also serves the mutating commands the
  Python tools use (`checkout`, `add`, `commit`, `rebase`, `merge --ff-only`, `update-ref`, `fetch`, `clone`, `worktree
  add/remove`, `archive`).
- `shsm_git_split_lines`, `shsm_git_split_nul`.

Status (`slot.is_dirty`, `merge_lane`, `gate.py`, `unit.py`, `draft.py`)
- `shsm_git_status(a, dir, untracked_all, &vec, err)` `status --porcelain=v1 -z [--untracked-files=all]` ->
  `ShsmGitStatusEntry {xy, path, orig_path, has_orig}`; `shsm_git_parse_status(z, &vec)` the parser alone.
- `shsm_git_dirty(a, dir)` `bool(status --porcelain .strip())`; raises `git status failed in <dir>`.

Revisions and refs
- `shsm_git_rev_parse(a, dir, rev)` `rev-parse --verify -q REV^{commit}` -> sha or NULL.
- `shsm_git_rev_parse_object` (any object), `shsm_git_object_id(rev, path)` (`--verify -q REV:PATH`).
- `shsm_git_branch` (`--abbrev-ref HEAD`), `shsm_git_short_head`, `shsm_git_ref_exists(ref)`, `shsm_git_branch_exists`.
- `shsm_git_roots(&{top, common})` (`--show-toplevel --git-common-dir` in one spawn), `shsm_git_toplevel`,
  `shsm_git_common_dir`, `shsm_git_git_path(name)` (`--git-path`, absolute, normalised).
- `shsm_git_merge_base(x, y)`; `shsm_git_is_ancestor(anc, desc)` 0 yes / 1 no / -1 error; `shsm_git_rev_count(range, &n)`.

Diff and objects
- `shsm_git_diff_names(args..., &vec, err)` `diff --name-only -z ARGS`; `shsm_git_diff_text`; `shsm_git_diff_tree_names`.
- `shsm_git_blob(spec, &data)` `cat-file blob SPEC` (bytes), `shsm_git_show_blob(rev, path, &data)`,
  `shsm_git_blob_head(spec, n, &data)` (first n bytes, stops git), `shsm_git_object_exists` (`cat-file -e`),
  `shsm_git_object_size` (`cat-file -s`), `shsm_git_cat_batch(names, n, &vec, err)` (`cat-file --batch`).

Index and trees
- `shsm_git_ls_files(args, &vec, err)` (`ls-files -z ARGS`), `shsm_git_ls_files_stage` (`-s -z`: mode as the octal
  number, stage, sha, path; `SHSM_GIT_MODE_SUBMODULE`), `shsm_git_ls_tree(rev, paths, &vec, err)` (`-r -z`),
  `shsm_git_check_ignore(paths, n, &ignored, err)` (`check-ignore -z --stdin`; exit 1 = none).

Worktrees and merges
- `shsm_git_worktrees(dir, &vec, err)` / `shsm_git_parse_worktrees(text, &vec)`: `ShsmGitWorktree {path, npath, head,
  branch, bare, detached, locked, prunable, locked_reason, prunable_reason}`. `path` is as git prints it
  (`E:/shsm-wt/slot3`); `npath` is `os.path.normpath(path)` (backslashes) for comparing with Windows paths.
  `branch` is the full ref (`refs/heads/x`) or NULL; `shsm_git_branch_short` removes `refs/heads/` once.
  `shsm_git_path_key` is slot.py's `norm()`: absolute, normalised separators and case, no `\\?\` prefix. The
  first entry is the main work tree.
- `shsm_git_merge_tree(x, y, &{clean, tree, conflicts}, err)`: `merge-tree --write-tree --name-only --no-messages -z`;
  exit 0 clean, 1 conflicts, anything else is a failure. It writes tree objects into the object database.

Log
- `shsm_git_last_commit_for_path`, `shsm_git_log_path_shas(path, first_parent, &vec, err)` (oldest first),
  `shsm_git_subject`, `shsm_git_commit_info(rev, &time, &subject)` (`show -s --format=%ct%x09%s`),
  `shsm_git_branches_no_merged(onto, &vec, err)`.

## Notes on the Python tools (defects seen while porting)

- `slot.unmerged()` treats every `merge-base --is-ancestor` failure (unknown revision, not a repository) as "not
  merged" (rc != 0). `shsm_git_is_ancestor` returns -1 for those, so a caller can tell them apart; the port of `slot`
  should decide whether to keep the Python's reading (D-shsm_git-5 is only the API).
- `slot.Repo.__init__` resolves `--git-common-dir` against the top-level, but git prints it relative to the folder it
  ran in; both are the same only when `cwd` is the top-level.
- `run_tests.py` splits `check-ignore` output on whitespace, so a test path containing a space would never match.
- `hooks/pre_commit.staged_entries` passes every staged name on one command line (`ls-files -s -z -- NAMES`), which
  fails past the 32 K command-line limit of a Windows process on a very large commit. `shsm_git_ls_files_stage` takes the
  same arguments; a port should chunk, or list the whole index and filter.
- `doctor.run()` reports a timeout as `(-1, str(e))`; use `ShsmGitOpts.timeout` with `timed_out`/`soft_start`.
