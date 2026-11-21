// Unit name from the class-name string "CCharacter" (lbl_803E2BA4), which this unit's static initialiser
// registers. The unit holds the statics of that class and the destructor of its list object.
// Parked (NonMatching): both functions match, but the object's .text and .data order differ from
// retail (the static initialiser is emitted after the inline destructor, and the class-name string
// after the vtable), so the unit does not pass the gate.

#include <game/CCharacter.h>
#include <climax/AttributeHandler.h>

// Statics of this unit. Their constructors and destructor registrations run in the static
// initialiser __sinit_\CCharacter_cpp (0x800436D8), which the compiler generates from these
// definitions in declaration order.
unk_803E2BB0            lbl_80469B78;
unk_801A92A8            lbl_80529330;
unk_801A92A8            lbl_80529338;
u32                     lbl_80529340 = fn_801A7A84( "CCharacter" );
