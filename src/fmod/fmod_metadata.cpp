/*
 * fmod_metadata.cpp: FMOD::Metadata and FMOD::TagNode, metadata tag list management. Allocation tags
 * carry the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmodex fmod_metadata.obj)

#include <new>
#include <string.h>

#include <fmod/fmod.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_metadata.h>
#include <fmod/fmod_string.h>

namespace FMOD {

/*
====================
TagNode::init

Allocates and copies the tag name and payload, adding NUL padding for string datatypes.
====================
*/
FMOD_RESULT TagNode::init( FMOD_TAGTYPE type, const char *name, void *data, unsigned int datalen, FMOD_TAGDATATYPE datatype ) {
    int memsize;

    mName = FMOD_strdup( name );
    if ( !mName ) {
        return FMOD_ERR_MEMORY;
    }

    memsize = datalen;
    if ( datatype == FMOD_TAGDATATYPE_STRING ) {
        memsize = datalen + 1;
    } else if ( datatype == FMOD_TAGDATATYPE_STRING_UTF16 || datatype == FMOD_TAGDATATYPE_STRING_UTF16BE ) {
        memsize = datalen + 2;
    }

#line 52
    mData[0] = FMOD_Memory_Calloc( memsize );
    if ( !mData[0] ) {
        return FMOD_ERR_MEMORY;
    }

    memcpy( mData[0], data, datalen );

    mDataLen       = memsize;
    mType          = type;
    mDataType      = datatype;
    mUpdated       = true;
    mUnique        = false;
    mCurrentBuffer = 0;

    return FMOD_OK;
} // 0x8033D270

