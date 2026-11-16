// Reference: zeldaret/tp libs/revolution/src/kpad/KPAD.c
// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/kpad/R4QK01/KPAD.c
//
// Retail is the Jun 2009 SDK (0x4302_145), a later KPAD than the reference: the library keeps per-controller copies
// of the tuning values, a 16-sample ring buffer that a caller's buffer can extend (KPADInitEx), and a 0xb0-byte
// KPADStatus. The Hori/Dist/Acc parameter setters of the reference are not linked.
//
// Both formerly open register residues close with the reference's spellings: calc_obj_horizon keeps the square
// root in a local f1 and the reciprocal in a second local f2 (instead of reading sec_length back), and KPADInitEx
// walks the controllers with a do/while loop.
//
// All 29 functions are at 100%; the static data layout matches the retail split (gatecheck layout). The unit
// stays NonMatching until a clean gate flips it.
#include <revolution/kpad.h>
#include <math.h>
#include <revolution/os.h>
#include <revolution/wpad/__wpad.h>
#include <string.h>

static const char* __KPADVersion = "<< RVL_SDK - KPAD 	release build: Jun 22 2009 18:32:13 (0x4302_145) >>";

// Default tuning values of a controller. Zero-initialised ones live in .sbss, so declaration order differs from
// the order in .sdata.
static Vec2 icenter_org = {0.000f, 0.000f};
static f32 idist_org = 1.000f;
static Vec2 iaccXY_nrm_hori = {0.000f, -1.000f};
static Vec2 isec_nrm_hori = {1.000f, 0.000f};
f32 kp_obj_interval = 0.200f;

f32 kp_acc_horizon_pw = 0.050f;
f32 kp_ah_circle_radius = 0.070f;
f32 kp_ah_circle_pw = 0.060f;
u16 kp_ah_circle_ct = 100;

f32 kp_err_outside_frame = 0.050f;
f32 kp_err_dist_min;
f32 kp_err_dist_max = 3.000f;
f32 kp_err_dist_speed = 0.040f;
f32 kp_err_first_inpr = 0.900f;
f32 kp_err_next_inpr = 0.900f;
f32 kp_err_acc_inpr = 0.900f;
f32 kp_err_up_inpr = 0.700f;
f32 kp_err_near_pos = 0.100f;

static f32 kp_dist_vv1;

static s32 kp_fs_fstick_min = 15;
static s32 kp_fs_fstick_max = 71;
static s32 kp_cl_stick_min = 60;
static s32 kp_cl_stick_max = 308;
static s32 kp_cl_trigger_min = 30;
static s32 kp_cl_trigger_max = 180;
static f32 kp_rm_acc_max = 3.4f;
static f32 kp_fs_acc_max = 2.1f;

// Ranges of the guitar's and train controller's analogue inputs (lbl_8052B064/80528E14: triggers, lbl_8052B068/
// 80528E18: right stick), the switch between the two stick clamps (lbl_8052B060) and further constants read by
// read_kpad_stick and KPADiRead; purpose not yet established. The zero ones are in .sbss, the others in .sdata (declaration order within each).
static s32 lbl_8052B060 = 0;
static s32 lbl_8052B064 = 0;
static s32 lbl_80528E14 = 256;
static s32 lbl_8052B068 = 0;
static s32 lbl_80528E18 = 1024;
static u8 lbl_80528E1C = 50;
static f32 lbl_80528E20 = 400.0f;
static f32 kp_fs_acc_rot_deg = 24.0f; // angle of the fixed rotation applied to the extension's acceleration

static u8 kpad_initialized = FALSE;

KPADInsideStatus inside_kpads[WPAD_MAX_CONTROLLERS];
static Mtx fs_acc_rotation;
static Vec2 Vec2_0 = {0.0f, 0.0f};

// Balance Board (WPAD_DEV_BALANCE_CHECKER) zero-point measurement, run from KPADiSamplingCallback. A command to
// the board is issued from state 0; a measurement then waits lbl_80528E1C samples (phase 1), averages the four
// load cells over lbl_80528E20 samples (phase 2) and hands the averages to the board library (phase 3). Names are
// the symbol addresses until the other readers of these variables (read_kpad_stick) are written. read_kpad_stick resets
// them, folds each sample into the averages and reports the result in KPADEXStatus.
static f64 lbl_804F1368[4]; // running mean of the cells during phase 2
static f64 lbl_804F1388[4]; // slow running average of the cells (read in read_kpad_stick)
static u8 lbl_8052B086;     // a board command is in flight
static u8 lbl_8052B085;
static u8 lbl_8052B084;
static u8 lbl_8052B083;
static u8 lbl_8052B082;     // phase of the measurement
static u16 lbl_8052B080;    // samples seen in the current phase
static f64 lbl_8052B078;    // weight reported by the board library (read_kpad_stick)

/*
 * KPADSetBtnRepeat
 *
 * Sets the delay before, and the interval between, button auto-repeat pulses; no pulse interval turns repeat off.
 * Not part of the retail image (the linker dropped it); its constants still lead the unit's literal pool.
 */
void KPADSetBtnRepeat(s32 chan, f32 delay_sec, f32 pulse_sec) {
    KPADInsideStatus* kp = &inside_kpads[chan];

    if (pulse_sec) {
        kp->btn_repeat_delay = (u16)(s32)(delay_sec + 0.5f);
        kp->btn_repeat_pulse = (u16)(s32)(pulse_sec + 0.5f);
    } else {
        kp->btn_repeat_delay = KPAD_BTN_NO_RPT_DELAY;
        kp->btn_repeat_pulse = 0;
    }

    kp->btn_repeat_time = 0;
    kp->btn_repeat_next = kp->btn_repeat_delay;
    kp->btn_cl_repeat_time = 0;
    kp->btn_cl_repeat_next = kp->btn_repeat_delay;
}

/*
 * KPADSetPosParam
 *
 * Sets the pointer's dead-zone radius and sensitivity for one controller.
 */
void KPADSetPosParam(s32 chan, f32 play_radius, f32 sensitivity) {
    inside_kpads[chan].pos_play_radius = play_radius;
    inside_kpads[chan].pos_sensitivity = sensitivity;
} // 0x802780A0

/*
 * reset_kpad
 *
 * Returns a controller's state to its defaults: no buttons, pointer at the centre, acceleration at rest.
 */
static void reset_kpad(KPADInsideStatus* kp) {
    KPADObject* op;
    KPADStatus* sp = &kp->status;

    kp->needs_reset = FALSE;

    kp->kobj_frame_min.x = -1.0f + kp_err_outside_frame;
    kp->kobj_frame_max.x = 1.0f - kp_err_outside_frame;
    kp->kobj_frame_min.y = -0.75f + kp_err_outside_frame;
    kp->kobj_frame_max.y = 0.75f - kp_err_outside_frame;
    kp->kp_err_dist_speed_1 = 1.0f / kp_err_dist_speed;
    kp->kp_err_dist_speedM_1 = -1.0f / kp_err_dist_speed;
    kp->kp_ah_circle_radius2 = kp_ah_circle_radius * kp_ah_circle_radius;
    kp->kp_err_dist_min = kp_err_dist_min;
    kp->kp_dist_vv1 = kp_dist_vv1;

    sp->hold = sp->trig = sp->release = 0;
    kp->btn_repeat_time = 0;
    kp->btn_repeat_next = kp->btn_repeat_delay;

    sp->dpd_valid_fg = 0;
    kp->dpd_valid2_ct = 0;

    sp->pos = sp->vec = Vec2_0;
    sp->speed = 0.0f;

    sp->horizon.x = kp->acc_horizon.x = kp->obj_horizon.x = 1.0f;
    sp->horizon.y = kp->acc_horizon.y = kp->obj_horizon.y = 0.0f;
    sp->hori_vec = Vec2_0;
    sp->hori_speed = 0.0f;

    sp->acc_vertical.x = 1.0f;
    sp->acc_vertical.y = 0.0f;

    sp->dist = kp->dist_org;
    sp->dist_vec = sp->dist_speed = 0.0f;

    kp->sec_dist = sp->dist;
    kp->sec_length = kp->trust_sec_length = kp->kp_dist_vv1 / kp->sec_dist;
    kp->sec_nrm = kp->sec_nrm_hori;

    sp->acc.x = sp->acc.z = 0.0f;
    sp->acc.y = -1.0f;
    sp->acc_value = 1.0f;
    sp->acc_speed = 0.0f;
    kp->hard_acc = sp->acc;

    kp->ah_circle_pos = kp->acc_horizon;
    kp->ah_circle_ct = kp_ah_circle_ct;

    kp->valid_objs = 0;

    op = &kp->kobj_sample[WPAD_DPD_MAX_OBJECTS - 1];
    do {
        op->error_fg = -1;
    } while (--op >= kp->kobj_sample);

    op = &kp->kobj_regular[KPAD_USE_OBJECTS - 1];
    do {
        op->error_fg = -1;
    } while (--op >= kp->kobj_regular);

    kp->ring_count = 0;
    kp->dev_changed = TRUE;
} // 0x802780C0

/*
 * fn_80278290
 *
 * Raises two flags of a controller (purpose unknown).
 */
void fn_80278290(s32 chan) {
    inside_kpads[chan].field_0x55C = TRUE;
    inside_kpads[chan].field_0x55D = TRUE;
} // 0x80278290

/*
 * calc_button_repeat
 *
 * Adds the auto-repeat flag to the held buttons of the Wii Remote and of a Classic-style extension.
 */
