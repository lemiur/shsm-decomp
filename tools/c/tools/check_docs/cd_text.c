/* check_docs exceptions and text: ledger exceptions-check folded in (check_exceptions), and the hygiene of the
 * tracked text files (check_text). */
#include "check_docs.h"
#include "shsm_cli.h"
#include "shsm_re.h"

#include <windows.h>

static AvpArena *A(void) { return avp_arena(); }

/* ---- exceptions ------------------------------------------------------------------------------------------------ */
static const char *repr_argv(const char *const *argv, size_t argc) {
    AvpBuf b;
    avp_buf_init(&b, A());
    avp_buf_putc(&b, '[');
    for (size_t i = 0; i < argc; i++) {
        if (i) avp_buf_putz(&b, ", ");
        avp_buf_put(&b, avp_repr_str(A(), avp_str(argv[i])));
    }
    avp_buf_putc(&b, ']');
    return avp_buf_cz(&b);
}

bool cd_run_exceptions(void *ctx, const char *const *argv, size_t argc, const char *root, i64 *rc, AvpStr *out,
                       const char **err) {
    (void)ctx;
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = argv, .cwd = root, .in = AVP_IO_NULL, .out = AVP_IO_CAPTURE,
                                             .err = AVP_IO_CAPTURE, .text = true, .errors = AVP_ERRORS_REPLACE,
                                             .timeout = 900, .arena = A()});
    if (r.status == AVP_PROC_START_FAILED) {
        *err = r.message ? r.message : "cannot start";
        return false;
    }
    if (r.status == AVP_PROC_TIMEOUT) {
        *err = avp_sprintf(A(), "Command '%s' timed out after 900 seconds", repr_argv(argv, argc));
        return false;
    }
    *rc = r.returncode;
    *out = avp_str_concat(A(), r.out, r.err);
    return true;
}

i64 cd_check_exceptions(const char *root, CdReport *rep, const char *exe_dir, CdExcRunner runner, void *ctx) {
    const char *ledger = cd_tool_exe(A(), exe_dir, root, "ledger.exe");
    if (!ledger) {
        cd_report_add(rep, "exceptions", "build/tools/ledger.exe", 0, false,
                      "exceptions-check could not run: no ledger.exe beside check_docs.exe or in build/tools (" CD_BUILD_HINT
                      ")");
        return -1;
    }
    const char *argv[] = {ledger, "exceptions-check", NULL};
    i64 rc = 0;
    AvpStr out = {0};
    const char *err = NULL;
    if (!(runner ? runner : cd_run_exceptions)(ctx, argv, 2, root, &rc, &out, &err)) {
        cd_report_add(rep, "exceptions", "build/tools/ledger.exe", 0, false, "exceptions-check could not run: %s", err);
        return -1;
    }
    if (rc == 0) return 0;
    AvpStrVec all = avp_str_splitlines(out, false);
    size_t kept = 0;
    for (size_t i = 0; i < all.n; i++) {
        AvpStr ln = all.v[i];
        if (!avp_str_strip(ln).n) continue;
        kept++;
        ShsmReMatch m;
        if (shsm_re_match(shsm_re("^([\\w./\\\\-]+\\.\\w+):(\\d+):\\s*(.*)$", 0), ln, 0, AVP_END, &m)) {
            AvpStr file = avp_str_replace(A(), shsm_re_group(&m, 1), AVP_S("\\"), AVP_S("/"), -1);
            i64 line = 0;
            avp_parse_int(shsm_re_group(&m, 2), 10, &line, NULL);
            AvpStr rest = shsm_re_group(&m, 3);
            cd_report_add(rep, "exceptions", avp_cz(A(), file), line, false, "%.*s", AVP_SF(rest));
        } else {
            AvpStr s = avp_str_strip(ln);
            cd_report_add(rep, "exceptions", "docs/exceptions.md", 0, false, "%.*s", AVP_SF(s));
        }
    }
    if (!kept)
        cd_report_add(rep, "exceptions", "docs/exceptions.md", 0, false, "exceptions-check exited %lld without output",
                      (long long)rc);
    if (rc != 1)
        cd_report_add(rep, "exceptions", "build/tools/ledger.exe", 0, false, "exceptions-check exited %lld (environment error)",
                      (long long)rc);
    return rc;
}

/* ---- text ------------------------------------------------------------------------------------------------------ */
static const char *const TEXT_EXTS[] = {".c",   ".cpp",  ".cp",  ".h",   ".hpp", ".inc", ".py", ".md",
                                        ".txt", ".yml",  ".yaml", ".json", ".toml", ".cfg", ".ini", ".sh",
                                        ".bat", ".cmd", ".java", ".tsv", ".csv", ".sha1", ".s",  ""};

static AvpStr ext_of(const char *rel) {
    AvpStr root, ext;
    avp_path_splitext(rel, &root, &ext);
    return avp_str_lower(A(), ext);
}

