// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common/Src/extras.c
/* Unit name unconfirmed: MSL extras.c, as in the public decomps; no DOL string names the file. */

#include <ctype.h>
#include <extras.h>

/*
 * strlwr
 *
 * Converts a string to lower case in place through the current locale.
 */
char* strlwr(char* str) {
    char* p = str;

    while (*p) {
        *p = tolower(*p);
        p++;
    }

    return str;
} // 0x801A60E4

/*
 * stricmp
 *
 * Compares two strings without regard to case; returns -1, 0 or 1.
 */
int stricmp(const char* str1, const char* str2) {
    char a_var;
    char b_var;

    do {
        b_var = tolower(*str1++);
        a_var = tolower(*str2++);

        if (b_var < a_var) {
            return -1;
        }

        if (b_var > a_var) {
            return 1;
        }
    } while (b_var != 0);

    return 0;
} // 0x801A6138

/*
 * strupr
 *
 * Converts a string to upper case in place through the current locale.
 */
char* strupr(char* str) {
    char* p = str;

    while (*p) {
        *p = toupper(*p);
        p++;
    }

    return str;
} // 0x801A61D8
