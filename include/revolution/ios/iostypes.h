// Reference: zeldaret/tp libs/revolution/include/revolution/private/iostypes.h
//
// IOS crypto (IOSC) types: the subset the ES ticket structures in revolution/esp.h need.
#ifndef REVOLUTION_IOS_IOSTYPES_H
#define REVOLUTION_IOS_IOSTYPES_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef s32 IOSError;
typedef s32 IOSFd;
typedef s32 IOSHeapId;

typedef u8 IOSCHash[20];
typedef u8 IOSCEccPublicKey[60];
typedef u8 IOSCAesKey[16];
typedef u8 IOSCRsaSig2048[256];
typedef u8 IOSCSigDummy[60];
typedef u8 IOSCName[64];

typedef enum {
    IOSC_SIG_RSA4096 = 0x00010000,
    IOSC_SIG_RSA2048,
    IOSC_SIG_ECC
} IOSCCertSigType;

typedef struct {
    IOSCCertSigType sigType;
    IOSCRsaSig2048 sig;
    IOSCSigDummy dummy;
    IOSCName issuer;
} IOSCSigRsa2048;

#ifdef __cplusplus
}
#endif

#endif
