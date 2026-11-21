#ifndef CLIMAX_ATTRIBUTEHANDLER_H
#define CLIMAX_ATTRIBUTEHANDLER_H

#include <revolution/types.h>
#include <climax/Colour.h>
#include <climax/Frame.h>
#include <rwsdk/plcore/bastream.h>

struct RwTexture;      // defined in rwsdk/batextur.h
struct RwRaster;
struct RwTexDictionary;
class  CAttributeHandler;
// The zone a handler belongs to: registered as "CZone" (vtable lbl_803E1440, constructor
// 0x8002598C, size 0x4F0; unit 0x8002598C-0x80027DEC not yet reconstructed). Gormiti's CZone has
// another layout.
class  CZone;
// The registry's record of a class (0x5C bytes, allocated by the registration object
// unk_801A8F6C: factory 0x0, instance size 0x4, interned class name 0x8, behaviour name 0xC,
// asset list 0x10, matrix 0x14, reference count 0x54, a flag byte 0x58; unit not yet
// reconstructed). Gormiti: SClassInfo, the same members without the flag byte.
struct SClassInfo;
class  CZoneChangeInfo;    // game/CZoneChangeInfo.h

/*
 * The Climax attribute framework: objects configured by packets of attribute commands, and the
 * class registry that creates them by name (the registry itself is in units not yet
 * reconstructed).
 * CAttributeHandler is named by Ghost Rider's class-name literals; the other names are invented
 * unless a comment says they come from Gormiti (Wii, same engine, DWARF).
 * Header name invented.
 */

namespace RWS {

// Gormiti: RWS::RWSGUID, four words. Identifies a placed object across packets.
struct RWSGUID {
    u32     m_Data[4];
};

// Gormiti: RWS::AssetTracker, a namespace of the registry of live handlers and their assets
// (unit not yet reconstructed). Remove and RegisterForDelete are Gormiti's names for functions
// that pair by call shape; their symbols name the handler as ::CAttributeHandler.
namespace AssetTracker {
    // Gormiti: RWS::AssetTracker::CAssetList. Only DecRefCount is recovered here (its body is in a
    // unit not yet reconstructed); the other members are not yet known.
    class CAssetList {
    public:
        void                    DecRefCount();
    };
    ::CAttributeHandler*    Find( const RWSGUID& id );
    void                    Remove( ::CAttributeHandler* handler );
    void                    RegisterForDelete( ::CAttributeHandler* handler );
}

}   // namespace RWS

/*
 * Chunk types of an attribute packet. The values come from the retail code that tests them; the
 * names are invented (Gormiti's Build*Chunk members suggest the roles, not the values).
 */
enum {
    kChunkCreateClassId = 0x20000000,   // the class a creation packet instantiates
    kChunkInstanceId    = 0x40000000,   // the RWSGUID of the instance
    kChunkClassId       = 0x80000000    // starts the commands addressed to one class
};

/*
 * One command in an attribute packet: a self-relative link to the next command, an id, its data.
 * Name invented. Gormiti: RWS::CAttributeDataChunk (size_, type_, data_, NextClassID); the
 * chunks of kChunkClassId type use the last word to link the class chunks of the packet.
 */
struct unk_AttributeCommand {
    const char* GetString() const { return data; }                  // name invented
    u32         GetU32() const { return *( const u32* )data; }      // name invented
    float       GetFloat() const { return *( const float* )data; }  // name invented
    unk_RGBA    GetRGBA() const { return *( const unk_RGBA* )data; }    // name invented
    const unk_RwV3d& GetVector() const { return *( const unk_RwV3d* )data; }  // three floats; name invented
    const RWS::RWSGUID& GetGUID() const { return *( const RWS::RWSGUID* )data; }    // name invented

    // the chunk after this one; a const member, like the iterator's Next(). Name invented.
    const unk_AttributeCommand* Next() const {
                    return ( const unk_AttributeCommand* )( ( const char* )this + next );
                }

    s32     next;       // 0x0, byte offset to the next command; 0 ends the packet
    u32     id;         // 0x4, command id within the target class, or a chunk type
    union {
        char    data[4];    // 0x8, payload; a string for string commands
        u32     classID;    // kChunkClassId chunks: the class name, then its registered id
    };
    s32     nextClass;  // 0xC, kChunkClassId chunks: byte offset to the next class chunk
};


