/*
 * jconfig.h
 *
 * IJG JPEG library 6b, configuration for the Wii build (CodeWarrior, big-endian PowerPC).
 */

#ifndef JCONFIG_H
#define JCONFIG_H

#define HAVE_PROTOTYPES
#define HAVE_UNSIGNED_CHAR
#define HAVE_UNSIGNED_SHORT
#undef void
#undef const
#undef CHAR_IS_UNSIGNED
#define HAVE_STDDEF_H
#define HAVE_STDLIB_H
#undef NEED_BSD_STRINGS
#undef NEED_SYS_TYPES_H
#undef NEED_FAR_POINTERS
#undef NEED_SHORT_EXTERNAL_NAMES
#undef INCOMPLETE_TYPES_BROKEN

#ifdef JPEG_INTERNALS

#undef RIGHT_SHIFT_IS_UNSIGNED

#endif /* JPEG_INTERNALS */

#endif /* JCONFIG_H */
