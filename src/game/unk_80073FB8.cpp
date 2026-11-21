// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80073FB8.h>

RwPluginRegistry lbl_803E6590 = { sizeof( RpClump ), sizeof( RpClump ), 0, 0, NULL, NULL };

/*
====================
fn_80073FB8

Builds a copy of the first spec's clump, its frame tree cloned with it, and adds to the copy a clone of
each atomic that each spec's types select from that spec's clump. The copy is dropped on any failure.
Open: retail keeps the template clump, the copy and the failure flag in stack slots and shares the add
block between the frame and destroy branches; the compiled layout differs there (NonMatching).
====================
*/
// 0x80073FB8
extern "C" RpClump* fn_80073FB8( unk_ClumpTypeSpec* specs, int numSpecs ) {
    RpClump* templateClump;
    RpClump* newClump;
    RwBool allAdded;

    if ( specs == NULL || numSpecs <= 0 ) {
        return NULL;
    }

    templateClump = specs->clump;
    newClump = RpClumpCreate();
    if ( newClump == NULL ) {
        return NULL;
    }

    if ( ( newClump->object.parent = _rwFrameCloneAndLinkClones( RpClumpGetFrame( templateClump ) ) ) == NULL ) {
        RpClumpDestroy( newClump );
        return NULL;
    }

    allAdded = TRUE;
    for ( int i = 0; i < numSpecs; i++ ) {
        unk_ClumpTypeSpec* spec = &specs[i];
        for ( int j = 0; j < spec->numTypes; j++ ) {
            RpAtomic* newAtomic = RpAtomicClone( AtomicTags::FindAtomicByType( spec->clump, spec->types[j] ) );
            if ( newAtomic == NULL ) {
                allAdded = FALSE;
            } else if ( RpClumpGetFrame( newClump ) != NULL ) {
                RpAtomicSetFrame( newAtomic, RpClumpGetFrame( newClump )->root );
                RpClumpAddAtomic( newClump, newAtomic );
            } else {
                RpAtomicDestroy( newAtomic );
            }
        }
    }

    _rwFramePurgeClone( RpClumpGetFrame( templateClump ) );

    if ( !allAdded ) {
        RpClumpDestroy( newClump );
        return NULL;
    }

    newClump->callback = templateClump->callback;
    _rwPluginRegistryCopyObject( &lbl_803E6590, newClump, templateClump );
    return newClump;
}
