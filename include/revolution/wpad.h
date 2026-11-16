#ifndef REVOLUTION_WPAD_H
#define REVOLUTION_WPAD_H

// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/revolution/wpad/WPAD.h
// Reference: ThePlayerRolo/sdk_2009-12-11 include/revolution/wpad.h

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wii Remote library. Only what the KPAD library uses is declared so far: the sampled controller status types,
 * the device and data-format codes and the controller-state calls. */

#define WPAD_BUTTON_LEFT        (1 <<  0)
#define WPAD_BUTTON_RIGHT       (1 <<  1)
#define WPAD_BUTTON_DOWN        (1 <<  2)
#define WPAD_BUTTON_UP          (1 <<  3)
#define WPAD_BUTTON_PLUS        (1 <<  4)
#define WPAD_BUTTON_2           (1 <<  8)
#define WPAD_BUTTON_1           (1 <<  9)
#define WPAD_BUTTON_B           (1 << 10)
#define WPAD_BUTTON_A           (1 << 11)
#define WPAD_BUTTON_MINUS       (1 << 12)
#define WPAD_BUTTON_Z           (1 << 13)
#define WPAD_BUTTON_C           (1 << 14)
#define WPAD_BUTTON_HOME        (1 << 15)

#define WPAD_CL_BUTTON_UP       (1 <<  0)
#define WPAD_CL_BUTTON_LEFT     (1 <<  1)
#define WPAD_CL_TRIGGER_ZR      (1 <<  2)
#define WPAD_CL_BUTTON_X        (1 <<  3)
#define WPAD_CL_BUTTON_A        (1 <<  4)
#define WPAD_CL_BUTTON_Y        (1 <<  5)
#define WPAD_CL_BUTTON_B        (1 <<  6)
#define WPAD_CL_TRIGGER_ZL      (1 <<  7)
#define WPAD_CL_RESERVED        (1 <<  8)
#define WPAD_CL_TRIGGER_R       (1 <<  9)
#define WPAD_CL_BUTTON_PLUS     (1 << 10)
#define WPAD_CL_BUTTON_HOME     (1 << 11)
#define WPAD_CL_BUTTON_MINUS    (1 << 12)
#define WPAD_CL_TRIGGER_L       (1 << 13)
#define WPAD_CL_BUTTON_DOWN     (1 << 14)
#define WPAD_CL_BUTTON_RIGHT    (1 << 15)

#define WPAD_DEV_CORE               0
#define WPAD_DEV_FS                 1
#define WPAD_DEV_CLASSIC            2
#define WPAD_DEV_BALANCE_CHECKER    3
#define WPAD_DEV_VSM                4
#define WPAD_DEV_MOTION_PLUS        5
#define WPAD_DEV_MPLS_PT_FS         6
#define WPAD_DEV_MPLS_PT_CLASSIC    7
#define WPAD_DEV_TRAIN              16
#define WPAD_DEV_GUITAR             17
#define WPAD_DEV_DRUM               18
#define WPAD_DEV_TAIKO              19
#define WPAD_DEV_TURNTABLE          20

#define WPAD_DEV_MPLS_PT_FUTURE     250
#define WPAD_DEV_FUTURE             251
#define WPAD_DEV_NOT_SUPPORTED      252
#define WPAD_DEV_NONE               253
#define WPAD_DEV_INITIALIZING       255

enum WPADResult_et {
    WPAD_ERR_OK = 0,

    WPAD_ERR_NO_CONTROLLER  = -1,
    WPAD_ERR_BUSY           = -2,
    WPAD_ERR_TRANSFER       = -3,
    WPAD_ERR_INVALID        = -4,
    WPAD_ERR_NOPERM         = -5,
    WPAD_ERR_BROKEN         = -6,
    WPAD_ERR_CORRUPTED      = -7,
};

