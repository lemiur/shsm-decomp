#ifndef REVOLUTION_WUD_H
#define REVOLUTION_WUD_H

#include <revolution/bte.h>
#include <revolution/sc.h>
#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Wii Remote Bluetooth driver (public interface).
 * Reference: zeldaret/tp libs/revolution/include/revolution/wud.h (via wpad/wud.h)
 */

#define WUD_MAX_DEV_ENTRY_FOR_STD 10
#define WUD_MAX_DEV_ENTRY_FOR_SMP 6
#define WUD_MAX_DEV_ENTRY (WUD_MAX_DEV_ENTRY_FOR_STD + WUD_MAX_DEV_ENTRY_FOR_SMP)
#define WUD_DEV_HANDLE_INVALID (-1)

typedef enum {
    WUD_LIB_STATUS_0,
    WUD_LIB_STATUS_1,
    WUD_LIB_STATUS_2,
    WUD_LIB_STATUS_3,
    WUD_LIB_STATUS_4,
    WUD_LIB_STATUS_5,
} WUDLibStatus;

typedef enum {
    WUD_CHAN0,
    WUD_CHAN1,
    WUD_CHAN2,
    WUD_CHAN3,
    WUD_MAX_CHANNELS,
    WUD_CHAN_INVALID = -1
} WUDChannel;

typedef void* (*WUDAllocFunc)(u32 size);
typedef BOOL (*WUDFreeFunc)(void* pBlock);
typedef void (*WUDSyncDeviceCallback)(s32 result, s32 num);
typedef void (*WUDClearDeviceCallback)(s32 result);
typedef struct WUDDevInfo WUDDevInfo;
typedef void (*WUDHidConnCallback)(WUDDevInfo* devInfo, u8 open);
typedef void (*WUDHidRecvCallback)(UINT8 devHandle, UINT8* pReport, UINT16 len);

/* One paired-device record (0x60 bytes). `conf` is the SC settings entry. */
struct WUDDevInfo {
    /* 0x00 */ SCDevInfo conf;
    /* 0x40 */ BD_ADDR devAddr;
    /* 0x46 */ LINK_KEY linkKey;
    /* 0x56 */ UINT8 devHandle;
    /* 0x57 */ UINT8 subclass;
    /* 0x58 */ UINT8 appID;
    /* 0x59 */ u8 status;
    /* 0x5A */ s8 unk_0x5A;
    /* 0x5B */ u8 sync_type;
    /* 0x5C */ u8 unk_0x5C;
    /* 0x5D */ u8 unk_0x5D[1];
    /* 0x5E */ tBTA_HH_ATTR_MASK hhAttrMask;
};

BOOL WUDInit(void);
BOOL WUDIsBusy(void);
BOOL WUDIsLinkedWBC(void);
void WUDRegisterAllocator(WUDAllocFunc pAllocFunc, WUDFreeFunc pFreeFunc);
void* App_MEMalloc(u32 size);
BOOL App_MEMfree(void* pBlock);
u32 WUDGetAllocatedMemSize(void);
void WUDShutdown(BOOL saveSimpleDevs);
WUDLibStatus WUDGetStatus(void);
u8 WUDGetBufferStatus(void);
void WUDSetSniffMode(BD_ADDR addr, s32 interval);
WUDSyncDeviceCallback WUDSetSyncSimpleCallback(WUDSyncDeviceCallback pCallback);
WUDClearDeviceCallback WUDSetClearDeviceCallback(WUDClearDeviceCallback pCallback);
BOOL WUDStartSyncDevice(void);
BOOL WUDStartSyncSimple(void);
BOOL WUDStartFastSyncSimple(void);
BOOL WUDStopSyncSimple(void);
BOOL WUDCancelSyncDevice(void);
BOOL WUDStartClearDevice(void);
BOOL WUDSetDisableChannel(s8 afhChannel);
WUDHidRecvCallback WUDSetHidRecvCallback(WUDHidRecvCallback pCallback);
WUDHidConnCallback WUDSetHidConnCallback(WUDHidConnCallback pCallback);
void WUDSetVisibility(u8 disc, u8 conn);
void WUDUpdateSCSetting(void);
void WUDSetDeviceHistory(WUDChannel channel, const BD_ADDR addr);
BOOL WUDIsLatestDevice(WUDChannel channel, const BD_ADDR addr);
u8 WUDGetConnectable(void);

void WUDiGetFirmwareVersion(void);
void WUDiInitSub(void);
void WUDiEnableStack(void);
void WUDiAutoSync(void);
void WUDiDeleteAllLinkKeys(void);
void WUDiRegisterDevice(BD_ADDR addr);
void WUDiRemoveDevice(BD_ADDR addr);
WUDDevInfo* WUDiGetDevInfo(BD_ADDR addr);
WUDDevInfo* WUDiGetDevInfoIndex(int idx);
WUDDevInfo* WUDiGetNewDevInfo(void);
void WUDiRemoveDevInfo(BD_ADDR addr);
void WUDiClearDevice(void);
u8 WUDiGetDevNumber(void);
u8 WUDiGetDevSmpNumber(void);
u8 WUDiGetConnNumber(void);
u8 WUDiGetLinkNum(void);
void WUDiMoveTopSmpDevInfoPtr(WUDDevInfo* pInfo);
void WUDiMoveBottomSmpDevInfoPtr(WUDDevInfo* pInfo);
WUDDevInfo* WUDiGetRemoveSmpDevice(void);
void WUDiMoveTopOfDisconnectedSmpDevice(WUDDevInfo* pInfo);
void WUDiMoveTopStdDevInfoPtr(WUDDevInfo* pInfo);
void WUDiMoveBottomStdDevInfoPtr(WUDDevInfo* pInfo);
WUDDevInfo* WUDiGetRemoveStdDevice(void);
void WUDiMoveTopOfDisconnectedStdDevice(WUDDevInfo* pInfo);

BD_ADDR_PTR _WUDGetDevAddr(UINT8 handle);
u16 _WUDGetQueuedSize(s8 dev_handle);
u16 _WUDGetNotAckedSize(s8 dev_handle);
u8 _WUDGetLinkNumber(void);

#ifdef __cplusplus
}
#endif

#endif
