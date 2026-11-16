// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/private/bte/hidd_int.h (Broadcom BTE, Apache-2.0)
#ifndef BTE_HID_DEVICE_INT_H
#define BTE_HID_DEVICE_INT_H

/* Original source:
 * bluedroid <android.googlesource.com/platform/external/bluetooth/bluedroid>
 * stack/hid/hidd_int.h
 */

/******************************************************************************
 *
 *  Copyright (C) 2002-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/* Includes changes by muff1n1634 and the mscharged-decomp contributors; adapted for this project. */

#include <revolution/bte/bt_types.h>
#include <revolution/bte/data_types.h>

#include <revolution/bte/gki.h>
#include <revolution/bte/hid_conn.h>
#include <revolution/bte/hidd_api.h>
#include <revolution/bte/hiddefs.h>
#include <revolution/bte/l2c_api.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef tHID_STATUS tHID_DEV_PROC_EVT(UINT8 event, void *p_data);

typedef struct hid_control_block
{
    BD_ADDR                     host_addr;      // 0x000
    BOOLEAN                     host_known;     // 0x006
    UINT8                       virtual_cable;  // 0x007
    UINT8                       dev_state;      // 0x008
    UINT8                       conn_tries;     // 0x009
    UINT8                       sec_mask;       // 0x00a
    UINT16                      get_rep_buf_sz; // 0x00c
    tHID_CONN                   conn;           // 0x010
    TIMER_LIST_ENT              idle_tle;       // 0x034
    tHID_DEV_PWR_MD             pm_params[3];   // 0x04c
    tHID_DEV_CURR_PM_SETTING    curr_pm;        // 0x06a
    BOOLEAN                     pm_ctrl_busy;   // 0x06e
    UINT8                       conn_substate;  // 0x06f
    tHID_DEV_PWR_MD             final_pm;       // 0x070
    BOOLEAN                     use_qos_flg;    // 0x07a
    BOOLEAN                     unplug_on;      // 0x07b
    tQOS_INFO                   qos_info;       // 0x07c
    tHID_DEV_REG_INFO_CBACK     *callback;      // 0x0c4
    tL2CAP_CFG_INFO             l2cap_ctrl_cfg; // 0x0c8
    tL2CAP_CFG_INFO             l2cap_int_cfg;  // 0x104
    BOOLEAN                     reg_flag;       // 0x140
    UINT8                       trace_level;    // 0x141
} tHID_DEV_CTB; // size 0x144

extern tHID_DEV_CTB hd_cb;
extern tHID_DEV_PROC_EVT * const hidd_sm_proc_evt[];

tHID_STATUS hidd_conn_reg(void);
void hidd_conn_dereg(void);
void hidd_conn_disconnect(void);
tHID_STATUS hidd_conn_initiate(void);
void hidd_sec_check_complete_term(BD_ADDR bd_addr, void *p_ref_data, UINT8 res);
void hidd_sec_check_complete_orig(BD_ADDR bd_addr, void *p_ref_data, UINT8 res);
void hidd_l2c_connected(tHID_CONN *p_hcon);
tHID_STATUS hidd_conn_snd_data(tSND_DATA_PARAMS *p_data);

tHID_STATUS hidd_mgmt_process_evt(UINT8 event, void *p_data);
void hidd_proc_repage_timeout(void);
void hidd_mgmt_conn_closed(UINT16 reason);

UINT8 hidd_pm_set_power_mode(tHID_DEV_PWR_MD *p_req_mode);
UINT8 hidd_pm_stop(void);
void hidd_pm_init(void);
UINT8 hidd_pm_start(void);
UINT8 hidd_pm_suspend_evt(void);
void hidd_pm_proc_mode_change(UINT8 hci_status, UINT8 mode, UINT16 interval);

#ifdef __cplusplus
}
#endif

#endif // BTE_HID_DEVICE_INT_H
