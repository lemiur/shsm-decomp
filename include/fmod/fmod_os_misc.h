/*
 * FMOD's operating-system layer (fmod_os_misc.cpp): memory, files, time, threads, critical sections and
 * semaphores over the Revolution SDK. The type name FMOD_OS_CRITICALSECTION is Gormiti's; parameter names
 * follow Gormiti's DWARF where it records them (names marked invented are not in it).
 * Names and layout: Gormiti link map and DWARF (FMOD fmod_os_misc).
 */
#ifndef _FMOD_OS_MISC_H
#define _FMOD_OS_MISC_H

#include "fmod.h"
#include <revolution/os.h>   // before dvd.h: the two headers include each other
#include <revolution/dvd.h>

typedef struct FMOD_OS_CRITICALSECTION FMOD_OS_CRITICALSECTION;
class FMOD_OS_SEMAPHORE;   // opaque (Gormiti DWARF: no members)

// An open file: the DVD file and the read position (Gormiti DWARF, sizeof 0x40).
class GCFILE {
public:
    DVDFileInfo  fileinfo;  // 0x00
    unsigned int pos;       // 0x3C
};

// Thread priorities of the operating-system layer (Gormiti DWARF, fmod_os_misc.cpp).
typedef enum {
    FMOD_THREAD_PRIORITY_VERYLOW  = -2,
    FMOD_THREAD_PRIORITY_LOW      = -1,
    FMOD_THREAD_PRIORITY_NORMAL   = 0,
    FMOD_THREAD_PRIORITY_HIGH     = 1,
    FMOD_THREAD_PRIORITY_VERYHIGH = 2,
    FMOD_THREAD_PRIORITY_CRITICAL = 3
} FMOD_THREAD_PRIORITY;

void       *FMOD_OS_Memory_Alloc( int size );                                                  // allocates from the operating system's heap
void       *FMOD_OS_Memory_Realloc( void *ptr, int size );                                     // reallocates, copying what fits
void        FMOD_OS_Memory_Free( void *ptr );                                                  // frees (parameter name invented)

FMOD_RESULT FMOD_OS_CriticalSection_Create( FMOD_OS_CRITICALSECTION **crit, bool memorycrit );  // creates a critical section
FMOD_RESULT FMOD_OS_CriticalSection_Free(FMOD_OS_CRITICALSECTION *crit, bool memorycrit );   // frees a critical section
FMOD_RESULT FMOD_OS_CriticalSection_Enter( FMOD_OS_CRITICALSECTION *crit );                    // enters a critical section
FMOD_RESULT FMOD_OS_CriticalSection_Leave( FMOD_OS_CRITICALSECTION *crit );                    // leaves a critical section

// The operating system layer's heap (fmod_os_misc.cpp); the default allocation callbacks of a MemPool call them.
void *FMOD_OS_Memory_Alloc( int size );
void *FMOD_OS_Memory_Realloc( void *ptr, int size );
void  FMOD_OS_Memory_Free( void *ptr );

// Scope guard over a critical section: leaves it on destruction if it was entered through this
// object. Layout and class name: Gormiti DWARF (sizeof 0x8); the members are inline in retail.
class LocalCriticalSection {
public:
    FMOD_OS_CRITICALSECTION *mCrit;     // 0x00
    bool                     mEntered;  // 0x04

    LocalCriticalSection( FMOD_OS_CRITICALSECTION *crit ) {
        mEntered = false;
        mCrit    = crit;
    }

    ~LocalCriticalSection() {
        if ( mEntered ) {
            leave();
        }
    }

    // Member names invented (the DWARF records only the data).
    void enter() {
        FMOD_OS_CriticalSection_Enter( mCrit );
        mEntered = true;
    }

    void leave() {
        FMOD_OS_CriticalSection_Leave( mCrit );
        mEntered = false;
    }
};

FMOD_RESULT FMOD_OS_Thread_GetCurrentID( unsigned int *id );                                   // the calling thread's id
FMOD_RESULT FMOD_OS_Thread_Create( const char *name, unsigned int (*func)( void * ), void *param,
                                   FMOD_THREAD_PRIORITY priority, void *stack, int stacksize,
                                   void **handle );                                              // starts a thread (parameter names: Gormiti DWARF; stack name invented)
FMOD_RESULT FMOD_OS_Thread_Destroy( void *handle );                                            // ends a thread
FMOD_RESULT FMOD_OS_Semaphore_Create( FMOD_OS_SEMAPHORE **sema );                              // creates a semaphore
FMOD_RESULT FMOD_OS_Semaphore_Free( FMOD_OS_SEMAPHORE *sema );                                 // frees a semaphore (parameter name invented)
FMOD_RESULT FMOD_OS_Time_GetNs( unsigned int *ns );                                            // the time in nanoseconds
FMOD_RESULT FMOD_OS_Time_GetMs( unsigned int *ms );                                            // the time in milliseconds
FMOD_RESULT FMOD_OS_Time_Sleep( unsigned int ms );                                             // sleeps the calling thread (parameter name invented)
FMOD_RESULT FMOD_OS_Semaphore_Wait( FMOD_OS_SEMAPHORE *sema );                                 // waits on a semaphore (parameter name invented)
FMOD_RESULT FMOD_OS_Semaphore_Signal( FMOD_OS_SEMAPHORE *sema, bool flag );                    // signals a semaphore (parameter names invented)

FMOD_RESULT FMOD_OS_File_Open( const char *name, char *mode, int unicode, unsigned int *filesize, void **handle );  // unicode: invented
FMOD_RESULT FMOD_OS_File_Close( void *handle );
FMOD_RESULT FMOD_OS_File_Cancel( void *handle );
FMOD_RESULT FMOD_OS_File_AsyncRead( GCFILE *fp, void *addr, int length, int offset, unsigned int *read );
FMOD_RESULT FMOD_OS_File_Read( void *handle, void *buf, unsigned int count, unsigned int *read );
FMOD_RESULT FMOD_OS_File_Seek( void *handle, unsigned int pos );

// The disk-read buffer and its 32-byte aligned address (Gormiti DWARF); the Revolution output frees the buffer.
extern char *FMOD_OS_GC_TempBufMem;
extern char *FMOD_OS_GC_TempBuf;
FMOD_RESULT FMOD_OS_CheckDriverList( bool *devicelistchanged );                                 // parameter name invented

#endif /* _FMOD_OS_MISC_H */
