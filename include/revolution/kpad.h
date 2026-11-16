#ifndef REVOLUTION_KPAD_H
#define REVOLUTION_KPAD_H

#include <revolution/types.h>
#include <revolution/mtx.h>
#include <revolution/wpad.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wii Remote controller-state library: turns the raw WPAD samples into buttons, acceleration, pointer position,
 * distance and horizon. Names follow zeldaret/tp and doldecomp/ogws; the retail (2009) layout is larger. */

#define KPAD_BUTTON_MASK 0x0000ffff
#define KPAD_BUTTON_RPT 0x80000000

#define KPAD_BTN_RPT_TIME_MAX 20000
#define KPAD_BTN_NO_RPT_DELAY 40000

#define KPAD_CMOS_RESO_WX 128
#define KPAD_CMOS_RESO_WY 96
#define KPAD_CMOS_RESO_CX (KPAD_CMOS_RESO_WX / 2)
#define KPAD_CMOS_RESO_CY (KPAD_CMOS_RESO_WY / 2)

#define KPAD_DPD_RESO_WX WPAD_DPD_IMG_RESO_WX
#define KPAD_DPD_RESO_WY WPAD_DPD_IMG_RESO_WY
#define KPAD_DPD_RESO_CX (KPAD_DPD_RESO_WX / 2)
#define KPAD_DPD_RESO_CY (KPAD_DPD_RESO_WY / 2)

#define KPAD_USE_OBJECTS 2

/* Samples kept per controller in the library's own ring buffer (a caller's buffer extends it). Name invented. */
#define KPAD_RING_SAMPLES 16

typedef enum KPADPlayMode { KPAD_PLAY_MODE_LOOSE = 0, KPAD_PLAY_MODE_TIGHT } KPADPlayMode;

typedef s32 KPADResult;

/* Result of a read (the error out-parameter of KPADiRead). Names invented; the values are read from the code. */
#define KPAD_READ_ERR_NONE 0
#define KPAD_READ_ERR_NO_DATA -1
#define KPAD_READ_ERR_NO_CONTROLLER -2
#define KPAD_READ_ERR_NOT_READY -3
#define KPAD_READ_ERR_BUSY -4
#define KPAD_READ_ERR_UNINITIALIZED -5

typedef union KPADEXStatus {
    struct {
        Vec2 stick;
        Vec acc;
        f32 acc_value;
        f32 acc_speed;
    } fs; /* size 0x1c */

    struct {
        u32 hold;
        u32 trig;
        u32 release;
        Vec2 lstick;
        Vec2 rstick;
        f32 ltrigger;
        f32 rtrigger;
    } cl; /* size 0x24 */

    /* Balance Board; names invented (the layout is read from the board path of read_kpad_stick). */
    struct {
        f64 weight;      /* total weight reported by the board library, corrected for temperature */
        f64 cell[4];     /* the four load cells, calibrated by the board library */
        f64 cell_avg[4]; /* slow running average of the cells */
        s32 err;         /* board library result, or a KPAD error (-1 to -4) */
        u32 zeroing;     /* the zero point is still being measured */
    } bl; /* size 0x50 */
} KPADEXStatus;

/* The sample handed to the game; 0xb0 bytes in the 2009 SDK (the Balance Board member fills the extension union). */
typedef struct KPADStatus {
    /* 0x00 */ u32 hold;
    /* 0x04 */ u32 trig;
    /* 0x08 */ u32 release;
    /* 0x0C */ Vec acc;
    /* 0x18 */ f32 acc_value;
    /* 0x1C */ f32 acc_speed;
    /* 0x20 */ Vec2 pos;
    /* 0x28 */ Vec2 vec;
    /* 0x30 */ f32 speed;
    /* 0x34 */ Vec2 horizon;
    /* 0x3C */ Vec2 hori_vec;
    /* 0x44 */ f32 hori_speed;
    /* 0x48 */ f32 dist;
    /* 0x4C */ f32 dist_vec;
    /* 0x50 */ f32 dist_speed;
    /* 0x54 */ Vec2 acc_vertical;
    /* 0x5C */ u8 dev_type;
    /* 0x5D */ s8 wpad_err;
    /* 0x5E */ s8 dpd_valid_fg;
    /* 0x5F */ u8 data_format;
    /* 0x60 */ KPADEXStatus ex_status;
} KPADStatus;