static void calc_button_repeat(KPADInsideStatus* kp, u32 dev_type, u32 repeat_count) {
    KPADStatus* status = &kp->status;
    KPADEXStatus* ep = &status->ex_status;

    if (status->trig != 0 || status->release != 0) {
        kp->btn_repeat_time = 0;
        kp->btn_repeat_next = kp->btn_repeat_delay;
        if (status->trig && kp->btn_repeat_pulse) {
            status->hold |= KPAD_BUTTON_RPT;
        }
    } else if (status->hold != 0) {
        kp->btn_repeat_time += repeat_count;
        if (kp->btn_repeat_time >= KPAD_BTN_NO_RPT_DELAY) {
            kp->btn_repeat_time -= KPAD_BTN_NO_RPT_DELAY;
        }

        if (kp->btn_repeat_time >= kp->btn_repeat_next) {
            status->hold |= KPAD_BUTTON_RPT;
            kp->btn_repeat_next += kp->btn_repeat_pulse;
            if (kp->btn_repeat_time >= KPAD_BTN_RPT_TIME_MAX) {
                kp->btn_repeat_time -= KPAD_BTN_RPT_TIME_MAX;
                kp->btn_repeat_next -= KPAD_BTN_RPT_TIME_MAX;
            }
        }
    }

    if (dev_type == WPAD_DEV_CLASSIC || dev_type == WPAD_DEV_TRAIN || dev_type == WPAD_DEV_GUITAR) {
        if (ep->cl.trig != 0 || ep->cl.release != 0) {
            kp->btn_cl_repeat_time = 0;
            kp->btn_cl_repeat_next = kp->btn_repeat_delay;
            if (ep->cl.trig && kp->btn_repeat_pulse) {
                ep->cl.hold |= KPAD_BUTTON_RPT;
            }
        } else if (ep->cl.hold != 0) {
            kp->btn_cl_repeat_time += repeat_count;
            if (kp->btn_cl_repeat_time >= KPAD_BTN_NO_RPT_DELAY) {
                kp->btn_cl_repeat_time -= KPAD_BTN_NO_RPT_DELAY;
            }

            if (kp->btn_cl_repeat_time >= kp->btn_cl_repeat_next) {
                ep->cl.hold |= KPAD_BUTTON_RPT;
                kp->btn_cl_repeat_next += kp->btn_repeat_pulse;
                if (kp->btn_cl_repeat_time >= KPAD_BTN_RPT_TIME_MAX) {
                    kp->btn_cl_repeat_time -= KPAD_BTN_RPT_TIME_MAX;
                    kp->btn_cl_repeat_next -= KPAD_BTN_RPT_TIME_MAX;
                }
            }
        }
    }
} // 0x802782B0

/*
 * read_kpad_button
 *
 * Takes the new button states of the Wii Remote and its extension and derives pressed/released edges and repeat flags.
 */
static void read_kpad_button(KPADInsideStatus* kp, u32 dev_type, u32 repeat_count, u32 core_button, u32 fs_button,
                      u32 cl_button) {
    KPADEXStatus* ep = &kp->status.ex_status;
    u32 old_fg_core = kp->status.hold & KPAD_BUTTON_MASK;
    u32 old_fg_cl;
    u32 change_fg;

    kp->status.hold = core_button & (KPAD_BUTTON_MASK & ~(WPAD_BUTTON_Z | WPAD_BUTTON_C));
    if (dev_type == WPAD_DEV_FS) {
        kp->status.hold |= (kp->dev_changed ? 0 : fs_button) & (WPAD_BUTTON_Z | WPAD_BUTTON_C);
    }

    change_fg = kp->status.hold ^ old_fg_core;
    kp->status.trig = change_fg & kp->status.hold;
    kp->status.release = change_fg & old_fg_core;

    if (dev_type == WPAD_DEV_CLASSIC || dev_type == WPAD_DEV_TRAIN || dev_type == WPAD_DEV_GUITAR) {
        cl_button = kp->dev_changed ? 0 : cl_button;
        old_fg_cl = ep->cl.hold & KPAD_BUTTON_MASK;
        ep->cl.hold = cl_button & KPAD_BUTTON_MASK;

        change_fg = ep->cl.hold ^ old_fg_cl;
        ep->cl.trig = change_fg & ep->cl.hold;
        ep->cl.release = change_fg & old_fg_cl;
    }

    calc_button_repeat(kp, dev_type, repeat_count);
} // 0x80278450

/*
 * calc_acc
 *
 * Moves one smoothed acceleration axis towards a new reading. Small changes are damped inside the play radius;
 * in tight mode the play radius is a dead zone that the filter follows the reading out of.
 */
static void calc_acc(KPADInsideStatus* kp, f32* acc, f32 acc2) {
    f32 f1, f2;

    f2 = acc2 - *acc;

    if (kp->acc_play_mode == KPAD_PLAY_MODE_LOOSE) {
        if (f2 < 0.0f) {
            f1 = -f2;
        } else {
            f1 = f2;
        }

        if (f1 >= kp->acc_play_radius) {
            f1 = 1.0f;
        } else {
            f1 /= kp->acc_play_radius;
            f1 *= f1;
            f1 *= f1;
        }
        f1 *= kp->acc_sensitivity;
        *acc += f1 * f2;
    } else {
        if (f2 < -kp->acc_play_radius) {
            *acc += kp->acc_sensitivity * (f2 + kp->acc_play_radius);
        } else if (f2 > kp->acc_play_radius) {
            *acc += kp->acc_sensitivity * (f2 - kp->acc_play_radius);
        }
    }
} // 0x802784F0

/*
 * calc_acc_horizon
 *
 * Slowly turns the remembered horizon direction towards the one the hard acceleration points at, and counts how
 * long that direction has stayed inside a small circle.
 */
static void calc_acc_horizon(KPADInsideStatus* kp) {
    f32 f1, vx, vy, ax, ay;
    f1 = sqrt(kp->hard_acc.x * kp->hard_acc.x + kp->hard_acc.y * kp->hard_acc.y);
    if (f1 == 0.0f || f1 >= 2.0f) {
        return;
    }
    ax = kp->hard_acc.x / f1;
    ay = kp->hard_acc.y / f1;

    if (f1 > 1.0f) {
        f1 = 2.0f - f1;
    }
    f1 *= f1 * kp_acc_horizon_pw;

    vx = kp->accXY_nrm_hori.x * ax + kp->accXY_nrm_hori.y * ay;
    vy = kp->accXY_nrm_hori.y * ax - kp->accXY_nrm_hori.x * ay;
    ax = (vx - kp->acc_horizon.x) * f1 + kp->acc_horizon.x;
    ay = (vy - kp->acc_horizon.y) * f1 + kp->acc_horizon.y;

    f1 = sqrt(ax * ax + ay * ay);
    if (f1 == 0.0f) {
        return;
    }
    kp->acc_horizon.x = ax / f1;
    kp->acc_horizon.y = ay / f1;

    kp->ah_circle_pos.x += (kp->acc_horizon.x - kp->ah_circle_pos.x) * kp_ah_circle_pw;
    kp->ah_circle_pos.y += (kp->acc_horizon.y - kp->ah_circle_pos.y) * kp_ah_circle_pw;

    vx = kp->acc_horizon.x - kp->ah_circle_pos.x;
    vy = kp->acc_horizon.y - kp->ah_circle_pos.y;
    if (vx * vx + vy * vy <= kp->kp_ah_circle_radius2) {
        if (kp->ah_circle_ct) {
            --kp->ah_circle_ct;
        }
    } else {
        kp->ah_circle_ct = kp_ah_circle_ct;
    }
} // 0x802785B0

/*
 * calc_acc_vertical
 *
 * Slowly turns the remembered vertical direction towards the one the hard acceleration points at.
 */
static void calc_acc_vertical(KPADInsideStatus* kp) {
    KPADStatus* sp = &kp->status;
    f32 f1;
    f32 f2;
    f32 ax;
    f32 ay;

    ax = sqrt(f2 = kp->hard_acc.x * kp->hard_acc.x + kp->hard_acc.y * kp->hard_acc.y);
    ay = -kp->hard_acc.z;
    f1 = sqrt(f2 + ay * ay);
    if (f1 == 0.0f || f1 >= 2.0f) {
        return;
    }
    ax = ax / f1;
    ay /= f1;

    if (f1 > 1.0f) {
        f1 = 2.0f - f1;
    }
    f1 *= f1 * kp_acc_horizon_pw;

    ax = (ax - sp->acc_vertical.x) * f1 + sp->acc_vertical.x;
    ay = (ay - sp->acc_vertical.y) * f1 + sp->acc_vertical.y;

    f1 = sqrt(ax * ax + ay * ay);
    if (f1 == 0.0f) {
        return;
    }
    sp->acc_vertical.x = ax / f1;
    sp->acc_vertical.y = ay / f1;
} // 0x80278750

/*
 * clamp_acc
 *
 * Limits an acceleration reading to plus or minus clamp.
 */
static f32 clamp_acc(f32 acc, f32 clamp) {
    if (acc < 0.0f) {
        if (acc < -clamp) {
            return -clamp;
        }
    } else {
        if (acc > clamp) {
            return clamp;
        }
    }
    return acc;
}

/*
 * read_kpad_acc
 *
 * Takes the acceleration of the Wii Remote, and of the Nunchuk when present, out of a sample and filters it.
 * WPAD_FMT_GUITAR (11) is the one extension format that also carries the Wii Remote's acceleration and, with the
 * pointer sensor on, its objects (the 2009 SDK's "extended" core format; retail dev type 0x11 uses it).
 */
static void read_kpad_acc(KPADInsideStatus* kp, KPADUnifiedWpadStatus* uwp) {
    KPADStatus* sp = &kp->status;
    Vec fs_acc;
    Vec vec;
    s32 fmt = uwp->data_format;

    if ((fmt >= WPAD_FMT_CORE_BTN_ACC && fmt <= WPAD_FMT_CORE_BTN_ACC_DPD) ||
        (fmt >= WPAD_FMT_FS_BTN_ACC && fmt <= WPAD_FMT_FS_BTN_ACC_DPD) ||
        (fmt >= WPAD_FMT_CLASSIC_BTN_ACC && fmt <= WPAD_FMT_CLASSIC_BTN_ACC_DPD) ||
        fmt == WPAD_FMT_GUITAR) {
        kp->hard_acc.x = clamp_acc(-uwp->u.core.accX * kp->acc_scale.x, kp_rm_acc_max);
        kp->hard_acc.y = clamp_acc(-uwp->u.core.accZ * kp->acc_scale.z, kp_rm_acc_max);
        kp->hard_acc.z = clamp_acc(uwp->u.core.accY * kp->acc_scale.y, kp_rm_acc_max);

        vec = sp->acc;

        calc_acc(kp, &sp->acc.x, kp->hard_acc.x);
        calc_acc(kp, &sp->acc.y, kp->hard_acc.y);
        calc_acc(kp, &sp->acc.z, kp->hard_acc.z);
        sp->acc_value = sqrt(sp->acc.x * sp->acc.x + sp->acc.y * sp->acc.y + sp->acc.z * sp->acc.z);

        vec.x -= sp->acc.x;
        vec.y -= sp->acc.y;
        vec.z -= sp->acc.z;
        sp->acc_speed = sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
        calc_acc_horizon(kp);
        calc_acc_vertical(kp);

        if (uwp->u.core.err != WPAD_ERR_OK || uwp->u.core.dev != WPAD_DEV_FS ||
            (uwp->data_format != WPAD_FMT_FS_BTN_ACC && uwp->data_format != WPAD_FMT_FS_BTN_ACC_DPD)) {
            return;
        }

        fs_acc.x = clamp_acc(-uwp->u.fs.fsAccX * kp->fs_acc_scale.x, kp_fs_acc_max);
        fs_acc.y = clamp_acc(-uwp->u.fs.fsAccZ * kp->fs_acc_scale.z, kp_fs_acc_max);
        fs_acc.z = clamp_acc(uwp->u.fs.fsAccY * kp->fs_acc_scale.y, kp_fs_acc_max);
        if (kp->fs_acc_rotate) {
            PSMTXMultVec(fs_acc_rotation, &fs_acc, &fs_acc);
        }

        vec = sp->ex_status.fs.acc;

        calc_acc(kp, &sp->ex_status.fs.acc.x, fs_acc.x);
        calc_acc(kp, &sp->ex_status.fs.acc.y, fs_acc.y);
        calc_acc(kp, &sp->ex_status.fs.acc.z, fs_acc.z);
        sp->ex_status.fs.acc_value = sqrt(sp->ex_status.fs.acc.x * sp->ex_status.fs.acc.x +
                                          sp->ex_status.fs.acc.y * sp->ex_status.fs.acc.y +
                                          sp->ex_status.fs.acc.z * sp->ex_status.fs.acc.z);

        vec.x -= sp->ex_status.fs.acc.x;
        vec.y -= sp->ex_status.fs.acc.y;
        vec.z -= sp->ex_status.fs.acc.z;
        sp->ex_status.fs.acc_speed = sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
    }
} // 0x80278880

