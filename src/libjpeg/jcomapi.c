/*
 * jcomapi.c
 *
 * IJG JPEG library 6b: application interface routines that are used for both
 * compression and decompression.
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"

/*
 * Abort processing of a JPEG compression or decompression operation,
 * but don't destroy the object itself.
 *
 * Closing a data source or destination, if necessary, is the application's
 * responsibility.
 */
// 0x801B86C8
GLOBAL(void)
jpeg_abort(j_common_ptr cinfo)
{
  int pool;

  /* Do nothing if called on a not-initialized or destroyed JPEG object. */
  if (cinfo->mem == NULL)
    return;

  /* Releasing pools in reverse order might help avoid fragmentation
   * with some (brain-damaged) malloc libraries.
   */
  for (pool = JPOOL_NUMPOOLS - 1; pool > JPOOL_PERMANENT; pool--) {
    (*cinfo->mem->free_pool)(cinfo, pool);
  }

  /* Reset overall state for possible reuse of object */
  if (cinfo->is_decompressor) {
    cinfo->global_state = DSTATE_START;
    /* Try to keep application from accessing now-deleted marker list.
     * A bit kludgy to do it here, but this is the most central place.
     */
    ((j_decompress_ptr) cinfo)->marker_list = NULL;
  } else {
    cinfo->global_state = CSTATE_START;
  }
}

/*
 * Destruction of a JPEG object.
 *
 * Everything gets deallocated except the master jpeg_compress_struct itself
 * and the error manager struct.  Both of these are supplied by the application
 * and must be freed, if necessary, by the application.
 */
// 0x801B8734
GLOBAL(void)
jpeg_destroy(j_common_ptr cinfo)
{
  /* We need only tell the memory manager to release everything. */
  /* NB: mem pointer is NULL if memory mgr failed to initialize. */
  if (cinfo->mem != NULL)
    (*cinfo->mem->self_destruct)(cinfo);
  cinfo->mem = NULL;       /* be safe if jpeg_destroy is called twice */
  cinfo->global_state = 0; /* mark it destroyed */
}

/*
 * Convenience routines for allocating quantization and Huffman tables.
 * (Would jutils.c be a more reasonable place to put these?)
 */
// 0x801B8780
GLOBAL(JQUANT_TBL *)
jpeg_alloc_quant_table(j_common_ptr cinfo)
{
  JQUANT_TBL *tbl;

  tbl = (JQUANT_TBL *) (*cinfo->mem->alloc_small)(cinfo, JPOOL_PERMANENT, SIZEOF(JQUANT_TBL));
  tbl->sent_table = FALSE; /* make sure this is false in any new table */
  return tbl;
}

// 0x801B87BC
GLOBAL(JHUFF_TBL *)
jpeg_alloc_huff_table(j_common_ptr cinfo)
{
  JHUFF_TBL *tbl;

  tbl = (JHUFF_TBL *) (*cinfo->mem->alloc_small)(cinfo, JPOOL_PERMANENT, SIZEOF(JHUFF_TBL));
  tbl->sent_table = FALSE; /* make sure this is false in any new table */
  return tbl;
}
