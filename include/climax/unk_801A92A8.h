#ifndef CLIMAX_UNK_801A92A8_H
#define CLIMAX_UNK_801A92A8_H

#include <revolution/types.h>

struct unk_EventRecord;

/*
 * An 8-byte message identifier of the Climax event framework (src/climax/EventHandler.cpp).
 * Game units define one global per message and register it by name at start-up
 * (fn_801A93B4 takes the object, its name string and a payload type string such as
 * "RumbleStartData*"); handlers then link to it. The class name is unknown (no string
 * names it): placeholder; member names invented.
 */
class unk_801A92A8 {
public:
            unk_801A92A8();
            ~unk_801A92A8();

    void    fn_801A92FC( unk_EventRecord* record );
    unk_EventRecord*    GetRecord() const { return record; }   // name invented
    operator unk_EventRecord*() const { return record; }       // Gormiti DWARF: RWS::CEventId::operator RWS::CRegisteredMsgs*() const, CEventHandler.h:461
    unk_EventRecord*    operator*() const { return record; }   // Gormiti DWARF: RWS::CEventId::operator*() const, CEventHandler.h:554

    unk_EventRecord*    record;         // 0x0, the registered message; lbl_8052A530.record when unregistered
    s16                 links;          // 0x4, handler links made through this id
    s16                 registrations;  // 0x6, registrations of the name through this id
};

#endif