/*
 * calc_horizon
 *
 * Direction of the line through two sensor objects, relative to the sensor bar's nominal direction, and the
 * distance it implies.
 */
static f32 calc_horizon(KPADInsideStatus* kp, Vec2* p1, Vec2* p2, Vec2* hori) {
    f32 f1, f2, vx, vy;

    vx = p2->x - p1->x;
    vy = p2->y - p1->y;
    f2 = sqrt(vx * vx + vy * vy);
    f1 = 1.0f / f2;
    vx *= f1;
    vy *= f1;

    hori->x = kp->sec_nrm_hori.x * vx + kp->sec_nrm_hori.y * vy;
    hori->y = kp->sec_nrm_hori.y * vx - kp->sec_nrm_hori.x * vy;

    return kp->kp_dist_vv1 * f1;
}

/*
 * select_2obj_first
 *
 * Picks the pair of sensor objects that best lines up with the acceleration horizon, when two are visible.
 */
static s8 select_2obj_first(KPADInsideStatus* kp) {
    KPADObject *op1, *op2, *rp1, *rp2;
    Vec2 hori;
    f32 f1, max = kp_err_first_inpr;

    op1 = kp->kobj_sample;
    do {
        if (op1->error_fg != 0) {
            continue;
        }

        op2 = op1 + 1;
        do {
            if (op2->error_fg != 0) {
                continue;
            }

            f1 = calc_horizon(kp, &op1->center, &op2->center, &hori);

            if (f1 <= kp->kp_err_dist_min || f1 >= kp_err_dist_max) {
                continue;
            }

            f1 = kp->acc_horizon.x * hori.x + kp->acc_horizon.y * hori.y;
            if (f1 < 0.0f) {
                if (-f1 > max) {
                    max = -f1;
                    rp1 = op2;
                    rp2 = op1;
                }
            } else {
                if (f1 > max) {
                    max = f1;
                    rp1 = op1;
                    rp2 = op2;
                }
            }

        } while (++op2 <= &kp->kobj_sample[WPAD_DPD_MAX_OBJECTS - 1]);
    } while (++op1 < &kp->kobj_sample[WPAD_DPD_MAX_OBJECTS - 1]);

    if (max == kp_err_first_inpr) {
        return 0;
    }

    kp->kobj_regular[0] = *rp1;
    kp->kobj_regular[1] = *rp2;

    return 2;
} // 0x80278CE0

/*
 * select_2obj_continue
 *
 * Picks the pair of sensor objects that best continues the pair tracked so far (distance and direction).
 */
static s8 select_2obj_continue(KPADInsideStatus* kp) {
    KPADObject *op1, *op2, *rp1, *rp2;
    Vec2 nrm;
    s32 rev_fg;
    f32 f1, f2, f3, vx, vy, min = 2.0f;
    op1 = kp->kobj_sample;
    do {
        if (op1->error_fg != 0) {
            continue;
        }

        op2 = op1 + 1;
        do {
            if (op2->error_fg != 0) {
                continue;
            }
            vx = op2->center.x - op1->center.x;
            vy = op2->center.y - op1->center.y;
            f3 = sqrt(vx * vx + vy * vy);
            f1 = 1.0f / f3;
            nrm.x = vx * f1;
            nrm.y = vy * f1;

            f1 *= kp->kp_dist_vv1;
            if (f1 <= kp->kp_err_dist_min || f1 >= kp_err_dist_max) {
                continue;
            }

            f1 -= kp->sec_dist;
            if (f1 < 0.0f) {
                f1 *= kp->kp_err_dist_speedM_1;
            } else {
                f1 *= kp->kp_err_dist_speed_1;
            }
            if (f1 >= 1.0f) {
                continue;
            }

            f2 = kp->sec_nrm.x * nrm.x + kp->sec_nrm.y * nrm.y;
            if (f2 < 0.0f) {
                f2 = -f2;
                rev_fg = 1;
            } else {
                rev_fg = 0;
            }
            if (f2 <= kp_err_next_inpr) {
                continue;
            }
            f2 = (1.0f - f2) / (1.0f - kp_err_next_inpr);
            f1 += f2;
            if (f1 < min) {
                min = f1;
                if (rev_fg) {
                    rp1 = op2;
                    rp2 = op1;
                } else {
                    rp1 = op1;
                    rp2 = op2;
                }
            }

        } while (++op2 <= &kp->kobj_sample[WPAD_DPD_MAX_OBJECTS - 1]);
    } while (++op1 < &kp->kobj_sample[WPAD_DPD_MAX_OBJECTS - 1]);

    if (min == 2.0f) {
        return 0;
    }

    kp->kobj_regular[0] = *rp1;
    kp->kobj_regular[1] = *rp2;

    return 2;
} // 0x80278ED0

/*
 * select_1obj_first
 *
 * With only one sensor object visible, guesses the other end of the sensor bar from the acceleration horizon and
 * the last trusted bar length; the guess must lie inside the sensor frame.
 */
static s8 select_1obj_first(KPADInsideStatus* kp) {
    KPADObject* op1;
    f32 vx, vy;
    Vec2 p1, p2;

    vx = kp->sec_nrm_hori.x * kp->acc_horizon.x + kp->sec_nrm_hori.y * kp->acc_horizon.y;
    vy = kp->sec_nrm_hori.y * kp->acc_horizon.x - kp->sec_nrm_hori.x * kp->acc_horizon.y;

    vx *= kp->trust_sec_length;
    vy *= kp->trust_sec_length;

    op1 = kp->kobj_sample;
    do {
        if (op1->error_fg != 0) {
            continue;
        }

        p1.x = op1->center.x - vx;
        p1.y = op1->center.y - vy;
        p2.x = op1->center.x + vx;
        p2.y = op1->center.y + vy;

        if (p1.x <= kp->kobj_frame_min.x || p1.x >= kp->kobj_frame_max.x || p1.y <= kp->kobj_frame_min.y ||
            p1.y >= kp->kobj_frame_max.y) {
            if (p2.x > kp->kobj_frame_min.x && p2.x < kp->kobj_frame_max.x && p2.y > kp->kobj_frame_min.y &&
                p2.y < kp->kobj_frame_max.y) {
                kp->kobj_regular[1] = *op1;
                kp->kobj_regular[0].center = p1;
                kp->kobj_regular[0].error_fg = 0;
                kp->kobj_regular[0].state_fg = -1;

                return -1;
            }
        } else {
            if (p2.x <= kp->kobj_frame_min.x || p2.x >= kp->kobj_frame_max.x || p2.y <= kp->kobj_frame_min.y ||
                p2.y >= kp->kobj_frame_max.y) {
                kp->kobj_regular[0] = *op1;

                kp->kobj_regular[1].center = p2;
                kp->kobj_regular[1].error_fg = 0;
                kp->kobj_regular[1].state_fg = -1;

                return -1;
            }
        }
    } while (++op1 < &kp->kobj_sample[WPAD_DPD_MAX_OBJECTS]);

    return 0;
} // 0x80279100

/*
 * select_1obj_continue
 *
 * With one sensor object visible, finds which tracked object it continues and rebuilds the other from the
 * remembered bar direction and length.
 */
static s8 select_1obj_continue(KPADInsideStatus* kp) {
    KPADObject *op1, *op2, *rp1, *rp2;
    f32 f1, vx, vy;
    f32 min = kp_err_near_pos * kp_err_near_pos;

    op1 = kp->kobj_regular;
    do {
        if (op1->error_fg != 0) {
            continue;
        }
        if (op1->state_fg != 0) {
            continue;
        }

        op2 = kp->kobj_sample;
        do {
            if (op2->error_fg != 0) {
                continue;
            }

            vx = op1->center.x - op2->center.x;
            vy = op1->center.y - op2->center.y;
            f1 = vx * vx + vy * vy;
            if (f1 < min) {
                min = f1;
                rp1 = op1;
                rp2 = op2;
            }
        } while (++op2 < &kp->kobj_sample[WPAD_DPD_MAX_OBJECTS]);
    } while (++op1 < &kp->kobj_regular[KPAD_USE_OBJECTS]);

    if (min == kp_err_near_pos * kp_err_near_pos)
        return 0;

    *rp1 = *rp2;
    kp->sec_nrm.x = kp->sec_nrm_hori.x * kp->acc_horizon.x + kp->sec_nrm_hori.y * kp->acc_horizon.y;
    kp->sec_nrm.y = kp->sec_nrm_hori.y * kp->acc_horizon.x - kp->sec_nrm_hori.x * kp->acc_horizon.y;

    vx = kp->sec_length * kp->sec_nrm.x;
    vy = kp->sec_length * kp->sec_nrm.y;
    if (rp1 == &kp->kobj_regular[0]) {
        kp->kobj_regular[1].center.x = rp1->center.x + vx;
        kp->kobj_regular[1].center.y = rp1->center.y + vy;
        kp->kobj_regular[1].error_fg = 0;
        kp->kobj_regular[1].state_fg = -1;
    } else {
        kp->kobj_regular[0].center.x = rp1->center.x - vx;
        kp->kobj_regular[0].center.y = rp1->center.y - vy;
        kp->kobj_regular[0].error_fg = 0;
        kp->kobj_regular[0].state_fg = -1;
    }

    if (kp->status.dpd_valid_fg < 0) {
        return -1;
    } else {
        return 1;
    }
} // 0x802792C0

/*
 * calc_dpd_variable
 *
 * Filters the pointer position, horizon and distance derived from the two tracked sensor objects into the sample
 * handed to the game. valid_fg_next becomes the sample's pointer-valid flag.
 */
