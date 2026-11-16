// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/bte/bte_init.c (Broadcom BTE, Apache-2.0)

/* Original source:
 * bluedroid <android.googlesource.com/platform/external/bluetooth/bluedroid>
 * main/bte_init.c
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

#include <revolution/bte/bte.h>
#include <revolution/bte/gap_api.h>
#include <revolution/bte/hidd_api.h>
#include <revolution/bte/hidh_api.h>
#include <revolution/bte/port_api.h>

void BTE_InitStack(void)
{
    RFCOMM_Init();
    GAP_Init();
    HID_DevInit();
    HID_HostInit();
} // 0x8024D8E0
