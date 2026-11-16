#ifndef GAME_UNK_POINTOFINTERESTMANAGER_H
#define GAME_UNK_POINTOFINTERESTMANAGER_H

#include <revolution/types.h>
#include <string.h>
#include <climax/Frame.h>
#include <climax/unk_List.h>
#include <climax/Utils.h>
#include <game/CPointOfInterest.h>

class CPlayer;
class unk_8003C454;

/*
 * The registry of the live points of interest (the object lbl_80529A88 points to; unit
 * 0x800C552C-0x800C5FCC). CPointOfInterest files itself in it on creation and removes itself in
 * its destructor. Once a frame the player's view picks the point the player is looking at
 * (fn_800C5800), and the player can activate it (fn_800C5D60, fn_800C5DA4). Size 0x430. Class and
 * member names invented; the list at 0x40C is never filled in this unit, and its element type is
 * a guess (only its vtable lbl_803EBAE8, with the list's destructor, is known).
 */
class unk_PointOfInterestManager {
public:
                        unk_PointOfInterestManager() :
                            pointCount( 0 ),
                            chosen( NULL ),
                            nearestInView( NULL ),
                            heightMax( 3.0f ),
                            heightMin( -0.5f ),
                            rangeMargin( 1.0f ),
                            viewDotNarrow( 0.5f ),
                            viewDotWide( 0.258f ) {
                            memset( points, 0, sizeof( points ) );
                        }

    void                fn_800C56A4( CPointOfInterest* poi );   // adds a point
    void                fn_800C5704( CPointOfInterest* poi );   // removes a point
    bool                fn_800C57A8( const unk_RwV3d* from, const unk_RwV3d* to );
    bool                fn_800C57E4( const CPointOfInterest* poi, float distance );
    void                fn_800C5800( CPlayer* player );
    bool                fn_800C5D60( unk_8003C454* who );
    bool                fn_800C5DA4( unk_8003C454* who );
    CPointOfInterest*   fn_800C5E24();
    CPointOfInterest*   fn_800C5E2C( CPointOfInterest* poi );
    CPointOfInterest*   fn_800C5E8C( CPointOfInterest* poi );
    CPointOfInterest*   fn_800C5F4C( const char* name );

    /* The index of poi in points, -1 when it is not filed. Name invented. */
    int                 IndexOf( const CPointOfInterest* poi ) const {
                            int found = -1;
                            for ( int i = 0; i < pointCount; i++ ) {
                                if ( poi == points[i] ) {
                                    found = i;
                                    break;
                                }
                            }
                            return found;
                        }

    /* The first point from index i on whose name is name (case ignored). Name invented. */
    CPointOfInterest*   FindByName( int i, const char* name ) const {
                            CPointOfInterest* found = NULL;
                            for ( ; i != pointCount; i++ ) {
                                if ( points[i]->name != NULL && fn_801B6138( points[i]->name, name ) ) {
                                    found = points[i];
                                    break;
                                }
                            }
                            return found;
                        }

    CPointOfInterest*   points[256];                // 0x000
    int                 pointCount;                 // 0x400
    CPointOfInterest*   chosen;                     // 0x404, the point fn_800C5800 chose
    CPointOfInterest*   nearestInView;              // 0x408, the nearest of the points in view
    unk_List< CPointOfInterest* > field_0x40C;      // never used
    float               heightMax;                  // 0x41C, the highest a point may lie above the player
    float               heightMin;                  // 0x420, the lowest (below the player)
    float               rangeMargin;                // 0x424, added to a point's own range
    float               viewDotNarrow;              // 0x428
    float               viewDotWide;                // 0x42C
};

extern unk_PointOfInterestManager* lbl_80529A88;

void fn_800C5604();

#endif