/*
 * Walks the chunks of a packet one after the other. Name invented; Gormiti: RWS::CAttributeDataChunkIterator
 * (pCurrChunk_; operator*, GetDataChunk, ClassIdNextClassID).
 */
class unk_AttributeChunkIterator {
public:
    unk_AttributeChunkIterator( const unk_AttributeCommand& chunk ) : current( &chunk ) {}

    const unk_AttributeCommand& operator*() const { return *current; }
    // Gormiti: IsFinished, true at the terminator chunk
    bool    IsFinished() const { return current->next == 0; }
    void    operator++() {
                current = ( const unk_AttributeCommand* )( ( const char* )current + current->next );
            }
    // jumps to the next class chunk (the current one is a class chunk)
    void    ClassIdNextClassID() {
                current = ( const unk_AttributeCommand* )( ( const char* )current + current->nextClass );
            }

    const unk_AttributeCommand* current;    // 0x0
};
/*
 * A packet of attribute commands handed to a handler. Gormiti: RWS::CAttributePacket
 * (firstChunk_). Name invented.
 */
struct unk_AttributePacket {
    // Gormiti: GetInstanceId, ReprocessAttributePacket, Clone and CreateEntity
    RWS::RWSGUID        GetInstanceId() const;
    void                ReprocessAttributePacket();
    unk_AttributePacket*    Clone() const;
    CAttributeHandler*  CreateEntity( RwStream* stream ) const;

    unk_AttributeCommand    first;
};

/*
 * Walks the commands of a packet that are addressed to one class. Name invented.
 * Gormiti: RWS::CAttributeCommandIterator (chunkIt_, pTargetClass_); there the class is a name
 * and SkipToNextAttributeCommand is SkipToNextAttributeCommand here, retail keeps the registered id.
 */
class unk_AttributeCommandIterator {
public:
                unk_AttributeCommandIterator( const unk_AttributePacket& packet, u32 classID );

    bool        IsFinished() const { return current->next == 0; }
    // returns the member by reference, so each use reads current again
    const unk_AttributeCommand* const& operator->() const { return current; }
    void        operator++() {
                    current = Next();
                    SkipToNextAttributeCommand( true );
                }

    // the command after the current one; a const member, so its read of current is the const one
    // that operator->() makes (PlaneTrigger::fn_801A73A8 reuses that load). Name invented.
    const unk_AttributeCommand* Next() const {
                    return ( const unk_AttributeCommand* )( ( const char* )current + current->next );
                }
    // Gormiti: SkipToNextAttributeCommand( bool lastClassIdChunkWasForTargetClass )
    void        SkipToNextAttributeCommand( bool lastClassIdChunkWasForTargetClass );

    const unk_AttributeCommand* current;    // 0x0
    u32         classID;                    // 0x4
};

// Gormiti: RWS::GetClassNameHash( const char* ). Retail's is a branch to the registry, which
// returns the registered id of a class name (the name table itself is not in this unit).
u32 fn_801A7A84( const char* name );

/*
 * The base of every object configured by attribute packets (vtable lbl_80428170, destructor
 * 0x801A7870, defined in climax/cattributehandler.cpp). Gormiti: RWS::CAttributeHandler, whose
 * DWARF gives the members and the names marked below; its vtable has seven slots (~, PostCreate,
 * HandleAttributes, HandleOwnerAttributes, GetAttributeTable, GetAttrPostUpdateFunc, SetZone)
 * and retail's has ten, so slots are named after their first implementation, and only
 * HandleAttributes (fn_801A73A8) and SetZone (fn_801A7BFC) are identified (by role).
 * The constructor and the five empty or trivial defaults are inline: every unit that emits a
 * vtable holding a default keeps a weak copy of it, and the link keeps the first in link order,
 * CCollisionCapsule's (0x8000CFA8-0x8000CFD8, the lowest vtable lbl_803DFBB0 to use them).
 */
