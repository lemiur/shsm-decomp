// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/bte/btu_init.c (Broadcom BTE, Apache-2.0)

/* Original source:
 * bluedroid <android.googlesource.com/platform/external/bluetooth/bluedroid>
 * stack/btu/btu_init.c
 */

/******************************************************************************
 *
 *  Copyright (C) 2000-2012 Broadcom Corporation
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

#include <string.h> // memset

#include <revolution/bte/btu.h>

#include <revolution/bte/bt_types.h>
#include <revolution/bte/data_types.h>

#include <revolution/bte/btm_int.h>
#include <revolution/bte/hcidefs.h>
#include <revolution/bte/l2c_int.h>
#include <revolution/bte/sdp_int.h>

BD_ADDR const BT_BD_ANY = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void btu_init_core(void)
{
    btm_init();
    l2c_init();
    sdp_init();

    btm_discovery_db_init();
} // 0x8025F5DC

void BTE_Init(void)
{
    memset(&btu_cb, 0, sizeof btu_cb);

    btu_cb.hcit_acl_pkt_size = BTU_DEFAULT_DATA_SIZE + HCI_DATA_PREAMBLE_SIZE;
    btu_cb.controller_cmd_window = 1;
} // 0x8025F608

UINT16 BTU_AclPktSize(void)
{
    return btu_cb.hcit_acl_pkt_size;
} // no copy in retail (dead-stripped)
