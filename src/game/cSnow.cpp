#include <game/cSnow.h>
#include <stdlib.h>
#include <climax/Heap.h>
#include <climax/Frame.h>
#include <game/unk_80491B38.h>
#include <game/Messages.h>

/* Defined elsewhere, unit not yet reconstructed: the snow's per-frame setup. Name invented. */
extern "C" void fn_80095154();
// In the unreconstructed auto unit at 0x800953B0; takes the snow it tears down.
extern "C" void fn_800953B0( cSnow* snow );

/* The head of the static list of live snows; see cSnow.h. */
cSnow* lbl_805296F8;

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803E84E8 size 0x9 lbl_803E84E8
//   .data 0x803E84F4 size 0x68 jumptable_803E84F4
//   .data 0x803E8560 size 0x58 lbl_803E8560
//   .data 0x803E85B8 size 0x5C lbl_803E85B8
//   .data 0x803E8614 size 0xE lbl_803E8614
//   .data 0x803E8624 size 0xE lbl_803E8624
//   .bss 0x8046A628 size 0xC lbl_8046A628
//   .bss 0x8046A634 size 0xC lbl_8046A634
//   .sdata 0x80527280 size 0x6 lbl_80527280
//   .sdata 0x80527288 size 0x6 lbl_80527288
//   .sbss 0x805296E8 size 0x4 lbl_805296E8
//   .sbss 0x805296EC size 0x4 lbl_805296EC
//   .sbss 0x805296F0 size 0x4 lbl_805296F0
//   .sbss 0x805296F4 size 0x4 lbl_805296F4
//   .sbss 0x805296F8 size 0x4 lbl_805296F8
//   .sbss 0x805296FC size 0x4 lbl_805296FC
//   .sdata2 0x8052C0C0 size 0x4 lbl_8052C0C0
//   .sdata2 0x8052C0C4 size 0x4 lbl_8052C0C4
//   .sdata2 0x8052C0C8 size 0x4 lbl_8052C0C8
//   .sdata2 0x8052C0CC size 0x4 lbl_8052C0CC
//   .sdata2 0x8052C0D0 size 0x4 lbl_8052C0D0
//   .sdata2 0x8052C0D4 size 0x4 lbl_8052C0D4
//   .sdata2 0x8052C0D8 size 0x4 lbl_8052C0D8
//   .sdata2 0x8052C0DC size 0x4 lbl_8052C0DC
//   .sdata2 0x8052C0E0 size 0x4 lbl_8052C0E0
//   .sdata2 0x8052C0E4 size 0x4 lbl_8052C0E4
//   .sdata2 0x8052C0E8 size 0x4 lbl_8052C0E8
//   .sdata2 0x8052C0EC size 0x4 lbl_8052C0EC
//   .sdata2 0x8052C0F0 size 0x4 lbl_8052C0F0
//   .sdata2 0x8052C0F8 size 0x8 lbl_8052C0F8
//   .sdata2 0x8052C100 size 0x4 lbl_8052C100
//   .sdata2 0x8052C104 size 0x4 lbl_8052C104
//   .sdata2 0x8052C108 size 0x4 lbl_8052C108
//   .sdata2 0x8052C10C size 0x4 lbl_8052C10C
//   .sdata2 0x8052C110 size 0x4 lbl_8052C110
//   .sdata2 0x8052C114 size 0x4 lbl_8052C114
//   .sdata2 0x8052C118 size 0x4 lbl_8052C118
//   .sdata2 0x8052C11C size 0x4 lbl_8052C11C
//   .sdata2 0x8052C120 size 0x4 lbl_8052C120
//   .sdata2 0x8052C124 size 0x4 lbl_8052C124
//   ... 2 more

