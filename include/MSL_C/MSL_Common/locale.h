#ifndef _MSL_LOCALE_H
#define _MSL_LOCALE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*__mbtowc_func)(wchar_t* pwc, const char* s, size_t n);
typedef int (*__wctomb_func)(char* s, wchar_t wchar);

struct _loc_coll_cmpt;
struct _loc_mon_cmpt;
struct _loc_num_cmpt;
struct _loc_time_cmpt;

struct _loc_ctype_cmpt {
    char CmptName[8];
    const unsigned short* ctype_map_ptr;
    const unsigned char* upper_map_ptr;
    const unsigned char* lower_map_ptr;
    const unsigned short* wctype_map_ptr;
    const wchar_t* wupper_map_ptr;
    const wchar_t* wlower_map_ptr;
    __mbtowc_func decode_mb;
    __wctomb_func encode_wc;
};

struct __locale {
    struct __locale* next_locale;
    char locale_name[48];
    struct _loc_coll_cmpt* coll_cmpt_ptr;
    struct _loc_ctype_cmpt* ctype_cmpt_ptr;
    struct _loc_mon_cmpt* mon_cmpt_ptr;
    struct _loc_num_cmpt* num_cmpt_ptr;
    struct _loc_time_cmpt* time_cmpt_ptr;
};

/* The C locale's numeric and monetary conventions (localeconv). */
struct lconv {
    char* decimal_point;
    char* thousands_sep;
    char* grouping;
    char* mon_decimal_point;
    char* mon_thousands_sep;
    char* mon_grouping;
    char* positive_sign;
    char* negative_sign;
    char* currency_symbol;
    char frac_digits;
    char p_cs_precedes;
    char n_cs_precedes;
    char p_sep_by_space;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
    char* int_curr_symbol;
    char int_frac_digits;
    char int_p_cs_precedes;
    char int_n_cs_precedes;
    char int_p_sep_by_space;
    char int_n_sep_by_space;
    char int_p_sign_posn;
    char int_n_sign_posn;
};

extern struct __locale _current_locale;
extern struct lconv __lconv;

#ifdef __cplusplus
}
#endif

#endif
