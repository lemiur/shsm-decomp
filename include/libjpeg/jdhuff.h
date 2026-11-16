/*
 * jdhuff.h
 *
 * IJG JPEG library 6b: declarations for Huffman entropy decoding routines
 * that are shared between the sequential decoder (jdhuff.c) and the
 * progressive decoder (jdphuff.c).  No other modules need to see these.
 */

#ifndef JDHUFF_H
#define JDHUFF_H

/* Derived data constructed for each Huffman table */

#define HUFF_LOOKAHEAD 8 /* # of bits of lookahead */

typedef struct {
  /* Basic tables: (element [0] of each array is unused) */
  INT32 maxcode[18];  /* largest code of length k (-1 if none) */
  /* (maxcode[17] is a sentinel to ensure jpeg_huff_decode terminates) */
  INT32 valoffset[17]; /* huffval[] offset for codes of length k */
  /* valoffset[k] = huffval[] index of 1st symbol of code length k, less
   * the smallest code of length k; so given a code of length k, the
   * corresponding symbol is huffval[code + valoffset[k]]
   */

  /* Link to public Huffman table (needed only in jpeg_huff_decode) */
  JHUFF_TBL *pub;

  /* Lookahead tables: indexed by the next HUFF_LOOKAHEAD bits of
   * the input data stream.  If the next Huffman code is no more
   * than HUFF_LOOKAHEAD bits long, we can obtain its length and
   * the corresponding symbol directly from these tables.
   */
  int look_nbits[1 << HUFF_LOOKAHEAD]; /* # bits, or 0 if too long */
  UINT8 look_sym[1 << HUFF_LOOKAHEAD]; /* symbol, or unused */
} d_derived_tbl;

/* Expand a Huffman table definition into the derived format */
EXTERN(void) jpeg_make_d_derived_tbl(j_decompress_ptr cinfo, boolean isDC, int tblno,
                                     d_derived_tbl **pdtbl);

/*
 * Fetching the next N bits from the input stream is a time-critical operation
 * for the Huffman decoders.  The decoder state is kept in local variables
 * (get_buffer, bits_left) and flushed to the permanent state only at the end
 * of an MCU.
 */

typedef INT32 bit_buf_type; /* type of bit-extraction buffer */
#define BIT_BUF_SIZE 32     /* size of buffer in bits */

/* Persistent state; this is stored in the decoder object between MCUs. */
typedef struct {
  bit_buf_type get_buffer; /* current bit-extraction buffer */
  int bits_left;           /* # of unused bits in it */
} bitread_perm_state;

/* Working state used within the decoding of one MCU. */
typedef struct {
  const JOCTET *next_input_byte; /* => next byte to read from source */
  size_t bytes_in_buffer;        /* # of bytes remaining in source buffer */
  bit_buf_type get_buffer;       /* current bit-extraction buffer */
  int bits_left;                 /* # of unused bits in it */
  j_decompress_ptr cinfo;        /* back link to decompress master record */
} bitread_working_state;

/* Macros to declare and load/save bitread local variables. */
#define BITREAD_STATE_VARS \
  register bit_buf_type get_buffer; \
  register int bits_left; \
  bitread_working_state br_state

#define BITREAD_LOAD_STATE(cinfop, permstate) \
  br_state.cinfo = cinfop; \
  br_state.next_input_byte = cinfop->src->next_input_byte; \
  br_state.bytes_in_buffer = cinfop->src->bytes_in_buffer; \
  get_buffer = permstate.get_buffer; \
  bits_left = permstate.bits_left;

#define BITREAD_SAVE_STATE(cinfop, permstate) \
  cinfop->src->next_input_byte = br_state.next_input_byte; \
  cinfop->src->bytes_in_buffer = br_state.bytes_in_buffer; \
  permstate.get_buffer = get_buffer; \
  permstate.bits_left = bits_left

/*
 * These macros provide the in-line portion of bit fetching.
 * Use CHECK_BIT_BUFFER to ensure there are N bits in get_buffer
 * before using GET_BITS, PEEK_BITS, or DROP_BITS.
 * The variables get_buffer and bits_left are assumed to be locals,
 * but the state struct might not be (jpeg_huff_decode needs this).
 */

#define CHECK_BIT_BUFFER(state, nbits, action) \
  { if (bits_left < (nbits)) { \
      if (!jpeg_fill_bit_buffer(&(state), get_buffer, bits_left, nbits)) \
        { action; } \
      get_buffer = (state).get_buffer; bits_left = (state).bits_left; } }

#define GET_BITS(nbits) \
  (((int) (get_buffer >> (bits_left -= (nbits)))) & ((1 << (nbits)) - 1))

#define PEEK_BITS(nbits) \
  (((int) (get_buffer >> (bits_left - (nbits)))) & ((1 << (nbits)) - 1))

#define DROP_BITS(nbits) \
  (bits_left -= (nbits))

/* Load up the bit buffer to a depth of at least nbits */
EXTERN(boolean) jpeg_fill_bit_buffer(bitread_working_state *state, register bit_buf_type get_buffer,
                                     register int bits_left, int nbits);

/*
 * Code for extracting the next Huffman-coded symbol from the input stream.
 * Try the lookahead table first; if the code is longer than HUFF_LOOKAHEAD
 * bits (or the buffer cannot supply that many), fall back to
 * jpeg_huff_decode.
 */

#define HUFF_DECODE(result, state, htbl, failaction, slowlabel) \
{ register int nb, look; \
  if (bits_left < HUFF_LOOKAHEAD) { \
    if (!jpeg_fill_bit_buffer(&state, get_buffer, bits_left, 0)) { failaction; } \
    get_buffer = state.get_buffer; bits_left = state.bits_left; \
    if (bits_left < HUFF_LOOKAHEAD) { \
      nb = 1; goto slowlabel; \
    } \
  } \
  look = PEEK_BITS(HUFF_LOOKAHEAD); \
  if ((nb = htbl->look_nbits[look]) != 0) { \
    DROP_BITS(nb); \
    result = htbl->look_sym[look]; \
  } else { \
    nb = HUFF_LOOKAHEAD + 1; \
slowlabel: \
    if ((result = jpeg_huff_decode(&state, get_buffer, bits_left, htbl, nb)) < 0) \
      { failaction; } \
    get_buffer = state.get_buffer; bits_left = state.bits_left; \
  } \
}

/* Out-of-line case for Huffman code fetching */
EXTERN(int) jpeg_huff_decode(bitread_working_state *state, register bit_buf_type get_buffer,
                             register int bits_left, d_derived_tbl *htbl, int min_bits);

#endif /* JDHUFF_H */