static void calc_dpd_variable(KPADInsideStatus* kp, s8 valid_fg_next) {
    KPADStatus* sp = &kp->status;
    f32 f1, f2, f4, dist;
    Vec2 pos, vec;

    if (valid_fg_next == 0) {
        sp->dpd_valid_fg = 0;
        return;
    }

    pos.x = kp->sec_nrm_hori.x * kp->sec_nrm.x + kp->sec_nrm_hori.y * kp->sec_nrm.y;
    pos.y = kp->sec_nrm_hori.y * kp->sec_nrm.x - kp->sec_nrm_hori.x * kp->sec_nrm.y;

    if (!sp->dpd_valid_fg) {
        sp->horizon = pos;
        sp->hori_vec = Vec2_0;
        sp->hori_speed = 0.0f;
    } else {
        vec.x = pos.x - sp->horizon.x;
        vec.y = pos.y - sp->horizon.y;
        f1 = sqrt(vec.x * vec.x + vec.y * vec.y);

        if (kp->hori_play_mode == KPAD_PLAY_MODE_LOOSE) {
            if (f1 >= kp->hori_play_radius) {
                f4 = 1.0f;
            } else {
                f4 = f1 / kp->hori_play_radius;
                f4 *= f4;
                f4 *= f4;
            }
            f4 *= kp->hori_sensitivity;
            vec.x = f4 * vec.x + sp->horizon.x;
            vec.y = f4 * vec.y + sp->horizon.y;
            f4 = sqrt(vec.x * vec.x + vec.y * vec.y);
            vec.x /= f4;
            vec.y /= f4;

            sp->hori_vec.x = vec.x - sp->horizon.x;
            sp->hori_vec.y = vec.y - sp->horizon.y;
            sp->hori_speed = sqrt(sp->hori_vec.x * sp->hori_vec.x + sp->hori_vec.y * sp->hori_vec.y);

            sp->horizon = vec;
        } else if (f1 > kp->hori_play_radius) {
            f4 = kp->hori_sensitivity * ((f1 - kp->hori_play_radius) / f1);
            vec.x = sp->horizon.x + vec.x * f4;
            vec.y = sp->horizon.y + vec.y * f4;
            f4 = sqrt(vec.x * vec.x + vec.y * vec.y);
            vec.x /= f4;
            vec.y /= f4;

            sp->hori_vec.x = vec.x - sp->horizon.x;
            sp->hori_vec.y = vec.y - sp->horizon.y;
            sp->hori_speed = sqrt(sp->hori_vec.x * sp->hori_vec.x + sp->hori_vec.y * sp->hori_vec.y);

            sp->horizon = vec;
        } else {
            sp->hori_vec = Vec2_0;
            sp->hori_speed = 0.0f;
        }
    }

    dist = kp->kp_dist_vv1 / kp->sec_length;

    if (!sp->dpd_valid_fg) {
        sp->dist = dist;
        sp->dist_vec = 0.0f;
        sp->dist_speed = 0.0f;
    } else {
        f2 = dist - sp->dist;
        if (f2 < 0.0f) {
            f1 = -f2;
        } else {
            f1 = f2;
        }

        if (kp->dist_play_mode == KPAD_PLAY_MODE_LOOSE) {
            if (f1 >= kp->dist_play_radius) {
                f1 = 1.0f;
            } else {
                f1 /= kp->dist_play_radius;
                f1 *= f1;
                f1 *= f1;
            }
            f1 *= kp->dist_sensitivity;

            sp->dist_vec = f1 * f2;
            if (sp->dist_vec < 0.0f) {
                sp->dist_speed = -sp->dist_vec;
            } else {
                sp->dist_speed = sp->dist_vec;
            }

            sp->dist += sp->dist_vec;
        } else if (f1 > kp->dist_play_radius) {
            f1 = kp->dist_sensitivity * ((f1 - kp->dist_play_radius) / f1);
            sp->dist_vec = f1 * f2;
            if (sp->dist_vec < 0.0f) {
                sp->dist_speed = -sp->dist_vec;
            } else {
                sp->dist_speed = sp->dist_vec;
            }

            sp->dist += sp->dist_vec;
        } else {
            sp->dist_vec = 0.0f;
            sp->dist_speed = 0.0f;
        }
    }

    pos.x = (kp->kobj_regular[0].center.x + kp->kobj_regular[1].center.x) * 0.5f;
    pos.y = (kp->kobj_regular[0].center.y + kp->kobj_regular[1].center.y) * 0.5f;

    f1 = kp->sec_nrm.x * kp->sec_nrm_hori.x + kp->sec_nrm.y * kp->sec_nrm_hori.y;
    f2 = -kp->sec_nrm.y * kp->sec_nrm_hori.x + kp->sec_nrm.x * kp->sec_nrm_hori.y;
    vec.x = f1 * pos.x - f2 * pos.y;
    vec.y = f2 * pos.x + f1 * pos.y;

    vec.x = (kp->center_org.x - vec.x) * kp->dpd2pos_scale;
    vec.y = (kp->center_org.y - vec.y) * kp->dpd2pos_scale;

    pos.x = -kp->accXY_nrm_hori.y * vec.x + kp->accXY_nrm_hori.x * vec.y;
    pos.y = -kp->accXY_nrm_hori.x * vec.x - kp->accXY_nrm_hori.y * vec.y;
    if (sp->dpd_valid_fg == 0) {
        sp->pos = pos;
        sp->vec = Vec2_0;
        sp->speed = 0.0f;
    } else {
        vec.x = pos.x - sp->pos.x;
        vec.y = pos.y - sp->pos.y;
        f1 = sqrt(vec.x * vec.x + vec.y * vec.y);

        if (kp->pos_play_mode == KPAD_PLAY_MODE_LOOSE) {
            if (f1 >= kp->pos_play_radius) {
                f4 = 1.0f;
            } else {
                f4 = f1 / kp->pos_play_radius;
                f4 *= f4;
                f4 *= f4;
            }
            f4 *= kp->pos_sensitivity;

            sp->vec.x = f4 * vec.x;
            sp->vec.y = f4 * vec.y;
            sp->speed = sqrt(sp->vec.x * sp->vec.x + sp->vec.y * sp->vec.y);

            sp->pos.x += sp->vec.x;
            sp->pos.y += sp->vec.y;
        } else if (f1 > kp->pos_play_radius) {
            f4 = kp->pos_sensitivity * ((f1 - kp->pos_play_radius) / f1);

            sp->vec.x = f4 * vec.x;
            sp->vec.y = f4 * vec.y;
            sp->speed = sqrt(sp->vec.x * sp->vec.x + sp->vec.y * sp->vec.y);

            sp->pos.x += sp->vec.x;
            sp->pos.y += sp->vec.y;
        } else {
            sp->vec = Vec2_0;
            sp->speed = 0.0f;
        }
    }

    sp->dpd_valid_fg = valid_fg_next;
} // 0x80279440

/*
 * get_kobj
 *
 * Converts the sensor objects of a sample to the library's coordinates: centred, and scaled to a width of two.
 */
static void get_kobj(KPADInsideStatus* kp, KPADUnifiedWpadStatus* uwp) {
    const f32 dpd_scale = 2.0f / (f32)KPAD_DPD_RESO_WX;
    const f32 dpd_cx = (f32)(KPAD_DPD_RESO_WX - 1) / (f32)KPAD_DPD_RESO_WX;
    const f32 dpd_cy = (f32)(KPAD_DPD_RESO_WY - 1) / (f32)KPAD_DPD_RESO_WX;

    KPADObject* kobj_p = &kp->kobj_sample[WPAD_DPD_MAX_OBJECTS - 1];
    DPDObject* wobj_p = &uwp->u.core.obj[WPAD_DPD_MAX_OBJECTS - 1];

    do {
        if (wobj_p->size) {
            kobj_p->center.x = (f32)(s32)wobj_p->x * dpd_scale - dpd_cx;
            kobj_p->center.y = (f32)(s32)wobj_p->y * dpd_scale - dpd_cy;

            kobj_p->error_fg = 0;
            kobj_p->state_fg = 0;
        } else {
            kobj_p->error_fg = -1;
        }

        --wobj_p;
    } while (--kobj_p >= kp->kobj_sample);
}

/*
 * check_kobj_outside_frame
 *
 * Flags the sensor objects that lie outside the usable part of the sensor frame.
 */
static void check_kobj_outside_frame(KPADInsideStatus* kp, KPADObject* kobj_t) {
    KPADObject* kobj_p = &kobj_t[WPAD_DPD_MAX_OBJECTS - 1];

    do {
        if (kobj_p->error_fg < 0) {
            continue;
        }

        if (kobj_p->center.x <= kp->kobj_frame_min.x || kobj_p->center.x >= kp->kobj_frame_max.x ||
            kobj_p->center.y <= kp->kobj_frame_min.y || kobj_p->center.y >= kp->kobj_frame_max.y) {
            kobj_p->error_fg |= 1;
        }
    } while (--kobj_p >= kobj_t);
}

/*
 * check_kobj_same_position
 *
 * Flags a sensor object that sits exactly on top of an earlier one.
 */
static void check_kobj_same_position(KPADObject* kobj_t) {
    KPADObject *op1, *op2;

    op1 = kobj_t;
    do {
        if (op1->error_fg != 0) {
            continue;
        }

        op2 = op1 + 1;
        do {
            if (op2->error_fg != 0) {
                continue;
            }

            if (op1->center.x == op2->center.x && op1->center.y == op2->center.y) {
                op2->error_fg |= 2;
            }
        } while (++op2 <= &kobj_t[WPAD_DPD_MAX_OBJECTS - 1]);
    } while (++op1 < &kobj_t[WPAD_DPD_MAX_OBJECTS - 1]);
}

/*
 * calc_obj_horizon
 *
 * Length, direction and apparent distance of the sensor bar from the two tracked objects.
 */
static void calc_obj_horizon(KPADInsideStatus* kp) {
    f32 f1, f2, vx, vy;

    vx = kp->kobj_regular[1].center.x - kp->kobj_regular[0].center.x;
    vy = kp->kobj_regular[1].center.y - kp->kobj_regular[0].center.y;
    f1 = sqrt(vx * vx + vy * vy);
    kp->sec_length = f1;

    f2 = 1.0f / f1;
    kp->sec_dist = kp->kp_dist_vv1 * f2;

    vx *= f2;
    vy *= f2;
    kp->sec_nrm.x = vx;
    kp->sec_nrm.y = vy;

    kp->obj_horizon.x = kp->sec_nrm_hori.x * vx + kp->sec_nrm_hori.y * vy;
    kp->obj_horizon.y = kp->sec_nrm_hori.y * vx - kp->sec_nrm_hori.x * vy;
}

/*
 * read_kpad_dpd
 *
 * Tracks the sensor-bar objects of a sample and turns them into the pointer position, horizon and distance.
 */
