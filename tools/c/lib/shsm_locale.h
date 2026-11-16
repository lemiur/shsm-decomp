/* The text encoding a Python 3.14 tool on Windows gets by default, for tools that call open(path) or print() without
 * naming an encoding: locale.getpreferredencoding() (cp<ACP>, or UTF-8 when PYTHONUTF8=1 or the ANSI code page is
 * UTF-8), and the uncaught-exception exit of such a tool. */
#pragma once
#include "avp/avp.h"

/* The code page Python uses: GetACP(), or 65001 in UTF-8 mode. */
unsigned shsm_locale_codepage(void);

/* open(path).read() with the default encoding and newline=None: the text as UTF-8 in `a`. false: the failure is
 * recorded as avp_io_err() (FileNotFoundError, PermissionError, UnicodeDecodeError...). cp1252 is decoded as Python's
 * codec does (the five undefined bytes fail); other code pages go through MultiByteToWideChar. */
bool shsm_locale_read_text(AvpArena *a, const char *path, AvpStr *out);

/* The bytes sys.stdout.write(text) puts out for `text` (UTF-8 in, newlines untouched; the CRT text mode of stdout
 * turns \n into \r\n as Python's does). false: UnicodeEncodeError, recorded as avp_io_err(). */
bool shsm_locale_encode(AvpArena *a, AvpStr text, AvpStr *out);

/* open(path, "w", encoding="utf-8").write(text): false with UnicodeEncodeError (kind AVP_IOE_ENCODE) when `text`
 * holds a lone surrogate (JSON "\ud800"), else avp_write_text with CRLF line ends. */
bool shsm_write_utf8_text(const char *path, AvpStr text);

/* An uncaught Python exception: "<script>: error: <cls>: <text>"
 * on stderr, stdout flushed, exit code 1. */
AVP_NORETURN void shsm_py_traceback(const char *script, const char *cls, AVP_PRINTF_FMT const char *fmt, ...);
/* The same for the exception recorded by the last failing avp_io call. */
AVP_NORETURN void shsm_py_traceback_io(const char *script);
