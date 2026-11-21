/*
 * fmod_file.cpp: FMOD::File, the base of FMOD's file readers (buffering, double buffering through the
 * file thread, endian swapping and decryption of what is read), and FMOD::FileThread.
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_file.obj)
 * Allocation tags carry the original file's line numbers, restored with #line at each tagged statement.
 */

#include <fmod/fmod.h>
#include <fmod/fmod_file.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_string.h>
#include <fmod/fmod_systemi.h>
#include <fmod/fmod_thread.h>
#include <new>
#include <stdio.h>
#include <string.h>

// The block size a File is created with, set by SystemI::setFileSystem.
unsigned int gBufferSize = 0x800;

// Set by SystemI::setFileSystem when the application supplied its own file callbacks.
bool gUsesUserCallbacks;

// Swaps the bytes of a 16 bit item.
#define FMOD_SWAP16( _x ) ( ( ( ( _x ) & 0xFF00 ) >> 8 ) | ( ( ( _x ) & 0x00FF ) << 8 ) )

// Swaps the bytes of a 32 bit item.
#define FMOD_SWAP32( _x ) ( ( ( ( ( _x ) >> 24 ) & 0xFF ) | ( ( ( _x ) >> 8 ) & 0xFF00 ) ) | ( ( ( _x ) << 24 ) | ( ( ( _x ) << 8 ) & 0xFF0000 ) ) )

// Reverses the order of the eight bits of a byte, the scrambling the encrypted files apply.
#define FMOD_REVERSE_BITS( _x ) \
    { \
        unsigned char __tmp = ( _x ); \
        ( _x ) = ( ( __tmp & 0x01 ) << 7 ); \
        ( _x ) |= ( ( __tmp & 0x02 ) << 5 ); \
        ( _x ) |= ( ( __tmp & 0x04 ) << 3 ); \
        ( _x ) |= ( ( __tmp & 0x08 ) << 1 ); \
        ( _x ) |= ( ( __tmp & 0x10 ) >> 1 ); \
        ( _x ) |= ( ( __tmp & 0x20 ) >> 3 ); \
        ( _x ) |= ( ( __tmp & 0x40 ) >> 5 ); \
        ( _x ) |= ( ( __tmp & 0x80 ) >> 7 ); \
    }

/*
====================
FMOD_File_SetDiskBusy

Holds the file lock while the application uses the disk, so the file thread leaves it alone.
====================
*/
FMOD_RESULT FMOD_File_SetDiskBusy( int busy ) {
    if ( busy ) {
        FMOD_OS_CriticalSection_Enter( lbl_80529020->mFileCrit );
        lbl_80529020->mFileBusy = true;
    } else {
        lbl_80529020->mFileBusy = false;
        FMOD_OS_CriticalSection_Leave( lbl_80529020->mFileCrit );
    }

    return FMOD_OK;
} // 0x80373840

