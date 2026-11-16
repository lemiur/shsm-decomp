#ifndef CLIMAX_HEAP_H
#define CLIMAX_HEAP_H

#include <revolution/types.h>
#include <stddef.h>
#include <new>

/*
 * The Climax memory interface (0x801AD194-0x801AD1AC, unit not yet reconstructed): allocation
 * from a numbered heap with flags. Header name invented. It stands apart from <climax/Memory.h>,
 * which also brings in the engine's stand-in RenderWare types, so a unit that uses the real
 * RenderWare headers can include it.
 */

/* Heap arguments every game factory passes; their meaning is unknown. Names invented. */
#define GAME_HEAP           2
#define GAME_HEAP_FLAGS     0x40000
/* The flags the audio system allocates with (Audio::System, the file callbacks); meaning unknown. Name invented. */
#define AUDIO_HEAP_FLAGS    0x4040000

// defined in 0x801AD194, unit not yet reconstructed
void*   operator new( size_t size, int heap, int flags );
// defined in 0x801AD1A0, unit not yet reconstructed: forwards to fn_801ACC20
void*   operator new[]( size_t size, int heap, int flags );

// defined in 0x801AD1AC, unit not yet reconstructed (Gormiti name RWSMalloc): allocates size bytes.
void*   RWSMalloc( unsigned long size, unsigned int flags );
// defined in 0x801AD1BC, unit not yet reconstructed (no counterpart in Gormiti; it frees a block RWSMalloc
// returned, as the free of zlib in ZStream shows):
extern "C" void fn_801AD1BC( void* block );

/* Deletes the array p unless it is NULL. Name invented. */
template< class T >
inline void SafeDeleteArray( T* p ) {
    if ( p != NULL ) {
        delete[] p;
    }
}

#endif
