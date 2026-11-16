// Reference: zeldaret/tp libs/revolution/src/sc/scapi.c
// Reference: doldecomp/ogws src/revolution/SC/scapi.c
//
// Retail keeps only the accessors the game links: the EURGB60 and progressive-mode getters of the reference are
// absent. SCGet/SetBtDeviceInfoArray are not in tp and SCGetBtDpdSensibility and the Wii Remote setting accessors
// are empty stubs there; their names and signatures follow ogws. The companion-device pair has no reference name.
#include <revolution/sc.h>

/*
 * SCGetAspectRatio
 *
 * Reads the aspect-ratio setting (1 is 16:9, anything else 4:3).
 */
u8 SCGetAspectRatio(void) {
    u8 aspect;

    if (!SCFindU8Item(&aspect, SC_ITEM_ID_IPL_ASPECT_RATIO)) {
        aspect = 0;
    } else {
        if (aspect != 1) {
            aspect = 0;
        }
    }

    return aspect;
} // 0x80248DE0

/*
 * SCGetDisplayOffsetH
 *
 * Reads the horizontal display offset, clamped to +-32 and made even.
 */
s8 SCGetDisplayOffsetH(void) {
    s8 offset;

    if (!SCFindS8Item(&offset, SC_ITEM_ID_IPL_DISPLAY_OFFSET_H)) {
        offset = 0;
    } else {
        if (offset < -32) {
            offset = -32;
        } else if (offset > 32) {
            offset = 32;
        }
    }

    offset &= ~1;
    return offset;
} // 0x80248E40

/*
 * SCGetIdleMode
 *
 * Reads the idle-mode setting (WiiConnect24 standby, slot light).
 */
BOOL SCGetIdleMode(SCIdleModeInfo* info) {
    return SCFindByteArrayItem(info, sizeof(*info), SC_ITEM_ID_IPL_IDLE_MODE);
} // 0x80248EC0

/*
 * SCGetLanguage
 *
 * Reads the system language; when the item is missing the product area picks Japanese or English,
 * and an out-of-range value reads as English.
 */
u8 SCGetLanguage(void) {
    u8 lang;
    s8 area;

    if (!SCFindU8Item(&lang, SC_ITEM_ID_IPL_LANGUAGE)) {
        area = SCGetProductArea();

        if (area == 0) {
            lang = SC_LANG_JAPANESE;
        } else {
            lang = SC_LANG_ENGLISH;
        }
    } else {
        if (lang > SC_LANG_KOREAN) {
            lang = SC_LANG_ENGLISH;
        }
    }

    return lang;
} // 0x80248ED0

/*
 * SCGetScreenSaverMode
 *
 * Reads the screen-saver flag (on when missing).
 */
u8 SCGetScreenSaverMode(void) {
    u8 mode;

    if (!SCFindU8Item(&mode, SC_ITEM_ID_IPL_SCREEN_SAVER_MODE)) {
        mode = 1;
    } else {
        if (mode != 1) {
            mode = 0;
        }
    }

    return mode;
} // 0x80248F40

/*
 * SCGetSoundMode
 *
 * Reads the sound mode (0 mono, 1 stereo, 2 surround); stereo when missing or out of range.
 */
u8 SCGetSoundMode(void) {
    u8 mode;

    if (!SCFindU8Item(&mode, SC_ITEM_ID_IPL_SOUND_MODE)) {
        mode = 1;
    } else {
        if (mode != 0 && mode != 1 && mode != 2) {
            mode = 1;
        }
    }

    return mode;
} // 0x80248FA0

/*
 * SCGetCounterBias
 *
 * Reads the real-time-clock counter bias in seconds (189388800 when missing).
 */
u32 SCGetCounterBias(void) {
    u32 bias;

    if (!SCFindU32Item(&bias, SC_ITEM_ID_IPL_COUNTER_BIAS)) {
        bias = 189388800;
    }

    return bias;
} // 0x80249000

/*
 * SCGetBtDeviceInfoArray
 *
 * Reads the table of paired Bluetooth devices.
 */
