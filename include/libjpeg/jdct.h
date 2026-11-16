/*
 * jdct.h
 *
 * IJG JPEG library 6b: private declarations for the DCT subsystem
 * (decoder half).  Shared by jddctmgr.c and the jidct*.c routines.
 */

/*
 * A forward DCT routine is given a pointer to a work area of type DCTELEM[];
 * the DCT is to be performed in-place in that buffer.  Type DCTELEM is int
 * for 8-bit samples.
 */
typedef int DCTELEM;

/*
 * Each IDCT routine is responsible for range-limiting its results and
 * converting them to unsigned form (0..MAXJSAMPLE).  The raw outputs could
 * be quite far out of range if the input data is corrupt, so a bulletproof
 * range-limiting step is required.  We use a mask-and-table-lookup method
 * to do the combined operations quickly.
 */

#define IDCT_range_limit(cinfo) ((cinfo)->sample_range_limit + CENTERJSAMPLE)
#define RANGE_MASK (MAXJSAMPLE * 4 + 3) /* 2 bits wider than legal samples */

/* Dequantization multiplier table types, one per IDCT method. */
typedef MULTIPLIER ISLOW_MULT_TYPE; /* short or int, whichever is faster */
typedef MULTIPLIER IFAST_MULT_TYPE; /* 16 bits is OK, use short if faster */
#define IFAST_SCALE_BITS  2         /* fractional bits in scale factors */
typedef FAST_FLOAT FLOAT_MULT_TYPE; /* preferred floating type */

/* Inverse DCT routines. */
EXTERN(void) jpeg_idct_islow(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                             JCOEFPTR coef_block, JSAMPARRAY output_buf, JDIMENSION output_col);
EXTERN(void) jpeg_idct_ifast(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                             JCOEFPTR coef_block, JSAMPARRAY output_buf, JDIMENSION output_col);
EXTERN(void) jpeg_idct_float(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                             JCOEFPTR coef_block, JSAMPARRAY output_buf, JDIMENSION output_col);
EXTERN(void) jpeg_idct_4x4(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                           JCOEFPTR coef_block, JSAMPARRAY output_buf, JDIMENSION output_col);
EXTERN(void) jpeg_idct_2x2(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                           JCOEFPTR coef_block, JSAMPARRAY output_buf, JDIMENSION output_col);
EXTERN(void) jpeg_idct_1x1(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                           JCOEFPTR coef_block, JSAMPARRAY output_buf, JDIMENSION output_col);

/*
 * Macros for handling fixed-point arithmetic; these are used by many but
 * not all of the DCT/IDCT modules.
 *
 * All values are expected to be of type INT32.
 * Fractional constants are scaled left by CONST_BITS bits.
 * CONST_BITS is defined within each module using these macros.
 */
#define ONE ((INT32) 1)
#define CONST_SCALE (ONE << CONST_BITS)

/* Convert a positive real constant to an integer scaled by CONST_SCALE. */
#define FIX(x) ((INT32) ((x) * CONST_SCALE + 0.5))

/* Descale and correctly round an INT32 value that's scaled by N bits. */
#define DESCALE(x, n) RIGHT_SHIFT((x) + (ONE << ((n) - 1)), n)

/* Multiply an INT32 variable by an INT32 constant to yield an INT32 result. */
#define MULTIPLY16C16(var, const) ((var) * (const))

/* Multiply two INT32 variables (used for 16x16 multiplies in jddctmgr.c). */
#define MULTIPLY16V16(var1, var2) ((var1) * (var2))
