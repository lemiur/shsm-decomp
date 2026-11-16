/*
 * FMOD::File: the base of FMOD's file readers (fmod_file.cpp), and the classes derived from it
 * (DiskFile, MemoryFile, NullFile, UserFile).
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_file.obj)
 */
#ifndef _FMOD_FILE_H
#define _FMOD_FILE_H

#include <fmod/fmod.h>
#include <fmod/fmod_linkedlist.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_thread.h>

// FMOD::File: retail vtable __vt__Q24FMOD4File (0x8044FA1C), 7 slots = Gormiti __vt__Q24FMOD4File
// Layout: Gormiti DWARF (sibling.exe types File)
// Virtuals: retail vtable __vt__Q24FMOD4File + Gormiti link map (__vt__Q24FMOD4File)
// File::mFlags bits (names invented; the bit meanings are read off fmod_file.cpp's code).
// FILE_FLAG_UNICODE: the name passed to reallyOpen is UTF-16.

enum {
    FILE_FLAG_SEEKABLE     = 0x1,   // the file can seek (UserFile clears it when its seek callback cannot)
    FILE_FLAG_NAMED        = 0x2,   // opened by name, so open() copies the name into mName (MemoryFile clears it)
    FILE_FLAG_UNICODE      = 0x4,
    FILE_FLAG_NOSWAP       = 0x8,   // read() leaves 16 and 32 bit items in file order
    FILE_FLAG_READING      = 0x10,  // the file thread is filling the buffer
    FILE_FLAG_STALLED      = 0x20,  // reading stalled: the disk was ejected, or checkBufferedStatus waits for the file thread
    FILE_FLAG_CANCELLED    = 0x40,  // cancel() or close() was called
    FILE_FLAG_FLIP         = 0x80,  // the file thread has a half buffer to fill
    FILE_FLAG_FIRSTHALF    = 0x100, // the half buffer last filled was the first
    FILE_FLAG_SECONDHALF   = 0x200, // the half buffer last filled was not the first
    FILE_FLAG_XORFIRST     = 0x400  // read() applies the encryption key before reversing the bits of each byte
};

namespace FMOD {

class Metadata;
class FileThread;
class SystemI;

class File : public LinkedListNode {
public:
    unsigned int mLength;                           // 0x0C
    unsigned int mLengthOriginal;                   // 0x10
    unsigned int mFileSize;                         // 0x14
    void *mRiderUserData;                           // 0x18
    void *mRiderHandle;                             // 0x1C
    int mDeviceType;                                // 0x20
    char mName[256];                                // 0x24
    char mEncryptionKey[32];                        // 0x124
    int mEncryptionKeyLength;                       // 0x144
    int mEncryptionKeyIndex;                        // 0x148
    unsigned int mBufferPos;                        // 0x14C
    unsigned int mBufferSize;                       // 0x150
    unsigned int mBufferSkip;                       // 0x154
    unsigned int mBlockSize;                        // 0x158
    unsigned int mBlockOffset;                      // 0x15C
    unsigned int mCurrentPosition;                  // 0x160
    unsigned int mNextPosition;                     // 0x164
    unsigned int mNextPositionDisplay;              // 0x168
    unsigned int mStartOffset;                      // 0x16C
    int mPercentBuffered;                           // 0x170
    FMOD_RESULT mAsyncError;                        // 0x174
    FileThread *mFileThread;                        // 0x178
    FMOD_OS_SEMAPHORE *mSema;                       // 0x17C
    unsigned int mFlags;                            // 0x180
    SystemI *mSystem;                               // 0x184
    void *mBuffer;                                  // 0x188
    void *mBufferMemory;                            // 0x18C

    File();                                         // 0x80373AEC
    FMOD_RESULT init( unsigned int length, int blocksize ); // 0x80373B40
    static FMOD_RESULT shutDown();                  // 0x80373BD8, releases every file thread (retail's copy differs from Gormiti's)
    FMOD_RESULT open( const char *name_or_data, unsigned int length, bool unicode, const char *encryptionkey );
    FMOD_RESULT close();
    FMOD_RESULT cancel();
    FMOD_RESULT flip( bool wait );                  // 0x80374128, fills a half buffer
    FMOD_RESULT seekAndReset();                     // 0x80374390
    FMOD_RESULT checkBufferedStatus();              // 0x8037447C
    // Reads count items of sizebytes each into buffer (buffered; the retail copy is fmod_file.cpp's, 0x803747A4).
    FMOD_RESULT read( void *buffer, unsigned int size, unsigned int count, unsigned int *rd );
    FMOD_RESULT getByte( unsigned char *val );
    FMOD_RESULT getByte( int *val );                // reads a signed byte
    FMOD_RESULT getWord( unsigned short *val );
    FMOD_RESULT getWord( unsigned int *val );
    FMOD_RESULT getWord( int *val );                // reads a signed 16 bit word

    // ---- added for fmod_eventgroupi ----
    // Moves the file position; mode is SEEK_SET, SEEK_CUR or SEEK_END.
    FMOD_RESULT seek( int offset, int mode );
    // Reports the file position (fmod_file.cpp, 0x8037516C).
    FMOD_RESULT tell( unsigned int *position );
    // ---- end fmod_eventgroupi ----
    FMOD_RESULT setStartOffset( unsigned int offset );
    FMOD_RESULT enableDoubleBuffer( unsigned int sizebytes, void *oldbuffer );
    FMOD_RESULT setName( char *name );              // sets mName

    // vtable pointer at 0x190: the first virtual below
    virtual FMOD_RESULT getMetadata( Metadata ** ) { return FMOD_ERR_TAGNOTFOUND; } // slot 0x08; inline (weak)
    virtual FMOD_RESULT getSize( unsigned int *size ) { *size = mLength; return FMOD_OK; } // slot 0x0C; inline (weak)
    virtual FMOD_RESULT reallyOpen( const char *name_or_data, unsigned int *filesize ) = 0; // slot 0x10, name from a derived override
    virtual FMOD_RESULT reallyClose() = 0;          // slot 0x14, name from a derived override
    virtual FMOD_RESULT reallyRead( void *buffer, unsigned int size, unsigned int *rd ) = 0; // slot 0x18, name from a derived override
    virtual FMOD_RESULT reallySeek( unsigned int ) = 0; // slot 0x1C, name from a derived override
    virtual FMOD_RESULT reallyCancel();             // slot 0x20, the base does nothing
private:
    FMOD_RESULT getFileThread();                    // 0x80373948, finds or starts the thread of this file's device
}; // size 0x194 (Gormiti DWARF); retail ~0x194 (estimate from accesses)

// The thread that fills the half buffers of the files of one device (Gormiti DWARF: size 0x154).
class FileThread : public LinkedListNode {
public:
    Thread                   mThread;           // 0xC
    bool                     mThreadActive;     // 0x130
    LinkedListNode           mFileListHead;     // 0x134, the files this thread serves
    LinkedListNode          *mFileListCurrent;  // 0x140
    LinkedListNode          *mFileListNext;     // 0x144
    FMOD_OS_CRITICALSECTION *mFileListCrit;     // 0x148
    int                      mDeviceType;       // 0x14C
    bool                     mOwned;            // 0x150, released by File::close once its last file is gone

    FileThread();
    FMOD_RESULT init( int devicetype, bool owned, SystemI *system );
    FMOD_RESULT release();
    FMOD_RESULT threadFunc();
};

} // namespace FMOD

extern unsigned int gBufferSize;                    // the block size the next File is created with (setFileSystem)
extern bool         gUsesUserCallbacks;             // set by setFileSystem when the application supplied file callbacks

#endif
