#ifndef CLIMAX_AUDIO_HELPER_H
#define CLIMAX_AUDIO_HELPER_H

// Names and layout: Gormiti link map and DWARF (Helper.obj, game/System/Audio/FMOD/Helper.cpp; the
// classes below are declared in its Helper.h)

#include <revolution/types.h>
#include <climax/Audio/Defs.h>

namespace Audio {

/*
 * The base of the sound projects: a reference count and the virtual calls that take and give it up.
 * Retail vtable lbl_8042A278 and Gormiti's __vt__Q25Audio16RefCountedObject give two slots.
 */
class RefCountedObject {
protected:
    int                 m_refCount;             // 0x00 (declared before the first virtual: the vptr follows it, at 0x04)

public:
                        RefCountedObject();
                        ~RefCountedObject();

    virtual void        AddRef();
    virtual void        ReleaseRef();

    int                 GetRefCount() const;
};

// Gormiti: Helper.h:31-43.
inline RefCountedObject::RefCountedObject() {
    m_refCount = 0;
}

inline RefCountedObject::~RefCountedObject() {
    m_refCount = 0;
}

inline void RefCountedObject::AddRef() {
    m_refCount++;
}

inline void RefCountedObject::ReleaseRef() {
    m_refCount--;
}

inline int RefCountedObject::GetRefCount() const {
    return m_refCount;
}

/*
 * A sound project: anything FMOD's sound project files are loaded into (Gormiti: Helper.h:111).
 */
class SProject : public RefCountedObject {
public:
    virtual             ~SProject();
};

inline SProject::~SProject() {
}

/*
 * A sound project held in memory: the project file and the sound file are memory streams. Retail
 * vtable lbl_8042A2FC (RefCountedObject's slots, then the destructor).
 */
class SoundProject : public SProject {
public:
                    SoundProject();

    virtual         ~SoundProject();

    char            name[64];                   // 0x08
    char*           data;                       // 0x48
    MemoryStream    projectFile;                // 0x4C
    MemoryStream    soundFile;                  // 0x98
};

// Gormiti: Helper.h:123 (out of line in Gormiti's System.obj; retail inlines it into LoadSoundProject).
inline SoundProject::SoundProject() : projectFile( "", NULL, 0 ), soundFile( "", NULL, 0 ) {
    data = NULL;
}


/*
 * A sound project read through the disk file callbacks: the project and sound files are DiskStreams
 * that System::FindDiskSoundProjectFile hands out (Gormiti DWARF).
 */
class DiskSoundProject : public SProject {
public:
                        DiskSoundProject();

    virtual void        ReleaseRef();
    virtual             ~DiskSoundProject();
    virtual bool        IsAsync() const;
    virtual bool        IsLoaded() const;

    char                name[64];               // 0x08
    FileHandle          handle;                 // 0x48
    DiskStream          projectFile;            // 0x60
    DiskStream          soundFile;              // 0xCC
};

// Gormiti: Helper.h (out of line in Gormiti's System.obj; retail inlines it into the loaders).
inline DiskSoundProject::DiskSoundProject() : projectFile( "", 0, 0 ), soundFile( "", 0, 0 ) {
    projectFile.m_parentProject = this;
    soundFile.m_parentProject = this;
}

// Gormiti: Helper.h:201.
inline bool DiskSoundProject::IsAsync() const {
    return false;
}

// Gormiti: Helper.h:206.
inline bool DiskSoundProject::IsLoaded() const {
    return true;
}

/*
 * A disk sound project whose project file is read by the async thread. Retail vtable lbl_8042A2E0;
 * the constructor is inline in LoadAsyncDiskSoundProject.
 */
class AsyncDiskSoundProject : public DiskSoundProject {
public:
                    AsyncDiskSoundProject();

    virtual         ~AsyncDiskSoundProject();
    virtual bool    IsAsync() const;
    virtual bool    IsLoaded() const;

    MemoryStream    memProjectFile;             // 0x138
    char*           m_data;                     // 0x184
    int             memStreamRead;              // 0x188 (retail's object ends here, size 0x18C; Gormiti's revision has
                                                //  m_needsDataRead and a 228-byte m_projectHeader after it)
};

// Gormiti: Helper.h:223 (out of line in Gormiti's System.obj; retail inlines it into LoadAsyncDiskSoundProject).
inline AsyncDiskSoundProject::AsyncDiskSoundProject() : memProjectFile( "", NULL, 0 ) {
    m_data = NULL;
    memStreamRead = 0;
    soundFile.m_isAsync = true;
    projectFile.m_parentProject = this;
    soundFile.m_parentProject = this;
}


// The file callbacks FMOD reads sound projects through: from memory, from disk, and through the
// async file system. Each returns 0 (FMOD_OK) unless it says otherwise.
int MemFileOpen( const char* name, unsigned int* fileSize, FileHandle* handle );
int MemFileClose( void* handle );
int MemFileRead( void* handle, void* buffer, unsigned int size, unsigned int* read );
int MemFileSeek( void* handle, unsigned int pos );
int MemFileGetPos( void* handle, unsigned int& pos );
int DiskFileOpen( const char* name, unsigned int* fileSize, FileHandle* handle );
int DiskFileClose( void* handle );
int DiskFileRead( void* handle, void* buffer, unsigned int bytesToRead, unsigned int* bytesRead );
int DiskFileSeek( void* handle, unsigned int pos );
int DiskFileGetPos( void* handle, unsigned int& pos );
int AsyncDiskFileClose( void* handle );
int AsyncCallback( void* userData, char* buffer, int readSize, int status );
int AsyncDiskFileRead( void* handle, void* buffer, unsigned int bytesToRead, unsigned int* bytesRead );
int AsyncDiskFileSeek( void* handle, unsigned int pos );
int AsyncDiskFileGetPos( void* handle, unsigned int& pos );

}

#endif