/* One WPAD sample of any device type; the slots of the ring buffers are 0x38 bytes. */
typedef struct KPADUnifiedWpadStatus {
    union {
        WPADStatus core;
        WPADFSStatus fs;
        WPADCLStatus cl;
        WPADTRStatus tr;
        WPADBLStatus bl;
    } u;
    /* 0x36 */ u8 data_format; /* WPAD data format the sample was read in, stored by KPAD */
} KPADUnifiedWpadStatus;

typedef struct KPADObject {
    Vec2 center;
    s8 error_fg;
    s8 state_fg;
    s8 field_0xA;
    s8 field_0xB;
} KPADObject;

typedef void (*KPADControlDpdCallback)(s32 chan, s32 reason);
typedef WPADCallback KPADCallback;

/* Per-controller working state; inside_kpads holds one per channel. Member names that tp and ogws do not carry are
 * invented: ring_head, ring_count, acc_scale, fs_acc_scale, sampling_callback, connect_callback, dpd_enabled,
 * fs_acc_rotate, the four *_play_mode members and needs_reset, dev_changed, want_dpd, dpd_cmd_busy, dpd_cmd,
 * dpd_off_sent, dpd_on_sent, btn_per_sample, reload_acc_scale (the retail 2009 layout is larger than the
 * references'). field_0x55C, field_0x55D and field_0x570 keep their offsets: their purpose is not established. */