/*
====================
TagNode::release

Frees the tag name, payload buffer and node itself. Inlined into callers; its out-of-line copy is
dead-stripped in retail (Gormiti map: UNUSED).
====================
*/
FMOD_RESULT TagNode::release() {
    if ( mName ) {
#line 90
        FMOD_Memory_Free( mName );
        mName = NULL;
    }

    if ( mData[0] ) {
#line 98
        FMOD_Memory_Free( mData[0] );
        mData[0] = NULL;
    }

#line 102
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // no copy in retail (dead-stripped or inlined)

/*
====================
TagNode::update

Replaces the tag's data if it changed, or marks it updated without reallocating if identical.
Inlined into callers; its out-of-line copy is dead-stripped in retail (Gormiti map: UNUSED).
====================
*/
FMOD_RESULT TagNode::update( void *data, unsigned int datalen ) {
    if ( mDataLen == datalen && memcmp( mData[0], data, datalen ) == 0 ) {
        mUpdated = true;
        return FMOD_OK;
    }

    if ( mData[0] ) {
#line 140
        FMOD_Memory_Free( mData[0] );
        mData[0] = NULL;
    }

#line 144
    mData[0] = FMOD_Memory_Alloc( datalen );
    if ( !mData[0] ) {
        return FMOD_ERR_MEMORY;
    }

    memcpy( mData[0], data, datalen );
    mDataLen = datalen;
    mUpdated = true;

    return FMOD_OK;
} // no copy in retail (dead-stripped or inlined)

/*
====================
Metadata::release

Unlinks and frees every tag node, then frees the Metadata container.
====================
*/
FMOD_RESULT Metadata::release() {
    TagNode *node = (TagNode *)mList.getNext();

    while ( node != &mList ) {
        TagNode *next = (TagNode *)node->getNext();
        node->removeNode();
        node->release();
        node = next;
    }

#line 186
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // 0x8033D34C

/*
====================
Metadata::getNumTags

Counts total tags and tags marked updated.
====================
*/
FMOD_RESULT Metadata::getNumTags( int *numtags, int *numtagsupdated ) {
    LinkedListNode *current;
    int             num = 0, numupdated = 0;

    current = mList.getNext();
    while ( current != &mList ) {
        TagNode *node = (TagNode *)current;

        num++;
        if ( node->mUpdated ) {
            numupdated++;
        }

        current = current->getNext();
    }

    if ( numtags ) {
        *numtags = num;
    }
    if ( numtagsupdated ) {
        *numtagsupdated = numupdated;
    }

    return FMOD_OK;
} // 0x8033D444

/*
====================
Metadata::getTag

Finds a tag by name and index (or the first updated tag when index < 0), copies it out, and clears
its updated flag.
====================
*/
FMOD_RESULT Metadata::getTag( const char *name, int index, FMOD_TAG *tag ) {
    int             i;
    TagNode        *node;
    LinkedListNode *current;

    if ( index >= 0 ) {
        if ( !name ) {
            current = mList.getNodeByIndex( index );
            if ( !current ) {
                return FMOD_ERR_TAGNOTFOUND;
            }
        } else {
            i       = 0;
            current = mList.getNext();
            while ( 1 ) {
                if ( current == &mList ) {
                    return FMOD_ERR_TAGNOTFOUND;
                }

                node = (TagNode *)current;
                if ( !FMOD_strcmp( node->mName, name ) ) {
                    if ( i == index ) {
                        break;
                    }
                    i++;
                }

                current = current->getNext();
            }
        }
    } else if ( !name ) {
        current = mList.getNext();
        while ( 1 ) {
            if ( current == &mList ) {
                return FMOD_ERR_TAGNOTFOUND;
            }

            node = (TagNode *)current;
            if ( node->mUpdated ) {
                break;
            }

            current = current->getNext();
        }
    } else {
        current = mList.getNext();
        while ( 1 ) {
            if ( current == &mList ) {
                return FMOD_ERR_TAGNOTFOUND;
            }

            node = (TagNode *)current;
            if ( node->mUpdated && !FMOD_strcmp( node->mName, name ) ) {
                break;
            }

            current = current->getNext();
        }
    }

    node          = (TagNode *)current;
    tag->type     = node->mType;
    tag->datatype = node->mDataType;
    tag->name     = node->mName;
    tag->data     = node->mData[0];
    tag->datalen  = node->mDataLen;
    tag->updated  = node->mUpdated;

    if ( node->mUpdated ) {
        node->mUpdated = false;
    }

    return FMOD_OK;
} // 0x8033D494

/*
====================
Metadata::add

Moves all tags from metadata into this container, updating existing unique tags in place.
====================
*/
FMOD_RESULT Metadata::add( Metadata *metadata ) {
    TagNode *node = (TagNode *)metadata->mList.getNext();

    while ( node != &metadata->mList ) {
        TagNode *next = (TagNode *)node->getNext();
        node->removeNode();

        if ( node->mUnique ) {
            LinkedListNode *current = mList.getNext();
            TagNode        *existingnode;

            while ( 1 ) {
                if ( current == &mList ) {
                    current = NULL;
                    break;
                }

                if ( !FMOD_strcmp( ( (TagNode *)current )->mName, node->mName ) ) {
                    break;
                }

                current = current->getNext();
            }

            existingnode = (TagNode *)current;
            if ( existingnode ) {
                existingnode->update( node->mData[0], node->mDataLen );
                node->release();
            } else {
                node->addBefore( &mList );
            }
        } else {
            node->addBefore( &mList );
        }

        node = next;
    }

    return FMOD_OK;
} // 0x8033D610

/*
====================
Metadata::addTag

Adds a new tag or updates an existing unique tag with the same name and type.
====================
*/
FMOD_RESULT Metadata::addTag( FMOD_TAGTYPE type, const char *name, void *data, unsigned int datalen, FMOD_TAGDATATYPE datatype, bool unique ) {
    FMOD_RESULT     result;
    TagNode        *node;
    LinkedListNode *current = NULL;

    if ( unique ) {
        current = mList.getNext();
        while ( 1 ) {
            if ( current == &mList ) {
                current = NULL;
                break;
            }

            if ( !FMOD_strcmp( ( (TagNode *)current )->mName, name ) && ( (TagNode *)current )->mType == type ) {
                break;
            }

            current = current->getNext();
        }
    }

    node = (TagNode *)current;
    if ( node && unique ) {
        result = node->update( data, datalen );
    } else {
#line 495
        node = new ( FMOD_Memory_Alloc( sizeof( TagNode ) ) ) TagNode;
        if ( !node ) {
            return FMOD_ERR_MEMORY;
        }

        node->init( type, name, data, datalen, datatype );
        node->addBefore( &mList );
        result = FMOD_OK;
    }

    if ( unique ) {
        node->mUnique = true;
    }

    return result;
} // 0x8033D80C

} // namespace FMOD
