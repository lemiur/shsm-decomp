/*
 * fmod_os_misc.cpp: FMOD's operating-system layer for the Wii: the heap, files, time, threads, critical
 * sections and semaphores, over the Revolution SDK.
 */
// Names and layout: Gormiti link map and DWARF (fmodex.a fmod_os_misc.obj)

#include <string.h>

#include <revolution/os.h>
#include <revolution/dvd.h>

#include <fmod/fmod.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_output_revolution.h>
#include <fmod/fmod_string.h>

// The disk-read buffer for the tail of a read that is not a multiple of the DVD's 32 bytes, and its
// 32-byte aligned address.
char *FMOD_OS_GC_TempBufMem;
char *FMOD_OS_GC_TempBuf;

static OSMutex gMemoryCrit;                  // the critical section of the memory pool

static unsigned int gRevolutionAsyncThreadID;   // the thread named "FMOD thread for FMOD_NONBLOCKING"
static unsigned int gRevolutionFileThreadID;    // the thread named "FMOD file thread"

static int gLastDVDStatus;                   // the drive status at the last check

/*
====================
FMOD_OS_Memory_Alloc

Allocates from the operating system's heap, rounded up to the DVD's 32 bytes.
====================
*/
void *FMOD_OS_Memory_Alloc( int size ) {
    return OSAllocFromHeap( __OSCurrHeap, ( size + 31 ) & ~31 );
} // 0x80357E08

/*
====================
FMOD_OS_Memory_Realloc

Moves the block into a new allocation of the size, keeping as much of its data as the old block held.
====================
*/
void *FMOD_OS_Memory_Realloc( void *ptr, int size ) {
    char *newdata;

    newdata = (char *)FMOD_OS_Memory_Alloc( size );
    if ( newdata && ptr ) {
        Cell        *cell   = (Cell *)( (char *)ptr - OSRoundUp32B( sizeof( Cell ) ) );
        unsigned int curlen = cell->size - OSRoundUp32B( sizeof( Cell ) );

        memcpy( newdata, ptr, (unsigned int)size > curlen ? curlen : size );
    }
    if ( ptr ) {
        FMOD_OS_Memory_Free( ptr );
    }
    return newdata;
} // 0x80357E1C

/*
====================
FMOD_OS_Memory_Free

Gives the block back to the operating system's heap.
====================
*/
void FMOD_OS_Memory_Free( void *ptr ) {
    OSFreeToHeap( __OSCurrHeap, ptr );
} // 0x80357EB4