/* The older names of the result codes, as the SDK's own wpad.h spells them for its callers. */
#define WPAD_ESUCCESS   WPAD_ERR_OK
#define WPAD_ENODEV     WPAD_ERR_NO_CONTROLLER
#define WPAD_EBUSY      WPAD_ERR_BUSY
#define WPAD_ETRANSFER  WPAD_ERR_TRANSFER
#define WPAD_EINVAL     WPAD_ERR_INVALID

/* Result codes of the command callbacks, spelled "(code + 0)" as in the SDK's own wpad.h. */
#define WPAD_CESUCCESS  (WPAD_ERR_OK + 0)
#define WPAD_CENODEV    (WPAD_ERR_NO_CONTROLLER + 0)
#define WPAD_CETRANSFER (WPAD_ERR_TRANSFER + 0)

#define WPAD_DPD_MAX_OBJECTS 4
#define WPAD_DPD_IMG_RESO_WX 1024
#define WPAD_DPD_IMG_RESO_WY 768

#define WPAD_STATE_DISABLED     0
#define WPAD_STATE_ENABLING     1
#define WPAD_STATE_ENABLED      2
#define WPAD_STATE_SETUP        3
#define WPAD_STATE_DISABLING    4

#define WPAD_CHAN0          0
#define WPAD_CHAN1          1
#define WPAD_CHAN2          2
#define WPAD_CHAN3          3
#define WPAD_CHAN_INVALID   -1

#define WPAD_MAX_CONTROLLERS 4

#define WPAD_FMT_CORE_BTN              0
#define WPAD_FMT_CORE_BTN_ACC          1
#define WPAD_FMT_CORE_BTN_ACC_DPD      2
#define WPAD_FMT_FS_BTN                3
#define WPAD_FMT_FS_BTN_ACC            4
#define WPAD_FMT_FS_BTN_ACC_DPD        5
#define WPAD_FMT_CLASSIC_BTN           6
#define WPAD_FMT_CLASSIC_BTN_ACC       7
#define WPAD_FMT_CLASSIC_BTN_ACC_DPD   8
#define WPAD_FMT_BTN_ACC_DPD_EXTENDED  9
#define WPAD_FMT_TRAIN                 10
#define WPAD_FMT_GUITAR                11
#define WPAD_FMT_BALANCE_CHECKER       12
#define WPAD_FMT_VSM                   13
#define WPAD_FMT_UNK_14                14 /* sized like VSM in the retail format table; no source names it */
#define WPAD_FMT_DRUM                  15

#define WPAD_VSM_STATUS_SIZE 0x4A

#define WPAD_DPD_DISABLE   0
#define WPAD_DPD_BASIC     1
#define WPAD_DPD_STANDARD  3
#define WPAD_DPD_EXTENDED  5

#define WPAD_BATTERY_LEVEL_CRITICAL 0
#define WPAD_BATTERY_LEVEL_LOW      1
#define WPAD_BATTERY_LEVEL_MEDIUM   2
#define WPAD_BATTERY_LEVEL_HIGH     3
#define WPAD_BATTERY_LEVEL_MAX      4

#define WPAD_MAX_DPD_X (WPAD_DPD_IMG_RESO_WX - 1)
#define WPAD_MAX_DPD_Y (WPAD_DPD_IMG_RESO_WY - 1)

/* Operating mode of an extension, as read from register 0xFE (the Classic Controller's report layout). */
#define WPAD_DEV_MODE_NORMAL            0
#define WPAD_DEV_MODE_CLASSIC_REDUCED   1
#define WPAD_DEV_MODE_CLASSIC_EXTENDED  2
#define WPAD_DEV_MODE_CLASSIC_STANDARD  3

#define WPAD_ACC_GRAVITY_UNIT_CORE 0
#define WPAD_ACC_GRAVITY_UNIT_FS   1

/* Value of WPADGetSensorBarPosition. */
#define WPAD_SENSOR_BAR_BOTTOM 0
#define WPAD_SENSOR_BAR_TOP    1