typedef struct KPADInsideStatus {
    /* 0x000 */ KPADStatus status;
    /* 0x0B0 */ f32 pos_play_radius;
    /* 0x0B4 */ f32 pos_sensitivity;
    /* 0x0B8 */ f32 hori_play_radius;
    /* 0x0BC */ f32 hori_sensitivity;
    /* 0x0C0 */ f32 dist_play_radius;
    /* 0x0C4 */ f32 dist_sensitivity;
    /* 0x0C8 */ f32 acc_play_radius;
    /* 0x0CC */ f32 acc_sensitivity;
    /* 0x0D0 */ f32 dist_org;
    /* 0x0D4 */ Vec2 accXY_nrm_hori;
    /* 0x0DC */ Vec2 sec_nrm_hori;
    /* 0x0E4 */ Vec2 center_org;
    /* 0x0EC */ f32 dpd2pos_scale;
    /* 0x0F0 */ KPADObject kobj_sample[WPAD_DPD_MAX_OBJECTS];
    /* 0x120 */ KPADObject kobj_regular[KPAD_USE_OBJECTS];
    /* 0x138 */ s16 valid_objs;
    /* 0x13A */ u8 ring_head;
    /* 0x13B */ u8 ring_count;
    /* 0x13C */ KPADUnifiedWpadStatus uniRingBuf[KPAD_RING_SAMPLES];
    /* 0x4BC */ KPADUnifiedWpadStatus* uniRingBufExt;
    /* 0x4C0 */ u32 uniRingBufExtLength;
    /* 0x4C4 */ f32 sec_length;
    /* 0x4C8 */ Vec2 sec_nrm;
    /* 0x4D0 */ f32 sec_dist;
    /* 0x4D4 */ f32 trust_sec_length;
    /* 0x4D8 */ Vec hard_acc;
    /* 0x4E4 */ Vec2 obj_horizon;
    /* 0x4EC */ Vec2 acc_horizon;
    /* 0x4F4 */ Vec2 ah_circle_pos;
    /* 0x4FC */ u16 ah_circle_ct;
    /* 0x4FE */ u8 dpd_valid2_ct;
    /* 0x4FF */ u8 reading;
    /* 0x500 */ u16 btn_repeat_time;
    /* 0x502 */ u16 btn_repeat_next;
    /* 0x504 */ u16 btn_repeat_delay;
    /* 0x506 */ u16 btn_repeat_pulse;
    /* 0x508 */ u16 btn_cl_repeat_time;
    /* 0x50A */ u16 btn_cl_repeat_next;
    /* 0x50C */ KPADControlDpdCallback dpd_ctrl_callback;
    /* 0x510 */ Vec acc_scale; /* reciprocal of the remote's 1 G calibration per axis */
    /* 0x51C */ Vec fs_acc_scale; /* the same for the extension's accelerometer */
    /* 0x528 */ Vec2 kobj_frame_min;
    /* 0x530 */ Vec2 kobj_frame_max;
    /* 0x538 */ f32 kp_err_dist_speed_1;
    /* 0x53C */ f32 kp_err_dist_speedM_1;
    /* 0x540 */ f32 kp_ah_circle_radius2;
    /* 0x544 */ f32 kp_dist_vv1;
    /* 0x548 */ f32 kp_err_dist_min;
    /* 0x54C */ WPADSamplingCallback sampling_callback; /* the callback WPAD held before KPAD's own */
    /* 0x550 */ WPADConnectCallback connect_callback; /* the game's callback, chained from KPAD's own */
    /* 0x554 */ u8 needs_reset; /* set at start-up: the first read resets the controller */
    /* 0x555 */ u8 dev_changed; /* the device changed or was reset: the extension's buttons and state start afresh */
    /* 0x556 */ u8 want_dpd; /* the pointer sensor is wanted (picks the second row of the format table) */
    /* 0x557 */ u8 dpd_enabled; /* WPAD reports the pointer sensor on */
    /* 0x558 */ u8 dpd_cmd_busy; /* a pointer sensor command is in flight */
    /* 0x559 */ u8 dpd_cmd; /* the pointer sensor command last issued */
    /* 0x55A */ u8 dpd_off_sent; /* the pointer sensor callback was told the sensor is going off */
    /* 0x55B */ u8 dpd_on_sent; /* ... and that it is back on */
    /* 0x55C */ u8 field_0x55C;
    /* 0x55D */ u8 field_0x55D;
    /* 0x55E */ u8 fs_acc_rotate; /* turns the extension's acceleration by the library's fixed matrix */
    /* 0x55F */ u8 btn_per_sample; /* 1: buttons are folded sample by sample instead of once per read */
    /* 0x560 */ KPADPlayMode pos_play_mode; /* how the dead zone of each filter behaves */
    /* 0x564 */ KPADPlayMode hori_play_mode;
    /* 0x568 */ KPADPlayMode dist_play_mode;
    /* 0x56C */ KPADPlayMode acc_play_mode;
    /* 0x570 */ u32 field_0x570;
    /* 0x574 */ u8 reload_acc_scale; /* the accelerometer calibration must be read again */
    /* 0x575 */ u8 field_0x575[0x578 - 0x575];
} KPADInsideStatus;

extern KPADInsideStatus inside_kpads[WPAD_MAX_CONTROLLERS];

void KPADSetBtnRepeat(s32 chan, f32 delay_sec, f32 pulse_sec);
void KPADSetObjInterval(f32 interval);
void KPADSetPosParam(s32 chan, f32 play_radius, f32 sensitivity);

s32 KPADRead(s32 chan, KPADStatus* sampling_bufs, s32 length);
s32 KPADiRead(s32 chan, KPADStatus* sampling_bufs, s32 length, s32* error, BOOL keep_first);

void KPADInit(void);
void KPADInitEx(KPADUnifiedWpadStatus* uwp_bufs, u32 length);

WPADConnectCallback KPADSetConnectCallback(s32 chan, WPADConnectCallback cb);


/* Placeholder-named internals (purpose not yet established). */
void fn_80278290(s32 chan);
void KPADSetPosPlayMode(s32 chan, KPADPlayMode mode);

#ifdef __cplusplus
}
#endif

#endif
