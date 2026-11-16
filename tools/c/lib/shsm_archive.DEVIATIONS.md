# shsm_archive: accepted differences from Python's `tarfile` and the tools' loops

## D-shsm_archive-1. Member names that leave the folder are refused
- Behaviour: `shsm_tar_extract_files` stops with an error ("... would be extracted outside the destination") on a
  member whose name is absolute, has a drive letter, or climbs out of the folder with `..`. merge_lane.py and
  neutral.py join the name unchecked (`os.path.join(dest, m.name)`), so the Python would write such a file outside
  the folder.
- Reason: a safety rule of the port. `git archive` of a git tree never produces such a name (git refuses `..`
  components and absolute paths in a tree), so the inputs the tools read never reach the difference.

## D-shsm_archive-2. A member whose data runs past the buffer
- Behaviour: the reader reports the error at that member (`shsm_tar_next` returns -1); tarfile yields the member and
  raises at the next step, so a Python loop sees one member more before the error.
- Reason: only a truncated archive shows it; `git archive` output read whole from the pipe is never truncated when
  git exits 0, and the tools stop on a non-zero exit before reading.
