# download_tool.exe: allowed differences from `python tools/download_tool.py`

Proven equal to the retired Python tools/download_tool.py by the differential harness, removed at retirement, commit 66ee84d3. Last run: 74 cases, 0 differ.

`build/tools/download_tool.exe` (tools/c/tools/download_tool) builds the same URL for each tool from the tag
(`binutils`, `compilers`, `dtk`, `objdiff-cli`, `sjiswrap`, `wibo`; `windows`/`amd64` as `platform.uname()` reports them),
prints the same `Downloading <url> to <output>` line (the output as `str(Path)`), writes a non-zip response to the output
file and unpacks a `.zip` URL like `ZipFile.extractall` (member names made relative, `..` and drive parts dropped,
`:<>|"?*` replaced by `_`, trailing dots and spaces removed, directories created, existing files overwritten, CRC-32
checked), clears the read-only attribute of every extracted file and touches the output directory. An unknown tool is a
`KeyError` traceback, a failing download a traceback (exit 1). The Python has no hash check of its own: the only integrity
check is the zip's CRC-32, which the C tool repeats. The pinned versions and sha256 values live in the callers
(`tools/project.py` / `configure.py` pass `--tag`), not in this tool.


## D-download_tool-1. Test seam: SHSM_DOWNLOAD_TOOL_MIRROR
- Behaviour: when the environment variable `SHSM_DOWNLOAD_TOOL_MIRROR` is set to `http://host:port`, the request is sent to
  that server with the path of the real URL (the progress line still names the real URL) and the system proxy is not
  used. The Python has no such variable; the harness patches its `urlopen` instead (ref.py).
- Reason: the tests must never reach the network, and the real hosts are hard-coded.

## D-download_tool-2. HTTP client
- Behaviour: the C tool uses WinHTTP (`User-Agent: Mozilla/5.0`, redirects followed up to 10, the system proxy settings,
  the Windows certificate store) where the Python uses `urllib.request`. Differences: a redirect from https to http is
  refused by WinHTTP; receive timeouts exist (60 s connect/send, 300 s receive) where urllib waits without limit; proxy
  variables (`HTTP_PROXY`) are not read. Errors are reported as `urllib.error.HTTPError: HTTP Error <code>: <reason>` or
  `urllib.error.URLError: <urlopen error [WinError n] ...>` (WinHTTP's message text), exit 1.
- Reason: no TLS stack is vendored; WinHTTP is part of Windows. The https path is not exercised by the tests (they
  never use the network); the http path is, against the local server.

## D-download_tool-3. Certificate failures
- Behaviour: the Python, on `CERTIFICATE_VERIFY_FAILED`, retries with the `certifi` bundle or, without `certifi`, prints
  a hint and exits 0. The C tool reports a certificate failure as a `URLError` traceback (exit 1) and has no retry.
- Reason: WinHTTP validates against the Windows certificate store, which is what `certifi`'s retry approximates; there is
  no bundle to fall back to.

## D-download_tool-4. Zip features
- Behaviour: zip64 archives are refused (`NotImplementedError`); only the stored and deflate methods are read (the
  Python also reads bzip2 and lzma); a member with a bad CRC-32 leaves an empty file and ends with `BadZipFile` (the
  Python's partly written file depends on its read buffer size).
- Reason: the toolchain archives are small deflate zips; the cases that matter (names, directories, overwrite, errors)
  are in the harness.
