#include <string.h>

/* Zero-byte tests over a word: the constants of the classic has-zero-byte trick. */
#define K1 0x80808080
#define K2 0xFEFEFEFF

/*
 * strcpy
 *
 * Copies word by word once both strings share an alignment.
 */
char* strcpy(char* dst, const char* src) {
    register unsigned char* destb;
    register unsigned char* fromb;
    register unsigned long w, t, align;
    register unsigned long k1 = K1;
    register unsigned long k2 = K2;

    fromb = (unsigned char*)src;
    destb = (unsigned char*)dst;

    if ((align = ((int)fromb & 3)) != ((int)destb & 3)) {
        goto bytecopy;
    }

    if (align) {
        if ((*destb = *fromb) == 0) {
            return dst;
        }

        for (align = 3 - align; align; align--) {
            if ((*(++destb) = *(++fromb)) == 0) {
                return dst;
            }
        }

        ++destb;
        ++fromb;
    }

    w = *((unsigned long*)fromb);

    t = w + k2;
    t = t & ~w;
    t = t & k1;
    if (t) {
        goto bytecopy;
    }

    destb -= 4;

    do {
        destb += 4;
        *((unsigned long*)destb) = w;
        fromb += 4;
        w = *((unsigned long*)fromb);
        t = w + k2;
        t = t & ~w;
        t = t & k1;
    } while (!t);

    destb += 4;

bytecopy:
    if ((*destb = *fromb) == 0) {
        return dst;
    }

    do {
        *(++destb) = *(++fromb);
    } while (*destb != 0);

    return dst;
} // 0x8019D564

/*
 * strncpy
 *
 * Copies at most n bytes, padding the rest of the destination with zeros once
 * the source ends.
 */
char* strncpy(char* dst, const char* src, size_t n) {
    const unsigned char* p = (const unsigned char*)src - 1;
    unsigned char* q = (unsigned char*)dst - 1;

    n++;

    while (--n) {
        if (!(*++q = *++p)) {
            while (--n) {
                *++q = 0;
            }
            break;
        }
    }

    return dst;
} // 0x8019D624

/*
 * strcat
 *
 * Appends src to the end of dst.
 */
char* strcat(char* dst, const char* src) {
    const unsigned char* p = (const unsigned char*)src - 1;
    unsigned char* q = (unsigned char*)dst - 1;

    while (*++q) {
    }

    q--;

    while ((*++q = *++p)) {
    }

    return dst;
} // 0x8019D668

/*
 * strncat
 *
 * Appends at most n bytes of src to dst and always terminates the result.
 */
char* strncat(char* dst, const char* src, size_t n) {
    const unsigned char* p = (const unsigned char*)src - 1;
    unsigned char* q = (unsigned char*)dst - 1;

    while (*++q) {
    }

    q--;

    n++;

    while (--n) {
        if (!(*++q = *++p)) {
            q--;
            break;
        }
    }

    *++q = 0;

    return dst;
} // 0x8019D694

/*
 * strcmp
 *
 * Compares word by word once both strings share an alignment, falling back to
 * bytes at the first word that contains a zero byte or differs.
 */
int strcmp(const char* str1, const char* str2) {
    register unsigned char* left = (unsigned char*)str1;
    register unsigned char* right = (unsigned char*)str2;
    register unsigned long l, r, t, k1 = K1, k2 = K2;
    register unsigned long align, a, b;
    unsigned long c1, c2;
    int diff;

    a = *left;
    b = *right;
    if ((diff = a - b) != 0) {
        return diff;
    }

    if ((align = ((int)left & 3)) != ((int)right & 3)) {
        goto bytecmp;
    }

    if (align) {
        if (a == 0) {
            return 0;
        }

        for (align = 3 - align; align; align--) {
            a = *(++left);
            b = *(++right);
            if ((diff = a - b) != 0) {
                return diff;
            }
            if (a == 0) {
                return 0;
            }
        }

        left++;
        right++;
    }

    l = *(unsigned long*)left;
    r = *(unsigned long*)right;

    t = l + k2;
    t = t & ~l;
    if (t & k1) {
        goto adjust;
    }

    while (l == r) {
        l = *(++((unsigned long*)left));
        r = *(++((unsigned long*)right));
        if ((l + k2) & k1) {
            goto adjust;
        }
    }

adjust:
    a = *left;
    b = *right;
    if ((diff = a - b) != 0) {
        return diff;
    }

bytecmp:
    if (a == 0) {
        return 0;
    }

    for (;;) {
        c1 = *(++left);
        c2 = *(++right);
        if ((diff = c1 - c2) != 0) {
            return diff;
        }
        if (c1 == 0) {
            return 0;
        }
    }
} // 0x8019D6E0

/*
 * strncmp
 *
 * Compares at most n bytes.
 */
int strncmp(const char* str1, const char* str2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)str1 - 1;
    const unsigned char* p2 = (const unsigned char*)str2 - 1;
    unsigned long c1, c2;

    n++;

    while (--n) {
        c1 = *++p1;
        c2 = *++p2;
        if (c1 != c2) {
            return c1 - c2;
        }
        if (!c1) {
            break;
        }
    }

    return 0;
} // 0x8019D7FC

/*
 * strchr
 *
 * Finds the first occurrence of a character, the terminator included.
 */
char* strchr(const char* str, int chr) {
    const unsigned char* p = (const unsigned char*)str - 1;
    unsigned long c = (chr & 0xFF);
    unsigned long ch;

    while ((ch = *++p) != 0) {
        if (ch == c) {
            return (char*)p;
        }
    }

    return c ? NULL : (char*)p;
} // 0x8019D83C

/*
 * strtok
 *
 * Splits a string into tokens separated by any character of set; the
 * position reached is kept between calls.
 */
char* strtok(char* str, const char* set) {
    static char* empty = "";
    static char* next = "";
    unsigned char map[32] = {0};
    unsigned char* p;
    unsigned char* token;
    unsigned long ch;

    if (str) {
        next = str;
    }

    for (p = (unsigned char*)set - 1; (ch = *++p) != 0;) {
        map[(ch >> 3) & 0x1F] |= (unsigned char)(1 << (ch & 7));
    }

    for (p = (unsigned char*)next - 1; (ch = *++p) != 0;) {
        if (!(map[(ch >> 3) & 0x1F] & (unsigned char)(1 << (ch & 7)))) {
            break;
        }
    }

    if (ch == 0) {
        next = empty;
        return NULL;
    }

    token = p;

    for (; (ch = *++p) != 0;) {
        if (map[(ch >> 3) & 0x1F] & (unsigned char)(1 << (ch & 7))) {
            break;
        }
    }

    if (ch == 0) {
        next = empty;
    } else {
        next = (char*)p + 1;
        *p = 0;
    }

    return (char*)token;
} // 0x8019D86C

/*
 * strstr
 *
 * Finds the first occurrence of the string pat in str.
 */
char* strstr(const char* str, const char* pat) {
    unsigned char* s1 = (unsigned char*)str - 1;
    unsigned char* p1 = (unsigned char*)pat;
    unsigned long firstc, c1, c2;

    if (!pat || !(firstc = *p1)) {
        return (char*)str;
    }

    while ((c1 = *++s1) != 0) {
        if (c1 == firstc) {
            const unsigned char* s2 = s1 - 1;
            const unsigned char* p2 = p1 - 1;

            while ((c1 = *++s2) == (c2 = *++p2) && c1) {
            }

            if (!c2) {
                return (char*)s1;
            }
        }
    }

    return NULL;
} // 0x8019D990
