// Unit name and boundary: unit plan heuristic (no string or sibling evidence); name unconfirmed.
// The IGC event registration (fn_800ABE6C, the IGC message names and payload types from the DOL's
// strings), a time wrapper, and a length-prefixed byte block read from a stream.
// NonMatching: fn_800ABE6C (96.5%) keeps the new object in a local, which retail does not (its
// constructor's return is stored directly), and addresses the payload-type strings from one pooled
// table (lbl_803E9F68) that the source's string literals do not reproduce.

#include <string.h>
#include <game/unk_800ABE6C.h>

// The message names, one pointer each (the registration reads them from these words).
const char* const lbl_8052C458 = "IGCStart";
const char* const lbl_8052C45C = "IGCStop";
const char* const lbl_8052C460 = "IGCEnd";
const char* const lbl_8052C464 = "IGCSkipRequest";
const char* const lbl_8052C468 = "IGCSkip";
const char* const lbl_8052C46C = "IGCActive";
const char* const lbl_8052C470 = "IGCTerminate";
const char* const lbl_8052C474 = "IGC_EventStart";
const char* const lbl_8052C478 = "IGC_EventStop";
const char* const lbl_8052C47C = "IGC_PreAnimCharPlay";
const char* const lbl_8052C480 = "IGC_PostAnimCharPlay";
const char* const lbl_8052C484 = "IGC_IsInteractiveCharacterAvailable";
const char* const lbl_8052C488 = "IGC_AnimCharShowAtomic";
const char* const lbl_8052C48C = "IGC_AnimCharHideAtomic";

/*
====================
fn_800ABE6C
====================
*/
// 0x800ABE6C
extern "C" int fn_800ABE6C() {
    if ( lbl_80529940 == NULL ) {
        void* obj = operator new( 0x104, GAME_HEAP, GAME_HEAP_FLAGS );
        if ( obj != NULL ) {
            fn_800B6210( obj );
        }
        lbl_80529940 = obj;
    }

    lbl_80529878 = 0;
    memset( lbl_8046E958, 0, sizeof( lbl_8046E958 ) );
    memset( lbl_8046E9D8, 0, sizeof( lbl_8046E9D8 ) );
    lbl_80527348 = -1;

    fn_801A93B4( lbl_805298A0, lbl_8052C458, "IGCInfo*" );
    fn_801A93B4( lbl_805298A8, lbl_8052C45C, "IGCInfo*" );
    fn_801A93B4( lbl_805298B0, lbl_8052C460, "IGCInfo*" );
    fn_801A93B4( lbl_805298C0, lbl_8052C464, "" );
    fn_801A93B4( lbl_805298B8, lbl_8052C468, "IGCInfo*" );
    fn_801A93B4( lbl_805298C8, lbl_8052C46C, "*" );
    fn_801A93B4( lbl_805298D0, lbl_8052C470, "" );
    fn_801A93B4( lbl_805298D8, lbl_8052C474, "EventMsgInfo*" );
    fn_801A93B4( lbl_805298E0, lbl_8052C478, "EventMsgInfo*" );
    fn_801A93B4( lbl_805298E8, lbl_8052C47C, "AnimationEvent*" );
    fn_801A93B4( lbl_805298F0, lbl_8052C480, "AnimationEvent*" );
    fn_801A93B4( lbl_805298F8, lbl_8052C484, "InteractiveCharInfo*" );
    fn_801A93B4( lbl_80529900, lbl_8052C488, "const char*" );
    fn_801A93B4( lbl_80529908, lbl_8052C48C, "const char*" );

    return 1;
}

/*
====================
fn_800ABFE0
====================
*/
// 0x800ABFE0
extern "C" u32 fn_800ABFE0() {
    return RWS::Time::GetTime();
}

/*
====================
fn_800ABFE4
====================
*/
// 0x800ABFE4
extern "C" int fn_800ABFE4( unk_ByteBlock* block, void* stream ) {
    if ( block->data != NULL ) {
        delete[] block->data;
        block->data = NULL;
        block->count = 0;
    }

    fn_800AF188( stream, &block->count, 2 );
    if ( block->count != 0 ) {
        block->data = new ( GAME_HEAP, GAME_HEAP_FLAGS ) u8[ block->count ];
        fn_800AF064( stream, block->data, block->count );
    }
    return 0;
}

/*
====================
fn_800AC078
====================
*/
// 0x800AC078
extern "C" int fn_800AC078( unk_ByteBlock* block, void* stream ) {
    fn_800AF088( stream, &block->count, 2 );
    if ( block->data != NULL ) {
        fn_800AF090( stream, block->data, block->count );
    }
    return 0;
}
