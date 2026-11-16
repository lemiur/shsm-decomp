// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Os/dolphin/target_options.c

#include "TRK/target_options.h"

/* Whether console and file output go through the serial link. */
static u8 useSerialIO;

/* Selects whether console and file output go through the serial link. */
void SetUseSerialIO(u8 serialIO) {
    useSerialIO = serialIO;
} // 0x80195D18

/* Returns whether console and file output go through the serial link. */
u8 GetUseSerialIO(void) {
    return useSerialIO;
} // 0x80195D20
