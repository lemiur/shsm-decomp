// unit name unconfirmed: class AtomicTags from the dtk signature names of its functions; no string or
// class-name evidence names this file; rename it when evidence appears and keep the open item in docs/exceptions.md.

#include <game/AtomicTags.h>
#include <climax/Heap.h>
#include <rwsdk/plugin/userdata/rpusrdat.h>
#include <string.h>

char lbl_803DFB00[0x44] = "atomicType\0\0ForceAlpha\0\0PreEffects\0\0ShadowMesh\0\0Missing Atomic Type";

/*
====================
fn_8000C32C
====================
*/
// 0x8000C32C
extern "C" AtomicTags::E_AtomicType fn_8000C32C( RpAtomic* atomic ) {
    AtomicTags::E_AtomicType atomicType = AtomicTags::Other;

    if ( atomic != NULL ) {
        RpGeometry* geometry = atomic->geometry;

        if ( geometry != NULL ) {
            RwInt32 count = RpGeometryGetUserDataArrayCount( geometry );

            for ( RwInt32 i = 0; i < count; i++ ) {
                RpUserDataArray* userData = RpGeometryGetUserDataArray( geometry, i );
                RpUserDataFormat format = RpUserDataArrayGetFormat( userData );
                const char* name = RpUserDataArrayGetName( userData );

                if ( format == rpINTUSERDATA && strstr( name, "atomicType" ) != NULL ) {
                    RwInt32 value = RpUserDataArrayGetInt( userData, 0 );

                    // Values of 16 or more are not atomic types: the default stays.
                    if ( value < 16 ) {
                        atomicType = ( AtomicTags::E_AtomicType )value;
                    }
                    break;
                }
            }
        }
    }

    return atomicType;
}

/*
====================
fn_8000C3E4

Returns the display name of a tag type; a type past Invalid gives the missing-type text.
====================
*/
// 0x8000C3E4
extern "C" const char* fn_8000C3E4( AtomicTags::E_AtomicType type ) {
    const char* names = lbl_803DFB00;
    switch ( type ) {
    case AtomicTags::Face:
        return "Face";
    case AtomicTags::Other:
        return "Other";
    case AtomicTags::Ropes:
        return "Ropes";
    case AtomicTags::Weapon:
        return "Weapon";
    case AtomicTags::Hair:
        return "Hair";
    case AtomicTags::NiceFX:
        return "NiceFX";
    case AtomicTags::Phone:
        return "Phone";
    case AtomicTags::ForceAlpha:
        return names + 0xC;
    case AtomicTags::PreEffects:
        return names + 0x18;
    case AtomicTags::Body:
        return "Body";
    case AtomicTags::Head:
        return "Head";
    case AtomicTags::Glasses:
        return "Glasses";
    case AtomicTags::Prop1:
        return "Prop1";
    case AtomicTags::Prop2:
        return "Prop2";
    case AtomicTags::Prop3:
        return "Prop3";
    case AtomicTags::ShadowMesh:
        return names + 0x24;
    case AtomicTags::Invalid:
        return "Invalid";
    default:
        return names + 0x30;
    }
}

/*
====================
AtomicTags::FindAtomicByTypeCB

Clump walk callback: records the first atomic of the searched tag type and stops the walk on it.
====================
*/
// 0x8000C49C
RpAtomic* AtomicTags::FindAtomicByTypeCB( RpAtomic* atomic, void* data ) {
    if ( atomic != NULL && data != NULL ) {
        TypeSearch* search = ( TypeSearch* )data;
        E_AtomicType tag = fn_8000C32C( atomic );
        if ( tag == search->type ) {
            search->found = atomic;
            return NULL;
        }
    }
    return atomic;
}

/*
====================
AtomicTags::FindAtomicByType

Returns the first atomic of a clump with the given tag type, or NULL.
====================
*/
// 0x8000C500
RpAtomic* AtomicTags::FindAtomicByType( RpClump* clump, E_AtomicType type ) {
    TypeSearch search;

    if ( clump != NULL ) {
        search.found = NULL;
        search.type = type;
        RpClumpForAllAtomics( clump, FindAtomicByTypeCB, &search );
        return search.found;
    }
    return NULL;
}

/*
====================
AtomicTags::FindAllAtomicsByTypeCB

Clump walk callback: pushes each atomic of the searched tag type onto the search's list.
====================
*/
// 0x8000C54C
RpAtomic* AtomicTags::FindAllAtomicsByTypeCB( RpAtomic* atomic, void* data ) {
    if ( atomic != NULL && data != NULL ) {
        TypeListSearch* search = ( TypeListSearch* )data;
        E_AtomicType tag = fn_8000C32C( atomic );
        if ( tag == search->type ) {
            AtomicNode* node = new ( GAME_HEAP, GAME_HEAP_FLAGS ) AtomicNode;
            node->atomic = atomic;
            node->next = search->head;
            search->head = node;
        }
    }
    return atomic;
}