namespace FMOD {

/*
====================
fileThreadFunc

The file thread's entry: runs the thread's pass.
====================
*/
void fileThreadFunc( void *data ) {
    FileThread *fileThread = static_cast< FileThread * >( data );

    fileThread->threadFunc();
} // 0x8037389C

/*
====================
FileThread::FileThread

Inlined into File::getFileThread; its out-of-line copy is dead-stripped (Gormiti map: UNUSED).
====================
*/
FileThread::FileThread() {
    mFileListCrit    = NULL;
    mFileListCurrent = NULL;
    mFileListNext    = NULL;
    mThreadActive    = false;
    mDeviceType      = 0;
}

/*
====================
FileThread::init

Creates the thread's lock, starts the thread and links it into the list of file threads.
Inlined into File::getFileThread; its out-of-line copy is dead-stripped (Gormiti map: UNUSED).
====================
*/
FMOD_RESULT FileThread::init( int devicetype, bool owned, SystemI *system ) {
    FMOD_RESULT result;

    mDeviceType = devicetype;
    mOwned      = owned;

    result = FMOD_OS_CriticalSection_Create( &mFileListCrit, false );
    if ( result != FMOD_OK ) {
        return result;
    }

    result = mThread.initThread( "FMOD file thread", fileThreadFunc, this, Thread::PRIORITY_HIGH, NULL, 16 * 1024, true, 0,
                                 system );
    if ( result != FMOD_OK ) {
        return result;
    }

    mThreadActive = true;
    addAfter( &lbl_80529020->mFileThreadHead );

    return FMOD_OK;
}

/*
====================
FileThread::release

Unlinks the thread, stops it and frees it.
Inlined into File::close and File::shutDown; its out-of-line copy is dead-stripped (Gormiti map: UNUSED).
====================
*/
FMOD_RESULT FileThread::release() {
    removeNode();
    mThreadActive = false;
    mThread.closeThread();

    if ( mFileListCrit ) {
        FMOD_OS_CriticalSection_Free( mFileListCrit, false );
    }

#line 314
    FMOD_Memory_Free( this );

    return FMOD_OK;
}

/*
====================
FileThread::threadFunc

One pass of the thread: fills the half buffer of every file that asked for one.
Inlined into fileThreadFunc; its out-of-line copy is dead-stripped (Gormiti map: UNUSED).
====================
*/
FMOD_RESULT FileThread::threadFunc() {
    if ( mThreadActive ) {
        FMOD_OS_CriticalSection_Enter( mFileListCrit );

        mFileListCurrent = mFileListHead.getNext();
        while ( mFileListCurrent != &mFileListHead ) {
            File *file;

            mFileListNext = mFileListCurrent->getNext();

            file = static_cast< File * >( mFileListCurrent );
            if ( file->mFlags & FILE_FLAG_FLIP ) {
                FMOD_OS_CriticalSection_Leave( mFileListCrit );
                file->flip( false );
                FMOD_OS_CriticalSection_Enter( mFileListCrit );
            }

            mFileListCurrent = mFileListNext;
        }

        FMOD_OS_CriticalSection_Leave( mFileListCrit );
    }

    return FMOD_OK;
}

/*
====================
File::getFileThread

Finds the thread of this file's device, starting it when there is none.
====================
*/
FMOD_RESULT File::getFileThread() {
    FMOD_RESULT     result;
    LinkedListNode *current;
    bool            found      = false;
    FileThread     *filethread = NULL;

    for ( current = lbl_80529020->mFileThreadHead.getNext(); current != &lbl_80529020->mFileThreadHead;
          current = current->getNext() ) {
        filethread = static_cast< FileThread * >( current );
        if ( filethread->mDeviceType == 5 ) {
            found = true;
            break;
        }
    }

    if ( !found ) {
#line 386
        filethread = new ( FMOD_Memory_Alloc( sizeof( FileThread ) ) ) FileThread;
        if ( !filethread ) {
            return FMOD_ERR_MEMORY;
        }

        result = filethread->init( 5, false, mSystem );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    mFileThread = filethread;

    return FMOD_OK;
} // 0x80373948

/*
====================
File::File
====================
*/
File::File() {
    init( 0, gBufferSize );
} // 0x80373AEC

/*
====================
File::init

Puts the file in its closed state: nothing buffered, positioned at the start.
====================
*/
FMOD_RESULT File::init( unsigned int length, int blocksize ) {
    mBlockSize           = blocksize;
    mFlags               = FILE_FLAG_SEEKABLE | FILE_FLAG_NAMED;
    mFileThread          = NULL;
    mRiderHandle         = NULL;
    mRiderUserData       = NULL;
    mSystem              = NULL;
    mStartOffset         = 0;
    mBuffer              = NULL;
    mBufferMemory        = NULL;
    mBufferSize          = 0;
    mCurrentPosition     = 0;
    mNextPosition        = 0;
    mNextPositionDisplay = 0;
    mBlockOffset         = 0;
    mBufferPos           = 0;
    mBufferSkip          = 0;
    mAsyncError          = FMOD_OK;
    mEncryptionKeyLength = 0;
    mEncryptionKeyIndex  = 0;
    mSema                = NULL;
    mPercentBuffered     = 0;
    mLengthOriginal      = length;
    mLength              = length;
    mFileSize            = length;
    memset( mName, 0, sizeof( mName ) );

    return FMOD_OK;
} // 0x80373B40

/*
====================
File::shutDown

Releases every file thread and the lock they share.
====================
*/
FMOD_RESULT File::shutDown() {
    LinkedListNode *current;

    current = lbl_80529020->mFileThreadHead.getNext();
    while ( current != &lbl_80529020->mFileThreadHead ) {
        LinkedListNode *next = current->getNext();

        static_cast< FileThread * >( current )->release();
        current = next;
    }

    if ( lbl_80529020->mFileCrit ) {
        FMOD_OS_CriticalSection_Free( lbl_80529020->mFileCrit, false );
        lbl_80529020->mFileCrit = NULL;
    }

    return FMOD_OK;
} // 0x80373BD8

/*
====================
File::open

Opens the file through the derived class and sets up its buffer. name_or_data is a name, or the
memory of a memory file; length 0 takes the length the open reports.
====================
*/
FMOD_RESULT File::open( const char *name_or_data, unsigned int length, bool unicode, const char *encryptionkey ) {
    FMOD_RESULT result;

    mStartOffset         = 0;
    mCurrentPosition     = 0;
    mNextPosition        = 0;
    mNextPositionDisplay = 0;
    mBlockOffset         = 0;
    mBufferPos           = 0;
    mBufferSkip          = 0;
    mAsyncError          = FMOD_OK;
    mFlags &= ~FILE_FLAG_NOSWAP;
    mFlags &= ~( FILE_FLAG_STALLED | FILE_FLAG_CANCELLED );
    mEncryptionKeyLength = 0;
    mEncryptionKeyIndex  = 0;
    mLength              = length;
    mFileSize            = length;

    if ( unicode ) {
        mFlags |= FILE_FLAG_UNICODE;
    } else {
        mFlags &= ~FILE_FLAG_UNICODE;
    }

    if ( encryptionkey ) {
        mEncryptionKeyLength = FMOD_strlen( encryptionkey );
        if ( mEncryptionKeyLength > 32 ) {
            mEncryptionKeyLength = 32;
        }

        memset( mEncryptionKey, 0, 32 );
        FMOD_strncpy( mEncryptionKey, encryptionkey, mEncryptionKeyLength );
    }

    mBufferSize = mBlockSize;
    if ( mBufferSize && !mBuffer ) {
#line 585
        mBufferMemory = FMOD_Memory_Calloc( mBufferSize + 32 );
        if ( !mBufferMemory ) {
            return FMOD_ERR_MEMORY;
        }

        mBuffer = (void *)( ( (unsigned int)mBufferMemory + 31 ) & ~31 );
    }

    result = reallyOpen( name_or_data, &mFileSize );
    if ( result != FMOD_OK ) {
        if ( mBufferMemory ) {
#line 603
            FMOD_Memory_Free( mBufferMemory );
            mBufferMemory = NULL;
            mBuffer       = NULL;
        }

        return result;
    }

    if ( name_or_data && ( mFlags & FILE_FLAG_NAMED ) ) {
        if ( mFlags & FILE_FLAG_UNICODE ) {
            FMOD_strncpyW( (short *)mName, (const short *)name_or_data, 128 );
            FMOD_wtoa( mName );
        } else {
            FMOD_strncpy( mName, name_or_data, 256 );
        }

        mName[255] = 0;
    }

    if ( mSystem && mSystem->mOpenRiderCallback ) {
        mSystem->mOpenRiderCallback( name_or_data, unicode, &mFileSize, &mRiderHandle, &mRiderUserData );
    }

    if ( !mLength ) {
        mLength = mFileSize;
    }
    mLengthOriginal = mLength;

    return FMOD_OK;
} // 0x80373CC4

/*
====================
File::close

Cancels what the file thread is doing for the file, takes the file off the thread, closes it through
the derived class and frees its buffer.
====================
*/
FMOD_RESULT File::close() {
    FMOD_RESULT result;

    mFlags |= FILE_FLAG_CANCELLED;
    reallyCancel();

    if ( mSema && ( mFlags & FILE_FLAG_READING ) ) {
        FMOD_OS_Semaphore_Wait( mSema );
        FMOD_OS_Semaphore_Signal( mSema, false );
    }

    if ( mFileThread ) {
        FMOD_OS_CriticalSection_Enter( mFileThread->mFileListCrit );
        if ( this == mFileThread->mFileListNext ) {
            mFileThread->mFileListNext = getNext();
        }
        removeNode();
        FMOD_OS_CriticalSection_Leave( mFileThread->mFileListCrit );

        if ( mFileThread->mOwned ) {
            mFileThread->release();
        }
        mFileThread = NULL;
    }

    result = reallyClose();

    if ( mSema ) {
        FMOD_OS_Semaphore_Free( mSema );
    }

    if ( mSystem && mSystem->mCloseRiderCallback ) {
        mSystem->mCloseRiderCallback( mRiderHandle, mRiderUserData );
    }

    if ( mBufferMemory ) {
#line 712
        FMOD_Memory_Free( mBufferMemory );
        mBufferMemory = NULL;
        mBuffer       = NULL;
    }

    return result;
} // 0x80373F1C

/*
====================
File::cancel

Asks for what the file is doing to stop.
====================
*/
FMOD_RESULT File::cancel() {
    mFlags |= FILE_FLAG_CANCELLED;

    return reallyCancel();
} // 0x80374104

/*
====================
File::reallyCancel

Nothing to cancel in the base class.
====================
*/
FMOD_RESULT File::reallyCancel() {
    return FMOD_OK;
} // 0x80374120

/*
====================
File::flip

Reads the next half buffer into the buffer. wait waits for the file's semaphore first (the file thread
passes false).
====================
*/
FMOD_RESULT File::flip( bool wait ) {
    FMOD_RESULT result = FMOD_OK;

    if ( wait && mSema ) {
        FMOD_OS_Semaphore_Wait( mSema );
    }

    {
        unsigned int bytestoread = mBlockSize - mBufferSkip;
        char        *destptr     = (char *)mBuffer + mBlockOffset + mBufferSkip;

        mFlags |= FILE_FLAG_READING;

        while ( bytestoread ) {
            unsigned int r = 0;

            result = reallyRead( destptr, bytestoread, &r );
            if ( result != FMOD_OK ) {
                bytestoread = r;

                if ( (int)mLength == -1 && result == FMOD_ERR_FILE_EOF && mNextPositionDisplay ) {
                    mLength = mFileSize = mLengthOriginal = mNextPositionDisplay;
                }
            }

            if ( mSystem && mSystem->mReadRiderCallback ) {
                mSystem->mReadRiderCallback( mRiderHandle, destptr, r, 0, mRiderUserData );
            }

            if ( !r || r > bytestoread ) {
                break;
            }

            bytestoread -= r;
            destptr += r;
            mNextPositionDisplay += r;

            mPercentBuffered = (int)( 100.0f * ( ( (float)mNextPositionDisplay - (float)mCurrentPosition ) / (float)mBufferSize ) );
            if ( mPercentBuffered < 0 ) {
                mPercentBuffered = 0;
            }

            if ( mFlags & FILE_FLAG_CANCELLED ) {
                mFlags &= ~FILE_FLAG_CANCELLED;
                break;
            }
        }
    }

    if ( mBlockOffset == 0 ) {
        mFlags |= FILE_FLAG_FIRSTHALF;
    } else {
        mFlags |= FILE_FLAG_SECONDHALF;
    }

    mBlockOffset += mBlockSize;
    if ( mBlockOffset >= mBufferSize ) {
        mBlockOffset = 0;
    }

    mBufferSkip = 0;
    mAsyncError = result;
    mFlags &= ~FILE_FLAG_FLIP;
    mFlags &= ~FILE_FLAG_READING;

    if ( mSema ) {
        FMOD_OS_Semaphore_Signal( mSema, false );
    }

    return result;
} // 0x80374128

/*
====================
File::seekAndReset

Repositions the underlying file at the block holding the current position and empties the buffer.
====================
*/
FMOD_RESULT File::seekAndReset() {
    FMOD_RESULT  result;
    unsigned int alignedpos;

    if ( mSema && ( mFlags & FILE_FLAG_READING ) ) {
        FMOD_OS_Semaphore_Wait( mSema );
        FMOD_OS_Semaphore_Signal( mSema, false );
    }

    alignedpos   = mCurrentPosition / mBufferSize;
    mBlockOffset = 0;
    mBufferSkip  = 0;
    mFlags &= ~( FILE_FLAG_FIRSTHALF | FILE_FLAG_SECONDHALF );
    alignedpos *= mBufferSize;
    mNextPosition        = alignedpos;
    mBufferPos           = mCurrentPosition - alignedpos;
    mNextPositionDisplay = alignedpos;

    result = reallySeek( alignedpos );

    if ( mSystem && mSystem->mSeekRiderCallback ) {
        mSystem->mSeekRiderCallback( mRiderHandle, alignedpos, mRiderUserData );
    }

    return result;
} // 0x80374390

/*
====================
File::checkBufferedStatus

Keeps the buffer ahead of the read position: restarts the file thread's reads when the position has
left the buffered range, and asks for the next half buffer when the current one runs out.
====================
*/
FMOD_RESULT File::checkBufferedStatus() {
    FMOD_RESULT result;
    int         diff;

    if ( mAsyncError != FMOD_OK && mAsyncError != FMOD_ERR_FILE_EOF && mAsyncError != FMOD_ERR_FILE_DISKEJECTED ) {
        return mAsyncError;
    }

    if ( mNextPosition < mCurrentPosition ||
         ( mBufferSize > mBlockSize && !( mFlags & ( FILE_FLAG_FLIP | FILE_FLAG_SECONDHALF ) ) &&
           mCurrentPosition < mNextPosition - mBlockSize ) ) {
        diff = -1;
    } else {
        mPercentBuffered = (int)( 100.0f * ( ( (float)mNextPositionDisplay - (float)mCurrentPosition ) / (float)mBufferSize ) );
        if ( mPercentBuffered < 0 || mBufferSkip ) {
            mPercentBuffered = 0;
        }

        diff = mNextPosition - mCurrentPosition;
        diff += mBlockSize - 1;
        diff /= mBlockSize;
    }

    if ( mBufferSkip ) {
        if ( diff > 2 ) {
            mBufferSkip = 0;
        } else {
            diff                 = -1;
            mNextPositionDisplay = mBufferSkip;
            mNextPosition        = mCurrentPosition - mBufferPos;
        }
    }

    if ( diff != 2 && ( mFlags & FILE_FLAG_READING ) ) {
        mFlags |= FILE_FLAG_STALLED;
        FMOD_OS_Semaphore_Wait( mSema );
        FMOD_OS_Semaphore_Signal( mSema, false );
        mFlags &= ~FILE_FLAG_STALLED;
    }

    if ( diff == 1 && mBufferSize > mBlockSize ) {
        mFlags |= FILE_FLAG_FLIP | FILE_FLAG_READING;
        FMOD_OS_Semaphore_Wait( mSema );
        mFileThread->mThread.wakeupThread( false );
        mNextPositionDisplay = mNextPosition;
        mNextPosition += mBlockSize;

        return FMOD_OK;
    }

    if ( ( mBufferSize > mBlockSize && diff == 2 ) || ( mBufferSize == mBlockSize && diff == 1 ) ) {
        return FMOD_OK;
    }

    if ( diff != 0 && ( mFlags & FILE_FLAG_SEEKABLE ) ) {
        result = seekAndReset();
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    result = flip( true );
    if ( result != FMOD_OK && result != FMOD_ERR_FILE_EOF ) {
        if ( result == FMOD_ERR_FILE_DISKEJECTED ) {
            mFlags |= FILE_FLAG_STALLED;
        }

        return result;
    }

    mFlags &= ~FILE_FLAG_STALLED;

    if ( mBufferSize == mBlockSize && result == FMOD_ERR_FILE_EOF && (int)mLength == -1 ) {
        return result;
    }

    mNextPositionDisplay = mNextPosition;
    mNextPosition += mBlockSize;

    if ( mBufferPos >= mBlockSize ) {
        result = flip( true );
        if ( result != FMOD_OK && result != FMOD_ERR_FILE_EOF ) {
            if ( result == FMOD_ERR_FILE_DISKEJECTED ) {
                mFlags |= FILE_FLAG_STALLED;
            }

            return result;
        }

        mFlags &= ~FILE_FLAG_STALLED;
        mNextPositionDisplay = mNextPosition;
        mNextPosition += mBlockSize;
    }

    return result;
} // 0x8037447C

/*
====================
File::read

Reads count items of size bytes into buffer through the file's buffer, swaps the items to the
platform's byte order and decrypts them; rd receives the number of items read. A read cut short by the
end of the file returns FMOD_ERR_FILE_EOF. The direct read's round-down to whole blocks is two statements,
as the sibling's line table shows.
====================
*/
FMOD_RESULT File::read( void *buffer, unsigned int size, unsigned int count, unsigned int *rd ) {
    FMOD_RESULT  result = FMOD_OK;
    unsigned int itemsize = size;
    unsigned int bytesleft;
    unsigned int r   = 0;
    bool         eof = false;

    if ( !buffer ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( (int)( size * count ) < 0 ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    mFlags &= ~FILE_FLAG_CANCELLED;

    size *= count;
    if ( mCurrentPosition + size > mStartOffset + mLength ) {
        if ( mCurrentPosition <= mStartOffset + mLength ) {
            size = mStartOffset + mLength - mCurrentPosition;
        } else {
            return FMOD_ERR_FILE_BAD;
        }

        eof = true;
    }

    bytesleft = size;
    while ( bytesleft ) {
        size = bytesleft;

        if ( mBlockSize == mBufferSize && bytesleft > mBlockSize && mBufferPos == 0 && ( mFlags & FILE_FLAG_SEEKABLE ) &&
             ( ( mDeviceType == 5 || mDeviceType == 0 ) ? !( ( (unsigned int)buffer + r ) & 31 ) : true ) ) {
            if ( mBlockSize ) {
                if ( mCurrentPosition != mNextPosition ) {
                    result = seekAndReset();
                    if ( result != FMOD_OK ) {
                        return result;
                    }
                }

                size /= mBlockSize;
                size *= mBlockSize;
            }

            result = reallyRead( (char *)buffer + r, size, &size );
            mFlags &= ~FILE_FLAG_READING;

            if ( mSystem && mSystem->mReadRiderCallback ) {
                mSystem->mReadRiderCallback( mRiderHandle, (char *)buffer + r, size, 0, mRiderUserData );
            }

            if ( result != FMOD_OK && result != FMOD_ERR_FILE_EOF ) {
                return result;
            }

            mNextPositionDisplay = mNextPosition;
            mNextPosition += size;

            if ( size == 0 ) {
                result = FMOD_ERR_FILE_EOF;
            }

            if ( result == FMOD_ERR_FILE_EOF ) {
                break;
            }
        } else {
            result = checkBufferedStatus();
            if ( result == FMOD_ERR_FILE_EOF ) {
                if ( mBlockSize != mBufferSize || (int)mLength != -1 ) {
                    result = FMOD_OK;
                }
            }

            if ( result != FMOD_OK ) {
                break;
            }

            if ( size > mBlockSize - mBufferPos % mBlockSize ) {
                size = mBlockSize - mBufferPos % mBlockSize;
            }

            memcpy( (char *)buffer + r, (char *)mBuffer + mBufferPos, size );

            mBufferPos += size;
            if ( mBufferPos >= mBufferSize ) {
                mBufferPos = 0;
            }
        }

        mCurrentPosition += size;
        bytesleft -= size;
        r += size;
    }

    if ( itemsize == 2 ) {
        if ( !( mFlags & FILE_FLAG_NOSWAP ) ) {
            unsigned short *wptr = (unsigned short *)buffer;
            unsigned int    i;

            for ( i = 0; i < r / itemsize; i++ ) {
                wptr[i] = FMOD_SWAP16( wptr[i] );
            }
        }
    } else if ( itemsize == 4 ) {
        if ( !( mFlags & FILE_FLAG_NOSWAP ) ) {
            unsigned int *dptr = (unsigned int *)buffer;
            unsigned int  i;

            for ( i = 0; i < r / itemsize; i++ ) {
                dptr[i] = FMOD_SWAP32( dptr[i] );
            }
        }
    }

    r /= itemsize;

    if ( mEncryptionKeyLength ) {
        unsigned char *val   = (unsigned char *)buffer;
        unsigned int   count = r;
        unsigned int   i;

        if ( mFlags & FILE_FLAG_XORFIRST ) {
            for ( i = 0; i < count; i++ ) {
                *val ^= mEncryptionKey[mEncryptionKeyIndex];
                FMOD_REVERSE_BITS( *val );

                mEncryptionKeyIndex++;
                if ( mEncryptionKeyIndex >= mEncryptionKeyLength ) {
                    mEncryptionKeyIndex = 0;
                }
                val++;
            }
        } else {
            for ( i = 0; i < count; i++ ) {
                FMOD_REVERSE_BITS( *val );
                *val ^= mEncryptionKey[mEncryptionKeyIndex];

                mEncryptionKeyIndex++;
                if ( mEncryptionKeyIndex >= mEncryptionKeyLength ) {
                    mEncryptionKeyIndex = 0;
                }
                val++;
            }
        }
    }

    if ( rd ) {
        *rd = r;
    }

    if ( result == FMOD_OK && eof ) {
        result = FMOD_ERR_FILE_EOF;
    }

    return result;
} // 0x803747A4

/*
====================
File::getByte

Reads one byte.
====================
*/
FMOD_RESULT File::getByte( unsigned char *val ) {
    FMOD_RESULT   result;
    unsigned int  rd;
    unsigned char temp;

    result = read( &temp, 1, 1, &rd );
    if ( val ) {
        *val = temp;
    }

    return result;
} // 0x80374E1C

/*
====================
File::getByte

Reads one signed byte.
====================
*/
FMOD_RESULT File::getByte( int *val ) {
    FMOD_RESULT  result;
    unsigned int rd;
    signed char  temp;

    result = read( &temp, 1, 1, &rd );
    if ( val ) {
        *val = temp;
    }

    return result;
} // 0x80374E68

/*
====================
File::getWord

Reads one 16 bit word.
====================
*/
FMOD_RESULT File::getWord( unsigned short *val ) {
    FMOD_RESULT    result;
    unsigned int   rd;
    unsigned short temp;

    result = read( &temp, 2, 1, &rd );
    if ( val ) {
        *val = temp;
    }

    return result;
} // 0x80374EB8

/*
====================
File::getWord

Reads one 16 bit word into an unsigned int.
====================
*/
FMOD_RESULT File::getWord( unsigned int *val ) {
    FMOD_RESULT    result;
    unsigned int   rd;
    unsigned short temp;

    result = read( &temp, 2, 1, &rd );
    if ( val ) {
        *val = temp;
    }

    return result;
} // 0x80374F04

/*
====================
File::getWord

Reads one signed 16 bit word.
====================
*/
FMOD_RESULT File::getWord( int *val ) {
    FMOD_RESULT  result;
    unsigned int rd;
    short        temp;

    result = read( &temp, 2, 1, &rd );
    if ( val ) {
        *val = temp;
    }

    return result;
} // 0x80374F50

/*
====================
File::seek

Moves the read position; mode is SEEK_SET, SEEK_CUR or SEEK_END. A position outside the file is
clamped to its end, and a position the buffer no longer holds on a file that cannot seek fails.
====================
*/
FMOD_RESULT File::seek( int offset, int mode ) {
    FMOD_RESULT  result = FMOD_OK;
    unsigned int newpos = 0;

    if ( (unsigned int)mode > SEEK_END ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    mFlags &= ~FILE_FLAG_CANCELLED;

    if ( mode == SEEK_SET ) {
        newpos = mStartOffset + offset;
    } else if ( mode == SEEK_CUR ) {
        newpos = mCurrentPosition + offset;
    } else if ( mode == SEEK_END ) {
        newpos = mStartOffset + mLength + offset;
    }

    if ( newpos > mStartOffset + mLength ) {
        newpos = ( offset >= 0 ) ? mStartOffset + mLength : 0;
    }

    if ( mNextPosition == 0 && !( mFlags & FILE_FLAG_SEEKABLE ) && newpos >= mBlockSize ) {
        return FMOD_ERR_FILE_COULDNOTSEEK;
    }

    if ( mBlockSize == mBufferSize && !( mFlags & FILE_FLAG_SEEKABLE ) && mNextPosition >= mBlockSize ) {
        if ( mNextPosition && newpos < mNextPosition - mBlockSize ) {
            return FMOD_ERR_FILE_COULDNOTSEEK;
        }

        if ( newpos >= mNextPosition + mBlockSize ) {
            return FMOD_ERR_FILE_COULDNOTSEEK;
        }
    }

    mCurrentPosition = newpos;

    if ( mEncryptionKeyLength ) {
        mEncryptionKeyIndex = mCurrentPosition % mEncryptionKeyLength;
    }

    if ( mBufferSize ) {
        mBufferPos = mCurrentPosition % mBufferSize;
    } else {
        result = reallySeek( newpos );

        if ( mSystem && mSystem->mSeekRiderCallback ) {
            mSystem->mSeekRiderCallback( mRiderHandle, newpos, mRiderUserData );
        }
    }

    return result;
} // 0x80374F9C

/*
====================
File::tell

Reports the read position, counted from the start offset.
====================
*/
FMOD_RESULT File::tell( unsigned int *position ) {
    if ( !position ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    mFlags &= ~FILE_FLAG_CANCELLED;

    *position = mCurrentPosition;
    *position -= mStartOffset;

    return FMOD_OK;
} // 0x8037516C

/*
====================
File::setStartOffset

Makes the file start offset bytes into the underlying file, shortening its length to fit.
====================
*/
FMOD_RESULT File::setStartOffset( unsigned int offset ) {
    mStartOffset = offset;
    mLength      = mLengthOriginal;

    if ( offset + mLengthOriginal > mFileSize ) {
        mLength = mFileSize - offset;
    }

    return FMOD_OK;
} // 0x803751A4

/*
====================
File::enableDoubleBuffer

Turns the buffer into two halves of sizebytes each (at least 2048 and one block, rounded down to whole blocks).
The file joins its file thread's list so the thread keeps the second half filled. oldbuffer is copied into the
new buffer, or NULL to reallocate the buffer already held.
Wall: register/schedule (91%). The build reloads mBlockSize after the clamp's join, where retail keeps one load
in r4 for the clamp, the divide and mBufferSkip, and the build keeps sizebytes in r28 where retail uses r30.
Tried: a blockcount local for the divide (94%, but an extra local the sibling DWARF does not list), the store
orders of mBlockOffset, mBufferSkip and oldbuffersize, and the swapped clamp comparison (`mBlockSize > sizebytes`).
====================
*/
FMOD_RESULT File::enableDoubleBuffer( unsigned int sizebytes, void *oldbuffer ) {
    unsigned int oldbuffersize;
    FMOD_RESULT  result;

    if ( mBlockSize == 0 ) {
        return FMOD_OK;
    }

    result = FMOD_OS_Semaphore_Create( &mSema );
    if ( result != FMOD_OK ) {
        return result;
    }
    FMOD_OS_Semaphore_Signal( mSema, false );

    if ( sizebytes < 0x800 ) {
        sizebytes = 0x800;
    }
    if ( sizebytes < mBlockSize ) {
        sizebytes = mBlockSize;
    }

    mBufferSkip          = mBlockSize;
    mBlockSize           = ( sizebytes / mBlockSize ) * mBlockSize;
    mBlockOffset         = 0;
    oldbuffersize        = mBufferSize;
    mNextPosition        = mBlockSize;
    mNextPositionDisplay = mBlockSize;
    mBufferSize          = mBlockSize * 2;

    if ( oldbuffer ) {
#line 1978
        mBufferMemory = FMOD_Memory_CallocType( mBufferSize + 32, FMOD_MEMORY_STREAM_FILE );
        if ( !mBufferMemory ) {
            return FMOD_ERR_MEMORY;
        }
        memcpy( mBufferMemory, oldbuffer, oldbuffersize );
    } else {
#line 1987
        mBufferMemory = FMOD_Memory_ReallocType( mBufferMemory, mBufferSize + 32, FMOD_MEMORY_STREAM_FILE );
        if ( !mBufferMemory ) {
            return FMOD_ERR_MEMORY;
        }
    }

    mBuffer = (void *)( ( (unsigned int)mBufferMemory + 31 ) & ~31 );

    result = getFileThread();
    if ( result != FMOD_OK ) {
        return result;
    }

    FMOD_OS_CriticalSection_Enter( mFileThread->mFileListCrit );
    addAfter( &mFileThread->mFileListHead );
    FMOD_OS_CriticalSection_Leave( mFileThread->mFileListCrit );

    result = checkBufferedStatus();
    if ( result != FMOD_OK && result != FMOD_ERR_FILE_EOF ) {
        return result;
    }
    return FMOD_OK;
} // 0x803751D0

/*
====================
File::setName

Sets the name the file reports.
====================
*/
FMOD_RESULT File::setName( char *name ) {
    if ( !name ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    FMOD_strncpy( mName, name, 256 );
    mName[255] = 0;

    return FMOD_OK;
} // 0x80375388

} // namespace FMOD
