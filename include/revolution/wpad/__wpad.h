#ifndef REVOLUTION_WPAD_PRIVATE_H
#define REVOLUTION_WPAD_PRIVATE_H

// Internal declarations of the Wii Remote library (WPAD). The public interface is <revolution/wpad.h>.
// Reference: zeldaret/tp libs/revolution/src/wpad/__wpad.h (the retail 2009 SDK differs; extended as units are rebuilt).

#include <revolution/types.h>
#include <revolution/os.h>
#include <revolution/wpad.h>

#ifdef __cplusplus
extern "C" {
#endif


// Wiimote extension register addresses: the I2C device select in bits 16..23 and bit 26 for "register" access.
#define WPAD_EXT_REG_SPEAKER      0xA2
#define WPAD_EXT_REG_EXTENSION    0xA4
#define WPAD_EXT_REG_MOTION_PLUS  0xA6
#define WPAD_EXT_REG_DPD          0xB0

#define WM_EXT_REG_ADDR(type_, addr_)     (((addr_) & 0xFFFF) | ((WPAD_EXT_REG_##type_) << 16) | (1 << 26))

// Controller memory addresses (the 16-bit address of a read/write data report).
#define WM_MEM_ADDR(addr_)               ((addr_) & 0xFFFF)
#define WM_ADDR_MEM_DEV_CONFIG_0         WM_MEM_ADDR(0x0000)
#define WM_ADDR_MEM_GAME_INFO_0          WM_MEM_ADDR(0x002A)
#define WM_ADDR_MEM_GAME_INFO_1          WM_MEM_ADDR(0x0062)
#define WM_ADDR_MEM_176C                 WM_MEM_ADDR(0x176C)
#define WM_ADDR_MEM_1770                 WM_MEM_ADDR(0x1770)

// Extension register addresses.
#define WM_REG_SPEAKER_01        WM_EXT_REG_ADDR(SPEAKER, 0x01)
#define WM_REG_SPEAKER_08        WM_EXT_REG_ADDR(SPEAKER, 0x08)
#define WM_REG_SPEAKER_09        WM_EXT_REG_ADDR(SPEAKER, 0x09)
#define WM_REG_DPD_CONFIG_BLOCK_1  WM_EXT_REG_ADDR(DPD, 0x00)
#define WM_REG_DPD_CONFIG_BLOCK_2  WM_EXT_REG_ADDR(DPD, 0x1A)
#define WM_REG_DPD_30            WM_EXT_REG_ADDR(DPD, 0x30)
#define WM_REG_DPD_DATA_FORMAT   WM_EXT_REG_ADDR(DPD, 0x33)
#define WM_REG_EXTENSION_CONFIG          WM_EXT_REG_ADDR(EXTENSION, 0x20)
#define WM_REG_EXTENSION_40              WM_EXT_REG_ADDR(EXTENSION, 0x40)
#define WM_REG_EXTENSION_46              WM_EXT_REG_ADDR(EXTENSION, 0x46)
#define WM_REG_EXTENSION_4C              WM_EXT_REG_ADDR(EXTENSION, 0x4C)
#define WM_REG_EXTENSION_F0              WM_EXT_REG_ADDR(EXTENSION, 0xF0)
#define WM_REG_EXTENSION_CERT_CHALLENGE  WM_EXT_REG_ADDR(EXTENSION, 0xF1)
#define WM_REG_EXTENSION_FA              WM_EXT_REG_ADDR(EXTENSION, 0xFA)
#define WM_REG_EXTENSION_FB              WM_EXT_REG_ADDR(EXTENSION, 0xFB)

// HID output report ids and the flag bits of the first data byte of an output report.
#define RPTID_SET_RUMBLE        0x10
#define RPTID_SET_PORT          0x11
#define RPTID_SET_DATA_REPORT_MODE 0x12
#define RPTID_ENABLE_DPD        0x13
#define RPTID_ENABLE_SPEAKER    0x14
#define RPTID_REQUEST_STATUS    0x15
#define RPTID_WRITE_DATA        0x16
#define RPTID_READ_DATA         0x17
#define RPTID_SEND_SPEAKER_DATA 0x18
#define RPTID_MUTE_SPEAKER      0x19
#define RPTID_SEND_DPD_CSB      0x1A

// Input report ids that carry the data of a data format (the id the controller is asked to send).
#define RPTID_DATA_BTN                 0x30
#define RPTID_DATA_BTN_ACC             0x31
#define RPTID_DATA_BTN_EXT8            0x32
#define RPTID_DATA_BTN_ACC_DPD12       0x33
#define RPTID_DATA_BTN_EXT19           0x34
#define RPTID_DATA_BTN_ACC_EXT16       0x35
#define RPTID_DATA_BTN_ACC_DPD10_EXT9  0x37
#define RPTID_DATA_EXT21               0x3D
#define RPTID_DATA_BTN_ACC_DPD18_1     0x3E

// Payload sizes and field offsets of the output reports the library builds (payload = bytes after the report id).
// Size of the speaker configuration block written to the speaker registers.
#define SPK_CONFIG_SIZE         7

// Sizes of the two pointer sensor sensitivity blocks.
#define DPD_CONFIG1_SIZE        9
#define DPD_CONFIG2_SIZE        2

#define RPT_MAX_SIZE            21
#define RPT11_SIZE              1
#define RPT12_SIZE              2
#define RPT13_SIZE              1
#define RPT14_SIZE              1
#define RPT15_SIZE              1
#define RPT16_SIZE              RPT_MAX_SIZE
#define RPT17_SIZE              6
#define RPT18_SIZE              RPT_MAX_SIZE
#define RPT19_SIZE              1
#define RPT1A_SIZE              1
#define RPT16_DATA_DST_ADDRESS  0
#define RPT16_DATA_LENGTH       4
#define RPT16_DATA              5
#define RPT18_DATA_LENGTH       0
#define RPT18_DATA             1
#define RPT17_DATA_SRC_ADDRESS  0
#define RPT17_DATA_LENGTH       4

#define RPT_OUT_FLAGS             1
#define RPT_OUT_FLAG_REQUEST_ACK_RPT 1
#define RPT_OUT_FLAG_RUMBLE       0

// Range of the pointer sensor sensitivity setting.
#define WPAD_MIN_DPD_SENS 1
#define WPAD_MAX_DPD_SENS 5

// Physical address of the byte where the system software publishes the WiFi channel in use.
#define OS_PHYS_WIFI_AFH_CHANNEL 0x000031A2

// Report and queue sizes of the 2009 control block: 24 standard and 12 extension queue slots (the extension list
// ends at 0x838, where the WPADInfo of the block starts).
#define WPAD_COMMAND_CMD_MAX_LEN 24
#define WPAD_COMMAND_EXT_MAX_LEN 12

// Size in bytes of one receive buffer slot (WPADRxStatus).
#define WPAD_RX_DATASIZE 96

// Noise filters of the change detector: one pair of counters (differing / equal samples in a row) each.
typedef enum WPADNZFilter {
    WPAD_NZFILTER_ACC,
    WPAD_NZFILTER_DPD,
    WPAD_NZFILTER_EXT,
    WPAD_MAX_NZFILTERS
} WPADNZFilter;

typedef struct WPADInfo {
    /* 0x00 */ BOOL dpd;
    /* 0x04 */ BOOL speaker;
    /* 0x08 */ BOOL attach;
    /* 0x0C */ BOOL lowBat;
    /* 0x10 */ BOOL nearempty;
    /* 0x14 */ u8 battery;
    /* 0x15 */ u8 led;
    /* 0x16 */ u8 protocol;
    /* 0x17 */ u8 firmware;
} WPADInfo;

// One queued HID output report.
typedef struct WPADCommand {
    /* 0x00 */ u32 reportId;
    /* 0x04 */ u8 data[22];
    /* 0x1A */ u16 length;
    /* 0x1C */ void* readBuf;
    /* 0x20 */ u16 readLength;
    /* 0x24 */ u32 readAddress;
    /* 0x28 */ WPADInfo* info;
    /* 0x2C */ WPADCallback callback;
} WPADCommand;

// Ring buffer of WPADCommand; head and tail are slot indices.
typedef struct WPADCmdQueue {
    /* 0x00 */ s8 head;
    /* 0x01 */ s8 tail;
    /* 0x04 */ WPADCommand* commands;
    /* 0x08 */ u32 capacity;
} WPADCmdQueue;

// Game information read back from the controller's memory.
typedef struct WPADMEMGameInfo {
    /* 0x00 */ OSTime calendarTimeTick;
    /* 0x08 */ u16 gameTitle[17];
    /* 0x2A */ char gameId[4];
    /* 0x2E */ u8 gameType;
    /* 0x2F */ u8 checkSum;
    /* 0x30 */ u8 reserved[8];
} WPADMEMGameInfo;

// A sample slot of the receive buffers: the largest status type (0x60 bytes).
typedef struct WPADRxStatus {
    /* 0x00 */ WPADStatus base;
    /* 0x2A */ u8 field_0x2A[0x60 - 0x2A];
} WPADRxStatus;

typedef struct WPADDevConfig {
    /* 0x00 */ DPDObject obj[WPAD_DPD_MAX_OBJECTS];
    /* 0x20 */ WPADAccGravityUnit acc0g;
    /* 0x26 */ WPADAccGravityUnit acc1g;
    /* 0x2C */ u8 motor;
    /* 0x2D */ u8 volume;
} WPADDevConfig;

typedef struct WPADStick {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 xMin;
    /* 0x04 */ s16 xMax;
    /* 0x06 */ s16 y;
    /* 0x08 */ s16 yMin;
    /* 0x0A */ s16 yMax;
} WPADStick;

typedef struct WPADFsConfig {
    /* 0x00 */ WPADStick stick;
    /* 0x0C */ WPADAccGravityUnit acc0g;
    /* 0x12 */ WPADAccGravityUnit acc1g;
} WPADFsConfig;

typedef struct WPADClConfig {
    /* 0x00 */ WPADStick lstk;
    /* 0x0C */ WPADStick rstk;
    /* 0x18 */ u8 triggerL;
    /* 0x19 */ u8 triggerR;
} WPADClConfig;

typedef union WPADExtConfig {
    WPADFsConfig fs;
    WPADClConfig cl;
} WPADExtConfig;

// Pending memory/register access of a channel (tp's WPADMEMControlBlock, kept inside the control block here).
typedef struct WPADMemBlock {
    /* 0x00 */ BOOL used;
    /* 0x04 */ u8* buf;
    /* 0x08 */ u16 length;
    /* 0x0C */ u32 address;
    /* 0x10 */ WPADCallback callback;
} WPADMemBlock;

// Control block of one channel, 0x9C0 bytes (retail layout of the Jun 2009 SDK; neither tp's nor ogws's matches).
// Members named field_0x... are accessed but their purpose is not established yet. Names follow tp and ogws
// (doldecomp/ogws WPADInternal.h has the same layout up to 0x988).
typedef struct WPADControlBlock {
    /* 0x000 */ WPADMEMGameInfo gameInfo;
    /* 0x038 */ s32 gameInfoErr[2];
    /* 0x040 */ WPADRxStatus prBuf[1];
    /* 0x0A0 */ WPADRxStatus rxBufs[2];
    /* 0x160 */ WPADCmdQueue stdCmdQueue;
    /* 0x16C */ WPADCommand stdCmdQueueList[WPAD_COMMAND_CMD_MAX_LEN];
    /* 0x5EC */ WPADCmdQueue extCmdQueue;
    /* 0x5F8 */ WPADCommand extCmdQueueList[WPAD_COMMAND_EXT_MAX_LEN];
    /* 0x838 */ WPADInfo info;
    /* 0x850 */ WPADInfo* infoOut;
    /* 0x854 */ WPADDevConfig devConf;
    /* 0x882 */ WPADExtConfig extConf;
    /* 0x89C */ WPADCallback cmdBlkCallback;
    /* 0x8A0 */ WPADExtensionCallback extensionCallback;
    /* 0x8A4 */ WPADConnectCallback connectCallback;
    /* 0x8A8 */ WPADSamplingCallback samplingCallback;
    /* 0x8AC */ void* samplingBuf;
    /* 0x8B0 */ u32 samplingBufIndex;
    /* 0x8B4 */ u32 samplingBufLength;
    /* 0x8B8 */ u32 dataFormat;
    /* 0x8BC */ s32 status;
    /* 0x8C0 */ u8 statusReqBusy;
    /* 0x8C1 */ u8 devType;
    /* 0x8C2 */ u8 devMode;
    /* 0x8C3 */ s8 devHandle;
    /* 0x8C4 */ s32 field_0x8C4;
    /* 0x8C8 */ u8 rxBufIndex;
    /* 0x8C9 */ u8 field_0x8C9;
    /* 0x8CA */ u8 defaultDpdSize;
    /* 0x8CB */ u8 currentDpdCommand;
    /* 0x8CC */ u8 pendingDpdCommand;
    /* 0x8CD */ u8 radioQuality;
    /* 0x8CE */ u8 radioQualityOkMs;
    /* 0x8CF */ u8 audioFrames;
    /* 0x8D0 */ BOOL motorBusy;
    /* 0x8D4 */ BOOL motorRunning;
    /* 0x8D8 */ BOOL used;
    /* 0x8DC */ BOOL handshakeFinished;
    /* 0x8E0 */ BOOL oldFw;
    /* 0x8E4 */ OSThreadQueue threadQueue;
    /* 0x8F0 */ OSTime lastUpdateTime;
    /* 0x8F8 */ u16 filterDiff[WPAD_MAX_NZFILTERS];
    /* 0x8FE */ u16 filterSame[WPAD_MAX_NZFILTERS];
    /* 0x908 */ OSTime lastReportSendTime;
    /* 0x910 */ u8 cmdTimeoutAction;
    /* 0x911 */ u8 calibrated;
    /* 0x912 */ u16 recalibrateCount;
    /* 0x914 */ u8 key[16];
    /* 0x924 */ u8 ft[8];
    /* 0x92C */ u8 sb[8];
    /* 0x934 */ u8 wmReadDataBuf[64];
    /* 0x974 */ void* wmReadDataPtr;
    /* 0x978 */ u32 wmReadAddr;
    /* 0x97C */ s32 wmReadErr;
    /* 0x980 */ u16 wmReadLength;
    /* 0x982 */ u8 keyIdx;
    /* 0x983 */ u8 radioSensitivity;
    /* 0x984 */ u16 packetCnt;
    /* 0x986 */ u8 disconnect;
    /* 0x987 */ u8 lastReportId;
    /* 0x988 */ WPADCallback getInfoCB;
    /* 0x98C */ u8 getInfoBusy;
    /* 0x98D */ u8 extState;
    /* 0x98E */ u8 savePower;
    /* 0x98F */ u8 blcBattery;
    /* 0x990 */ u8 savedDevType;
    /* 0x991 */ u8 extWasDisconnected;
    /* 0x992 */ s16 reconnectExtMs;
    /* 0x994 */ u8 dpdBusy;
    /* 0x998 */ WPADMemBlock memBlock;
    /* 0x9AC */ u8 field_0x9AC[0x9C0 - 0x9AC];
} __attribute__((aligned(32))) WPADControlBlock;

// The per-channel control blocks (WPAD.c): _wpdcb[chan] points at _wpd[chan].
extern WPADControlBlock _wpd[WPAD_MAX_CONTROLLERS];
extern s8 _dev_handle_index[16];
extern WPADControlBlock* _wpdcb[WPAD_MAX_CONTROLLERS];

// WPAD.c: asks a controller for its status (battery, extension, LEDs) and stores it in `info`.
s32 WPADGetInfoAsync(s32 chan, WPADInfo* info, WPADCallback callback);

// WPAD.c: queue a write of `len` bytes to a controller address.
BOOL WPADiSendWriteData(WPADCmdQueue* queue, void* buf, u16 len, u32 addr, WPADCallback callback);

// WPAD.c: queue a write of one byte to a controller address, a read of `len` bytes from one, and the receive-side
// helpers WPADiHIDParser runs after every parsed report.
BOOL WPADiSendWriteDataCmd(WPADCmdQueue* queue, u8 cmd, u32 addr, WPADCallback callback);
BOOL WPADiSendReadData(WPADCmdQueue* queue, void* dst, u16 len, u32 addr, WPADCallback callback);
void WPADiExcludeButton(s32 chan);
void WPADiCopyOut(s32 chan);

// WPAD.c: extension families the application enabled (sbss flags set by the WPADEnable... calls).
BOOL WPADIsEnabledVSM(void);
BOOL WPADIsEnabledTRN(void);
BOOL WPADIsEnabledGTR(void);
BOOL WPADIsEnabledDRM(void);

// WPADHIDParser.c: the HID input report dispatcher.
s32 WPADiHIDParser(u8 chan, u8* report);
void WPADiRecvCallback(u8 devHandle, u8* report, u16 length);
void WPADiClearQueue(WPADCmdQueue* queue);
BOOL WPADiSendSetReportType(WPADCmdQueue* queue, u32 format, u8 savePower, WPADCallback callback);

// WPADEncrypt.c: extension key generation (first- and third-party) and decoding of encrypted extension data.
void WPADiCreateKey(s32 chan);
void WPADiCreateKeyFor3rd(s32 chan);
void WPADiDecode(s32 chan, void* buf, u16 len, u16 offset);

// WPADMem.c
void WPADiClearMemBlock(s32 chan);
s32 WPADWriteExtReg(s32 chan, void* buf, u16 len, u16 addr, WPADCallback callback);

// wpad_debug_msg.c: the release build keeps the call sites of the debug printer and drops its body.
void DEBUGPrint(const char* fmt, ...);

// WPAD.c: default entry points of the Balance Board (WBC) hooks. Each reports its own name through DEBUGPrint and
// returns -1 (no controller).
// Parameters are read from the KPAD call sites (the bodies ignore them).
s32 WBCReadDummy(WPADBLStatus* bl, f64* cells, s32 count);
s32 WBCSetZEROPointDummy(f64* zero_point, s32 count);
s32 WBCGetTGCWeightDummy(f64 weight, f64* tgc_weight, WPADBLStatus* bl);

#ifdef __cplusplus
}
#endif

#endif
