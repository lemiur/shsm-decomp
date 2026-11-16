/* shsm_placeholder: see shsm_placeholder.h. */
#include "shsm_placeholder.h"

bool shsm_is_placeholder_body(AvpStr text) {
    i64 open = avp_str_find(text, AVP_S("{"), 0, AVP_END);
    i64 close = -1;
    for (size_t i = text.n; i > 0; i--)
        if (text.p[i - 1] == '}') {
            close = (i64)i - 1;
            break;
        }
    if (open < 0 || close < open) return false;
    AvpStr body = avp_str_strip(avp_str_slice(text, open + 1, close));
    return avp_str_eqz(body, "// TODO");
}
