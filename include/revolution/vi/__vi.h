#ifndef REVOLUTION_VI_INTERNAL_H
#define REVOLUTION_VI_INTERNAL_H

#include <revolution/vi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* i2c.c */
void WaitMicroTime(s32 usec);
int __VISendI2CData(u8 slaveAddr, u8* pData, int nBytes);

/* vi.c */
void __VIInit(VITVMode mode);
void __VIDisplayPositionToXY(u32 hct, u32 vct, s16* x, s16* y);
void __VIGetCurrentPosition(s16* x, s16* y);
BOOL __VIResetSIIdle(void);
BOOL __VIResetRFIdle(void);
BOOL __VIResetDev0Idle(void);

/* vi3in1.c */
void __VISetFilter4EURGB60(VIBool enable);
void __VISetGamma1_0(void);
void __VISetYUVSEL(VIBool outsel);
void __VISetCGMS(void);
void __VISetWSS(void);
void __VISetClosedCaption(void);
void __VISetMacrovision(void);
void __VISetGamma(void);
void __VISetTrapFilter(void);
void __VISetRGBOverDrive(void);
void __VISetRevolutionModeSimple(void);
void __VISetVolume(u8 volumeL, u8 volumeR);
void __VISetTiming(VITiming timing);
void __VISetVBICtrl(VIBool cgms, VIBool wss, VIBool cc);
void __VISetCGMSClear(void);
void __VISetMacrovisionImm(VIMacroVisionObj* mparam);
void __VISetGammaImm(VIGammaObj* gamma);
void __VISetTrapFilterImm(VIBool filter);
extern volatile u32 Vdac_Flag_Changed; // volatile: SDK declaration (zeldaret/tp vi3in1.c); the retrace handler in vi.c polls it

#ifdef __cplusplus
}
#endif

#endif
