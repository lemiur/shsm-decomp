// unit name unconfirmed: no string or class-name evidence names this file (its class is unk_800A8100,
// named for its constructor's address); rename it when evidence appears and keep the open item in
// docs/exceptions.md.

#include <game/unk_800A8100.h>
#include <game/unk_ResourceManager.h>

/*
====================
unk_800A8100::fn_800A8640
====================
*/
// 0x800A8640
void unk_800A8100::fn_800A8640( unk_RpClump* clump ) {
}

/*
====================
unk_800A8100::fn_800A82E4
====================
*/
// Parked (wall.exe): control flow matches; register/schedule differs (this in r29 not r30, and the
// 16-word RwMatrix copy is interleaved, retail loads all sixteen words first).
// 0x800A82E4
unk_RpClump* unk_800A8100::fn_800A82E4() {
    unk_RwMatrix matrix;
    unk_RpClump* oldClump;
    unk_RpClump* newClump;
    unk_AssetEntry* asset;
    unk_RpHAnimHierarchy* hierarchy;

    if ( stream == NULL ) {
        if ( cacheHandle != NULL ) {
            stream = fn_801B1E3C( lbl_8052A5C0, name, this, 0 );
        }
        if ( stream != NULL ) {
            assets = RWS::FileSystem::Process( ( RwStream* )stream, false );
            if ( assets != NULL ) {
                assets->IncRefCount();

                asset = fn_801AED0C( assets, "rwID_CLUMP" );
                if ( asset != NULL && asset->clump != NULL ) {
                    newClump = RpClumpClone( asset->clump );
                    if ( newClump != NULL ) {
                        /* The new clump takes the old clump's placement, or the identity when there is none. */
                        oldClump = clump;
                        if ( oldClump != NULL ) {
                            matrix = RpClumpGetFrame( oldClump )->modelling;
                            oldClump->object.parent = NULL;
                            if ( world != NULL ) {
                                RpWorldRemoveClump( world, oldClump );
                            }
                            RpClumpDestroy( oldClump );
                            clump = NULL;
                        } else {
                            matrix.right.x = 1.0f;
                            matrix.right.y = 0.0f;
                            matrix.right.z = 0.0f;
                            // RenderWare identity-matrix flags, set on the default placement

                            matrix.flags |= 0x20003;
                            matrix.up.x = 0.0f;
                            matrix.up.y = 1.0f;
                            matrix.up.z = 0.0f;
                            matrix.at.x = 0.0f;
                            matrix.at.y = 0.0f;
                            matrix.at.z = 1.0f;
                            matrix.pos.x = 0.0f;
                            matrix.pos.y = 0.0f;
                            matrix.pos.z = 0.0f;
                        }

                        clump = newClump;
                        if ( RpClumpGetWorld( clump ) != NULL ) {
                            RpWorldRemoveClump( RpClumpGetWorld( clump ), clump );
                        }
                        if ( world != NULL ) {
                            RpWorldAddClump( world, clump );
                        }
                        if ( attachment != NULL ) {
                            fn_8009EB2C( attachment, clump );
                        }

                        RpClumpGetFrame( clump )->modelling = matrix;
                        RwFrameUpdateObjects( RpClumpGetFrame( clump ) );
                        fn_800A8640( clump );
                    }
                }

                asset = fn_801AED0C( assets, "rwID_HANIMANIMATION" );
                if ( asset != NULL ) {
                    hierarchy = fn_801B6AD8( clump );
                    if ( hierarchy != NULL ) {
                        fn_801B6B44( clump, hierarchy );
                        // hierarchy flag set before the hierarchy is attached

                        hierarchy->flags |= 0x1000;
                        RpHAnimHierarchyAttach( hierarchy );
                    }
                    if ( asset->animation != NULL && hierarchy != NULL ) {
                        RtAnimInterpolatorSetCurrentAnim( hierarchy->interpolator, asset->animation );
                        RpHAnimHierarchyUpdateMatrices( hierarchy );
                    }
                }
            }

            fn_801B2004( lbl_8052A5C0, stream );
            stream = NULL;
            fn_801B0B8C( lbl_8052A5C0, cacheHandle, this, 1 );
            cacheHandle = NULL;
        }
    }
    return clump;
}
