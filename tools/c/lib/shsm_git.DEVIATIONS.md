# shsm_git: accepted differences from the Python

## D-shsm_git-1. A git that cannot be started raises a usage error
- Behaviour: `shsm_git_run` raises `SHSM_EXC_USAGE` `cannot run git: [WinError 2] The system cannot find the file
  specified` (exit 2, `<prog>: error: ...`); with `ShsmGitOpts.soft_start` it returns `started == false`, `rc == -1`
  and the message in `err` (doctor.py's `(-1, str(e))`).
- Reason: Python raised `FileNotFoundError` (a traceback or the tool's own handler). The one-line error is the class
  of failure `cli.main` reports for the rest of the tools.

## D-shsm_git-2. Lines split on "\n" only
- Behaviour: line outputs (`branch`, `log`, `rev-parse`, `worktree list`) are split at `\n`. Python's
  `str.splitlines()` also splits at `\v \f \x1c-\x1e \x85 U+2028 U+2029`.
- Reason: git ends its records with `\n`. A file or branch name holding one of those characters would be cut by the
  Python; it is kept whole here. Text mode has already translated `\r\n` and `\r`, as `text=True` does.

## D-shsm_git-3. NUL-terminated output for names
- Behaviour: status, diff name lists, `diff-tree`, `ls-files`, `ls-tree`, `merge-tree` and `check-ignore` run with
  `-z` and are read as bytes. `slot.py`/`merge_lane.py` read the line form with `core.quotepath=off`, `unit.py`/
  `draft.py` the quoted `status --porcelain -uall` form (`line[3:]`).
- Reason: with `quotepath=off` git still quotes names holding control characters, quotes or backslashes; the `-z`
  form never quotes, so every name comes back exactly. Names with only ordinary characters give the same strings.
  Binary mode does not translate `\r\n` inside a name (text mode would).

## D-shsm_git-4. stdin is the null device
- Behaviour: git runs with stdin NUL unless `ShsmGitOpts.input` is given. `subprocess.run` inherited this process's
  stdin.
- Reason: none of the commands read stdin without being asked to; a console prompt can never block a tool.

## D-shsm_git-5. `merge-base --is-ancestor` has three answers
- Behaviour: `shsm_git_is_ancestor` returns 0 (ancestor), 1 (not), -1 (any other failure, including an unknown
  revision and a timeout). The Python callers (`slot.unmerged`) tested `rc != 0`, which folds -1 into "not an ancestor".
- Reason: the port can keep the old reading by testing `!= 0`; the distinction is available to callers that need it.

## D-shsm_git-6. The common directory is resolved against the folder git ran in
- Behaviour: `shsm_git_roots().common`, `shsm_git_common_dir` and `shsm_git_git_path` join a relative answer with
  `checkout` (the process folder for NULL). `slot.Repo.__init__` joined it with the top-level.
- Reason: git prints these paths relative to the folder it ran in; the two joins agree whenever `checkout` is the
  top-level (always, for slot) and the C one is right for a sub-folder.

## D-shsm_git-7. `check-ignore` is exact for names with whitespace
- Behaviour: `shsm_git_check_ignore` sends NUL-separated names and reads NUL-terminated answers. `run_tests.py` sent
  newline-separated names and split the answer on whitespace.
- Reason: a name with a space matches in the C version and is mis-split in the Python.

## D-shsm_git-8. One failure message
- Behaviour: `shsm_git_require` raises `git <args> failed: <stderr or stdout, stripped>` for every checked call.
  The Python helpers each had their own text (`slot.py`: `git <two words>: <last line>`; `score_history.py`: `git
  <args> failed: <stderr>`; `pre_commit.py`: `git <args>: <stderr>`).
- Reason: a caller that must print its own wording builds it from `shsm_git_run`'s result and
  `shsm_git_failure_text`.
