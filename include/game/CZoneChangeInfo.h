#ifndef GAME_CZONECHANGEINFO_H
#define GAME_CZONECHANGEINFO_H

// Header name from the class name ("CZoneChangeInfo*", a message payload type string at
// lbl_803E2788); the unit, in 0x80029004-0x8002D1A8, is not yet reconstructed.

#include <revolution/types.h>
#include <climax/AttributeHandler.h>
#include <climax/Frame.h>
#include <game/unk_RespotManager.h>

/*
 * Callees in the CZone unit, not yet reconstructed (symbols without a C++ name yet): they add
 * and drop a reference to a zone (the count at 0x208).
 */
class unk_ZoneHandle;
class unk_RespotManager;

extern "C" {
    void    fn_80028170( CZone* zone );
    void    fn_80028180( CZone* zone );
    // defined in 0x80031E18 (the respot manager's unit): unregisters a zone handle from the manager
    void    fn_80031E18( unk_RespotManager* manager, unk_ZoneHandle* handle );
}

/*
 * A counted handle to a zone. The constructor keeps the zone only if the zone manager
 * (lbl_80529260) still lists it, takes a reference and registers the handle with the manager;
 * the destructor unregisters it and drops the reference. Name invented.
 */
class unk_ZoneHandle {
public:
                    unk_ZoneHandle( CZone* candidate ); // 0x8002905C, defined inline in CZoneChangeInfo.cpp
    // the zone when the manager's list holds it, NULL otherwise (name invented)
    static CZone*   FindListedZone( CZone* candidate );
                    ~unk_ZoneHandle();                  // 0x800290F0

    unk_ZoneHandle& operator=( const unk_ZoneHandle& other ) {
                        if ( other.zone != NULL ) {
                            other.AddRef();
                        }
                        CZone* previous = zone;
                        zone = other.zone;
                        if ( previous != NULL ) {
                            fn_80028180( previous );
                        }
                        return *this;
                    }
    // takes a reference to the zone held
    void            AddRef() const { fn_80028170( GetZone() ); }

    // returns the member by reference, as the Climax accessors do, so each use reads it again
    CZone* const&   GetZone() const { return zone; }

    CZone*          zone;       // 0x0, NULL when the zone was not listed
};

/*
 * A change of zone, handed to CAttributeHandler::fn_801A7BFC (the zone-change slot): the zone
 * left and the zone entered, each NULL when there is none. Member names invented.
 */
class CZoneChangeInfo {
public:
                    CZoneChangeInfo();                  // 0x80029164, both zones NULL
                    ~CZoneChangeInfo();                 // 0x8002930C

    // 0x800293D4; keeps a copy of each of the three names (fn_801B67FC) in the owned strings
    void            fn_800293D4( const char* a, const char* b, const char* c );

    unk_ZoneHandle  oldZone;        // 0x0
    unk_ZoneHandle  newZone;        // 0x4
    char*           field_0x8;      // owned copies of the names, delete[] by the destructor
    char*           field_0xC;
    char*           field_0x10;
    unk_RwMatrix    field_0x14;     // identity after construction
    unk_RwMatrix    field_0x54;     // identity after construction
    u8              field_0x94;     // cleared by the constructor
    u8              field_0x95;
    u8              field_0x96;
    u8              field_0x97;
};

#endif