/*
====================
cSnow::cSnow
====================
*/
// 0x8008C62C
cSnow::cSnow() {
    fn_801A9DE8( lbl_8052A788 );
    field_0x38 = 10000;
    field_0x34 = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_SnowFlake[10000];
    field_0xA0 = 7.0f;
    field_0xA4 = 6.0f;
    field_0x118 = 1.25f;
    field_0x75 = 1;
    field_0xAC = 0.2f;
    field_0xB0 = 0.5f;
    field_0x114 = lbl_805296F8;
    lbl_805296F8 = this;
    field_0x11C = 0.0f;
    field_0x120 = 0.0f;
    field_0x124 = 0.0f;
    field_0xB4 = 5.0f;
    field_0x12C = 0;
    field_0x130 = 0;
    field_0x134 = 0;
    field_0x128 = 0;
    field_0xA8 = 0.025f;
    field_0x13C = 0;
    field_0x140 = 0;
    field_0x144 = 0;
    field_0x74 = 1;
    field_0x80 = 0.5f;
    field_0x84 = 0.1f;
    field_0x88 = 1.0f;
    field_0x8C = 0.5f;
    field_0x90 = 0.5f;
    field_0x100 = 0.0f;
    field_0x104 = 0.0f;
    field_0x108 = 0.0f;
    field_0xD8 = 0;
    field_0xDC = 0;
    field_0xE0 = 0;
    field_0xF0 = 0;
    field_0x76 = 0;
    field_0x138 = 0;
    field_0x139 = 0;
    field_0xF1 = 0;
    field_0x7A = 0;
    field_0x7C = 0.0f;
    field_0x77 = 0;
    field_0x78 = 0;
    unk_RwFrame* frame = GetPlayerRecord().field_0x14->field_0x30;
    field_0xCC = frame->modelling.pos.x;
    field_0xD0 = frame->modelling.pos.y;
    field_0xD4 = frame->modelling.pos.z;
    fn_80095154();
    fn_8008C82C();
    field_0x10C.fn_801A92FC( NULL );
    field_0x79 = 0;
    field_0x94 = 1.0f;
    field_0x98 = 1.0f;
    field_0xB8 = 0;
}

/*
====================
fn_8008C82C
====================
*/
// 0x8008C82C
void cSnow::fn_8008C82C() {
    unk_RwBBox box;
    box.sup.x = field_0xCC + field_0xA0;
    box.inf.x = field_0xCC - field_0xA0;
    box.inf.y = field_0xD0 - 1.0f;
    box.sup.y = box.inf.y + field_0xA4;
    box.inf.z = field_0xD4 - field_0xA0;
    box.sup.z = field_0xD4 + field_0xA0;
    const float cellScale = 16.0f * (1.0f / (2.0f * field_0xA0));
    unk_SnowFlake* flake = field_0x34;
    for (int i = 0; i < 10000; i++, flake++) {
        flake->y = field_0xA4 * ((float)rand() / 32767.0f);
        flake->x = 2.0f * (field_0xA0 * ((float)rand() / 32767.0f));
        float z = 2.0f * (field_0xA0 * ((float)rand() / 32767.0f));
        flake->x += box.inf.x;
        flake->y += box.inf.y;
        flake->z = z + box.inf.z;
        flake->field_0xE = 0;
        int col = (int)(cellScale * (flake->x - box.inf.x));
        int row = (int)(cellScale * (flake->z - box.inf.z));
        flake->cell = col | (row << 4);
        flake->field_0xC = 0;
    }
}

/*
====================
cSnow::~cSnow
====================
*/
// 0x8008CA40
cSnow::~cSnow() {
    fn_801A9E00( lbl_8052A788 );
    fn_801A9E00( field_0x10C );
    fn_801A9E00( field_0x3C );
    fn_801A9E00( field_0x44 );
    fn_801A9E00( field_0x4C );
    fn_801A9E00( field_0x54 );
    fn_801A9E00( field_0x5C );
    fn_801A9E00( field_0x64 );
    fn_801A9E00( field_0x6C );
    fn_801A9940( field_0x10C );
    fn_801A9940( field_0x3C );
    fn_801A9940( field_0x44 );
    fn_801A9940( field_0x4C );
    fn_801A9940( field_0x54 );
    fn_801A9940( field_0x5C );
    fn_801A9940( field_0x64 );
    fn_801A9940( field_0x6C );
    delete[] field_0x34;

    cSnow* next = lbl_805296F8;
    lbl_805296F8 = NULL;
    while (next != NULL) {
        cSnow* cur = next;
        next = cur->field_0x114;
        if (cur != this) {
            cur->field_0x114 = lbl_805296F8;
            lbl_805296F8 = cur;
        }
    }

    if (field_0x130 != NULL) {
        RwTextureDestroy( field_0x130 );
    }
    fn_800953B0( this );
}

