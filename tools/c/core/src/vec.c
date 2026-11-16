/* avpcore vectors: the growth step behind the avp_vec_* macros (heap-backed, doubling, like a Python list's
 * amortised append). */
#include "avp/avp.h"

void avp_vec__grow(void **v, size_t *cap, size_t need, size_t elem) {
    if (need <= *cap) return;
    size_t ncap = *cap < 4 ? 4 : *cap * 2;
    if (ncap < need) ncap = need;
    if (elem && ncap > SIZE_MAX / elem) avp_fatal("out of memory (vector of %zu elements)", ncap);
    *v = avp_xrealloc(*v, ncap * elem);
    *cap = ncap;
}
