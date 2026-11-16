#ifndef REVOLUTION_EUART_H
#define REVOLUTION_EUART_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Result of a UART write, as the debugger transport (MetroTRK) tests it. */
typedef enum UARTError {
    kUARTNoError = 0,
    kUARTUnknownBaudRate = 1,
    kUARTConfigurationError = 2,
    kUARTBufferOverflow = 3,
    kUARTNoData = 4
} UARTError;

typedef enum UARTBaudRate {
    kBaudHWFlow = 0,
    kBaud300 = 300,
    kBaud600 = 600,
    kBaud1200 = 1200,
    kBaud1800 = 1800,
    kBaud2000 = 2000,
    kBaud2400 = 2400,
    kBaud3600 = 3600,
    kBaud4800 = 4800,
    kBaud7200 = 7200,
    kBaud9600 = 9600,
    kBaud14400 = 14400,
    kBaud19200 = 19200,
    kBaud28800 = 28800,
    kBaud38400 = 38400,
    kBaud57600 = 57600,
    kBaud115200 = 115200,
    kBaud230400 = 230400
} UARTBaudRate;

/* Last error recorded by EUARTInit and WriteUARTN. */
typedef enum EUARTError {
    EUART_ERROR_NONE = 0,
    EUART_ERROR_UNINITIALIZED = 1,
    EUART_ERROR_CANNOT_USE = 2,
    EUART_ERROR_INTERNAL = 5
} EUARTError;

/* EXI clock selection of the EUART adapter (4, EXI_FREQ_16M): a .sdata2 constant ahead of the OS library's, not
   euart's own. */
extern const u32 __EXIFreq;

BOOL EUARTInit(void);
UARTError InitializeUART(UARTBaudRate rate);
UARTError WriteUARTN(const void* buf, unsigned long len);

#ifdef __cplusplus
}
#endif

#endif