/* Commands of WPADControlSpeaker. */
#define WPAD_SPEAKER_OFF    0
#define WPAD_SPEAKER_ON     1
#define WPAD_SPEAKER_MUTE   2
#define WPAD_SPEAKER_UNMUTE 3
#define WPAD_SPEAKER_PLAY   4
#define WPAD_SPEAKER_5      5 /* switches the speaker on like WPAD_SPEAKER_ON; no source names it */

#define WPAD_MAX_SPEAKER_VOLUME 127

#define WPAD_RADIO_QUALITY_GOOD 0
#define WPAD_RADIO_QUALITY_BAD  1

#define WPAD_MOTOR_STOP    0
#define WPAD_MOTOR_RUMBLE  1

typedef void (*WPADCallback)(s32 chan, s32 result);
typedef void (*WPADExtensionCallback)(s32 chan, s32 devType);
typedef void (*WPADConnectCallback)(s32 chan, s32 result);
typedef void (*WPADSamplingCallback)(s32 chan);

typedef void* (*WPADAlloc)(u32 size);
typedef BOOL (*WPADFree)(void* ptr);
typedef void (*WPADSyncDeviceCallback)(s32 result, s32 num);

typedef void (*WPADSaveCallback)(u32 result);

typedef s32 WPADChannel;
typedef s32 WPADResult;    /* a WPAD_ERR_* code, as the command callbacks receive it */
typedef u32 WPADDeviceType; /* a WPAD_DEV_* code (see the enum in the SDK's wpad.h) */

typedef struct WPADAcc {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s16 z;
} WPADAcc;

typedef struct DPDObject {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ u16 size;
    /* 0x06 */ u8 traceId;
} DPDObject;

typedef struct WPADStatus {
    /* 0x00 */ u16 button;
    /* 0x02 */ s16 accX;
    /* 0x04 */ s16 accY;
    /* 0x06 */ s16 accZ;
    /* 0x08 */ DPDObject obj[WPAD_DPD_MAX_OBJECTS];
    /* 0x28 */ u8 dev;
    /* 0x29 */ s8 err;
} WPADStatus;

/* Pointer sensor object in the extended format: bounding box, pixel count and radius. */
typedef struct DPDObjEx {
    /* 0x00 */ s16 range_x1;
    /* 0x02 */ s16 range_y1;
    /* 0x04 */ s16 range_x2;
    /* 0x06 */ s16 range_y2;
    /* 0x08 */ u16 pixel;
    /* 0x0A */ s8 radius;
} DPDObjEx;

typedef struct WPADStatusEx {
    /* 0x00 */ WPADStatus base;
    /* 0x2A */ DPDObjEx exp[WPAD_DPD_MAX_OBJECTS];
} WPADStatusEx;

typedef struct WPADFSStatus {
    /* 0x00 */ WPADStatus base;
    /* 0x2A */ s16 fsAccX;
    /* 0x2C */ s16 fsAccY;
    /* 0x2E */ s16 fsAccZ;
    /* 0x30 */ s8 fsStickX;
    /* 0x31 */ s8 fsStickY;
} WPADFSStatus;

typedef struct WPADCLStatus {
    /* 0x00 */ WPADStatus base;
    /* 0x2A */ u16 clButton;
    /* 0x2C */ s16 clLStickX;
    /* 0x2E */ s16 clLStickY;
    /* 0x30 */ s16 clRStickX;
    /* 0x32 */ s16 clRStickY;
    /* 0x34 */ u8 clTriggerL;
    /* 0x35 */ u8 clTriggerR;
} WPADCLStatus;

/* Train controller sample: the brake and master-controller (mascon) levers and the inverted button word. */
typedef struct WPADTRStatus {
    /* 0x00 */ WPADStatus base;
    /* 0x2A */ u16 trButton;
    /* 0x2C */ u8 brake;
    /* 0x2D */ u8 mascon;
} WPADTRStatus;

