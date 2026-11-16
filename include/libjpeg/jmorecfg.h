/*
 * jmorecfg.h
 *
 * IJG JPEG library 6b: additional configuration options and basic types.
 */

#ifndef JMORECFG_H
#define JMORECFG_H

/* Maximum number of components (color channels) allowed in a JPEG image. */
#define MAX_COMPONENTS 10

/* Basic data types. */

#define BITS_IN_JSAMPLE 8

typedef unsigned char JSAMPLE;
#define GETJSAMPLE(value) ((int) (value))

#define MAXJSAMPLE   255
#define CENTERJSAMPLE 128

typedef short JCOEF;

typedef unsigned char JOCTET;
#define GETJOCTET(value) (value)

typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef short INT16;
typedef long INT32;

typedef unsigned int JDIMENSION;

#define JPEG_MAX_DIMENSION 65500L

/* Function storage classes. */
#define METHODDEF(type) static type
#define LOCAL(type)     static type
#define GLOBAL(type)    type
#define EXTERN(type)    extern type

#define JMETHOD(type, methodname, arglist) type (*methodname) arglist

#define FAR

#ifndef HAVE_BOOLEAN
typedef int boolean;
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#ifdef JPEG_INTERNALS
#define JPEG_INTERNAL_OPTIONS
#endif

#ifdef JPEG_INTERNAL_OPTIONS

/* Capability options common to encoder and decoder. */
#define DCT_ISLOW_SUPPORTED
#define DCT_IFAST_SUPPORTED
#define DCT_FLOAT_SUPPORTED

/* Encoder capability options. */
#undef  C_ARITH_CODING_SUPPORTED
#define C_MULTISCAN_FILES_SUPPORTED
#define C_PROGRESSIVE_SUPPORTED
#define ENTROPY_OPT_SUPPORTED
#define INPUT_SMOOTHING_SUPPORTED

/* Decoder capability options. */
#undef  D_ARITH_CODING_SUPPORTED
#define D_MULTISCAN_FILES_SUPPORTED
#define D_PROGRESSIVE_SUPPORTED
#define SAVE_MARKERS_SUPPORTED
#define BLOCK_SMOOTHING_SUPPORTED
#define IDCT_SCALING_SUPPORTED
#undef  UPSAMPLE_SCALING_SUPPORTED
#define UPSAMPLE_MERGING_SUPPORTED
#define QUANT_1PASS_SUPPORTED
#define QUANT_2PASS_SUPPORTED

/* Ordering of RGB data in scanlines passed to or from the application. */
#define RGB_RED       0
#define RGB_GREEN     1
#define RGB_BLUE      2
/* This build emits 4 bytes per RGB pixel (the fourth byte is skipped, not
 * written): jdcolor and jdmerge advance their output pointers by 4. */
#define RGB_PIXELSIZE 4

/* Definitions for speed-related optimizations. */
#define INLINE
#define MULTIPLIER int
#define FAST_FLOAT float

#endif /* JPEG_INTERNAL_OPTIONS */

#endif /* JMORECFG_H */