/*
====================
FMOD_OS_File_Open

Opens a file on the disc for reading; the size reported is rounded up to 32 bytes.
====================
*/
FMOD_RESULT FMOD_OS_File_Open( const char *name, char *mode, int unicode, unsigned int *filesize, void **handle ) {
    GCFILE *fp;

    if ( !name ) {
        return FMOD_ERR_INVALID_PARAM;
    }

#line 186
    fp = (GCFILE *)FMOD_Memory_Calloc( sizeof( GCFILE ) );
    if ( !fp ) {
        return FMOD_ERR_MEMORY;
    }

    if ( !strchr( mode, 'r' ) ) {
#line 194
        FMOD_Memory_Free( fp );
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( !DVDOpen( name, &fp->fileinfo ) ) {
#line 200
        FMOD_Memory_Free( fp );
        return FMOD_ERR_FILE_NOTFOUND;
    }

    fp->pos = 0;
    if ( filesize ) {
        *filesize = ( fp->fileinfo.length + 31 ) & ~31;
    }
    *handle = fp;
    return FMOD_OK;
} // 0x80357EC0

/*
====================
FMOD_OS_File_Close

Closes the file and frees its record.
====================
*/
FMOD_RESULT FMOD_OS_File_Close( void *handle ) {
    GCFILE *fp = (GCFILE *)handle;

    if ( !fp ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    DVDClose( &fp->fileinfo );
#line 243
    FMOD_Memory_Free( fp );
    return FMOD_OK;
} // 0x80357FD4

/*
====================
FMOD_OS_File_Cancel

Cancels the read in progress on the file.
====================
*/
FMOD_RESULT FMOD_OS_File_Cancel( void *handle ) {
    GCFILE *fp = (GCFILE *)handle;

    if ( !fp ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    DVDCancel( &fp->fileinfo.cb );
    return FMOD_OK;
} // 0x80358034

/*
====================
FMOD_OS_File_AsyncRead

Starts a disc read and waits for it, sleeping between checks of the drive; gives up with an ejected-disc
error when the disc is lost to any thread but the file thread.
====================
*/
FMOD_RESULT FMOD_OS_File_AsyncRead( GCFILE *fp, void *addr, int length, int offset, unsigned int *read ) {
    int         status;
    DVDFileInfo *fileinfo = &fp->fileinfo;

    status        = DVDGetDriveStatus();
    gLastDVDStatus = status;
    if ( status == DVD_STATE_FATAL_ERROR ) {
        return FMOD_ERR_FILE_BAD;
    }

    if ( DVDReadAsyncPrio( fileinfo, addr, length, offset, NULL, FMOD::OutputRevolution::mDVDReadPriority ) ) {
        unsigned int threadid = (unsigned int)OSGetCurrentThread();

        for ( ;; ) {
            bool check_this_read = true;

            status         = DVDGetDriveStatus();
            gLastDVDStatus = status;
            switch ( status ) {
            case DVD_STATE_FATAL_ERROR:
                return FMOD_ERR_FILE_BAD;
            case DVD_STATE_NO_DISK:
            case DVD_STATE_COVER_OPEN:
            case DVD_STATE_WRONG_DISK:
            case DVD_STATE_RETRY:
                if ( threadid != gRevolutionFileThreadID ) {
                    DVDCancel( &fileinfo->cb );
                    return FMOD_ERR_FILE_DISKEJECTED;
                }
                check_this_read = false;
                break;
            }

            if ( check_this_read ) {
                switch ( DVDGetCommandBlockStatus( &fileinfo->cb ) ) {
                case DVD_STATE_FATAL_ERROR:
                    return FMOD_ERR_FILE_BAD;
                case DVD_STATE_END:
                    *read = length;
                    return FMOD_OK;
                case DVD_STATE_CANCELED:
                    *read = length;
                    return FMOD_OK;
                }
            }

            if ( threadid != FMOD::gRevolutionMainThreadID ) {
                FMOD_File_SetDiskBusy( 0 );
            }
            OSSleepTicks( OSMillisecondsToTicks( 1 ) );
            if ( threadid != FMOD::gRevolutionMainThreadID ) {
                FMOD_File_SetDiskBusy( 1 );
            }
        }
    }

    *read = 0;
    return FMOD_OK;
} // 0x80358068

/*
====================
FMOD_OS_File_Read

Reads count bytes at the file's position. The DVD reads whole 32-byte blocks into an aligned buffer, so the
tail is read into the spare buffer and copied.
====================
*/
FMOD_RESULT FMOD_OS_File_Read( void *handle, void *buf, unsigned int count, unsigned int *read ) {
    FMOD_RESULT  result;
    GCFILE      *fp = (GCFILE *)handle;
    unsigned int rd;
    unsigned int bytesleft = 0;
    int          realsize;

    if ( !fp ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( fp->pos >= fp->fileinfo.length ) {
        *read = 0;
        return FMOD_ERR_FILE_EOF;
    }

    if ( (unsigned int)buf & 31 ) {
        *read = 0;
        return FMOD_ERR_FILE_BAD;
    }

    realsize = count;
    if ( fp->pos + count > fp->fileinfo.length ) {
        realsize  = fp->fileinfo.length - fp->pos;
        bytesleft = realsize & 31;
        realsize &= ~31;
    }

    result = FMOD_OS_File_AsyncRead( fp, buf, realsize, fp->pos, &rd );
    if ( result != FMOD_OK ) {
        return result;
    }
    fp->pos += rd;

    if ( bytesleft ) {
        unsigned int rd2;

        if ( !FMOD_OS_GC_TempBufMem ) {
#line 508
            FMOD_OS_GC_TempBufMem = (char *)FMOD_Memory_Calloc( 0x40 );
            if ( !FMOD_OS_GC_TempBufMem ) {
                return FMOD_ERR_MEMORY;
            }
            FMOD_OS_GC_TempBuf = (char *)( ( (unsigned int)FMOD_OS_GC_TempBufMem + 31 ) & ~31 );
        }

        result = FMOD_OS_File_AsyncRead( fp, FMOD_OS_GC_TempBuf, 32, fp->pos, &rd2 );
        if ( result != FMOD_OK ) {
            return result;
        }
        memcpy( (char *)buf + rd, FMOD_OS_GC_TempBuf, bytesleft );
        fp->pos += bytesleft;
        *read = rd + bytesleft;
        return FMOD_OK;
    }

    *read  = rd;
    result = FMOD_OK;
    if ( rd != count ) {
        result = FMOD_ERR_FILE_EOF;
    }
    return result;
} // 0x803581E0

/*
====================
FMOD_OS_File_Seek

Sets the file's read position.
====================
*/
FMOD_RESULT FMOD_OS_File_Seek( void *handle, unsigned int pos ) {
    GCFILE *fp = (GCFILE *)handle;

    if ( !fp ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( (int)pos >= 0 ) {
        fp->pos = pos;
    }
    return FMOD_OK;
} // 0x80358370

/*
====================
FMOD_OS_Time_GetNs

The time as the timebase counter reads it, in microseconds modulo 32 bits.
====================
*/
FMOD_RESULT FMOD_OS_Time_GetNs( unsigned int *ns ) {
    unsigned long t = OSGetTick();

    *ns = OSTicksToMicroseconds( t );
    return FMOD_OK;
} // 0x80358394

/*
====================
FMOD_OS_Time_GetMs

The time as the timebase counter reads it, in milliseconds modulo 32 bits.
====================
*/
FMOD_RESULT FMOD_OS_Time_GetMs( unsigned int *ms ) {
    unsigned long t = OSGetTick();

    *ms = OSTicksToMilliseconds( t );
    return FMOD_OK;
} // 0x803583EC

/*
====================
FMOD_OS_Time_Sleep

Puts the calling thread to sleep for ms milliseconds.
====================
*/
FMOD_RESULT FMOD_OS_Time_Sleep( unsigned int ms ) {
    OSSleepTicks( OSMillisecondsToTicks( (OSTime)ms ) );
    return FMOD_OK;
} // 0x80358440

/*
====================
FMOD_OS_Thread_GetCurrentID

The calling thread's id: the address of its OSThread.
====================
*/
FMOD_RESULT FMOD_OS_Thread_GetCurrentID( unsigned int *id ) {
    *id = (unsigned int)OSGetCurrentThread();
    return FMOD_OK;
} // 0x8035849C

/*
====================
FMOD_OS_Thread_Create

Starts a thread. The stack and the OSThread are one allocation, the OSThread at its top; the two named FMOD
threads are remembered by id.
====================
*/
FMOD_RESULT FMOD_OS_Thread_Create( const char *name, unsigned int ( *func )( void * ), void *param,
                                   FMOD_THREAD_PRIORITY priority, void *stack, int stacksize, void **handle ) {
    long      gc_pri;
    OSThread *thread_struct;
    char     *threadstack;

    if ( !handle ) {
        return FMOD_ERR_INVALID_PARAM;
    }

#line 729
    threadstack = (char *)FMOD_Memory_Calloc( stacksize );
    if ( !threadstack ) {
        return FMOD_ERR_MEMORY;
    }

    threadstack += stacksize;
    *(int *)( threadstack - sizeof( OSThread ) - 4 ) = 1;
    thread_struct = (OSThread *)( threadstack - sizeof( OSThread ) );

    switch ( priority ) {
    case FMOD_THREAD_PRIORITY_VERYLOW:
        gc_pri = 31;
        break;
    case FMOD_THREAD_PRIORITY_LOW:
        gc_pri = 24;
        break;
    case FMOD_THREAD_PRIORITY_NORMAL:
        gc_pri = 16;
        break;
    case FMOD_THREAD_PRIORITY_HIGH:
        gc_pri = 12;
        break;
    case FMOD_THREAD_PRIORITY_VERYHIGH:
        gc_pri = 8;
        break;
    case FMOD_THREAD_PRIORITY_CRITICAL:
        gc_pri = 0;
        break;
    }

    if ( OSCreateThread( thread_struct, (OSThreadStartFunction)func, param,
                         threadstack - sizeof( OSThread ) - 4, stacksize - sizeof( OSThread ) - 4, gc_pri,
                         OS_THREAD_ATTR_DETACH ) ) {
        *handle = thread_struct;
        OSResumeThread( thread_struct );
    } else {
#line 772
        FMOD_Memory_Free( thread_struct->stackEnd );
        return FMOD_ERR_MEMORY;
    }

    if ( FMOD_strcmp( name, "FMOD thread for FMOD_NONBLOCKING" ) == 0 ) {
        gRevolutionAsyncThreadID = (unsigned int)thread_struct;
    } else if ( FMOD_strcmp( name, "FMOD file thread" ) == 0 ) {
        gRevolutionFileThreadID = (unsigned int)thread_struct;
    }

    return FMOD_OK;
} // 0x803584D0

/*
====================
FMOD_OS_Thread_Destroy

Releases a thread's stack and OSThread, once the thread has run to its end.
====================
*/
FMOD_RESULT FMOD_OS_Thread_Destroy( void *handle ) {
    OSThread *thread = (OSThread *)handle;

    OSCheckActiveThreads();
#line 807
    FMOD_Memory_Free( thread->stackEnd );
    return FMOD_OK;
} // 0x80358650

/*
====================
FMOD_OS_CriticalSection_Create

Creates a critical section: the memory pool's is the static one, any other a new allocation.
====================
*/
FMOD_RESULT FMOD_OS_CriticalSection_Create( FMOD_OS_CRITICALSECTION **crit, bool memorycrit ) {
    OSMutex *mutex;

    if ( !crit ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( memorycrit ) {
        mutex = &gMemoryCrit;
    } else {
#line 839
        mutex = (OSMutex *)FMOD_Memory_Calloc( sizeof( OSMutex ) );
        if ( !mutex ) {
            return FMOD_ERR_MEMORY;
        }
    }

    OSInitMutex( mutex );
    *crit = (FMOD_OS_CRITICALSECTION *)mutex;
    return FMOD_OK;
} // 0x803586A0

/*
====================
FMOD_OS_CriticalSection_Free

Frees a critical section; the memory pool's static one is not freed.
====================
*/
FMOD_RESULT FMOD_OS_CriticalSection_Free( FMOD_OS_CRITICALSECTION *crit, bool memorycrit ) {
    if ( !crit ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( memorycrit ) {
        return FMOD_OK;
    }

#line 875
    FMOD_Memory_Free( crit );
    return FMOD_OK;
} // 0x80358738

/*
====================
FMOD_OS_CriticalSection_Enter

Locks the critical section.
====================
*/
FMOD_RESULT FMOD_OS_CriticalSection_Enter( FMOD_OS_CRITICALSECTION *crit ) {
    if ( !crit ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    OSLockMutex( (OSMutex *)crit );
    return FMOD_OK;
} // 0x80358798

/*
====================
FMOD_OS_CriticalSection_Leave

Unlocks the critical section.
====================
*/
FMOD_RESULT FMOD_OS_CriticalSection_Leave( FMOD_OS_CRITICALSECTION *crit ) {
    if ( !crit ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    OSUnlockMutex( (OSMutex *)crit );
    return FMOD_OK;
} // 0x803587CC

/*
====================
FMOD_OS_Semaphore_Create

Creates a semaphore with a count of zero.
====================
*/
FMOD_RESULT FMOD_OS_Semaphore_Create( FMOD_OS_SEMAPHORE **sema ) {
    OSSemaphore *sem;

#line 944
    sem = (OSSemaphore *)FMOD_Memory_Calloc( sizeof( OSSemaphore ) );
    if ( !sem ) {
        return FMOD_ERR_MEMORY;
    }

    OSInitSemaphore( sem, 0 );
    *sema = (FMOD_OS_SEMAPHORE *)sem;
    return FMOD_OK;
} // 0x80358800

/*
====================
FMOD_OS_Semaphore_Free

Frees a semaphore.
====================
*/
FMOD_RESULT FMOD_OS_Semaphore_Free( FMOD_OS_SEMAPHORE *sema ) {
    if ( !sema ) {
        return FMOD_ERR_INVALID_PARAM;
    }

#line 976
    FMOD_Memory_Free( sema );
    return FMOD_OK;
} // 0x80358874

/*
====================
FMOD_OS_Semaphore_Wait

Waits for the semaphore's count to be positive and takes one.
====================
*/
FMOD_RESULT FMOD_OS_Semaphore_Wait( FMOD_OS_SEMAPHORE *sema ) {
    if ( !sema ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    OSWaitSemaphore( (OSSemaphore *)sema );
    return FMOD_OK;
} // 0x803588C4

/*
====================
FMOD_OS_Semaphore_Signal

Adds one to the semaphore's count; the flag is not used.
====================
*/
FMOD_RESULT FMOD_OS_Semaphore_Signal( FMOD_OS_SEMAPHORE *sema, bool flag ) {
    if ( !sema ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    OSSignalSemaphore( (OSSemaphore *)sema );
    return FMOD_OK;
} // 0x803588F8

/*
====================
FMOD_OS_CheckDriverList

The Wii has one fixed output device, so the list never changes.
====================
*/
FMOD_RESULT FMOD_OS_CheckDriverList( bool *devicelistchanged ) {
    return FMOD_OK;
} // 0x8035892C
