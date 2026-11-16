/*
 * jmemsys.h
 *
 * IJG JPEG library 6b: interface between the system-independent memory
 * manager (jmemmgr.c) and the system-dependent backend (jmemnobs.c here).
 */

#ifndef JMEMSYS_H
#define JMEMSYS_H

/*
 * These two functions are used to allocate and release small chunks of
 * memory.  (Typically the total amount requested through jpeg_get_small is
 * no more than 20K or so; this will be requested in chunks of a few K each.)
 */
EXTERN(void *) jpeg_get_small(j_common_ptr cinfo, size_t sizeofobject);
EXTERN(void) jpeg_free_small(j_common_ptr cinfo, void *object, size_t sizeofobject);

/*
 * These two functions are used to allocate and release large chunks of
 * memory (up to the total free space designated by jpeg_mem_available).
 */
EXTERN(void FAR *) jpeg_get_large(j_common_ptr cinfo, size_t sizeofobject);
EXTERN(void) jpeg_free_large(j_common_ptr cinfo, void FAR *object, size_t sizeofobject);

/*
 * The macro MAX_ALLOC_CHUNK designates the maximum number of bytes that may
 * be requested in a single call to jpeg_get_large (and jpeg_get_small for that
 * matter, but that case should never come into play).
 */
#ifndef MAX_ALLOC_CHUNK
#define MAX_ALLOC_CHUNK 1000000000L
#endif

/*
 * This routine computes the total space still available for allocation by
 * jpeg_get_large.  If more space than this is needed, backing store will be
 * used.
 */
EXTERN(long) jpeg_mem_available(j_common_ptr cinfo, long min_bytes_needed,
                                long max_bytes_needed, long already_allocated);

/*
 * This structure holds whatever state is needed to access a single
 * backing-store object.  The read/write/close method pointers are called
 * by jmemmgr.c to manipulate the backing-store object; all other fields
 * are private to the system-dependent backing store routines.
 */

#define TEMP_NAME_LENGTH 64 /* max length of a temporary file's name */

typedef struct backing_store_struct *backing_store_ptr;

typedef struct backing_store_struct {
  /* Methods for reading/writing/closing this backing-store object */
  JMETHOD(void, read_backing_store, (j_common_ptr cinfo, backing_store_ptr info,
                                     void FAR *buffer_address, long file_offset,
                                     long byte_count));
  JMETHOD(void, write_backing_store, (j_common_ptr cinfo, backing_store_ptr info,
                                      void FAR *buffer_address, long file_offset,
                                      long byte_count));
  JMETHOD(void, close_backing_store, (j_common_ptr cinfo, backing_store_ptr info));

  /* Private fields for system-dependent backing-store management */
  /* For a typical implementation with temp files, we need: */
  FILE *temp_file;                    /* stdio reference to temp file */
  char temp_name[TEMP_NAME_LENGTH];   /* name of temp file */
} backing_store_info;

/*
 * Initial opening of a backing-store object.  This must fill in the
 * read/write/close pointers in the object.
 */
EXTERN(void) jpeg_open_backing_store(j_common_ptr cinfo, backing_store_ptr info,
                                     long total_bytes_needed);

/*
 * These routines take care of any system-dependent initialization and
 * cleanup required.
 */
EXTERN(long) jpeg_mem_init(j_common_ptr cinfo);
EXTERN(void) jpeg_mem_term(j_common_ptr cinfo);

#endif /* JMEMSYS_H */
