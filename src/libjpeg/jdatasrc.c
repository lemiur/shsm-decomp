/*
 * jdatasrc.c
 *
 * IJG JPEG library 6b: source manager for reading compressed data from a
 * file.  The game's copy of the stdio source manager, with fread replaced by
 * the game's own file read.
 *
 * Translation unit name unconfirmed (linked after jutils).
 */

/* this is not a core library module, so it doesn't define JPEG_INTERNALS */
#include "jinclude.h"
#include "jpeglib.h"
#include "jerror.h"

/* Expanded data source object for stdio input */

typedef struct {
  struct jpeg_source_mgr pub; /* public fields */

  void *infile;               /* source stream */
  JOCTET *buffer;             /* start of buffer */
  boolean start_of_file;      /* have we gotten any data yet? */
} my_source_mgr;

typedef my_source_mgr *my_src_ptr;

#define INPUT_BUF_SIZE 4096 /* choose an efficiently fread'able size */

/*
 * Initialize source --- called by jpeg_read_header
 * before any data is actually read.
 */
// 0x801C9364
METHODDEF(void)
init_source(j_decompress_ptr cinfo)
{
  my_src_ptr src = (my_src_ptr) cinfo->src;

  /* We reset the empty-input-file flag for each image,
   * but we don't clear the input buffer.
   * This is correct behavior for reading a series of images from one source.
   */
  src->start_of_file = TRUE;
}

/*
 * Fill the input buffer --- called whenever buffer is emptied.
 */
// 0x801C9374
METHODDEF(boolean)
fill_input_buffer(j_decompress_ptr cinfo)
{
  my_src_ptr src = (my_src_ptr) cinfo->src;
  size_t nbytes;

  nbytes = RwStreamRead(src->infile, src->buffer, INPUT_BUF_SIZE);

  if (nbytes <= 0) {
    if (src->start_of_file) /* Treat empty input file as fatal error */
      ERREXIT(cinfo, JERR_INPUT_EMPTY);
    WARNMS(cinfo, JWRN_JPEG_EOF);
    /* Insert a fake EOI marker */
    src->buffer[0] = (JOCTET) 0xFF;
    src->buffer[1] = (JOCTET) JPEG_EOI;
    nbytes = 2;
  }

  src->pub.next_input_byte = src->buffer;
  src->pub.bytes_in_buffer = nbytes;
  src->start_of_file = FALSE;

  return TRUE;
}

/*
 * Skip data --- used to skip over a potentially large amount of
 * uninteresting data (such as an APPn marker).
 */
// 0x801C9448
METHODDEF(void)
skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
  my_src_ptr src = (my_src_ptr) cinfo->src;

  /* Just a dumb implementation for now.  Could use fseek() except
   * it doesn't work on pipes.  Not clear that being smart is worth
   * any trouble anyway --- large skips are infrequent.
   */
  if (num_bytes > 0) {
    while (num_bytes > (long) src->pub.bytes_in_buffer) {
      num_bytes -= (long) src->pub.bytes_in_buffer;
      (void) fill_input_buffer(cinfo);
      /* note we assume that fill_input_buffer will never return FALSE,
       * so suspension need not be handled.
       */
    }
    src->pub.next_input_byte += (size_t) num_bytes;
    src->pub.bytes_in_buffer -= (size_t) num_bytes;
  }
}

/*
 * An additional method that can be provided by data source modules is the
 * resync_to_restart method for error recovery in the presence of RST markers.
 * For the moment, this source module just uses the default resync method
 * provided by the JPEG library.
 */

/*
 * Terminate source --- called by jpeg_finish_decompress
 * after all data has been read.  Often a no-op.
 */
// 0x801C9548
METHODDEF(void)
term_source(j_decompress_ptr cinfo)
{
  /* no work necessary here */
}

/*
 * Prepare for input from a stdio stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing decompression.
 */
// 0x801C954C
GLOBAL(void)
jpeg_stdio_src(j_decompress_ptr cinfo, void *infile)
{
  my_src_ptr src;

  /* The source object and input buffer are made permanent so that a series
   * of JPEG images can be read from the same file by calling jpeg_stdio_src
   * only before the first one.  (If we discarded the buffer at the end of
   * one image, we'd likely lose the start of the next one.)
   * This makes it unsafe to use this manager and a different source
   * manager serially with the same JPEG object.  Caveat programmer.
   */
  if (cinfo->src == NULL) { /* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr *)
      (*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                 SIZEOF(my_source_mgr));
    src = (my_src_ptr) cinfo->src;
    src->buffer = (JOCTET *)
      (*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                 INPUT_BUF_SIZE * SIZEOF(JOCTET));
  }

  src = (my_src_ptr) cinfo->src;
  src->pub.init_source = init_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
  src->pub.term_source = term_source;
  src->infile = infile;
  src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
  src->pub.next_input_byte = NULL; /* until buffer loaded */
}
