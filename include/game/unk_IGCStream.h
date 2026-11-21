#ifndef GAME_UNK_IGCSTREAM_H
#define GAME_UNK_IGCSTREAM_H

#include <climax/Heap.h>
#include <game/CPreCache.h>

extern u32 lbl_80529870;                    // class-name hash of "PreCache"
extern unk_801A8F6C lbl_80529874;           // type-registry entry of the class

/*
 * A CPreCache whose asset name is given the ".IGCStream" suffix, in a buffer the object owns, when
 * the name has no extension. The derived class adds the buffer (0x64) and overrides CPreCache's
 * empty fn_80028BB0 slot. vtable __vt__13unk_IGCStream (0x64 bytes, the CPreCache slots), object
 * size 0x68. Class name invented (the role: the cache of an IGC stream asset).
 */
class unk_IGCStream : public CPreCache {
public:
                    unk_IGCStream( const unk_AttributePacket& packet ) : CPreCache( packet ) {
                        field_0x3C = GAME_HEAP_FLAGS;
                        streamName = NULL;
                    }
    virtual         ~unk_IGCStream();
    virtual void    fn_80028BB0();

    static CAttributeHandler* fn_800AB6C8( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_IGCStream( packet );
    }

    char*           streamName;     // 0x64, owned: the name with the suffix; deleted by the destructor
};

#endif