class CAttributeHandler {
public:
                    CAttributeHandler() :
                        mClassInfo( 0 ), mAssetList( 0 ), mZone( 0 ), mFilter( 0 ) {
                        m_InstanceId.m_Data[0] = 0;
                        m_InstanceId.m_Data[1] = 0;
                        m_InstanceId.m_Data[2] = 0;
                        m_InstanceId.m_Data[3] = 0;
                    }
    virtual         ~CAttributeHandler();
    virtual void    fn_8000CFD4() {}
    virtual void    fn_801A7A88();
    virtual void    fn_801A7B20();
    virtual void    fn_8000CFD0() {}
    virtual void    fn_8000CFCC() {}
    virtual void    fn_801A7BFC( CZoneChangeInfo* change );    // Gormiti: SetZone( CZone* )
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );   // Gormiti: HandleAttributes
    // the defaults clear what the out parameters point to and return 0; the parameter types are
    // unknown (word stores) and no override or caller is known
    virtual int     fn_8000CFB8( u32 arg, u32* outA, u32* outB ) {
                        *outA = 0;
                        *outB = 0;
                        return 0;
                    }
    virtual int     fn_8000CFA8( u32* out ) {
                        *out = 0;
                        return 0;
                    }

    // the texture called name, from the current texture dictionary or else from the handler's
    // asset list; addRef nonzero adds a reference. Gormiti: FindTexture( const char*, bool );
    // retail tests addRef as a word (cmpwi, no byte mask), so it stays an int here
    RwTexture*      FindTexture( const char* name, int addRef );
    // the raster of that texture. Gormiti: FindRaster( const char*, bool )
    RwRaster*       FindRaster( const char* name, int addRef );
    // posts OnRegisterDeleteEntity, then lists the handler for deletion
    void            RegisterForDelete();

    // Gormiti DWARF: RWS::CAttributeHandler::GetZone() const
    CZone*          GetZone() const { return mZone; }

    SClassInfo*                 mClassInfo;     // 0x4, the registry record of the class
    RWS::AssetTracker::CAssetList*  mAssetList; // 0x8
    CZone*                      mZone;          // 0xC
    char*                       mFilter;        // 0x10, owned, freed by the destructor
    RWS::RWSGUID                m_InstanceId;   // 0x14
};

/*
 * Registers a class by name with its factory and instance size; one static object per class.
 * The object has no data: the constructor files the class in a separately allocated record
 * (CModalTutorialMessageTrigger's registry object takes one byte of .sbss).
 * defined in 0x801A8F6C-0x801A9250, unit not yet reconstructed
 */
class unk_801A8F6C {
public:
    typedef CAttributeHandler* ( *Factory )( const unk_AttributePacket& packet );

            unk_801A8F6C( const char* name, Factory factory, u32 size );
            ~unk_801A8F6C();
};

/*
 * The registry's record of a class, the 0x5C-byte block that the registration allocates. Only
 * the factory, the size, the interned name, the behaviour name and the asset list are named;
 * the rest (the matrix, the reference count, the flag byte) is not reached by the code so far.
 * Gormiti: SClassInfo, the same members without the flag byte.
 */
struct SClassInfo {
    CAttributeHandler* ( *factory )( const unk_AttributePacket& packet );    // 0x0
    u32                 instanceSize;                                       // 0x4
    const char*         name;                                               // 0x8, interned
    const char*         behaviourName;                                      // 0xC
    RWS::AssetTracker::CAssetList* assetList;                               // 0x10
    u8                  field_0x14[0x48];                                   // 0x14
};

// The registry's table of class records and its count, in .sbss owned by units not yet
// reconstructed (the table itself is allocated elsewhere; both words are zero-initialised).
extern SClassInfo** lbl_805291BC;
extern int          lbl_805291C0;

/*
 * Callees in units not yet reconstructed (symbols without a C++ name yet).
 */
extern "C" {
    // the registry's interned name of a class name string, or NULL when none is registered
    // (Gormiti: RWS::CAttributeHandler::ReferenceClassName)
    const char*         fn_801B63CC( const char* name );
    // the registered id of a class name: files the name in the registry on first use
    u32                 fn_801B61E4( const char* name );
    // creates the handler of a creation packet (the class named by its first chunk)
    CAttributeHandler*  fn_801A7EB4( const char* className, const unk_AttributePacket& packet );
    // registers a created handler with the asset tracker (Gormiti: RWS::AssetTracker::Add)
    void                fn_801AF254( RwStream* stream, CAttributeHandler* handler,
                                     const unk_AttributePacket* packet, const RWS::RWSGUID& id );
    // the texture called name in an asset list
    RwTexture*          fn_801AEFEC( RWS::AssetTracker::CAssetList* list, const char* name );
    // the texture dictionary in use for loading
    RwTexDictionary*    fn_80206718();
}

#endif
