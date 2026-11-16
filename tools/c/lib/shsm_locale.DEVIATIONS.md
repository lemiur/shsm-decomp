# shsm_locale: allowed differences from Python

## D-shsm_locale-1. Traceback text
- Behaviour: `shsm_py_traceback` prints one line, `<script>: error: <Class>: <message>`; CPython printed every frame with its source line.
- Reason: callers need the exit code (1) and a non-empty stderr.

## D-shsm_locale-2. Code pages and encoding switches
- Behaviour: only cp1252 and UTF-8 are decoded and encoded as Python's codecs do. Another ANSI code page goes through
  `MultiByteToWideChar` / `WideCharToMultiByte` (`WC_NO_BEST_FIT_CHARS`): an invalid multi-byte sequence is replaced where
  Python raises, and the exception text of an unencodable character differs. `PYTHONIOENCODING`, `-X utf8`,
  `PYTHONLEGACYWINDOWSSTDIO` and a console stdout are not looked at.
- Reason: the project runs on a cp1252 machine; the tools' inputs are ASCII.

## D-shsm_locale-3. Position and character of some UnicodeEncodeErrors
- Behaviour: the message of an unencodable character is exact for cp1252 (character and position in code points). For a
  lone surrogate, and for a character a non-cp1252 code page lacks, it names U+D800 or U+FFFD and position 0.
- Reason: callers print it in a traceback; only the exit code and the class are compared.

## Traceback emulation dropped
- The emulated Python traceback text was dropped after the Python tools were retired: an internal error is one line on stderr, `shsm_locale.exe: error: <Class>: <message>`, with the same exit code as before.
