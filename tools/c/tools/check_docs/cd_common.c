/* check_docs: the report, the line reader, the document list and the markdown helpers. */
#include "check_docs.h"
#include "shsm_cli.h"
#include "shsm_re.h"
#include <stdarg.h>

const char *const CD_CHECKS[] = {"verbs", "paths", "pins", "exceptions", "text", NULL};

void cd_report_init(CdReport *rep, AvpArena *a) {
    memset(rep, 0, sizeof *rep);
    rep->a = a;
}

void cd_report_add(CdReport *rep, const char *check, const char *file, i64 line, bool note,
                   AVP_PRINTF_FMT const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *msg = avp_vsprintf(rep->a, fmt, ap);
    va_end(ap);
    AvpStr up = avp_str_upper(rep->a, avp_str(check));
    CdItem it = {avp_cz(rep->a, up), avp_strdup(rep->a, file), line, msg, note};
    avp_vec_push(&rep->items, it);
}

char *cd_item_text(AvpArena *a, const CdItem *it) {
    return avp_sprintf(a, "%s:%lld: %s %s%s", it->file, (long long)it->line, it->check, it->note ? "note: " : "",
                       it->message);
}

AvpStrVec cd_read_lines(AvpArena *a, const char *path) {
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8_REPLACE, &text)) shsm_raise_io();
    AvpStrVec out = {0};
    AvpFileLines it;
    avp_file_lines_init(&it, text, AVP_NL_DEFAULT);
    AvpStr line;
    while (avp_file_lines_next(&it, &line)) avp_vec_push(&out, avp_str_rstrip_chars(line, AVP_S("\r\n")));
    return out;
}

AvpStrVec cd_doc_files(AvpArena *a, const char *root) {
    static const char *const fixed[] = {"AGENTS.md", "tools/rules/README.md", "tools/c/README.md"};
    AvpStrVec out = {0};
    for (size_t i = 0; i < 3; i++)
        if (avp_isfile(avp_path_join(a, root, fixed[i]))) avp_vec_push(&out, avp_str(fixed[i]));
    const char *docs = avp_path_join(a, root, "docs");
    if (avp_isdir(docs)) {
        AvpStrVec names = {0};
        if (!avp_listdir(a, docs, &names)) shsm_raise_io();
        AvpStrVec md = {0};
        for (size_t i = 0; i < names.n; i++)
            if (avp_str_endswith(names.v[i], AVP_S(".md")))
                avp_vec_push(&md, avp_str_concat(a, AVP_S("docs/"), names.v[i]));
        avp_sort_strs(md.v, md.n);
        for (size_t i = 0; i < md.n; i++) avp_vec_push(&out, md.v[i]);
    }
    return out;
}

AvpStrVec cd_split_cells(AvpArena *a, AvpStr line) {
    (void)a;
    AvpStrVec cells = shsm_re_split(shsm_re("(?<!\\\\)\\|", 0), avp_str_strip(line), 0);
    size_t lo = 0, hi = cells.n;
    if (hi > lo && cells.v[lo].n == 0) lo++;
    if (hi > lo && cells.v[hi - 1].n == 0) hi--;
    AvpStrVec out = {0};
    for (size_t i = lo; i < hi; i++) avp_vec_push(&out, avp_str_strip(cells.v[i]));
    return out;
}

AvpStrVec cd_code_spans(AvpArena *a, AvpStr text) {
    (void)a;
    AvpStrVec out = {0};
    ShsmReIter it;
    ShsmReMatch m;
    shsm_re_iter_init(&it, shsm_re("(`+)(.+?)\\1", 0), text, 0, AVP_END);
    while (shsm_re_iter_next(&it, &m)) avp_vec_push(&out, shsm_re_group(&m, 2));
    return out;
}

const char *cd_tool_exe(AvpArena *a, const char *exe_dir, const char *root, const char *name) {
    const char *beside = avp_path_join(a, exe_dir ? exe_dir : avp_exe_dir(), name);
    if (avp_isfile(beside)) return beside;
    const char *built = avp_join(a, root, "build", "tools", name);
    if (avp_isfile(built)) return built;
    return NULL;
}
