// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/bte/hidd_pm.c (Broadcom BTE, Apache-2.0)

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

#include <revolution/bte/hidd_int.h>

#include <string.h> // memcpy

#include <revolution/bte/data_types.h>

#include <revolution/bte/btm_api.h>
#include <revolution/bte/btu.h>
#include <revolution/bte/gki.h>
#include <revolution/bte/hidd_api.h>

static tHID_DEV_PWR_MD const pwr_modes[3] =
{
    {320, 160, 10, 20, 0},
    {320, 160, 10, 20, 2},
    {640, 320,  0,  0, 3},
};

void hidd_pm_init(void)
{
    int i;

    hd_cb.curr_pm.mode = 0;
    hd_cb.final_pm.mode = 0xff;

    for (i = 0; i < (int)(sizeof hd_cb.pm_params / sizeof hd_cb.pm_params[0]); ++i)
        memcpy(&hd_cb.pm_params[i], &pwr_modes[i], sizeof hd_cb.pm_params[i]);

    hd_cb.pm_ctrl_busy = FALSE;
} // no copy in retail (dead-stripped)

UINT8 hidd_pm_set_now(tHID_DEV_PWR_MD *p_req_mode)
{
    tHID_DEV_PWR_MD act_pm = {0};
    tBTM_STATUS st = 0;

    if (hd_cb.pm_ctrl_busy
        || (p_req_mode->mode == hd_cb.curr_pm.mode
            && (p_req_mode->mode == 0
                || (hd_cb.curr_pm.interval >= p_req_mode->min
                    && hd_cb.curr_pm.interval <= p_req_mode->max))))
    {
        hd_cb.final_pm.mode = 0xff;
        return 1;
    }

    switch (p_req_mode->mode)
    {
    case 0:
        if (hd_cb.curr_pm.mode == 2)
        {
            st = BTM_SetPowerMode(0x80, hd_cb.host_addr,
                                  (tBTM_PM_PWR_MD *)&act_pm);
            hd_cb.pm_ctrl_busy = TRUE;
        }
        else if (hd_cb.curr_pm.mode == 3)
        {
            st = BTM_SetPowerMode(0x80, hd_cb.host_addr,
                                  (tBTM_PM_PWR_MD *)&act_pm);
            hd_cb.pm_ctrl_busy = TRUE;
        }

        break;

    case 2:
        if (hd_cb.curr_pm.mode != 0)
        {
            hidd_pm_set_now(&act_pm);
        }
        else
        {
            st = BTM_SetPowerMode(0x80, hd_cb.host_addr,
                                  (tBTM_PM_PWR_MD *)p_req_mode);
            hd_cb.pm_ctrl_busy = TRUE;
        }

        break;

    case 3:
        if (hd_cb.curr_pm.mode != 0)
        {
            hidd_pm_set_now(&act_pm);
        }
        else
        {
            st = BTM_SetPowerMode(0x80, hd_cb.host_addr,
                                  (tBTM_PM_PWR_MD *)p_req_mode);
            hd_cb.pm_ctrl_busy = TRUE;
        }

        break;
    }

    if (st == 0 || st == 1)
        return 1;

    st += 55;

    if (hd_cb.callback)
        (*hd_cb.callback)(4, hd_cb.conn_substate,
                          (tHID_DEV_REG_INFO_CBACK_DATA *)&st);

    return 0;
} // 0x802631F8

UINT8 hidd_pm_set_power_mode(tHID_DEV_PWR_MD *p_req_mode)
{
    memcpy(&hd_cb.final_pm, p_req_mode, sizeof hd_cb.final_pm);

    return hidd_pm_set_now(p_req_mode);
} // no copy in retail (dead-stripped)

void hidd_pm_proc_mode_change(UINT8 hci_status, UINT8 mode, UINT16 interval)
{
    if (!hd_cb.reg_flag)
        return;

    hd_cb.pm_ctrl_busy = FALSE;

    if (hci_status != 0)
    {
        if (!hd_cb.callback)
            return;

        (*hd_cb.callback)(4, hd_cb.conn_substate,
                          (tHID_DEV_REG_INFO_CBACK_DATA *)&hci_status);

        return;
    }

    hd_cb.curr_pm.mode = mode;
    hd_cb.curr_pm.interval = interval;

    if (hd_cb.final_pm.mode != 0xff)
    {
        if (hd_cb.final_pm.mode != hd_cb.curr_pm.mode
            || (hd_cb.final_pm.mode != 0
                && (hd_cb.curr_pm.interval < hd_cb.final_pm.min
                    || hd_cb.curr_pm.interval > hd_cb.final_pm.max)))
        {
            hidd_pm_set_now(&hd_cb.final_pm);
        }
        else
        {
            hd_cb.final_pm.mode = 0xff;
        }
    }
    else if (hd_cb.curr_pm.mode == 0)
    {
        hidd_pm_start();
    }

    if (hd_cb.callback)
    {
        (*hd_cb.callback)(3, mode,
                          (tHID_DEV_REG_INFO_CBACK_DATA *)&interval);
    }
} // 0x802633D0

void hidd_pm_inact_timeout(void)
{
    hidd_pm_set_power_mode(&hd_cb.pm_params[1]);

    hd_cb.conn_substate = 1;
} // 0x80263528

UINT8 hidd_pm_start(void)
{
    hidd_pm_set_power_mode(&hd_cb.pm_params[0]);

    hd_cb.conn_substate = 0;

    hd_cb.idle_tle.param = (TIMER_PARAM_TYPE)&hidd_pm_inact_timeout;
    btu_start_timer(&hd_cb.idle_tle, 22, 60);

    return 0;
} // no copy in retail (dead-stripped)

UINT8 hidd_pm_stop(void)
{
    tHID_DEV_PWR_MD p_md = {0};

    hidd_pm_set_power_mode(&p_md);

    btu_stop_timer(&hd_cb.idle_tle);

    return 0;
} // no copy in retail (dead-stripped)

UINT8 hidd_pm_suspend_evt(void)
{
    if (hd_cb.conn_substate == 0)
        btu_stop_timer(&hd_cb.idle_tle);

    hidd_pm_set_power_mode(&hd_cb.pm_params[2]);

    hd_cb.conn_substate = 2;

    return 0;
} // no copy in retail (dead-stripped)
