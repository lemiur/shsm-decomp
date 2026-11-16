char *s = "a\"b\\" ; char c = '''; char d = '"';
char *u = "unterminated
int after_unterminated;
const char *m = L"wide" "cat";
#define STR "has // not a comment" /* yes */ x   continued
float f = 1.5e+3f + .5 + 0x1FUL + 1e + 08 + 1.f + 3..4;
int $dollar = a->b ++ <<= >>= ... :: ## %= ^=;
char *bs = "xy";
