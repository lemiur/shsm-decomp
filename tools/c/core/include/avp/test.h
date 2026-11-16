/* The unit-test harness (replaces unittest and the projects' check() helpers). Header-only: a test file is
 *
 *     #include "avp/test.h"
 *     AVP_TEST(split_on_whitespace) {
 *         AvpStrVec v = avp_str_split(AVP_S(" a  b "), AVP_S(""), -1);
 *         CHECK_EQ_U64(v.n, 2);
 *         CHECK_STR(v.v[0], "a");
 *     }
 *     AVP_TEST_MAIN("str")
 *
 * Tests run in the order the linker places their registrations (not necessarily the order of definition: keep
 * tests independent of each other); a failed CHECK reports file:line and continues with the next check (the
 * test counts as failed). REQUIRE stops the test. The exit code is 0 when every test passed, else 1. Arguments:
 * --filter <substring> runs the tests whose name contains it; -v prints every test name. The last line printed is
 * "OK (<n> tests)" or "FAILED (<k> of <n> tests)", which ci_local.sh shows. */
#pragma once
#include "str.h"
#include <stdio.h>

typedef void (*AvpTestFn)(void);
typedef struct {
    const char *name;
    AvpTestFn fn;
} AvpTestCase;

#define AVP_TEST_MAX 4096
static AvpTestCase avp_test__cases[AVP_TEST_MAX];
static int avp_test__count;
static int avp_test__failed_checks;

static inline void avp_test__register(const char *name, AvpTestFn fn) {
    AVP_ASSERT(avp_test__count < AVP_TEST_MAX);
    avp_test__cases[avp_test__count].name = name;
    avp_test__cases[avp_test__count].fn = fn;
    avp_test__count++;
}

/* MSVC C has no constructors: register through a .CRT$XCU initializer. The pointer has external linkage and a
 * linker /include so that /O2 /GL /OPT:REF keep it (a static one is optimised away). Test names are unique per
 * test executable. */
#pragma section(".CRT$XCU", read)
#define AVP_TEST(name)                                                                         \
    static void avp_test_##name(void);                                                         \
    static void avp_test_reg_##name(void) { avp_test__register(#name, avp_test_##name); }      \
    __declspec(allocate(".CRT$XCU")) void (*avp_test_init_##name)(void) = avp_test_reg_##name; \
    __pragma(comment(linker, "/include:avp_test_init_" #name))                                \
    static void avp_test_##name(void)

static inline void avp_test__fail(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s(%d): check failed: ", file, line);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    avp_test__failed_checks++;
}

#define CHECK(cond) \
    ((cond) ? (void)0 : avp_test__fail(__FILE__, __LINE__, "%s", #cond))
#define REQUIRE(cond) \
    do { if (!(cond)) { avp_test__fail(__FILE__, __LINE__, "%s (required)", #cond); return; } } while (0)
#define CHECK_MSG(cond, ...) \
    ((cond) ? (void)0 : avp_test__fail(__FILE__, __LINE__, __VA_ARGS__))
#define CHECK_EQ_I64(a, b)                                                                                 \
    do { long long avp_a_ = (long long)(a), avp_b_ = (long long)(b);                                       \
         if (avp_a_ != avp_b_) avp_test__fail(__FILE__, __LINE__, "%s == %s: %lld != %lld", #a, #b, avp_a_, avp_b_); \
    } while (0)
#define CHECK_EQ_U64(a, b)                                                                                 \
    do { unsigned long long avp_a_ = (unsigned long long)(a), avp_b_ = (unsigned long long)(b);            \
         if (avp_a_ != avp_b_) avp_test__fail(__FILE__, __LINE__, "%s == %s: %llu != %llu", #a, #b, avp_a_, avp_b_); \
    } while (0)
/* Bit-exact double comparison (the port must give Python's exact value). */
#define CHECK_EQ_F64(a, b)                                                                                 \
    do { double avp_a_ = (a), avp_b_ = (b);                                                                \
         if (memcmp(&avp_a_, &avp_b_, sizeof avp_a_) != 0)                                                 \
             avp_test__fail(__FILE__, __LINE__, "%s == %s: %.17g != %.17g", #a, #b, avp_a_, avp_b_);       \
    } while (0)
/* An AvpStr against a C string literal (exact bytes). */
#define CHECK_STR(s, lit)                                                                                  \
    do { AvpStr avp_s_ = (s); const char *avp_l_ = (lit);                                                  \
         if (!avp_str_eqz(avp_s_, avp_l_))                                                                 \
             avp_test__fail(__FILE__, __LINE__, "%s == \"%s\": got \"%.*s\"", #s, avp_l_, AVP_SF(avp_s_)); \
    } while (0)
#define CHECK_CZ(cz, lit)                                                                                  \
    do { const char *avp_c_ = (cz); const char *avp_l_ = (lit);                                            \
         if (!avp_c_ || strcmp(avp_c_, avp_l_) != 0)                                                       \
             avp_test__fail(__FILE__, __LINE__, "%s == \"%s\": got \"%s\"", #cz, avp_l_, avp_c_ ? avp_c_ : "(null)"); \
    } while (0)
#define CHECK_MEM(p, q, n) \
    CHECK_MSG(memcmp((p), (q), (n)) == 0, "%s and %s differ in their %zu bytes", #p, #q, (size_t)(n))

static inline int avp_test__run(int argc, char **argv) {
    const char *filter = NULL;
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc) filter = argv[++i];
        else if (strcmp(argv[i], "-v") == 0) verbose = true;
    }
    int ran = 0, failed = 0;
    for (int i = 0; i < avp_test__count; i++) {
        if (filter && !strstr(avp_test__cases[i].name, filter)) continue;
        int before = avp_test__failed_checks;
        if (verbose) printf("%s\n", avp_test__cases[i].name);
        avp_test__cases[i].fn();
        ran++;
        if (avp_test__failed_checks != before) {
            failed++;
            fprintf(stderr, "FAIL: %s\n", avp_test__cases[i].name);
        }
    }
    fflush(stderr);
    if (failed) printf("FAILED (%d of %d tests)\n", failed, ran);
    else printf("OK (%d tests)\n", ran);
    return failed ? 1 : 0;
}

#define AVP_TEST_MAIN(suite_name)                                    \
    static int avp_test__main(int argc, char **argv) {               \
        return avp_test__run(argc, argv);                            \
    }                                                                \
    AVP_MAIN(avp_test__main, "test_" suite_name)