static void read_kpad_dpd(KPADInsideStatus* kp, KPADUnifiedWpadStatus* uwp) {
    KPADStatus* sp = &kp->status;
    KPADObject* op1;
    s8 valid_fg_next;

    if (uwp->data_format == WPAD_FMT_CORE_BTN_ACC_DPD || uwp->data_format == WPAD_FMT_FS_BTN_ACC_DPD ||
        uwp->data_format == WPAD_FMT_CLASSIC_BTN_ACC_DPD ||
        (uwp->data_format == WPAD_FMT_GUITAR && kp->dpd_enabled)) {
        get_kobj(kp, uwp);
    } else {
        op1 = &kp->kobj_sample[WPAD_DPD_MAX_OBJECTS - 1];
        do {
            op1->error_fg = -1;
        } while (--op1 >= kp->kobj_sample);
    }

    check_kobj_outside_frame(kp, kp->kobj_sample);
    check_kobj_same_position(kp->kobj_sample);
    kp->valid_objs = 0;
    op1 = &kp->kobj_sample[WPAD_DPD_MAX_OBJECTS - 1];
    do {
        if (op1->error_fg == 0) {
            ++kp->valid_objs;
        }
    } while (--op1 >= kp->kobj_sample);

    if (!(sp->acc_vertical.x <= kp_err_up_inpr)) {
        if (sp->dpd_valid_fg == 2 || sp->dpd_valid_fg == -2) {
            if (kp->valid_objs >= 2) {
                valid_fg_next = select_2obj_continue(kp);
                if (valid_fg_next) {
                    goto LABEL_select_OK;
                }
            }
            if (kp->valid_objs >= 1) {
                valid_fg_next = select_1obj_continue(kp);
                if (valid_fg_next) {
                    goto LABEL_select_OK;
                }
            }
        } else if (sp->dpd_valid_fg == 1 || sp->dpd_valid_fg == -1) {
            if (kp->valid_objs >= 2) {
                valid_fg_next = select_2obj_first(kp);
                if (valid_fg_next) {
                    goto LABEL_select_OK;
                }
            }
            if (kp->valid_objs >= 1) {
                valid_fg_next = select_1obj_continue(kp);
                if (valid_fg_next) {
                    goto LABEL_select_OK;
                }
            }
        } else {
            if (kp->valid_objs >= 2) {
                valid_fg_next = select_2obj_first(kp);
                if (valid_fg_next) {
                    goto LABEL_select_OK;
                }
            }
            if (kp->valid_objs == 1) {
                valid_fg_next = select_1obj_first(kp);
                if (valid_fg_next) {
                    goto LABEL_select_OK;
                }
            }
        }
    }

    valid_fg_next = 0;

LABEL_select_OK:
    if (valid_fg_next) {
        calc_obj_horizon(kp);
        if (kp->ah_circle_ct == 0) {
            if (kp->obj_horizon.x * kp->acc_horizon.x + kp->obj_horizon.y * kp->acc_horizon.y <= kp_err_acc_inpr) {
                valid_fg_next = 0;

                kp->kobj_regular[0].error_fg = kp->kobj_regular[1].error_fg = 1;
            }
        }

        if (sp->dpd_valid_fg == 2 && valid_fg_next == 2) {
            if (kp->dpd_valid2_ct == 200) {
                kp->trust_sec_length = kp->sec_length;
            } else {
                ++kp->dpd_valid2_ct;
            }
        } else {
            kp->dpd_valid2_ct = 0;
        }
    } else {
        kp->dpd_valid2_ct = 0;
    }

    calc_dpd_variable(kp, valid_fg_next);
} // 0x80279A10

/*
 * clamp_stick_circle
 *
 * Maps a raw stick position to the unit circle, ignoring everything inside the dead zone of radius min and
 * saturating at max.
 */
static void clamp_stick_circle(Vec2* stick, s32 sx, s32 sy, s32 min, s32 max) {
    f32 length;
    f32 fx = (f32)sx;
    f32 fy = (f32)sy;
    f32 fmin = (f32)min;
    f32 fmax = (f32)max;

    length = sqrt(fx * fx + fy * fy);

    if (length <= fmin) {
        stick->x = stick->y = 0.0f;

    } else if (length >= fmax) {
        stick->x = fx / length;
        stick->y = fy / length;

    } else {
        length = (length - fmin) / (fmax - fmin) / length;
        stick->x = fx * length;
        stick->y = fy * length;
    }
} // 0x80279E90

/*
 * clamp_trigger
 *
 * Maps a raw trigger or stick axis reading to 0..1 between its dead zone and its saturation value.
 */
static void clamp_trigger(f32* trigger, s32 tr, s32 min, s32 max) {
    if (tr <= min) {
        *trigger = 0.0f;
    } else if (tr >= max) {
        *trigger = 1.0f;
    } else {
        *trigger = (f32)(tr - min) / (f32)(max - min);
    }
}

/*
 * clamp_stick
 *
 * Maps a raw stick position to the unit square axis by axis, then pulls it back inside the unit circle.
 */
static void clamp_stick(Vec2* stick, s32 x, s32 y, s32 min, s32 max) {
    if (x < 0) {
        clamp_trigger(&stick->x, -x, min, max);
        stick->x = -stick->x;
    } else {
        clamp_trigger(&stick->x, x, min, max);
    }
    if (y < 0) {
        clamp_trigger(&stick->y, -y, min, max);
        stick->y = -stick->y;
    } else {
        clamp_trigger(&stick->y, y, min, max);
    }

    if (stick->x * stick->x + stick->y * stick->y > 1.0f) {
        f32 rt = sqrt(stick->x * stick->x + stick->y * stick->y);
        stick->x /= rt;
        stick->y /= rt;
    }
} // 0x80279FC0

/*
 * reset_bb_measurement
 *
 * Abandons the Balance Board's zero-point measurement. Name invented.
 */
static inline void reset_bb_measurement(void) {
    s32 i;

    lbl_8052B082 = 0;
    lbl_8052B080 = 0;
    lbl_8052B083 = 0;

    for (i = 0; i < 4; i++) {
        lbl_804F1368[i] = 0.0;
        lbl_804F1388[i] = 0.0;
    }
}

/*
 * reset_ex_fs
 *
 * Puts the extension state of a Nunchuk back to its defaults once, after reset_kpad has raised the flag.
 */
static inline void reset_ex_fs(KPADInsideStatus* kp, KPADEXStatus* ep) {
    if (kp->dev_changed) {
        kp->dev_changed = FALSE;
        ep->fs.stick = Vec2_0;
        ep->fs.acc.z = 0.0f;
        ep->fs.acc.x = 0.0f;
        ep->fs.acc.y = -1.0f;
        ep->fs.acc_value = 1.0f;
        ep->fs.acc_speed = 0.0f;
    }
}

/*
 * reset_ex_cl
 *
 * The same for a Classic Controller, or for the controllers that share its layout.
 */
static inline void reset_ex_cl(KPADInsideStatus* kp, KPADEXStatus* ep) {
    if (kp->dev_changed) {
        kp->dev_changed = FALSE;
        ep->cl.lstick = Vec2_0;
        ep->cl.rstick = Vec2_0;
        ep->cl.rtrigger = 0.0f;
        ep->cl.ltrigger = 0.0f;
        ep->cl.release = 0;
        ep->cl.trig = 0;
        ep->cl.hold = 0;
        kp->btn_cl_repeat_time = 0;
        kp->btn_cl_repeat_next = kp->btn_repeat_delay;
    }
}

/*
 * read_kpad_stick
 *
 * Converts the analogue inputs of the attached extension (Nunchuk, Classic Controller, guitar, train controller)
 * to the ranges of KPADEXStatus, and runs the Balance Board's weight measurement. The reference's version
 * handles only the first two; the rest is the 2009 SDK's.
 */
static void read_kpad_stick(KPADInsideStatus* kp, KPADUnifiedWpadStatus* uwp) {
    void (*clamp)(Vec2*, s32, s32, s32, s32);
    KPADEXStatus* ep = &kp->status.ex_status;
    s32 i;

    clamp = lbl_8052B060 ? clamp_stick : clamp_stick_circle;

    if (uwp->u.core.dev == WPAD_DEV_FS && uwp->data_format >= WPAD_FMT_FS_BTN &&
        uwp->data_format <= WPAD_FMT_FS_BTN_ACC_DPD) {
        reset_ex_fs(kp, ep);
        clamp(&ep->fs.stick, uwp->u.fs.fsStickX, uwp->u.fs.fsStickY, kp_fs_fstick_min, kp_fs_fstick_max);
    } else if (uwp->u.core.dev == WPAD_DEV_CLASSIC && uwp->data_format >= WPAD_FMT_CLASSIC_BTN &&
               uwp->data_format <= WPAD_FMT_CLASSIC_BTN_ACC_DPD) {
        reset_ex_cl(kp, ep);
        clamp(&ep->cl.lstick, uwp->u.cl.clLStickX, uwp->u.cl.clLStickY, kp_cl_stick_min, kp_cl_stick_max);
        clamp(&ep->cl.rstick, uwp->u.cl.clRStickX, uwp->u.cl.clRStickY, kp_cl_stick_min, kp_cl_stick_max);
        clamp_trigger(&ep->cl.ltrigger, uwp->u.cl.clTriggerL, kp_cl_trigger_min, kp_cl_trigger_max);
        clamp_trigger(&ep->cl.rtrigger, uwp->u.cl.clTriggerR, kp_cl_trigger_min, kp_cl_trigger_max);
    } else if (uwp->u.core.dev == WPAD_DEV_GUITAR && uwp->data_format == WPAD_FMT_GUITAR) {
        reset_ex_cl(kp, ep);
        clamp(&ep->cl.lstick, uwp->u.cl.clLStickX, uwp->u.cl.clLStickY, kp_cl_stick_min, kp_cl_stick_max);
        clamp_trigger(&ep->cl.rstick.x, uwp->u.cl.clRStickX, lbl_8052B068, lbl_80528E18);
        clamp_trigger(&ep->cl.rstick.y, uwp->u.cl.clRStickY, lbl_8052B068, lbl_80528E18);
        clamp_trigger(&ep->cl.ltrigger, uwp->u.cl.clTriggerL, lbl_8052B064, lbl_80528E14);
        clamp_trigger(&ep->cl.rtrigger, uwp->u.cl.clTriggerR, lbl_8052B064, lbl_80528E14);
    } else if (uwp->u.core.dev == WPAD_DEV_TRAIN && uwp->data_format == WPAD_FMT_TRAIN) {
        reset_ex_cl(kp, ep);
        ep->cl.lstick.x = ep->cl.lstick.y = 0.0f;
        ep->cl.rstick.x = ep->cl.rstick.y = 0.0f;
        clamp_trigger(&ep->cl.ltrigger, uwp->u.tr.brake, lbl_8052B064, lbl_80528E14);
        clamp_trigger(&ep->cl.rtrigger, uwp->u.tr.mascon, lbl_8052B064, lbl_80528E14);
    } else if (uwp->u.core.dev == WPAD_DEV_BALANCE_CHECKER && uwp->data_format == WPAD_FMT_BALANCE_CHECKER) {
        if (kp->dev_changed) {
            kp->dev_changed = FALSE;
            reset_bb_measurement();
        }

        if (!WBCGetBatteryLevel(uwp->u.bl.battery)) {
            ep->bl.err = -1;
        } else if (lbl_8052B082 < 3) {
            ep->bl.err = -2;
        } else if (uwp->u.bl.temp == 0x7F || uwp->u.bl.temp == -0x80) {
            ep->bl.err = -3;
        } else {
            ep->bl.err = WBCReadDummy(&uwp->u.bl, ep->bl.cell, 4);
            if (ep->bl.err >= 0) {
                for (i = 0; i < 4; i++) {
                    lbl_804F1388[i] = (lbl_804F1388[i] * lbl_80528E20 + ep->bl.cell[i]) / (lbl_80528E20 + 1.0f);
                }

                if (lbl_8052B083) {
                    lbl_8052B080++;
                    for (i = 0; i < 4; i++) {
                        lbl_804F1368[i] = (lbl_804F1368[i] * (lbl_8052B080 - 1) + ep->bl.cell[i]) / lbl_8052B080;
                    }

                    if (lbl_8052B080 == lbl_80528E20) {
                        lbl_8052B083 = FALSE;
                        ep->bl.err = WBCGetTGCWeightDummy(lbl_804F1368[0] + lbl_804F1368[1] + lbl_804F1368[2] +
                                                              lbl_804F1368[3],
                                                          &lbl_8052B078, &uwp->u.bl);
                        if (lbl_8052B078 < -0.5) {
                            ep->bl.err = -4;
                        }
                    }
                }
            }

            for (i = 0; i < 4; i++) {
                ep->bl.cell_avg[i] = lbl_804F1388[i];
            }
            ep->bl.weight = lbl_8052B078;
            ep->bl.zeroing = lbl_8052B083;
        }
    }
} // 0x8027A1F0

