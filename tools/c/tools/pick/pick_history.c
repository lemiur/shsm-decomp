/* pick.py: history_rows and parse_lost_text. The Python ran `tools/score_history.py ... status --lost` as a
 * subprocess; the C tool calls score_history's C port in process (score_history_run). */
#include "pick.h"
#include "shsm_re.h"

#include <string.h>

#include "score_history.h"

static int linked_runner(void *ctx, AvpArena *a, int argc, const char *const *argv, AvpBuf *out, AvpBuf *err) {
    (void)ctx;
    return score_history_run(a, argc, argv, out, err);
}

static PickHistoryRunner g_runner = linked_runner;
static void *g_runner_ctx;

void pick_set_history_runner(PickHistoryRunner fn, void *ctx) {
    g_runner = fn ? fn : linked_runner;
    g_runner_ctx = fn ? ctx : NULL;
}

/* LOST_ROW = r"^(\S+)\s+(0x[0-9A-Fa-f]+|\S+)\s+([\d.]+|-)\s+([\d.]+|-)\s+([\d.]+|-)\s+(\S+)" */
static AvpJson *num(AvpArena *a, AvpStr x) {
    if (avp_str_eq(x, AVP_S("-"))) return avp_json_null();
    double v = 0;
    if (!avp_parse_float(x, &v)) shsm_raise(SHSM_EXC_VALUE, "could not convert string to float: '%.*s'", AVP_SF(x));
    return avp_json_float(a, v);
}

PickItems pick_parse_lost_text(AvpArena *a, AvpStr text) {
    ShsmRe *row_re = shsm_re("^(\\S+)\\s+(0x[0-9A-Fa-f]+|\\S+)\\s+([\\d.]+|-)\\s+([\\d.]+|-)\\s+([\\d.]+|-)\\s+(\\S+)", 0);
    ShsmRe *best_re = shsm_re("^(\\S+) (\\S+): best ", 0);
    PickItems rows = {0};
    AvpMap *by_sym = avp_map_new(); /* "unit\0symbol" -> row */
    AvpJson *current = NULL;
    AvpStrVec lines = avp_str_splitlines(text, false);
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr ln = lines.v[i];
        AvpStr s = avp_str_strip(ln);
        bool rec = avp_str_startswith(s, AVP_S("recover:"));
        if (rec || avp_str_startswith(s, AVP_S("uncommitted version"))) {
            if (current) {
                AvpStr v = s;
                if (rec) {
                    AvpStr before, after;
                    avp_str_partition(s, AVP_S(":"), &before, &after);
                    v = avp_str_strip(after);
                }
                avp_json_set(a, current, AVP_S("recovery"), avp_json_str(a, v));
            }
            continue;
        }
        ShsmReMatch m;
        if (shsm_re_match(row_re, ln, 0, AVP_END, &m) && !avp_str_eq(shsm_re_group(&m, 1), AVP_S("unit"))) {
            AvpJson *d = avp_json_obj(a);
            AvpStr unit = shsm_re_group(&m, 1), sym = shsm_re_group(&m, 6);
            avp_json_set(a, d, AVP_S("unit"), avp_json_str(a, unit));
            avp_json_set(a, d, AVP_S("address"), avp_json_str(a, shsm_re_group(&m, 2)));
            avp_json_set(a, d, AVP_S("current"), num(a, shsm_re_group(&m, 3)));
            avp_json_set(a, d, AVP_S("best"), num(a, shsm_re_group(&m, 4)));
            avp_json_set(a, d, AVP_S("historical"), num(a, shsm_re_group(&m, 5)));
            avp_json_set(a, d, AVP_S("symbol"), avp_json_str(a, sym));
            avp_json_set(a, d, AVP_S("lost"), avp_json_bool(true));
            avp_vec_push(&rows, d);
            avp_map_put(by_sym, avp_pyfmt(a, "%S%c%S", &unit, 0, &sym), NULL)->p = d;
            continue;
        }
        if (shsm_re_match(best_re, ln, 0, AVP_END, &m)) {
            AvpStr unit = shsm_re_group(&m, 1), sym = shsm_re_group(&m, 2);
            AvpVal *v = avp_map_get(by_sym, avp_pyfmt(a, "%S%c%S", &unit, 0, &sym));
            current = v ? v->p : NULL;
        }
    }
    avp_vec_free(&lines);
    avp_map_free(by_sym);
    return rows;
}

static AvpStr first_line_120(AvpArena *a, AvpStr text) {
    AvpStrVec ls = avp_str_splitlines(avp_str_strip(text), false);
    AvpStr out = {0};
    if (ls.n) out = avp_u8_slice(ls.v[0], 0, 120);
    else out.p = NULL;
    avp_vec_free(&ls);
    (void)a;
    return out;
}

PickItems *pick_history_rows(AvpArena *a, PickWorld *w, const char **note) {
    *note = NULL;
    const char *db = w->repo->score_history_db;
    if (!avp_isfile(db)) {
        *note = "history unavailable (no build/score_history.sqlite; run `build/tools/score_history.exe backfill`)";
        return NULL;
    }
    const char *base[] = {"--repo", w->repo->root, "--db", db, "status", "--lost", "--json"};
    AvpBuf out = {.a = a}, err = {.a = a};
    int rc = g_runner(g_runner_ctx, a, 7, base, &out, &err);
    if (rc == 0) {
        AvpJsonError je;
        AvpJson *data = avp_json_parse(a, (AvpStr){out.p, out.n}, &je);
        if (data) {
            if (avp_json_is_obj(data)) {
                AvpJson *f = avp_json_getz(data, "functions");
                if (!avp_json_truthy(f)) f = avp_json_getz(data, "rows");
                data = avp_json_truthy(f) ? f : avp_json_arr(a);
            }
            if (avp_json_is_arr(data)) {
                PickItems *rows = AVP_NEW(a, PickItems);
                for (size_t i = 0; i < avp_json_len(data); i++)
                    if (avp_json_is_obj(avp_json_at(data, i))) avp_vec_push(rows, avp_json_at(data, i));
                return rows;
            }
        }
    }
    AvpBuf out2 = {.a = a}, err2 = {.a = a};
    rc = g_runner(g_runner_ctx, a, 6, base, &out2, &err2);
    if (rc != 0) {
        AvpStr eo = err2.n ? (AvpStr){err2.p, err2.n} : (AvpStr){out2.p, out2.n};
        AvpStr first = first_line_120(a, eo);
        AvpStr tail = first.p ? avp_pyfmt(a, ": %S", &first) : AVP_S("");
        *note = avp_cz(a, avp_pyfmt(a, "history unavailable (score_history exit %d%S)", rc, &tail));
        return NULL;
    }
    PickItems *rows = AVP_NEW(a, PickItems);
    *rows = pick_parse_lost_text(a, (AvpStr){out2.p, out2.n});
    return rows;
}
