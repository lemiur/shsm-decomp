// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/bte/port_api.c (Broadcom BTE, Apache-2.0)

/* Original source:
 * bluedroid <android.googlesource.com/platform/external/bluetooth/bluedroid>
 * stack/rfcomm/port_api.c
 */

/******************************************************************************
 *
 *  Copyright (C) 1999-2012 Broadcom Corporation
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

#include <revolution/bte/bt_target.h>
#include <revolution/bte/bt_trace.h>
#include <revolution/bte/port_api.h>
#include <revolution/bte/rfc_int.h>

void RFCOMM_Init(void)
{
    memset(&rfc_cb, 0, sizeof rfc_cb);

    rfc_cb.rfc.last_mux = MAX_BD_CONNECTIONS;
    rfc_cb.trace_level = BT_TRACE_LEVEL_DEBUG;

    rfcomm_l2cap_if_init();
} // 0x8026C45C
