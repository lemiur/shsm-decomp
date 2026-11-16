/*
 * FMOD::Metadata and FMOD::TagNode: metadata tag list management (fmod_metadata.cpp).
 * Layout: Gormiti DWARF (sibling.exe types TagNode / Metadata).
 */
#ifndef _FMOD_METADATA_H
#define _FMOD_METADATA_H

#include <fmod/fmod.h>
#include <fmod/fmod_linkedlist.h>

namespace FMOD {

class TagNode : public LinkedListNode {
public:
    FMOD_TAGTYPE     mType;          // 0x0C
    FMOD_TAGDATATYPE mDataType;      // 0x10
    char            *mName;          // 0x14
    void            *mData[2];       // 0x18
    unsigned int     mDataLen;       // 0x20
    bool             mUpdated;       // 0x24
    bool             mUnique;        // 0x25
    int              mCurrentBuffer; // 0x28

    TagNode() {
        mType          = FMOD_TAGTYPE_UNKNOWN;
        mDataType      = FMOD_TAGDATATYPE_BINARY;
        mName          = NULL;
        mData[0]       = mData[1] = NULL;
        mDataLen       = 0;
        mUpdated       = true;
        mUnique        = false;
        mCurrentBuffer = 0;
    }

    FMOD_RESULT init( FMOD_TAGTYPE type, const char *name, void *data, unsigned int datalen, FMOD_TAGDATATYPE datatype );
    FMOD_RESULT release();
    FMOD_RESULT update( void *data, unsigned int datalen );
};

class Metadata {
public:
    TagNode mList; // 0x00

    FMOD_RESULT release();
    FMOD_RESULT getNumTags( int *numtags, int *numtagsupdated );
    FMOD_RESULT getTag( const char *name, int index, FMOD_TAG *tag );
    FMOD_RESULT add( Metadata *metadata );
    FMOD_RESULT addTag( FMOD_TAGTYPE type, const char *name, void *data, unsigned int datalen, FMOD_TAGDATATYPE datatype, bool unique );
};

} // namespace FMOD

#endif /* _FMOD_METADATA_H */
