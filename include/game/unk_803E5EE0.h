#ifndef GAME_UNK_803E5EE0_H
#define GAME_UNK_803E5EE0_H

#include <game/CBaseBehaviour.h>

// The global the constructor of unk_803E5EE0 stores the new behaviour into (its readers are not yet
// recovered; defined in src/game/CPlayer.cpp).
extern CBaseBehaviour* lbl_80529568;

/*
 * A behaviour that adds no fields to CBaseBehaviour (vtable lbl_803E5EE0, size 0x34, the size of
 * CBaseBehaviour). Its constructor is CBaseBehaviour's, then its own vptr, then the store of the
 * object into lbl_80529568. Name from the vtable address (invented); its overrides are not yet named.
 */
class unk_803E5EE0 : public CBaseBehaviour {
public:
                    unk_803E5EE0();
};

inline unk_803E5EE0::unk_803E5EE0() {
    lbl_80529568 = this;
}

#endif
