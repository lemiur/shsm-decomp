/*
 * dlmalloc.h: the mspace interface of Doug Lea's malloc as FMOD Ex embeds it (src/fmod/dlmalloc.cpp).
 */
// Reference: Doug Lea's malloc (dlmalloc) 2.8.3, public domain (malloc_params, mspace and the mspace_* routines)
#ifndef _FMOD_DLMALLOC_H
#define _FMOD_DLMALLOC_H

#include <stddef.h>

namespace FMOD {

/*
  malloc_params holds global properties, including those that can be
  dynamically set using mallopt. FMOD keeps the one instance in its global state
  (Global::mDLMallocParams) instead of a static of dlmalloc.cpp.
*/
struct malloc_params {
    size_t       magic;
    size_t       page_size;
    size_t       granularity;
    size_t       mmap_threshold;
    size_t       trim_threshold;
    unsigned int default_mflags;
};

/*
  mspace is an opaque type representing an independent
  region of space that supports mspace_malloc, etc.
*/
typedef void *mspace;

mspace create_mspace_with_base( void *base, size_t capacity, int locked );
void  *mspace_malloc( mspace msp, size_t bytes );
void   mspace_free( mspace msp, void *mem );
void  *mspace_realloc( mspace msp, void *mem, size_t newsize );

} // namespace FMOD

#endif /* _FMOD_DLMALLOC_H */