/*
 * KPADRead
 *
 * Reads the samples gathered since the last call.
 */
s32 KPADRead(s32 chan, KPADStatus* sampling_bufs, s32 length) {
    return KPADiRead(chan, sampling_bufs, length, NULL, FALSE);
} // 0x8027AAC0

static void KPADiControlDpdCallback(s32 chan, s32 result);

// Buttons of the Wii Remote proper (the Z and C bits of a Nunchuk travel in the same word but belong to the extension).
#define KPAD_CORE_BUTTONS                                                                                      \
    (WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT | WPAD_BUTTON_DOWN | WPAD_BUTTON_UP | WPAD_BUTTON_PLUS |             \
     WPAD_BUTTON_2 | WPAD_BUTTON_1 | WPAD_BUTTON_B | WPAD_BUTTON_A | WPAD_BUTTON_MINUS | WPAD_BUTTON_HOME)

// No button state read yet (a value no 16-bit button word can be mistaken for). Name invented.
#define KPAD_BUTTONS_UNSET 0xffff

// The slot of the ring buffer at idx: the controller's own 16 samples, then the caller's extension.
static inline KPADUnifiedWpadStatus* ring_slot(KPADInsideStatus* kp, s32 idx) {
    if (idx >= KPAD_RING_SAMPLES) {
        return &kp->uniRingBufExt[idx - KPAD_RING_SAMPLES];
    }
    return &kp->uniRingBuf[idx];
}

/*
 * KPADiRead
 *
 * Turns the samples gathered since the last call into KPADStatus entries, newest first. The raw samples are first
 * staged in the caller's buffer (a KPADStatus is larger than a sample) and converted in place, oldest first. The
 * newest sample stays in the ring as the history of the next call, so a read yields one entry fewer than it takes.
 * The result is the number of entries; *error says why there are none. keep_first is the Ex variant of the read:
 * it leaves entry 0 as it was when there is nothing new, reports a missing controller through entry 0, and keeps
 * the pointer-valid flag of entries that carry an error.
 */
s32 KPADiRead(s32 chan, KPADStatus* sampling_bufs, s32 length, s32* error, BOOL keep_first) {
    KPADInsideStatus* kp = &inside_kpads[chan];
    KPADStatus first_status;
    KPADStatus* out;
    KPADUnifiedWpadStatus* raw;
    u32 fs_button;
    s32 idx;
    s32 remaining;
    KPADStatus* cur;
    KPADStatus* dst;
    KPADUnifiedWpadStatus* slot;
    u32 dev_type;
    u32 core_button;
    u32 cl_button;
    BOOL enabled;
    BOOL corrupted;
    u32 taken;
    s32 i;
    u32 avail;
    s32 err;
    BOOL dev_changed;
    s32 count;

    err = KPAD_READ_ERR_NONE;
    count = 0;
    if (!kpad_initialized) {
        err = KPAD_READ_ERR_UNINITIALIZED;
    } else if (WPADGetStatus() != WPAD_STATE_SETUP) {
        err = KPAD_READ_ERR_NOT_READY;
    } else {
        enabled = OSDisableInterrupts();
        if (kp->reading) {
            OSRestoreInterrupts(enabled);
            err = KPAD_READ_ERR_BUSY;
        } else {
            kp->reading = TRUE;
            if (WPADProbe(chan, NULL) == WPAD_ERR_NO_CONTROLLER) {
                reset_kpad(kp);
                if (kp->dpd_ctrl_callback != NULL && kp->dpd_off_sent && !kp->dpd_on_sent) {
                    KPADiControlDpdCallback(chan, WPAD_ERR_OK);
                }
                kp->reading = FALSE;
                OSRestoreInterrupts(enabled);
                err = KPAD_READ_ERR_NO_CONTROLLER;
            } else {
                OSRestoreInterrupts(enabled);

                if (kp->needs_reset) {
                    kp->status.wpad_err = WPAD_ERR_INVALID;
                    reset_kpad(kp);
                }

                first_status = sampling_bufs[0];

                if (kp->ring_count > 1 && sampling_bufs != NULL && length != 0) {
                    enabled = OSDisableInterrupts();

                    // Of the samples to take, mark those that cannot be trusted: the one before a corrupted sample
                    // and everything older than a change of device.
                    avail = kp->ring_count;
                    if (avail > length) {
                        avail = length;
                    }
                    i = avail;
                    corrupted = FALSE;
                    dev_changed = FALSE;
                    idx = kp->ring_head - 1;
                    if (idx < 0) {
                        idx += kp->uniRingBufExtLength + KPAD_RING_SAMPLES;
                    }
                    slot = ring_slot(kp, idx);
                    do {
                        if (corrupted) {
                            if (slot->u.core.err == WPAD_ERR_OK) {
                                slot->u.core.err = WPAD_ERR_CORRUPTED;
                                corrupted = FALSE;
                            } else if (slot->u.core.err == WPAD_ERR_BUSY) {
                                corrupted = FALSE;
                            }
                        } else if (slot->u.core.err == WPAD_ERR_CORRUPTED) {
                            corrupted = TRUE;
                        }
                        if (kp->status.dev_type != slot->u.core.dev) {
                            dev_changed = TRUE;
                        }
                        if (dev_changed) {
                            slot->u.core.err = WPAD_ERR_INVALID;
                        }
                        idx--;
                        if (idx < 0) {
                            idx += kp->uniRingBufExtLength + KPAD_RING_SAMPLES;
                        }
                        slot = ring_slot(kp, idx);
                    } while (--i > 0);

                    taken = 1;
                    kp->ring_count = taken;
                    if (avail > 1) {
                        taken = avail - 1;
                    }

                    // Stage the samples in the caller's buffer, oldest in the last entry.
                    i = taken;
                    dst = sampling_bufs + taken;
                    idx = kp->ring_head - taken - 1;
                    if (idx < 0) {
                        idx += kp->uniRingBufExtLength + KPAD_RING_SAMPLES;
                    }
                    do {
                        slot = ring_slot(kp, idx);
                        dst--;
                        *(KPADUnifiedWpadStatus*)dst = *slot;
                        idx++;
                        if (idx >= kp->uniRingBufExtLength + KPAD_RING_SAMPLES) {
                            idx = 0;
                        }
                    } while (--i > 0);
                    OSRestoreInterrupts(enabled);

                    count = taken;
                    remaining = count;
                    out = &sampling_bufs[count];
                    cur = out;
                    if (kp->btn_per_sample == 1) {
                        do {
                            cur--;
                            raw = (KPADUnifiedWpadStatus*)cur;
                            if (kp->status.dev_type != raw->u.core.dev) {
                                kp->status.dev_type = raw->u.core.dev;
                                kp->dev_changed = TRUE;
                            }
                            kp->status.wpad_err = raw->u.core.err;
                            kp->status.data_format = raw->data_format;
                            dev_type = kp->status.dev_type;
                            core_button = fs_button = cl_button = KPAD_BUTTONS_UNSET;
                            switch (raw->u.core.err) {
                            case WPAD_ERR_OK:
                                if (dev_type == WPAD_DEV_FS) {
                                    fs_button = raw->u.core.button;
                                    cl_button = 0;
                                } else if (dev_type == WPAD_DEV_CLASSIC || dev_type == WPAD_DEV_TRAIN ||
                                           dev_type == WPAD_DEV_GUITAR) {
                                    cl_button = raw->u.cl.clButton;
                                    fs_button = 0;
                                } else {
                                    cl_button = 0;
                                    fs_button = 0;
                                }
                                core_button = raw->u.core.button & KPAD_CORE_BUTTONS;
                                break;
                            case WPAD_ERR_CORRUPTED:
                                cl_button = 0;
                                fs_button = 0;
                            case WPAD_ERR_BUSY:
                                core_button = raw->u.core.button & KPAD_CORE_BUTTONS;
                                break;
                            }
                            if (core_button == KPAD_BUTTONS_UNSET) {
                                core_button = kp->status.hold & KPAD_CORE_BUTTONS;
                            }
                            if (fs_button == KPAD_BUTTONS_UNSET) {
                                fs_button = kp->status.hold;
                            }
                            if (cl_button == KPAD_BUTTONS_UNSET) {
                                cl_button = kp->status.ex_status.cl.hold;
                            }
                            read_kpad_button(kp, dev_type, 1, core_button, fs_button, cl_button);

                            switch (raw->u.core.err) {
                            case WPAD_ERR_OK:
                                read_kpad_stick(kp, raw);
                            case WPAD_ERR_CORRUPTED:
                                read_kpad_acc(kp, raw);
                                read_kpad_dpd(kp, raw);
                                break;
                            default:
                                if (!keep_first) {
                                    kp->status.dpd_valid_fg = 0;
                                }
                                break;
                            }
                            *cur = kp->status;
                        } while (--remaining > 0);
                    } else {
                        core_button = fs_button = cl_button = KPAD_BUTTONS_UNSET;
                        do {
                            cur--;
                            raw = (KPADUnifiedWpadStatus*)cur;
                            if (kp->status.dev_type != raw->u.core.dev) {
                                kp->status.dev_type = raw->u.core.dev;
                                kp->dev_changed = TRUE;
                            }
                            dev_type = raw->u.core.dev;
                            switch (raw->u.core.err) {
                            case WPAD_ERR_OK:
                                if (dev_type == WPAD_DEV_FS) {
                                    fs_button = raw->u.core.button;
                                    cl_button = 0;
                                } else if (dev_type == WPAD_DEV_CLASSIC || dev_type == WPAD_DEV_TRAIN ||
                                           dev_type == WPAD_DEV_GUITAR) {
                                    cl_button = raw->u.cl.clButton;
                                    fs_button = 0;
                                } else {
                                    cl_button = 0;
                                    fs_button = 0;
                                }
                            case WPAD_ERR_CORRUPTED:
                            case WPAD_ERR_BUSY:
                                core_button = raw->u.core.button & KPAD_CORE_BUTTONS;
                                break;
                            }
                        } while (--remaining > 0);
                        if (core_button == KPAD_BUTTONS_UNSET) {
                            core_button = kp->status.hold & KPAD_CORE_BUTTONS;
                        }
                        if (fs_button == KPAD_BUTTONS_UNSET) {
                            fs_button = kp->status.hold;
                        }
                        if (cl_button == KPAD_BUTTONS_UNSET) {
                            cl_button = kp->status.ex_status.cl.hold;
                        }
                        read_kpad_button(kp, dev_type, count, core_button, fs_button, cl_button);

                        remaining = count;
                        cur = out;
                        do {
                            cur--;
                            raw = (KPADUnifiedWpadStatus*)cur;
                            kp->status.wpad_err = raw->u.core.err;
                            kp->status.data_format = raw->data_format;
                            switch (raw->u.core.err) {
                            case WPAD_ERR_OK:
                                read_kpad_stick(kp, raw);
                            case WPAD_ERR_CORRUPTED:
                                read_kpad_acc(kp, raw);
                                read_kpad_dpd(kp, raw);
                                break;
                            default:
                                if (!keep_first) {
                                    kp->status.dpd_valid_fg = 0;
                                }
                                break;
                            }
                            *cur = kp->status;
                        } while (--remaining > 0);
                    }
                }
                kp->reading = FALSE;
            }
        }
    }

    if (count == 0) {
        if (err == KPAD_READ_ERR_NONE) {
            if (keep_first) {
                sampling_bufs[0] = first_status;
            }
            err = KPAD_READ_ERR_NO_DATA;
        } else if (err == KPAD_READ_ERR_NO_CONTROLLER) {
            if (keep_first) {
                sampling_bufs[0].dev_type = WPAD_DEV_NONE;
                sampling_bufs[0].data_format = 0;
                sampling_bufs[0].wpad_err = WPAD_ERR_NO_CONTROLLER;
            }
        }
    }

    if (error != NULL) {
        *error = err;
    }
    return count;
}