static bool text_ext(AvpStr ext, bool no_json) {
    for (size_t i = 0; i < sizeof TEXT_EXTS / sizeof TEXT_EXTS[0]; i++)
        if (avp_str_eqz(ext, TEXT_EXTS[i])) return !(no_json && avp_str_eqz(ext, ".json"));
    return false;
}

static bool starts_any(const char *rel, const char *const *prefixes) {
    for (; *prefixes; prefixes++)
        if (!strncmp(rel, *prefixes, strlen(*prefixes))) return true;
    return false;
}

/* The offset of the first byte CPython's strict UTF-8 decoder rejects (UnicodeDecodeError.start), or -1. */
static i64 utf8_error_start(AvpStr d) {
    const u8 *p = (const u8 *)d.p;
    size_t n = d.n, i = 0;
    while (i < n) {
        u8 c = p[i];
        if (c < 0x80) {
            i++;
            continue;
        }
        size_t len;
        u8 lo = 0x80, hi = 0xBF;
        if (c >= 0xC2 && c <= 0xDF) len = 2;
        else if (c == 0xE0) len = 3, lo = 0xA0;
        else if (c == 0xED) len = 3, hi = 0x9F;
        else if (c >= 0xE1 && c <= 0xEF) len = 3;
        else if (c == 0xF0) len = 4, lo = 0x90;
        else if (c == 0xF4) len = 4, hi = 0x8F;
        else if (c >= 0xF1 && c <= 0xF3) len = 4;
        else return (i64)i;
        if (i + 1 >= n || p[i + 1] < lo || p[i + 1] > hi) return (i64)i;
        for (size_t k = 2; k < len; k++)
            if (i + k >= n || p[i + k] < 0x80 || p[i + k] > 0xBF) return (i64)i;
        i += len;
    }
    return -1;
}

/* Python's cp932 codec: Windows code page 932 (MB_ERR_INVALID_CHARS) for the double-byte codes, plus the single
 * bytes 0x80, 0xA0 and 0xFD-0xFF, which Python maps (to U+0080 and private-use characters) and Windows rejects. */
bool cd_cp932_valid(AvpStr data) {
    const u8 *p = (const u8 *)data.p;
    size_t n = data.n, i = 0;
    while (i < n) {
        u8 c = p[i];
        if (c < 0x81 || (c >= 0xA0 && c <= 0xDF) || c >= 0xFD) {
            i++;
            continue;
        }
        if (i + 1 >= n) return false;
        wchar_t w[4];
        if (MultiByteToWideChar(932, MB_ERR_INVALID_CHARS, (const char *)p + i, 2, w, 4) <= 0) return false;
        i += 2;
    }
    return true;
}

void cd_check_text_file(CdReport *rep, const char *rel, AvpStr data, CdFinalNewline rules) {
    size_t head = data.n < 8192 ? data.n : 8192;
    if (memchr(data.p, 0, head)) return; /* binary */
    if (avp_str_startswith(data, AVP_S("\xEF\xBB\xBF"))) cd_report_add(rep, "text", rel, 1, false, "UTF-8 byte-order mark");
    size_t crlf = avp_str_count(data, AVP_S("\r\n"));
    size_t lf = avp_str_count(data, AVP_S("\n")) - crlf;
    if (crlf && lf) {
        i64 first = 0;
        bool want_cr = crlf < lf;
        size_t start = 0, lineno = 1;
        for (size_t i = 0; i < data.n; i++) {
            if (data.p[i] != '\n') continue;
            bool cr = i > start && data.p[i - 1] == '\r';
            if (cr == want_cr) {
                first = (i64)lineno;
                break;
            }
            lineno++;
            start = i + 1;
        }
        cd_report_add(rep, "text", rel, first ? first : 1, false, "mixed line endings: %zu CRLF, %zu LF", crlf, lf);
    }
    static const char *const source_dirs[] = {"src/", "include/", NULL};
    if (starts_any(rel, source_dirs)) {
        i64 bad = utf8_error_start(data);
        if (bad >= 0) {
            i64 line = (i64)avp_str_count(avp_strn(data.p, (size_t)bad), AVP_S("\n")) + 1;
            const char *how = cd_cp932_valid(data) ? "valid Shift JIS, not UTF-8 (sources are UTF-8; sjiswrap converts them)"
                                                   : "neither UTF-8 nor Shift JIS";
            cd_report_add(rep, "text", rel, line, false, "byte 0x%02X at offset %lld: %s", (u8)data.p[bad], (long long)bad,
                          how);
        } else {
            size_t start = 0;
            i64 lineno = 1;
            for (size_t i = 0; i <= data.n; i++) {
                if (i < data.n && data.p[i] != '\n') continue;
                AvpStr ln = avp_str_rstrip_chars(avp_strn(data.p + start, i - start), AVP_S("\r"));
                for (size_t k = 0; k < ln.n; k++) {
                    u8 c = (u8)ln.p[k];
                    if ((c < 32 && c != '\t') || c == 127) {
                        cd_report_add(rep, "text", rel, lineno, false, "control character U+%04X", c);
                        break;
                    }
                }
                lineno++;
                start = i + 1;
            }
        }
    }
    if (!strncmp(rel, "tools/", 6) && avp_str_endswith(avp_str(rel), AVP_S(".py"))) {
        i64 line = cd_tab_indented_line(data);
        if (line) cd_report_add(rep, "text", rel, line, false, "tab in indentation (flake8 W191)");
    }
    static const char *const final_dirs[] = {"src/", "include/", "tools/", "docs/", NULL};
    if (data.n && (rules.insert_final_newline || rules.trim_final_newlines) && starts_any(rel, final_dirs) &&
        text_ext(ext_of(rel), true)) {
        if (rules.insert_final_newline && !avp_str_endswith(data, AVP_S("\n")))
            cd_report_add(rep, "text", rel, (i64)avp_str_count(data, AVP_S("\n")) + 1, false,
                          "no final newline (.vscode/settings.json insertFinalNewline)");
        else if (rules.trim_final_newlines &&
                 (avp_str_endswith(data, AVP_S("\n\n")) || avp_str_endswith(data, AVP_S("\r\n\r\n"))))
            cd_report_add(rep, "text", rel, (i64)avp_str_count(data, AVP_S("\n")), false,
                          "blank lines at the end of the file (.vscode/settings.json trimFinalNewlines)");
    }
}

