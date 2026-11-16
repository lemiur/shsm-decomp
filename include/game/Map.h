#ifndef GAME_MAP_H
#define GAME_MAP_H

#include <climax/Memory.h>

/*
 * The map and its waypoints, as the waypoint and layer behaviours use them (the map's unit,
 * after 0x8018AE2C, is not yet reconstructed). Header name invented.
 */

/*
 * A map waypoint as the map keeps it: built by the CMapWaypoint behaviour, copied by
 * unk_Map::fn_8018C524 (which resolves the two hashes). Size 0x1C. Name invented.
 */
struct unk_MapWaypoint {
            unk_MapWaypoint()
                : name( NULL ), field_0x4( NULL ), field_0x8( 0 ), field_0xC( 0 ),
                  field_0x10( 0 ), field_0x14( 0 ), field_0x18( 0 ), field_0x19( true ) {}
            ~unk_MapWaypoint() {
                if ( name != NULL ) {
                    delete[] name;
                    name = NULL;
                }
                if ( field_0x4 != NULL ) {
                    delete[] field_0x4;
                    field_0x4 = NULL;
                }
            }

    char*   name;           // 0x0, the waypoint's name (owned copy)
    char*   field_0x4;      // 0x4, a second name (owned copy)
    u32     field_0x8;      // 0x8, resolved from name by the map
    u32     field_0xC;      // 0xC, resolved from field_0x4 by the map
    s32     field_0x10;     // 0x10
    s32     field_0x14;     // 0x14
    u8      field_0x18;     // 0x18
    bool    field_0x19;     // 0x19
};

/* The owner of the map waypoints, reached through lbl_8052A3F0. Name invented. */
class unk_Map {
public:
    void    fn_8018C524( const unk_MapWaypoint* waypoint );    // defined in 0x8018C524: adds a waypoint
    void    fn_8018CA04();                                      // defined in 0x8018CA04: purges the waypoints
    void    fn_8018CD44( const char* name );                    // defined in 0x8018CD44: removes a waypoint by name
    void    fn_8018CFCC( const char* name, bool flag );         // defined in 0x8018CFCC: takes a map layer's name and flag
};

// defined in the map's unit, not yet reconstructed
extern unk_Map* lbl_8052A3F0;

#endif
