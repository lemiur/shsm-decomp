// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/bte/bta_sys_conn.c (Broadcom BTE, Apache-2.0)

/* Original source:
 * bluedroid <android.googlesource.com/platform/external/bluetooth/bluedroid>
 * bta/sys/bta_sys_conn.c
 */

/******************************************************************************
 *
 *  Copyright (C) 2003-2012 Broadcom Corporation
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

#include <revolution/bte/bta_sys.h>

#include <revolution/bte/bt_types.h>
#include <revolution/bte/data_types.h>

#include <revolution/bte/bta_sys_int.h>

void bta_sys_rm_register(tBTA_SYS_CONN_CBACK *p_cback)
{
    bta_sys_cb.prm_cb = p_cback;
} // 0x8024E138

void bta_sys_compress_register(tBTA_SYS_CONN_CBACK *p_cback)
{
    bta_sys_cb.compress_cb = p_cback;
} // 0x8024E148

void bta_sys_pm_register(tBTA_SYS_CONN_CBACK *p_cback)
{
    bta_sys_cb.ppm_cb = p_cback;
} // 0x8024E158

void bta_sys_conn_open(UINT8 id, UINT8 app_id, BD_ADDR peer_addr)
{
    if (bta_sys_cb.prm_cb)
        (*bta_sys_cb.prm_cb)(BTA_SYS_CONN_OPEN, id, app_id, peer_addr);

    if (bta_sys_cb.ppm_cb)
        (*bta_sys_cb.ppm_cb)(BTA_SYS_CONN_OPEN, id, app_id, peer_addr);

    if (bta_sys_cb.compress_cb)
        (*bta_sys_cb.compress_cb)(BTA_SYS_CONN_OPEN, id, app_id, peer_addr);
} // 0x8024E168

void bta_sys_conn_close(UINT8 id, UINT8 app_id, BD_ADDR peer_addr)
{
    if (bta_sys_cb.prm_cb)
        (*bta_sys_cb.prm_cb)(BTA_SYS_CONN_CLOSE, id, app_id, peer_addr);

    if (bta_sys_cb.ppm_cb)
        (*bta_sys_cb.ppm_cb)(BTA_SYS_CONN_CLOSE, id, app_id, peer_addr);

    if (bta_sys_cb.compress_cb)
        (*bta_sys_cb.compress_cb)(BTA_SYS_CONN_CLOSE, id, app_id, peer_addr);
} // 0x8024E22C

void bta_sys_app_open(UINT8 id, UINT8 app_id, BD_ADDR peer_addr)
{
    if (bta_sys_cb.ppm_cb)
        (*bta_sys_cb.ppm_cb)(BTA_SYS_APP_OPEN, id, app_id, peer_addr);
} // no copy in retail (dead-stripped)

void bta_sys_app_close(UINT8 id, UINT8 app_id, BD_ADDR peer_addr)
{
    if (bta_sys_cb.ppm_cb)
        (*bta_sys_cb.ppm_cb)(BTA_SYS_APP_CLOSE, id, app_id, peer_addr);
} // no copy in retail (dead-stripped)

void bta_sys_sco_open(UINT8 id, UINT8 app_id, BD_ADDR peer_addr)
{
    if (bta_sys_cb.ppm_cb)
        (*bta_sys_cb.ppm_cb)(BTA_SYS_SCO_OPEN, id, app_id, peer_addr);
} // no copy in retail (dead-stripped)

void bta_sys_sco_close(UINT8 id, UINT8 app_id, BD_ADDR peer_addr)
{
    if (bta_sys_cb.ppm_cb)
        (*bta_sys_cb.ppm_cb)(BTA_SYS_SCO_CLOSE, id, app_id, peer_addr);
} // 0x8024E2F0

void bta_sys_idle(UINT8 id, UINT8 app_id, BD_ADDR peer_addr)
{
    if (bta_sys_cb.ppm_cb)
        (*bta_sys_cb.ppm_cb)(BTA_SYS_CONN_IDLE, id, app_id, peer_addr);
} // 0x8024E324

void bta_sys_busy(UINT8 id, UINT8 app_id, BD_ADDR peer_addr)
{
    if (bta_sys_cb.ppm_cb)
        (*bta_sys_cb.ppm_cb)(BTA_SYS_CONN_BUSY, id, app_id, peer_addr);
} // 0x8024E358
