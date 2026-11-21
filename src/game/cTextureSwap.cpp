// cTextureSwap: class name from the string literals "cTextureSwap" (fn_80090978) and
// "TextureSwap" (fn_80090C64) and the vtable __vt__12cTextureSwap. Unit name and boundary: unit_plan.

#include <extras.h>
#include <game/cTextureSwap.h>

unk_TexSwapRegistry lbl_8046A668;

/*
====================
cTextureSwap::~cTextureSwap

Leaves the event handler. Destroys the texture the swap loaded, or, when it loaded none, removes
the swap from the owner list of its registry entry.
====================
*/
// 0x8008EF34
cTextureSwap::~cTextureSwap() {
    fn_801A9E00( id );
    fn_801A9940( id );
    if ( texture != NULL ) {
        RwTextureDestroy( texture );
    } else {
        const char* key = name;
        unk_TexSwapEntry* entry = NULL;
        unk_TexSwapEntry* candidate;
        unk_TexSwapNode* node = lbl_8046A668.head;
        for ( ; node != NULL; node = node->next ) {
            candidate = node->entry;
            if ( stricmp( candidate->name, key ) == 0 ) {
                entry = candidate;
                break;
            }
        }
        if ( entry != NULL ) {
            for ( unk_TexSwapOwnerNode* link = entry->owners; link != NULL; link = link->next ) {
                if ( link->owner->behaviour == this ) {
                    link->owner->behaviour = NULL;
                }
            }
        }
    }
}

/*
====================
cTextureSwap::fn_801A73A8

Takes the names, the flag, the event and the save-data keys of the swap from the attribute packet.
Open (not matched): retail ends with two empty string-walk loops over name and textureName (no
observable effect, kept by the optimiser). No evidence names their source, so they are left out,
and without them the registers of this and packet are swapped.
====================
*/
// 0x8008F670
void cTextureSwap::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529710 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetName:
            name = it->GetString();
            break;
        case CMD_SetTextureName:
            textureName = it->GetString();
            break;
        case CMD_SetFlag:
            field_0x40 = it->GetU32() != 0;
            break;
        case CMD_SetEvent:
            fn_801AA218( id, it->GetString(), NULL );
            break;
        case CMD_SetSaveDataA:
            saveDataA = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetSaveDataB:
            saveDataB = fn_801B67FC( it->GetString(), 0 );
            break;
        }
        ++it;
    }
}

// TODO: fn_8009CB1C (0x8008F04C), fn_801A7A88 (0x8008F7C0) and the other
// functions of this unit are not reconstructed yet.
extern "C" void fn_800902BC() {
    // TODO
}

extern "C" void fn_80090394() {
    // TODO
}

extern "C" void fn_800903B0() {
    // TODO
}

extern "C" void fn_80090944() {
    // TODO
}

extern "C" void fn_80090A28() {
    // TODO
}

extern "C" void fn_80090AAC() {
    // TODO
}

extern "C" void fn_80090B84() {
    // TODO
}

extern "C" void fn_80090C64() {
    // TODO
}

extern "C" void fn_800910F8() {
    // TODO
}

extern "C" void fn_80091100() {
    // TODO
}
