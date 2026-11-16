#ifndef REVOLUTION_SC_H
#define REVOLUTION_SC_H

#include <revolution/types.h>
#include <revolution/bte.h>

#ifdef __cplusplus
extern "C" {
#endif

/* System configuration library. */

/* Settings items of the configuration file (conf.txt keys in the comments of scsystem.c), numbered in file order. */
typedef enum SCItemID {
    SC_ITEM_ID_IPL_COUNTER_BIAS,
    SC_ITEM_ID_IPL_ASPECT_RATIO,
    SC_ITEM_ID_IPL_AUTORUN_MODE,
    SC_ITEM_ID_IPL_CONFIG_DONE,
    SC_ITEM_ID_IPL_CONFIG_DONE2,
    SC_ITEM_ID_IPL_DISPLAY_OFFSET_H,
    SC_ITEM_ID_IPL_EURGB60_MODE,
    SC_ITEM_ID_IPL_EULA,
    SC_ITEM_ID_IPL_FREE_CHANNEL_APP_COUNT,
    SC_ITEM_ID_IPL_IDLE_MODE,
    SC_ITEM_ID_IPL_INSTALLED_CHANNEL_APP_COUNT,
    SC_ITEM_ID_IPL_LANGUAGE,
    SC_ITEM_ID_IPL_OWNER_NICKNAME,
    SC_ITEM_ID_IPL_PARENTAL_CONTROL,
    SC_ITEM_ID_IPL_PROGRESSIVE_MODE,
    SC_ITEM_ID_IPL_SCREEN_SAVER_MODE,
    SC_ITEM_ID_IPL_SIMPLE_ADDRESS,
    SC_ITEM_ID_IPL_SOUND_MODE,
    SC_ITEM_ID_IPL_UPDATE_TYPE,
    SC_ITEM_ID_NET_CONFIG,
    SC_ITEM_ID_NET_CONTENT_RESTRICTIONS,
    SC_ITEM_ID_NET_PROFILE,
    SC_ITEM_ID_NET_WC_RESTRICTION,
    SC_ITEM_ID_NET_WC_FLAGS,
    SC_ITEM_ID_DEV_BOOT_MODE,
    SC_ITEM_ID_DEV_VIDEO_MODE,
    SC_ITEM_ID_DEV_COUNTRY_CODE,
    SC_ITEM_ID_DEV_DRIVESAVING_MODE,
    SC_ITEM_ID_BT_DEVICE_INFO,
    SC_ITEM_ID_BT_CMPDEV_INFO,
    SC_ITEM_ID_BT_DPD_SENSIBILITY,
    SC_ITEM_ID_BT_SPEAKER_VOLUME,
    SC_ITEM_ID_BT_MOTOR_MODE,
    SC_ITEM_ID_BT_SENSOR_BAR_POSITION,
    SC_ITEM_ID_DVD_CONFIG,
    SC_ITEM_ID_WWW_RESTRICTION,
    SC_ITEM_ID_TEMPORARY_TITLE_ID,
    SC_ITEM_ID_MAX_PLUS1
} SCItemID;

/* Values of the language setting. */
enum {
    SC_LANG_JAPANESE,
    SC_LANG_ENGLISH,
    SC_LANG_GERMAN,
    SC_LANG_FRENCH,
    SC_LANG_SPANISH,
    SC_LANG_ITALIAN,
    SC_LANG_DUTCH,
    SC_LANG_SIMP_CHINESE,
    SC_LANG_TRAD_CHINESE,
    SC_LANG_KOREAN
};

/* Item type byte: type tag in the top three bits, name length minus one in the low five. */
typedef u8 SCType;

/* One item of the configuration file, unpacked: integer items fill `integer` and typeInteger, byte arrays only typeByteArray. */
typedef struct SCItem {
    u64 integer;
    SCType typeInteger;
    SCType typeByteArray;
    u32 nameLen;
    u32 dataSize;
    char* name;
    u8* data;
    u32 packedSize;
} SCItem;

/* SCCheckStatus results, also passed to an SCFlushCallback (ogws names; scsystem.c also uses 3 internally while a
   reloaded file waits to be parsed). */
typedef enum {
    SC_STATUS_OK,
    SC_STATUS_BUSY,
    SC_STATUS_FATAL
} SCStatus;

/* SCGetWpadMotorMode/SCSetWpadMotorMode values (ogws names). */
typedef enum {
    SC_MOTOR_OFF,
    SC_MOTOR_ON
} SCMotorMode;

/* SCGetWpadSensorBarPosition values: where the sensor bar stands relative to the screen. */
typedef enum {
    SC_SENSOR_BAR_BOTTOM,
    SC_SENSOR_BAR_TOP
} SCSensorBarPosition;

typedef void (*SCFlushCallback)(u32 result);
typedef void (*SCReloadConfFileCallback)(s32 result);

void SCInit(void);
u32 SCCheckStatus(void);
s32 SCReloadConfFileAsync(u8* bufp, u32 bufSize, SCReloadConfFileCallback callback);
void SCFlushAsync(SCFlushCallback callback);

/* Item access (scsystem.c). */
BOOL SCFindByteArrayItem(void* data, u32 size, SCItemID id);
BOOL SCReplaceByteArrayItem(const void* data, u32 size, SCItemID id);
BOOL SCFindU8Item(u8* data, SCItemID id);
BOOL SCFindS8Item(s8* data, SCItemID id);
u8 SCGetAspectRatio(void);
s8 SCGetDisplayOffsetH(void);
BOOL SCFindU32Item(u32* data, SCItemID id);
BOOL SCReplaceU8Item(u8 data, SCItemID id);

/* Idle-mode setting: wc24 is 1 when WiiConnect24 keeps the console running in standby. */
typedef struct SCIdleModeInfo {
    u8 wc24;
    u8 slotLight;
} SCIdleModeInfo;

#define SC_MAX_DEV_ENTRY_FOR_STD 10
#define SC_MAX_DEV_ENTRY_FOR_SMP 6
#define SC_MAX_DEV_ENTRY (SC_MAX_DEV_ENTRY_FOR_STD + SC_MAX_DEV_ENTRY_FOR_SMP)

/* Bluetooth device tables (sizes from the item accessors). */
typedef struct SCDevInfo {
    /* 0x00 */ char devName[20];
    /* 0x14 */ char at_0x14[1];
    /* 0x15 */ char unk_0x15[0xB];
    /* 0x20 */ LINK_KEY linkKey;
    /* 0x30 */ char unk_0x30[0x10];
} SCDevInfo;

typedef struct SCBtDeviceInfo {
    /* 0x00 */ BD_ADDR addr;
    /* 0x06 */ SCDevInfo info;
} SCBtDeviceInfo;

typedef struct SCBtDeviceInfoArray {
    /* 0x000 */ u8 numRegist;
    union {
        struct {
            /* 0x001 */ SCBtDeviceInfo regist[SC_MAX_DEV_ENTRY_FOR_STD];
            /* 0x2C5 */ SCBtDeviceInfo active[SC_MAX_DEV_ENTRY_FOR_SMP];
        };
        /* 0x001 */ SCBtDeviceInfo devices[SC_MAX_DEV_ENTRY];
    };
} SCBtDeviceInfoArray;

typedef struct SCBtCmpDeviceInfo {
    /* 0x00 */ BD_ADDR addr;
    /* 0x06 */ u8 name[64];
    /* 0x46 */ LINK_KEY linkKey;
} SCBtCmpDeviceInfo;

typedef struct SCBtCmpDeviceInfoArray {
    /* 0x000 */ u8 numRegist;
    /* 0x001 */ SCBtCmpDeviceInfo devices[SC_MAX_DEV_ENTRY_FOR_SMP];
} SCBtCmpDeviceInfoArray;

/* Settings accessors (scapi.c). */
u32 SCGetCounterBias(void);
BOOL SCGetIdleMode(SCIdleModeInfo* info);
u8 SCGetLanguage(void);
u8 SCGetScreenSaverMode(void);
u8 SCGetSoundMode(void);
void SCGetBtDeviceInfoArray(SCBtDeviceInfoArray* array);
BOOL SCSetBtDeviceInfoArray(const SCBtDeviceInfoArray* array);
void SCGetBtCmpDeviceInfoArray(SCBtCmpDeviceInfoArray* array);
BOOL SCSetBtCmpDeviceInfoArray(const SCBtCmpDeviceInfoArray* array);
u32 SCGetBtDpdSensibility(void);
u8 SCGetWpadMotorMode(void);
BOOL SCSetWpadMotorMode(u8 mode);
u8 SCGetWpadSensorBarPosition(void);
u8 SCGetWpadSpeakerVolume(void);
BOOL SCSetWpadSpeakerVolume(u8 volume);

/* Product area (JPN=0, USA=1, EUR=2, ...) and game region (JP=0, US=1, EU=2, KR=4, CN=5); -1 when unknown. */
s8 SCGetProductArea(void);
/* Game regions (koopthekoopa/wii-ipl sc.h). */
#define SC_PRODUCT_GAME_REGION_UNKNOWN -1
#define SC_PRODUCT_GAME_REGION_JP 0
#define SC_PRODUCT_GAME_REGION_US 1
#define SC_PRODUCT_GAME_REGION_EU 2
#define SC_PRODUCT_GAME_REGION_KR 4
#define SC_PRODUCT_GAME_REGION_CN 5

s8 SCGetProductGameRegion(void);

#ifdef __cplusplus
}
#endif

#endif