/*
====================
fn_8009CB1C
====================
*/
// Open at 93%: the compiler lays out the first arm (the call to fn_8008D08C) out of line; retail keeps it inline.
// 0x8008CC14
void cSnow::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if (msg.record == lbl_8052A788) {
        fn_8008D08C( this, lbl_80491B38 );
        return;
    }
    if (msg.record == field_0x3C) {
        field_0x8C = field_0x90;
        field_0x74 = 1;
    } else if (msg.record == field_0x44) {
        field_0x74 = 0;
    } else if (msg.record == field_0x4C) {
        field_0x90 = field_0x80;
    } else if (msg.record == field_0x54) {
        field_0x90 = field_0x84;
    } else if (msg.record == field_0x5C) {
        field_0x90 = field_0x88;
    } else if (msg.record == field_0x10C) {
        RwV3dTransformPoint( &field_0xF4, &field_0x30->modelling.pos, static_cast<unk_RwMatrix*>( msg.data ) );
        field_0xF1 = 1;
    } else if (msg.record == field_0x64) {
        field_0x79 = 1;
    } else if (msg.record == field_0x6C) {
        field_0x79 = 0;
    }
}

/*
====================
fn_801A73A8
====================
*/
// 0x8008CD48
void cSnow::fn_801A73A8( const unk_AttributePacket& packet ) {
    // TODO
}

/*
====================
fn_801A7A88
====================
*/
// 0x8008D02C
void cSnow::fn_801A7A88() {
    if (field_0x12C != NULL) {
        field_0x130 = FindTexture( field_0x12C, 1 );
        if (field_0x130 != NULL) {
            field_0x134 = field_0x130->raster;
            RwTextureSetAddressing( field_0x130, rwTEXTUREADDRESSCLAMP );
        }
    }
}

/*
====================
fn_8008D08C
====================
*/
// 0x8008D08C
extern "C" void fn_8008D08C( cSnow* snow, unk_80491B38& player ) {
    // TODO
}

/*
====================
fn_8008E120
====================
*/
// 0x8008E120
extern "C" void fn_8008E120() {
    // TODO
}

/*
====================
fn_8008E1E8
====================
*/
// 0x8008E1E8
extern "C" void fn_8008E1E8() {
    // TODO
}

/*
====================
fn_8008E268
====================
*/
// 0x8008E268
extern "C" void fn_8008E268() {
    // TODO
}

/*
====================
fn_8008E2A4
====================
*/
// 0x8008E2A4
extern "C" void fn_8008E2A4() {
    // TODO
}

/*
====================
fn_8008E36C
====================
*/
// 0x8008E36C
extern "C" void fn_8008E36C() {
    // TODO
}

/*
====================
fn_8008E3DC
====================
*/
// 0x8008E3DC
extern "C" void fn_8008E3DC() {
    // TODO
}

/*
====================
fn_8008E570

Address taken by the static initialiser: probably an inline factory that belongs in the header.
====================
*/
// 0x8008E570
extern "C" void fn_8008E570() {
    // TODO
}

/*
====================
fn_8008E5B8

Address taken by the static initialiser: probably an inline factory that belongs in the header.
====================
*/
// 0x8008E5B8
extern "C" void fn_8008E5B8() {
    // TODO
}

/*
====================
fn_8008E62C
====================
*/
// 0x8008E62C
extern "C" void fn_8008E62C() {
    // TODO
}

/*
====================
fn_8008E634
====================
*/
// 0x8008E634
extern "C" void fn_8008E634() {
    // TODO
}

/*
====================
fn_8008E63C
====================
*/
// 0x8008E63C
extern "C" void fn_8008E63C() {
    // TODO
}

/*
====================
fn_8008E644
====================
*/
// 0x8008E644
extern "C" void fn_8008E644() {
    // TODO
}

// Compiler-generated, no source of their own: fn_8008E4D0 (0x8008E4D0).