/* Balance Board sample: the four load-cell readings and the board's temperature and battery bytes. Name invented
 * (the sample layout is read from KPAD's balance-board handling). */
typedef struct WPADBLStatus {
    /* 0x00 */ WPADStatus base;
    /* 0x2A */ u16 press[4];
    /* 0x32 */ s8 temp;
    /* 0x33 */ u8 battery;
} WPADBLStatus;

/* Sample of the Vitality Sensor (VSM): raw 10-bit and 8-bit channels; the member names follow the reference SDK,
 * which does not know their purpose. */
typedef struct WPADVSStatus {
    /* 0x00 */ WPADStatus base;
    /* 0x2A */ u16 unknown2A[5];
    /* 0x34 */ u8 unknown34[2];
    /* 0x36 */ u16 unknown36[5];
    /* 0x40 */ u8 unknown40;
    /* 0x42 */ u16 unknown42;
    /* 0x44 */ u16 unknown44;
    /* 0x46 */ u8 unknown46;
    /* 0x47 */ u8 unknown47[3];
} WPADVSStatus;

typedef struct WPADAccGravityUnit {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s16 z;
} WPADAccGravityUnit;

s32 WPADProbe(s32 chan, WPADDeviceType* devType);
void WPADRead(s32 chan, WPADStatus* status);
BOOL WPADIsDpdEnabled(s32 chan);
u32 WPADGetDataFormat(s32 chan);
s32 WPADSetDataFormat(s32 chan, u32 format);
void WPADGetAccGravityUnit(s32 chan, u32 type, WPADAccGravityUnit* acc);
u8 WPADGetSensorBarPosition(void);
void WPADSetAutoSleepTime(u8 minute);
u8 WPADGetSpeakerVolume(void);
void WPADSetSpeakerVolume(u8 volume);
void WPADEnableMotor(BOOL enable);
BOOL WPADCanSendStreamData(s32 chan);
BOOL WPADSaveConfig(WPADSaveCallback callback);
u8 WPADGetRadioSensitivity(s32 chan);
BOOL WPADIsSpeakerEnabled(s32 chan);
WPADExtensionCallback WPADSetExtensionCallback(s32 chan, WPADExtensionCallback callback);
BOOL WPADIsMotorEnabled(void);
BOOL WPADStartSimpleSync(void);
BOOL WPADStartFastSimpleSync(void);
BOOL WPADStopSimpleSync(void);
WPADSyncDeviceCallback WPADSetSimpleSyncCallback(WPADSyncDeviceCallback callback);
void WPADRegisterAllocator(WPADAlloc alloc, WPADFree free);
s32 WPADControlDpd(s32 chan, u32 command, WPADCallback cb);
void WPADControlMotor(s32 chan, u32 command);
s32 WPADGetStatus(void);
void WPADInit(void);
u8 WPADGetDpdSensitivity(void);
void WPADSetCallbackByKPAD(BOOL isKPAD);
WPADConnectCallback WPADSetConnectCallback(s32 chan, WPADConnectCallback cb);
WPADSamplingCallback WPADSetSamplingCallback(s32 chan, WPADSamplingCallback cb);

u8 WPADGetDpdFormat(s32 chan);
void WPADDisconnect(s32 chan);
s32 WPADControlLed(s32 chan, u8 flags, WPADCallback cb);
void WPADSetAutoSamplingBuf(s32 chan, void* buf, u32 len);
s32 WPADControlSpeaker(s32 chan, u32 command, WPADCallback cb);
s32 WPADSendStreamData(s32 chan, void* data, u16 len);
s32 WPADControlBLC(s32 chan, s32 command, WPADCallback cb);

/* Balance Board library hooks (weak in WPAD.c). */
s32 WBCSetupCalibration(void);
s32 WBCGetCalibrationStatus(void);
s32 WBCGetBatteryLevel(u8 chan);

#ifdef __cplusplus
}
#endif

#endif