/*
 * KPADInit
 *
 * Initialises the library with only its own sample ring buffers.
 */
void KPADInit(void) {
    KPADInitEx(NULL, 0);
} // 0x8027B200

/*
 * KPADSetObjInterval
 *
 * Sets the time between two sensor samples, from which the nearest believable distance of the remote follows.
 * Not part of the retail image (the linker dropped it); KPADInitEx has it expanded.
 */
void KPADSetObjInterval(f32 interval) {
    BOOL enabled = OSDisableInterrupts();

    kp_obj_interval = interval;
    // 0.383864f = tan(21 degrees), the half angle of the sensor's view
    kp_err_dist_min = interval / 0.383864f;
    kp_dist_vv1 = interval / 0.383864f;
    OSRestoreInterrupts(enabled);
}

/*
 * calc_dpd2pos_scale
 *
 * Scale that maps a position in the sensor frame, measured from the sensor bar's centre, to the pointer range.
 */
static inline void calc_dpd2pos_scale(KPADInsideStatus* kp) {
    f32 sx, sy;
    f32 scale;

    sx = 1.0f;
    sy = (f32)KPAD_DPD_RESO_WY / (f32)KPAD_DPD_RESO_WX;

    scale = sqrt(sx * sx + sy * sy);

    if (kp->center_org.x < 0.0f) {
        sx += kp->center_org.x;
    } else {
        sx -= kp->center_org.x;
    }

    if (kp->center_org.y < 0.0f) {
        sy += kp->center_org.y;
    } else {
        sy -= kp->center_org.y;
    }

    kp->dpd2pos_scale = scale / (sx < sy ? sx : sy);
}

/*
 * invalidate_samples
 *
 * Marks every sample slot of a controller, in the library's ring and in the caller's extension, as holding no
 * controller. Name invented.
 */
static void invalidate_samples(KPADInsideStatus* kp) {
    u32 i;

    for (i = 0; i < KPAD_RING_SAMPLES; i++) {
        kp->uniRingBuf[i].u.core.err = WPAD_ERR_NO_CONTROLLER;
    }

    for (i = 0; i < kp->uniRingBufExtLength; i++) {
        kp->uniRingBufExt[i].u.core.err = WPAD_ERR_NO_CONTROLLER;
    }
}

static void KPADiSamplingCallback(s32 chan);
static void KPADiConnectCallback(s32 chan, s32 result);

/*
 * KPADInitEx
 *
 * Initialises the library once. A caller's buffer of length samples extends the sample ring buffers; it is shared
 * out equally between the four controllers.
 */
void KPADInitEx(KPADUnifiedWpadStatus* uwp_bufs, u32 length) {
    s32 i;
    KPADInsideStatus* kp;

    if (!kpad_initialized) {
        WPADInit();
        memset(inside_kpads, 0, sizeof(inside_kpads));
        kp_err_dist_max = 1.0f + WPADGetDpdSensitivity();

        i = 0;
        do {
            kp = &inside_kpads[i];

            kp->connect_callback = WPADSetConnectCallback(i, KPADiConnectCallback);
            kp->sampling_callback = WPADSetSamplingCallback(i, KPADiSamplingCallback);

            kp->dpd_enabled = FALSE;
            lbl_8052B086 = FALSE;
            lbl_8052B085 = FALSE;
            lbl_8052B084 = FALSE;
            reset_bb_measurement();
            kp->want_dpd = TRUE;
            kp->dpd_cmd = WPADGetDpdFormat(i);

            kp->status.dev_type = WPAD_DEV_NONE;
            kp->status.data_format = 0;

            kp->dist_org = idist_org;
            kp->accXY_nrm_hori = iaccXY_nrm_hori;
            kp->sec_nrm_hori = isec_nrm_hori;
            kp->center_org = icenter_org;

            calc_dpd2pos_scale(kp);

            kp->pos_play_radius = kp->hori_play_radius = kp->dist_play_radius = kp->acc_play_radius = 0.0f;
            kp->pos_sensitivity = kp->hori_sensitivity = kp->dist_sensitivity = kp->acc_sensitivity = 1.0f;
            kp->pos_play_mode = kp->hori_play_mode = kp->dist_play_mode = kp->acc_play_mode = KPAD_PLAY_MODE_LOOSE;
            KPADSetBtnRepeat(i, 0.0f, 0.0f);

            kp->btn_per_sample = FALSE;
            kp->field_0x55C = TRUE;
            kp->field_0x55D = TRUE;
            kp->reload_acc_scale = TRUE;
            kp->fs_acc_rotate = FALSE;

            if (length != 0 && uwp_bufs != NULL) {
                kp->uniRingBufExtLength = length / WPAD_MAX_CONTROLLERS;
                kp->uniRingBufExt = uwp_bufs + kp->uniRingBufExtLength * i;
            } else {
                kp->uniRingBufExtLength = 0;
                kp->uniRingBufExt = NULL;
            }

            invalidate_samples(kp);
        } while (++i < WPAD_MAX_CONTROLLERS);

        WPADSetCallbackByKPAD(TRUE);
        KPADSetObjInterval(kp_obj_interval);

        kp = &inside_kpads[WPAD_MAX_CONTROLLERS - 1];
        for (i = WPAD_MAX_CONTROLLERS - 1; i >= 0; i--) {
            if (WPADGetStatus() == WPAD_STATE_SETUP) {
                WPADControlMotor(i, WPAD_MOTOR_STOP);
            }

            kp->needs_reset = TRUE;
            kp--;
        }

        fs_acc_rotation[0][0] = 1.0f;
        fs_acc_rotation[0][1] = 0.0f;
        fs_acc_rotation[0][2] = 0.0f;
        fs_acc_rotation[0][3] = 0.0f;
        fs_acc_rotation[1][0] = 0.0f;
        fs_acc_rotation[1][1] = cos(0.017453292f * kp_fs_acc_rot_deg);
        fs_acc_rotation[1][2] = -sin(0.017453292f * kp_fs_acc_rot_deg);
        fs_acc_rotation[1][3] = 0.0f;
        fs_acc_rotation[2][0] = 0.0f;
        fs_acc_rotation[2][1] = sin(0.017453292f * kp_fs_acc_rot_deg);
        fs_acc_rotation[2][2] = cos(0.017453292f * kp_fs_acc_rot_deg);
        fs_acc_rotation[2][3] = 0.0f;

        kpad_initialized = TRUE;
        OSRegisterVersion(__KPADVersion);
    }
} // 0x8027B210

/*
 * KPADiConnectCallback
 *
 * WPAD's connect callback for every controller: sets a connecting controller up, or voids the samples of one that
 * went away, then passes the event on to the game's callback.
 */
static void KPADiConnectCallback(s32 chan, s32 result) {
    KPADInsideStatus* kp = &inside_kpads[chan];

    if (result == WPAD_ERR_OK) {
        kp->field_0x570 = 0;
        kp->reload_acc_scale = TRUE;

        // Replace the sampling callback without WPAD warning that KPAD's own was overwritten.
        WPADSetCallbackByKPAD(FALSE);
        WPADSetSamplingCallback(chan, KPADiSamplingCallback);
        WPADSetCallbackByKPAD(TRUE);

        kp->dpd_enabled = FALSE;
        kp->dpd_cmd = 0;

        if (chan == WPAD_CHAN3) {
            lbl_8052B086 = FALSE;
            lbl_8052B085 = FALSE;
            lbl_8052B084 = FALSE;
            reset_bb_measurement();
        }
    } else {
        invalidate_samples(kp);
    }

    if (kp->connect_callback != NULL) {
        kp->connect_callback(chan, result);
    }
} // 0x8027B660

