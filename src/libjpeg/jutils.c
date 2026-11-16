/*
 * jutils.c
 *
 * IJG JPEG library 6b: tables and miscellaneous utility routines needed for
 * both compression and decompression.
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"

/*
 * jpeg_natural_order[i] is the natural-order position of the i'th element
 * of zigzag order.
 *
 * When reading corrupted data, the Huffman decoders could attempt
 * to reference an entry beyond the end of this array (if the decoded
 * zero run length reaches past the end of the block).  To prevent
 * wild stores without adding an inner-loop test, we put some extra
 * "63"s after the real entries.  This will cause the extra coefficient
 * to be stored in location 63 of the block, not somewhere random.
 * The worst case would be a run-length of 15, which means we need 16
 * fake entries.
 */
const int jpeg_natural_order[DCTSIZE2 + 16] = {
   0,  1,  8, 16,  9,  2,  3, 10,
  17, 24, 32, 25, 18, 11,  4,  5,
  12, 19, 26, 33, 40, 48, 41, 34,
  27, 20, 13,  6,  7, 14, 21, 28,
  35, 42, 49, 56, 57, 50, 43, 36,
  29, 22, 15, 23, 30, 37, 44, 51,
  58, 59, 52, 45, 38, 31, 39, 46,
  53, 60, 61, 54, 47, 55, 62, 63,
  63, 63, 63, 63, 63, 63, 63, 63, /* extra entries for safety in decoder */
  63, 63, 63, 63, 63, 63, 63, 63
};

/*
 * Arithmetic utilities
 */

/* Compute a/b rounded up to next integer, ie, ceil(a/b) */
/* Assumes a >= 0, b > 0 */
// 0x801C929C
GLOBAL(long)
jdiv_round_up(long a, long b)
{
  return (a + b - 1L) / b;
}

/* Compute a rounded up to next multiple of b, ie, ceil(a/b)*b */
/* Assumes a >= 0, b > 0 */
// 0x801C92AC
GLOBAL(long)
jround_up(long a, long b)
{
  a += b - 1L;
  return a - (a % b);
}

/* Copy some rows of samples from one place to another.
 * num_rows rows are copied from input_array[source_row++]
 * to output_array[dest_row++]; these areas may overlap for duplication.
 * The source and destination arrays must be at least as wide as num_cols.
 */
// 0x801C92C8
GLOBAL(void)
jcopy_sample_rows(JSAMPARRAY input_array, int source_row,
                  JSAMPARRAY output_array, int dest_row,
                  int num_rows, JDIMENSION num_cols)
{
  register JSAMPROW inptr, outptr;
  register size_t count = (size_t) (num_cols * SIZEOF(JSAMPLE));
  register int row;

  input_array += source_row;
  output_array += dest_row;

  for (row = num_rows; row > 0; row--) {
    inptr = *input_array++;
    outptr = *output_array++;
    MEMCOPY(outptr, inptr, count);
  }
}

/* Copy a row of coefficient blocks from one place to another. */
// 0x801C9344
GLOBAL(void)
jcopy_block_row(JBLOCKROW input_row, JBLOCKROW output_row, JDIMENSION num_blocks)
{
  MEMCOPY(output_row, input_row, num_blocks * (DCTSIZE2 * SIZEOF(JCOEF)));
}

/* Zero out a chunk of FAR memory. */
/* This might be sample-array data, block-array data, or alloc_large data. */
// 0x801C9358
GLOBAL(void)
jzero_far(void FAR *target, size_t bytestozero)
{
  MEMZERO(target, bytestozero);
}
