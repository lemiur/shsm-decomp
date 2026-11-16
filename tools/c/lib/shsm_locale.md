# shsm_locale (lib/shsm_locale.h)

What a Python 3.14 tool on Windows gets by default when it calls `open(path)` or `print()` without naming an encoding,
and the exit of such a tool on an uncaught exception. Used by `changes_fmt` and `transform_dep`, whose Python reads
with the locale's code page (cp1252 here) and prints through a cp1252 stdout.

| function | Python |
|---|---|
| `shsm_locale_codepage()` | the number in `locale.getpreferredencoding()` (`cp<ACP>`); 65001 (UTF-8) when `PYTHONUTF8=1` |
| `shsm_locale_read_text(a, path, &text)` | `open(path).read()`: decodes with that code page, `\r\n` and `\r` become `\n`. cp1252 is decoded as Python's codec does (the bytes 0x81 0x8D 0x8F 0x90 0x9D fail with `UnicodeDecodeError`); other code pages go through `MultiByteToWideChar`. A failure is recorded as `avp_io_err()`. |
| `shsm_locale_encode(a, text, &bytes)` | the bytes `print(text)` writes to a pipe: `text` (UTF-8) in the code page, strict (`UnicodeEncodeError` for a character the page lacks, or a lone surrogate). The CRT text mode of stdout adds the `\r` of `\n`, as Python's does. |
| `shsm_write_utf8_text(path, text)` | `open(path, "w", encoding="utf-8").write(text)`: CRLF line ends; a lone surrogate leaves the empty file Python has created and fails with `UnicodeEncodeError`. |
| `shsm_py_traceback(script, cls, fmt, ...)` | an internal error: `<script>: error: <cls>: <text>` on stderr, stdout flushed, exit code 1 |
| `shsm_py_traceback_io(script)` | the same for the exception of the last failing `fs.h` call (`avp_io_err_class()`, `avp_io_err_str()`) |

Deviations: `shsm_locale.DEVIATIONS.md`.