void SCGetBtDeviceInfoArray(SCBtDeviceInfoArray* array) {
    SCFindByteArrayItem(array, sizeof(*array), SC_ITEM_ID_BT_DEVICE_INFO);
} // 0x80249040

/*
 * SCSetBtDeviceInfoArray
 *
 * Replaces the table of paired Bluetooth devices.
 */
BOOL SCSetBtDeviceInfoArray(const SCBtDeviceInfoArray* array) {
    return SCReplaceByteArrayItem(array, sizeof(*array), SC_ITEM_ID_BT_DEVICE_INFO);
} // 0x80249050

/*
 * SCGetBtCmpDeviceInfoArray
 *
 * Reads the table of companion Bluetooth devices.
 * Name unverified: neither tp nor ogws has the companion-device accessors; named after SCGetBtDeviceInfoArray.
 */
void SCGetBtCmpDeviceInfoArray(SCBtCmpDeviceInfoArray* array) {
    SCFindByteArrayItem(array, sizeof(*array), SC_ITEM_ID_BT_CMPDEV_INFO);
} // 0x80249060

/*
 * SCSetBtCmpDeviceInfoArray
 *
 * Replaces the table of companion Bluetooth devices.
 * Name unverified: neither tp nor ogws has the companion-device accessors; named after SCSetBtDeviceInfoArray.
 */
BOOL SCSetBtCmpDeviceInfoArray(const SCBtCmpDeviceInfoArray* array) {
    return SCReplaceByteArrayItem(array, sizeof(*array), SC_ITEM_ID_BT_CMPDEV_INFO);
} // 0x80249070

/*
 * SCGetBtDpdSensibility
 *
 * Reads the pointer sensitivity, clamped to 1..5 (2 when missing).
 */
u32 SCGetBtDpdSensibility(void) {
    u32 sensibility;

    if (!SCFindU32Item(&sensibility, SC_ITEM_ID_BT_DPD_SENSIBILITY)) {
        sensibility = 2;
    } else {
        if (sensibility < 1) {
            sensibility = 1;
        } else if (sensibility > 5) {
            sensibility = 5;
        }
    }

    return sensibility;
} // 0x80249080

/*
 * SCGetWpadMotorMode
 *
 * Reads the remote rumble flag (on when missing).
 */
u8 SCGetWpadMotorMode(void) {
    u8 mode;

    if (!SCFindU8Item(&mode, SC_ITEM_ID_BT_MOTOR_MODE)) {
        mode = 1;
    } else {
        if (mode != 1) {
            mode = 0;
        }
    }

    return mode;
} // 0x802490F0

/*
 * SCSetWpadMotorMode
 *
 * Stores the remote rumble flag.
 */
BOOL SCSetWpadMotorMode(u8 mode) {
    return SCReplaceU8Item(mode, SC_ITEM_ID_BT_MOTOR_MODE);
} // 0x80249150

/*
 * SCGetWpadSensorBarPosition
 *
 * Reads the sensor bar position (0 below the screen, 1 above).
 */
u8 SCGetWpadSensorBarPosition(void) {
    u8 position;

    if (!SCFindU8Item(&position, SC_ITEM_ID_BT_SENSOR_BAR_POSITION)) {
        position = 0;
    } else {
        if (position != 1) {
            position = 0;
        }
    }

    return position;
} // 0x80249160

/*
 * SCGetWpadSpeakerVolume
 *
 * Reads the remote speaker volume, at most 127 (89 when missing).
 */
u8 SCGetWpadSpeakerVolume(void) {
    u8 volume;

    if (!SCFindU8Item(&volume, SC_ITEM_ID_BT_SPEAKER_VOLUME)) {
        volume = 89;
    } else {
        if (volume > 127) {
            volume = 127;
        }
    }

    return volume;
} // 0x802491C0

/*
 * SCSetWpadSpeakerVolume
 *
 * Stores the remote speaker volume.
 */
BOOL SCSetWpadSpeakerVolume(u8 volume) {
    return SCReplaceU8Item(volume, SC_ITEM_ID_BT_SPEAKER_VOLUME);
} // 0x80249220
