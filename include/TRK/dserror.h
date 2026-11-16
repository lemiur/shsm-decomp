// Reference: zeldaret/tp libs/PowerPC_EABI_Support/MetroTRK/trk.h

#ifndef TRK_DSERROR_H
#define TRK_DSERROR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Status codes returned by the MetroTRK nub. */
typedef enum DSError {
    DS_NoError = 0x0000,
    DS_StepError = 0x0001,
    DS_ParameterError = 0x0002,

    DS_EventQueueFull = 0x0100,

    DS_NoMessageBufferAvailable = 0x0300,
    DS_MessageBufferOverflow = 0x0301,
    DS_MessageBufferReadError = 0x0302,

    DS_DispatchError = 0x0500,

    DS_InvalidMemory = 0x0700,
    DS_InvalidRegister = 0x0701,
    DS_CWDSException = 0x0702,
    DS_UnsupportedError = 0x0703,
    DS_InvalidProcessID = 0x0704,
    DS_InvalidThreadID = 0x0705,
    DS_OSError = 0x0706,

    DS_Error800 = 0x0800
} DSError;

#ifdef __cplusplus
}
#endif

#endif /* TRK_DSERROR_H */
