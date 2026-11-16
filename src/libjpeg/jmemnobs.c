/*
 * jmemnobs.c
 *
 * IJG JPEG library 6b: the "no backing store" system-dependent memory manager.
 * It assumes that all memory requests can be satisfied with no backing store;
 * the game's heap stands in for malloc and free.
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "jmemsys.h" /* import the system-dependent declarations */

/*
 * Memory allocation and freeing are controlled by the regular library
 * routines malloc() and free().
 */
// 0x801C6274
GLOBAL(void *)
jpeg_get_small(j_common_ptr cinfo, size_t sizeofobject)
{
  return (void *) rw_malloc(sizeofobject);
}

// 0x801C627C
GLOBAL(void)
jpeg_free_small(j_common_ptr cinfo, void *object, size_t sizeofobject)
{
  rw_free(object);
}

/*
 * "Large" objects are treated the same as "small" ones.
 */
// 0x801C6284
GLOBAL(void FAR *)
jpeg_get_large(j_common_ptr cinfo, size_t sizeofobject)
{
  return (void FAR *) rw_malloc(sizeofobject);
}

// 0x801C628C
GLOBAL(void)
jpeg_free_large(j_common_ptr cinfo, void FAR *object, size_t sizeofobject)
{
  rw_free(object);
}

/*
 * This routine computes the total memory space available for allocation.
 * Here we always say, "we got all you want bud!"
 */
// 0x801C6294
GLOBAL(long)
jpeg_mem_available(j_common_ptr cinfo, long min_bytes_needed,
                   long max_bytes_needed, long already_allocated)
{
  return max_bytes_needed;
}

/*
 * Backing store (temporary file) management.
 * Since jpeg_mem_available always promised the moon,
 * this should never be called and we can just error out.
 */
// 0x801C629C
GLOBAL(void)
jpeg_open_backing_store(j_common_ptr cinfo, backing_store_ptr info,
                        long total_bytes_needed)
{
  ERREXIT(cinfo, JERR_NO_BACKING_STORE);
}

/*
 * These routines take care of any system-dependent initialization and
 * cleanup required.  Here, there isn't any.
 */
// 0x801C62B8
GLOBAL(long)
jpeg_mem_init(j_common_ptr cinfo)
{
  return 0; /* just set max_memory_to_use to 0 */
}

// 0x801C62C0
GLOBAL(void)
jpeg_mem_term(j_common_ptr cinfo)
{
  /* no work */
}
