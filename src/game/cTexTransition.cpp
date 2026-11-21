// cTexTransition: class name from the registration string lbl_803E86DC (also lbl_803E86EC).
// The unit name follows the registration, not the Gormiti link-map name of the factory.

#include <extras.h>
#include <game/cTexTransition.h>
#include <game/Messages.h>

u32 lbl_80529700 = fn_801A7A84( "cTexTransition" );
unk_801A8F6C lbl_80529704( "cTexTransition", cTexTransition::fn_8008EEDC, sizeof( cTexTransition ) );
cTexTransition* lbl_80529708;

/*
====================
cTexTransition::cTexTransition

Registers for the frame message and the second message, starts the progress at zero with a
duration of two, and links the new transition at the head of the global list.
====================
*/
// 0x8008E64C
cTexTransition::cTexTransition( const unk_AttributePacket& packet ) {
    fn_801A9DE8( lbl_8052A788 );
    fn_801A9DE8( lbl_8052A858 );
    field_0x4C = false;
    field_0x54 = 2.0f;
    field_0x50 = 0.0f;
    field_0x5C = NULL;
    field_0x4D = false;
    field_0x58 = 0;
    field_0x60 = lbl_80529708;
    lbl_80529708 = this;
    field_0x4E = 0;
    field_0x64 = false;
}

/*
====================
cTexTransition::~cTexTransition

Unregisters its messages, fades the progress back to zero, frees the texture list, and
removes the transition from the list of live transitions, marking the survivors finished.
Open: retail reloads the list head in the texture loop's condition, which MWCC merges with the
body's load (98.41%).
====================
*/
// 0x8008E724
cTexTransition::~cTexTransition() {
    fn_801A9E00( lbl_8052A788 );
    fn_801A9E00( lbl_8052A858 );
    fn_801A9E00( field_0x34 );
    fn_801A9E00( field_0x3C );
    fn_801A9E00( field_0x44 );
    fn_801A9940( field_0x34 );
    fn_801A9940( field_0x3C );
    fn_801A9940( field_0x44 );

    field_0x50 = 0.0f;
    fn_8008ED54();
    while ( field_0x5C != NULL ) {
        unk_TexTransitionNode* node = field_0x5C;
        field_0x5C = node->next;
        delete[] node->data;
        if ( node->texture != NULL ) {
            RwTextureDestroy( node->texture );
        }
        delete node;
    }

    cTexTransition* prev = NULL;
    cTexTransition* next = lbl_80529708;
    lbl_80529708 = NULL;
    while ( next != NULL ) {
        cTexTransition* cur = next;
        next = cur->field_0x60;
        if ( cur != this ) {
            cur->field_0x60 = prev;
            prev = cur;
            cur->field_0x64 = true;
            lbl_80529708 = cur;
        }
    }
}

/*
====================
cTexTransition::fn_8009CB1C

Overrides CBaseBehaviour::fn_8009CB1C. Not yet reconstructed. Its thunk (0x8008EF24) is the
compiler's this-adjusting entry for the CEventHandler base.
====================
*/
// 0x8008E8A0
void cTexTransition::fn_8009CB1C( unk_Msg& msg ) {
}

/*
====================
cTexTransition::fn_801A73A8

Overrides CAttributeHandler::fn_801A73A8 (HandleAttributes). Not yet reconstructed: calls the
base first, then walks the packet's attribute commands.
====================
*/
// 0x8008E994
void cTexTransition::fn_801A73A8( const unk_AttributePacket& packet ) {
}

/*
====================
cTexTransition::fn_801A7A88

Overrides CAttributeHandler::fn_801A7A88; tail-calls fn_8008EBC0 on the same object.
====================
*/
// 0x8008EADC
void cTexTransition::fn_801A7A88() {
    fn_8008EBC0();
}

/*
====================
cTexTransition::fn_8008EAE0

Adds a texture name to the list unless the name is empty or already listed. The new node is
pushed at the head of the list.
====================
*/
// 0x8008EAE0
void cTexTransition::fn_8008EAE0( const char* name ) {
    if ( name != NULL && strlen( name ) != 0 ) {
        unk_TexTransitionNode* node;
        for ( node = field_0x5C; node != NULL; node = node->next ) {
            if ( stricmp( node->data, name ) == 0 ) {
                return;
            }
        }
        node = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_TexTransitionNode;
        node->texture = NULL;
        node->data = new( GAME_HEAP, GAME_HEAP_FLAGS ) char[ strlen( name ) + 1 ];
        node->palette = NULL;
        node->field_0x10 = 0;
        strcpy( node->data, name );
        node->next = field_0x5C;
        field_0x5C = node;
    }
}

/*
====================
cTexTransition::fn_8008EBC0

Not yet reconstructed. Its debug string reads "Texture on cTexTransition m...".
====================
*/
// 0x8008EBC0
void cTexTransition::fn_8008EBC0() {
}

/*
====================
cTexTransition::fn_8008ECC0

Advances the progress by elapsed / duration, clamped to 0..1 (the reverse direction runs
down), and refreshes the palettes. Once the progress reaches its end the transition stops and
the finished flag is raised for one pass.
====================
*/
// 0x8008ECC0
void cTexTransition::fn_8008ECC0( float elapsed ) {
    if ( field_0x4C ) {
        if ( !field_0x4D ) {
            field_0x50 = field_0x50 + elapsed / field_0x54;
            if ( field_0x50 > 1.0f ) {
                field_0x50 = 1.0f;
                field_0x4C = false;
            }
        } else {
            field_0x50 = field_0x50 - elapsed / field_0x54;
            if ( field_0x50 < 0.0f ) {
                field_0x50 = 0.0f;
                field_0x4C = false;
            }
        }
        fn_8008ED54();
    } else if ( field_0x64 ) {
        field_0x64 = false;
        fn_8008ED54();
    }
}

/*
====================
cTexTransition::fn_8008ED54

Rebuilds every node's palette for the current progress: entry n is a grey level that fades
over the 256 entries, centred on progress * 264.
====================
*/
// 0x8008ED54
void cTexTransition::fn_8008ED54() {
    unk_TexTransitionNode* node;
    int level = (int)( 264.0f * field_0x50 );
    for ( node = field_0x5C; node != NULL; node = node->next ) {
        u16* palette = node->palette;
        if ( palette == NULL ) {
            continue;
        }
        for ( int i = 0; i < 256; i++ ) {
            int v = ( level - i ) * 30;
            if ( v < 0 ) {
                v = 0;
            }
            if ( v > 0xFF ) {
                v = 0xFF;
            }
            u32 c = 0xFF - v;
            *palette++ = (u16)( ( c << 8 ) | c );
        }
    }
}