/*
 * KPADSetConnectCallback
 *
 * Installs the game's connect callback (called from KPAD's own) and returns the previous one.
 */
WPADConnectCallback KPADSetConnectCallback(s32 chan, WPADConnectCallback cb) {
    BOOL enabled;
    WPADConnectCallback old;

    enabled = OSDisableInterrupts();
    old = inside_kpads[chan].connect_callback;
    inside_kpads[chan].connect_callback = cb;
    OSRestoreInterrupts(enabled);
    return old;
} // 0x8027B7D0

/*
 * fn_8027B830
 *
 * Completion callback of the first Balance Board command: remembers whether it succeeded and frees the command
 * slot. Name unknown.
 */
static void fn_8027B830(s32 chan, s32 result) {
    lbl_8052B085 = !result;
    lbl_8052B086 = FALSE;
} // 0x8027B830

/*
 * fn_8027B850
 *
 * Completion callback of the Balance Board's measurement command: sets the measurement phase to 1 on success and
 * frees the command slot (lbl_8052B082 is 0 or 1 after it runs). Name unknown.
 */
static void fn_8027B850(s32 chan, s32 result) {
    lbl_8052B082 = !result;
    lbl_8052B086 = FALSE;
} // 0x8027B850

/*
 * KPADiControlDpdCallback
 *
 * Completion callback of the pointer-sensor (DPD) control commands.
 */
static void KPADiControlDpdCallback(s32 chan, s32 result) {
    KPADInsideStatus* kp = &inside_kpads[chan];

    if (result == WPAD_ERR_OK && kp->dpd_ctrl_callback != NULL && !kp->dpd_on_sent) {
        kp->dpd_on_sent = TRUE;
        kp->dpd_ctrl_callback(chan, 1);
        kp->dpd_off_sent = FALSE;
    }

    kp->dpd_enabled = WPADIsDpdEnabled(chan);
    kp->dpd_cmd_busy = FALSE;
} // 0x8027B870

// Pointer sensor command and data format WPAD is asked for (name invented).
typedef struct DpdFormat {
    u8 dpd;
    u8 fmt;
} DpdFormat;

/*
 * KPADiSamplingCallback
 *
 * WPAD's sampling callback for every controller: takes the newest sample into the controller's ring buffer, keeps
 * the controller's configuration (sensor calibration, sensor bar position, data format, pointer sensor command) in
 * line with the attached device, runs the Balance Board's zero-point measurement and finally calls the callback
 * that WPAD held before KPAD's own.
 */
static void KPADiSamplingCallback(s32 chan) {
    KPADInsideStatus* kp = &inside_kpads[chan];
    KPADUnifiedWpadStatus* uwp;
    u32 type;
    u32 head;
    u32 idx;
    u8 dpd;
    WPADAccGravityUnit acc = {1, 1, 1};
    // Pointer sensor command and data format of each device, with the pointer sensor off and on.
    static DpdFormat table[] = {
        {WPAD_DPD_DISABLE, WPAD_FMT_CORE_BTN_ACC},          {WPAD_DPD_STANDARD, WPAD_FMT_CORE_BTN_ACC_DPD},
        {WPAD_DPD_DISABLE, WPAD_FMT_FS_BTN_ACC},            {WPAD_DPD_BASIC, WPAD_FMT_FS_BTN_ACC_DPD},
        {WPAD_DPD_DISABLE, WPAD_FMT_CLASSIC_BTN_ACC},       {WPAD_DPD_BASIC, WPAD_FMT_CLASSIC_BTN_ACC_DPD},
        {WPAD_DPD_DISABLE, WPAD_FMT_GUITAR},                {WPAD_DPD_BASIC, WPAD_FMT_GUITAR},
        {WPAD_DPD_DISABLE, WPAD_FMT_BALANCE_CHECKER},       {WPAD_DPD_DISABLE, WPAD_FMT_BALANCE_CHECKER},
        {WPAD_DPD_DISABLE, WPAD_FMT_TRAIN},                 {WPAD_DPD_DISABLE, WPAD_FMT_TRAIN},
    };

    if (WPADProbe(chan, &type) == WPAD_ERR_NO_CONTROLLER) {
        goto done;
    }

    head = kp->ring_head;
    if (head >= kp->uniRingBufExtLength + KPAD_RING_SAMPLES) {
        head = 0;
    }

    if (head >= KPAD_RING_SAMPLES) {
        uwp = &kp->uniRingBufExt[head - KPAD_RING_SAMPLES];
    } else {
        uwp = &kp->uniRingBuf[head];
    }

    WPADRead(chan, &uwp->u.core);
    uwp->data_format = WPADGetDataFormat(chan);
    kp->ring_head = head + 1;

    if (kp->ring_count < kp->uniRingBufExtLength + KPAD_RING_SAMPLES) {
        kp->ring_count++;
    }

    if (type != kp->field_0x570 && type == WPAD_DEV_FS) {
        kp->reload_acc_scale = TRUE;
    }

    if (kp->reload_acc_scale) {
        WPADGetAccGravityUnit(chan, WPAD_ACC_GRAVITY_UNIT_CORE, &acc);
        if (acc.x * acc.y * acc.z != 0) {
            kp->acc_scale.x = 1.0f / acc.x;
            kp->acc_scale.y = 1.0f / acc.y;
            kp->acc_scale.z = 1.0f / acc.z;
        } else {
            kp->acc_scale.x = 0.01f;
            kp->acc_scale.y = 0.01f;
            kp->acc_scale.z = 0.01f;
        }

        WPADGetAccGravityUnit(chan, WPAD_ACC_GRAVITY_UNIT_FS, &acc);
        if (acc.x * acc.y * acc.z != 0) {
            kp->fs_acc_scale.x = 1.0f / acc.x;
            kp->fs_acc_scale.y = 1.0f / acc.y;
            kp->fs_acc_scale.z = 1.0f / acc.z;
        } else {
            kp->fs_acc_scale.x = 0.005f;
            kp->fs_acc_scale.y = 0.005f;
            kp->fs_acc_scale.z = 0.005f;
        }

        kp->reload_acc_scale = FALSE;
    }

    if (kp->field_0x55C) {
        f32 shift;

        if (kp->field_0x55D) {
            if (WPADGetSensorBarPosition() == WPAD_SENSOR_BAR_TOP) {
                shift = 0.2f;
            } else {
                shift = -0.2f;
            }
        } else {
            shift = 0.0f;
        }

        kp->center_org.x = 0.0f;
        kp->center_org.y = -shift;
        calc_dpd2pos_scale(kp);
        kp->field_0x55C = FALSE;
    }

    switch (type) {
    case WPAD_DEV_CORE:
    case WPAD_DEV_FUTURE:
    case WPAD_DEV_NOT_SUPPORTED:
    case WPAD_DEV_INITIALIZING:
        idx = 0;
        break;
    case WPAD_DEV_FS:
        idx = 2;
        break;
    case WPAD_DEV_CLASSIC:
        idx = 4;
        break;
    case WPAD_DEV_GUITAR:
        idx = 6;
        break;
    case WPAD_DEV_BALANCE_CHECKER:
        idx = 8;
        break;
    case WPAD_DEV_TRAIN:
        idx = 10;
        break;
    default:
        goto done;
    }

    if (kp->want_dpd) {
        idx++;
    }

    dpd = WPADIsDpdEnabled(chan) ? kp->dpd_cmd : 0;

    if (dpd != table[idx].dpd) {
        if (kp->dpd_ctrl_callback != NULL && !kp->dpd_off_sent) {
            kp->dpd_off_sent = TRUE;
            kp->dpd_ctrl_callback(chan, 0);
            kp->dpd_on_sent = FALSE;
        }

        if (!kp->dpd_cmd_busy) {
            kp->dpd_cmd_busy = TRUE;
            if (WPADControlDpd(chan, table[idx].dpd, KPADiControlDpdCallback) == 0) {
                kp->dpd_cmd = table[idx].dpd;
            }
        }
    } else if (uwp->data_format != table[idx].fmt) {
        WPADSetDataFormat(chan, table[idx].fmt);
    }

    if (type != WPAD_DEV_BALANCE_CHECKER) {
        goto done;
    }

    // Balance Board: issue the commands that prepare it, then measure its zero point (see lbl_8052B082).
    if (!lbl_8052B086 && !lbl_8052B085) {
        if (WPADControlBLC(chan, 0xAA, fn_8027B830) == 0) {
            lbl_8052B086 = TRUE;
        }
    } else if (lbl_8052B085 && !lbl_8052B084) {
        if (!lbl_8052B086) {
            if (WBCSetupCalibration() == 0) {
                lbl_8052B086 = TRUE;
            }
        } else {
            lbl_8052B084 = WBCGetCalibrationStatus();
            lbl_8052B086 = lbl_8052B084 == 0;
        }
    } else if (!lbl_8052B086 && lbl_8052B084 && lbl_8052B082 < 3) {
        switch (lbl_8052B082) {
        case 0:
            if (WPADControlBLC(chan, 0, fn_8027B850) == 0) {
                lbl_8052B086 = TRUE;
            }
            break;
        case 1:
            if (uwp->u.bl.temp == 0x7F || uwp->u.bl.temp == -0x80) {
                reset_bb_measurement();
            } else {
                lbl_8052B080++;
                if (lbl_8052B080 > lbl_80528E1C) {
                    lbl_8052B082 = 2;
                    lbl_8052B080 = 0;
                }
            }
            break;
        case 2:
            if (uwp->u.core.err == WPAD_ERR_OK) {
                s32 i;

                lbl_8052B080++;
                for (i = 0; i < 4; i++) {
                    lbl_804F1368[i] = (lbl_804F1368[i] * (lbl_8052B080 - 1) + uwp->u.bl.press[i]) / lbl_8052B080;
                }

                if (lbl_8052B080 > lbl_80528E20) {
                    lbl_8052B082 = 3;
                    WBCSetZEROPointDummy(lbl_804F1368, 4);
                }
            }
            break;
        }
    }

done:
    if (kp->sampling_callback != NULL) {
        kp->sampling_callback(chan);
    }
} // 0x8027B910

/*
 * KPADSetPosPlayMode
 *
 * Chooses how the pointer filter treats its play radius: smoothing (loose) or dead zone (tight).
 */
void KPADSetPosPlayMode(s32 chan, KPADPlayMode mode) {
    inside_kpads[chan].pos_play_mode = mode;
} // 0x8027C080
