#ifndef GAME_CCOLLIDABLESCENEOBJECT_H
#define GAME_CCOLLIDABLESCENEOBJECT_H

#include <game/CSceneObject.h>
#include <climax/EventHandler.h>

/*
 * A scene object that answers the messages sent to its id. Derived from CSceneObject. Registered
 * as "CCollidableSceneObject" (class-name string in the DOL), vtable lbl_803E8FD0. Member names
 * invented.
 */
class CCollidableSceneObject : public CSceneObject {
public:
    unk_801A92A8       id;             // 0x148, the message id this object answers
};

#endif
