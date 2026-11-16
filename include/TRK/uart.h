#ifndef TRK_UART_H
#define TRK_UART_H

#include <revolution/types.h>
#include <revolution/euart.h>

#ifdef __cplusplus
extern "C" {
#endif

/* UARTError is shared with the SDK EUART transport (revolution/euart.h). */

/* Returns nonzero while input is waiting on the UART. */
int TRKPollUART(void);
UARTError TRKReadUARTN(void* bytes, u32 length);
UARTError TRKWriteUARTN(const void* bytes, u32 length);

/* External-interrupt handler for the UART, called from the first-level interrupt handler. */
void TRKUARTInterruptHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* TRK_UART_H */
