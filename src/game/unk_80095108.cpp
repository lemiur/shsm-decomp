// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.
// Stays NonMatching: the start-up function is exact, but the built .bss ends 8 bytes short of retail
// (the unit's handle is followed by 8 bytes of retail .bss before lbl_8046A700 that no source names).

#include <game/unk_80095108.h>

// Compiler-generated, no source of its own: __sinit_\unk_80095108_cpp. It constructs the handle at
// priority 1.5 and registers its destructor with __register_global_object.
// 0x80095108
RenderState::cTevCBHandle lbl_8046A6E8( 1.5f );