CdFinalNewline cd_vscode_rules(const char *root) {
    CdFinalNewline r = {false, false};
    const char *p = avp_join(A(), root, ".vscode", "settings.json");
    if (!avp_isfile(p)) return r;
    AvpStr text;
    if (!avp_read_text(A(), p, AVP_TM_UTF8_REPLACE, &text)) shsm_raise_io();
    text = shsm_re_sub(A(), shsm_re("(?m)^\\s*//.*$", 0), AVP_S(""), text, 0);
    r.insert_final_newline =
        shsm_re_search(shsm_re("\"files\\.insertFinalNewline\"\\s*:\\s*true", 0), text, 0, AVP_END, NULL);
    r.trim_final_newlines = shsm_re_search(shsm_re("\"files\\.trimFinalNewlines\"\\s*:\\s*true", 0), text, 0, AVP_END, NULL);
    return r;
}

static AvpStrVec tracked_files(const char *root) {
    const char *argv[] = {"git", "ls-files", "-z", NULL};
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = argv, .cwd = root, .out = AVP_IO_CAPTURE, .err = AVP_IO_CAPTURE,
                                             .timeout = 120, .arena = A()});
    if (r.status == AVP_PROC_START_FAILED) shsm_raise(SHSM_EXC_USAGE, "git ls-files failed: %s", r.message);
    if (r.status == AVP_PROC_TIMEOUT)
        shsm_raise(SHSM_EXC_USAGE, "git ls-files failed: Command '%s' timed out after 120 seconds", repr_argv(argv, 3));
    if (r.returncode != 0) {
        AvpStr e;
        if (!avp_decode_text(A(), r.err, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_REPLACE, AVP_NL_EMPTY), &e)) e = AVP_S("");
        e = avp_str_strip(e);
        shsm_raise(SHSM_EXC_USAGE, "git ls-files failed: %.*s", AVP_SF(e));
    }
    AvpStr out;
    if (!avp_decode_text(A(), r.out, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_REPLACE, AVP_NL_EMPTY), &out)) out = AVP_S("");
    AvpStrVec parts = avp_str_split(out, avp_strn("\0", 1), -1);
    AvpStrVec files = {0};
    for (size_t i = 0; i < parts.n; i++)
        if (parts.v[i].n) avp_vec_push(&files, parts.v[i]);
    return files;
}

static bool in_scope(const char *rel) {
    static const char *const dirs[] = {"src/", "include/", "tools/", "docs/", "config/", NULL};
    if (starts_any(rel, dirs)) return true;
    AvpStr r = avp_str(rel);
    return !strchr(rel, '/') && (avp_str_endswith(r, AVP_S(".py")) || avp_str_endswith(r, AVP_S(".md")));
}

void cd_check_text(const char *root, CdReport *rep, const AvpStrVec *files) {
    CdFinalNewline rules = cd_vscode_rules(root);
    AvpStrVec list = files ? *files : tracked_files(root);
    for (size_t i = 0; i < list.n; i++) {
        AvpArenaMark mark = avp_arena_mark(avp_scratch());
        const char *rel = avp_cz(A(), list.v[i]);
        if (!in_scope(rel)) continue;
        AvpStr native = avp_str_replace(avp_scratch(), list.v[i], AVP_S("/"), AVP_S("\\"), -1);
        const char *p = avp_path_join(avp_scratch(), root, avp_cz(avp_scratch(), native));
        if (!avp_isfile(p) || !text_ext(ext_of(rel), false)) {
            avp_arena_reset(avp_scratch(), mark);
            continue;
        }
        AvpBytes data = {0};
        if (!avp_read_all(NULL, p, &data)) shsm_raise_io();
        cd_check_text_file(rep, rel, avp_strn((const char *)data.v, data.n), rules);
        avp_vec_free(&data);
        avp_arena_reset(avp_scratch(), mark);
    }
}
