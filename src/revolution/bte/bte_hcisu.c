// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/bte/bte_hcisu.c (Broadcom BTE, Apache-2.0)

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

#include <revolution/bte/data_types.h> // UINT16
#include <revolution/bte/gki_target.h> // BTU_TASK

#include <revolution/bte/btu.h> // BTU_HCI_RCV_MBOX
#include <revolution/bte/gki.h>
#include <revolution/bte/hci.h>

tHCI_CFG *p_hcisu_cfg;
tHCI_IF *p_hcisu_if;

void bte_hcisu_send(HC_BT_HDR *p_msg, UINT16 event)
{
    p_msg->event = event;

    if (p_hcisu_if)
        (*p_hcisu_if->send)(p_msg);
    else
        GKI_freebuf(p_msg);
} // 0x8024D810

void bte_hcisu_task(int param_1)
{
    if (p_hcisu_if)
    {
        if (p_hcisu_if->init)
            (*p_hcisu_if->init)(2, 1, 0x800);

        if (p_hcisu_if->open)
            (*p_hcisu_if->open)(p_hcisu_cfg);
    }
} // 0x8024D830

void bte_hcisu_close(void)
{
    if (p_hcisu_if)
    {
        if (p_hcisu_if->close)
            (*p_hcisu_if->close)();
    }
} // 0x8024D8A0

void bta_ci_hci_msg_handler(void *p_data)
{
    HC_BT_HDR *p_msg = p_data;

    GKI_send_msg(BTU_TASK, BTU_HCI_RCV_MBOX, p_msg);
} // 0x8024D8D0
