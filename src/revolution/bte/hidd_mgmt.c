// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/bte/hidd_mgmt.c (Broadcom BTE, Apache-2.0)

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

#include <string.h> // memcpy

#include <revolution/bte/bt_target.h>
#include <revolution/bte/bt_trace.h>
#include <revolution/bte/bt_types.h>
#include <revolution/bte/data_types.h>

#include <revolution/bte/btu.h>
#include <revolution/bte/hidd_api.h>
#include <revolution/bte/hidd_int.h>
#include <revolution/bte/hiddefs.h>

static tHID_STATUS hidd_no_conn_proc_evt(UINT8 event, void *p_data);
static tHID_STATUS hidd_connecting_proc_evt(UINT8 event, void *p_data);
static tHID_STATUS hidd_connected_proc_evt(UINT8 event, void *p_data);
static tHID_STATUS hidd_disc_ing_proc_evt(UINT8 event, void *p_data);

tHID_DEV_PROC_EVT * const hidd_sm_proc_evt[] =
{
    &hidd_no_conn_proc_evt,
    &hidd_connecting_proc_evt,
    &hidd_connected_proc_evt,
    &hidd_disc_ing_proc_evt,
};

tHID_DEV_CTB hd_cb;

tHID_STATUS hidd_mgmt_process_evt(UINT8 event, void *p_data)
{
    HIDD_TRACE(DEBUG, "hidd_mgmt_process_evt known: %d, s:%d, e:%d",
               hd_cb.host_known, hd_cb.dev_state, event);

    if (hd_cb.host_known)
    {
        return (*hidd_sm_proc_evt[hd_cb.dev_state])(event, p_data);
    }
    else if (event == 0)
    {
        hd_cb.host_known = TRUE;
        memcpy(hd_cb.host_addr, p_data, BD_ADDR_LEN);

        hd_cb.dev_state = 2;

        (*hd_cb.callback)(0, 0,
                          (tHID_DEV_REG_INFO_CBACK_DATA *)hd_cb.host_addr);

        return 0;
    }
    else
    {
        return 13;
    }
} // no copy in retail (dead-stripped)

static tHID_STATUS hidd_no_conn_proc_evt(UINT8 event, void *p_data)
{
    tHID_STATUS st = 0;

    switch (event)
    {
    case 0:
        hd_cb.dev_state = 2;

        (*hd_cb.callback)(0, 0,
                          (tHID_DEV_REG_INFO_CBACK_DATA *)hd_cb.host_addr);
        break;

    case 4:
        hd_cb.conn_tries = 1;
        hd_cb.dev_state = 1;

        st = hidd_conn_initiate();
        if (st == 0)
            break;

        btu_start_timer(&hd_cb.conn.timer_entry, 22, 1);

        return 0;

    default:
        st = 4;
        break;
    }

    return st;
} // no copy in retail (dead-stripped)

void hidd_proc_repage_timeout(void)
{
    HIDD_TRACE(DEBUG, "hidd_proc_repage_timeout");

    ++hd_cb.conn_tries;

    if (hidd_conn_initiate())
    {
        if (hd_cb.conn_tries > 15)
        {
            hd_cb.dev_state = 0;
            (*hd_cb.callback)(1, 0, NULL);
        }
        else
        {
            btu_start_timer(&hd_cb.conn.timer_entry, 22, 1);
        }
    }
    else
    {
        (*hd_cb.callback)(2, hd_cb.conn_tries, NULL);
    }
} // 0x80263130

static tHID_STATUS hidd_connecting_proc_evt(UINT8 event, void *p_data)
{
    UINT16 reason;

    switch (event)
    {
    case 0:
        hd_cb.dev_state = 2;
        (*hd_cb.callback)(0, 0,
                          (tHID_DEV_REG_INFO_CBACK_DATA *)hd_cb.host_addr);
        break;

    case 3:
        if (hd_cb.conn_tries > 15)
        {
            reason = *(UINT16 *)p_data;
            hd_cb.dev_state = 0;
            (*hd_cb.callback)(1, reason, NULL);
        }
        else
        {
            btu_start_timer(&hd_cb.conn.timer_entry, 22, 1);
        }

        break;

    case 1:
    case 2:
        hd_cb.dev_state = 0;
        (*hd_cb.callback)(1, *(UINT16 *)p_data, NULL);
        break;

    case 5:
        hd_cb.dev_state = 0;

        btu_stop_timer(&hd_cb.conn.timer_entry);
        hidd_conn_disconnect();
        break;

    default:
        return 9;
    }

    return 0;
} // no copy in retail (dead-stripped)

void hidd_mgmt_conn_closed(UINT16 reason)
{
    if (hd_cb.unplug_on)
        hd_cb.host_known = FALSE;

    hd_cb.dev_state = 0;

    (*hd_cb.callback)(1, reason, NULL);
} // no copy in retail (dead-stripped)

static tHID_STATUS hidd_connected_proc_evt(UINT8 event, void *p_data)
{
    switch (event)
    {
    case 2:
        hd_cb.dev_state = 1;
        hd_cb.conn_tries = 0;

        btu_start_timer(&hd_cb.conn.timer_entry, 22, 1);
        break;

    case 1:
        hidd_mgmt_conn_closed(*(UINT16 *)p_data);
        break;

    case 5:
        hd_cb.dev_state = 3;
        hidd_conn_disconnect();
        break;

    case 6:
        return hidd_conn_snd_data(p_data);

    default:
        return 10;
    }

    return 0;
} // no copy in retail (dead-stripped)

static tHID_STATUS hidd_disc_ing_proc_evt(UINT8 event, void *p_data)
{
    switch (event)
    {
    case 1:
    case 2:
    case 3:
        hidd_mgmt_conn_closed(*(UINT16 *)p_data);
        break;

    default:
        return 11;
    }

    return 0;
} // no copy in retail (dead-stripped)
