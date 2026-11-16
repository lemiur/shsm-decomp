// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/bte/btu_task1.c (Broadcom BTE, Apache-2.0)

/* Original source:
 * bluedroid <android.googlesource.com/platform/external/bluetooth/bluedroid>
 * stack/btu/btu_task.c
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

#include <stddef.h>

#include <revolution/bte/bt_types.h>
#include <revolution/bte/data_types.h>

#include <revolution/bte/bta_sys.h>
#include <revolution/bte/bte.h>
#include <revolution/bte/btm_int.h>
#include <revolution/bte/btu.h>
#include <revolution/bte/gki.h>
#include <revolution/bte/hidh_int.h>
#include <revolution/bte/l2c_int.h>
#include <revolution/bte/rfc_int.h>
#include <revolution/bte/sdp_int.h>

#include <revolution/os/OSTime.h>

#if !defined(NDEBUG)
# define BTU_TASK_TRACE(msg_)        \
    do                                \
    {                                \
        if (btu_dbg_flag == TRUE)    \
            OSReport(msg_);            \
    } while (FALSE)
#else
# define BTU_TASK_TRACE(msg_)
#endif

tBTU_CB btu_cb;

static char btu_count = 1;
static int execute_btu = 1;

static int _btu_last_timer_tick;
static int _btu_g_count;
#if !defined(NDEBUG)
static BOOL btu_dbg_flag;
#endif

#if !defined(NDEBUG)
void btu_enable_dbg(int enable)
{
    btu_dbg_flag = enable;
}
#endif

/* Resets the BTU task state, then initializes the BTU core, the BTE stack and the BTA system manager. */
void btu_task_init(void)
{
    btu_count = 1;
    execute_btu = 1;
    _btu_g_count = 0;
    _btu_last_timer_tick = 0;

    btu_init_core();
    BTE_InitStack();
    bta_sys_init();
} // 0x8024DC20

/* Runs one pass of the BTU task: drains mailbox 0 (HCI/L2CAP/SCO traffic and registered event ranges), expires timers and drains mailbox 2 (BTA events). Skips the pass (counting it) while a previous one is still running. */
void btu_task_msg_handler(void)
{
    TIMER_LIST_ENT *p_tle;
    BT_HDR *p_msg;
    unsigned char i;
    unsigned short mask;
    unsigned char handled;
    tBTU_TIMER_CALLBACK *timer_cb;
    unsigned short event_range;
    unsigned char done;

    done = 0;
    OSGetTime(); // result is not kept in retail

    ++_btu_g_count;

    GKI_disable();

    if (execute_btu)
    {
        execute_btu = 0;
        btu_count = 1;
    }
    else
    {
        ++btu_count;

        GKI_enable();
        return;
    }

    GKI_enable();

    mask = 0x05;

    if ((unsigned)_btu_g_count > (unsigned)_btu_last_timer_tick + 500)
    {
        mask |= 0x30;
        _btu_last_timer_tick = _btu_g_count;
    }

    while (done == 0)
    {
        done = 1;

        if (mask & (1 << 0))
        {
            while ((p_msg = GKI_read_mbox(0)))
            {
                done = 0;

                BTU_TASK_TRACE("BTU Task got msg in MBOX0\n");

                switch (p_msg->event & BT_EVT_MASK)
                {
                case MSG_HC_TO_STACK_HCI_ACL:
                    l2c_rcv_acl_data(p_msg);
                    break;

                case MSG_HC_TO_STACK_L2C_SEG_XMIT:
                    l2c_link_segments_xmitted(p_msg);
                    break;

                case MSG_HC_TO_STACK_HCI_SCO:
                    btm_route_sco_data(p_msg);
                    break;

                case MSG_HC_TO_STACK_HCI_EVT:
                    btu_hcif_process_event(p_msg);
                    GKI_freebuf(p_msg);
                    break;

                case BT_EVT_TO_BTU_HCI_CMD:
                    btu_hcif_send_cmd(p_msg);
                    break;

                default:
                    i = 0;
                    event_range = p_msg->event & BT_EVT_MASK;
                    handled = 0;

                    for (; handled == 0 && i < BTU_MAX_REG_EVENT; ++i)
                    {
                        if (!btu_cb.event_reg[i].event_cb)
                            continue;

                        if (event_range != btu_cb.event_reg[i].event_range)
                            continue;

                        if (btu_cb.event_reg[i].event_cb)
                        {
                            (*btu_cb.event_reg[i].event_cb)(p_msg);
                            handled = 1;
                        }
                    }

                    if (handled == 0)
                        GKI_freebuf(p_msg);
                }
            }
        }

        if (mask & (1 << 4))
        {
            GKI_update_timer_list(&btu_cb.timer_queue, 1);
            mask &= ~(1 << 4);

            while (btu_cb.timer_queue.p_first
                   && btu_cb.timer_queue.p_first->ticks == 0)
            {
                done = 0;

                p_tle = btu_cb.timer_queue.p_first;
                GKI_remove_from_timer_list(&btu_cb.timer_queue, p_tle);

                switch (p_tle->event)
                {
                case 1:
                    btm_dev_timeout(p_tle);
                    break;

                case 9:
                    btm_acl_timeout(p_tle);
                    break;

                case 2:
                case 3:
                case 4:
                    l2c_process_timeout(p_tle);
                    break;

                case 5:
                    sdp_conn_timeout((tCONN_CB *)p_tle->param);
                    break;

                case 10:
                    btm_inq_rmt_name_failed();
                    break;

                case 8:
                    btm_discovery_timeout();
                    break;

                case 11:
                case 12:
                    rfcomm_process_timeout(p_tle);
                    break;

                case 60:
                    btu_hcif_cmd_timeout();
                    break;

                case 66:
                    hidh_proc_repage_timeout(p_tle);
                    break;

                case 22:
                    timer_cb = (tBTU_TIMER_CALLBACK *)p_tle->param;

                    (*timer_cb)(p_tle);
                    break;

                default:
                    i = 0;
                    handled = 0;

                    for (; handled == 0 && i < BTU_MAX_REG_TIMER; ++i)
                    {
                        if (!btu_cb.timer_reg[i].timer_cb)
                            continue;

                        if (btu_cb.timer_reg[i].p_tle != p_tle)
                            continue;

                        (*btu_cb.timer_reg[i].timer_cb)(p_tle);
                        handled = 1;
                    }
                }
            }
        }

        if (mask & (1 << 2))
        {
            while ((p_msg = GKI_read_mbox(2)))
            {
                BTU_TASK_TRACE("BTU Task got msg in MBOX2\n");

                done = 0;

                bta_sys_event(p_msg);
            }
        }

        if (mask & (1 << 5))
        {
            mask &= ~(1 << 5);
            bta_sys_timer_update();
        }

        if (mask & (1 << 15))
            break;
    }

    execute_btu = 1;
} // 0x8024DC60

/* (Re)starts p_tle on the BTU timer queue with the given event type and timeout in ticks. */
void btu_start_timer(TIMER_LIST_ENT *p_tle, UINT16 type, UINT32 timeout)
{
    GKI_remove_from_timer_list(&btu_cb.timer_queue, p_tle);

    p_tle->event = type;
    p_tle->ticks = timeout;

    GKI_add_to_timer_list(&btu_cb.timer_queue, p_tle);
} // 0x8024DFE0

/* Removes p_tle from the BTU timer queue. */
void btu_stop_timer(TIMER_LIST_ENT *p_tle)
{
    GKI_remove_from_timer_list(&btu_cb.timer_queue, p_tle);
} // 0x8024E050

/* Registers timer_cb for p_tle in the first free timer_reg slot (or updates the existing one) and starts the timer. */
void btu_register_timer(TIMER_LIST_ENT *p_tle, UINT16 type, UINT32 timeout,
                        tBTU_TIMER_CALLBACK *timer_cb)
{
    INT8 i = 0;
    INT8 first = -1;

    for (; i < BTU_MAX_REG_TIMER; ++i)
    {
        if (!btu_cb.timer_reg[i].p_tle && first < 0)
            first = i;

        if (btu_cb.timer_reg[i].p_tle == p_tle)
        {
            btu_cb.timer_reg[i].timer_cb = timer_cb;
            btu_start_timer(p_tle, type, timeout);

            first = -1;
            break;
        }
    }

    if (first >= 0 && first < BTU_MAX_REG_TIMER)
    {
        btu_cb.timer_reg[first].timer_cb = timer_cb;
        btu_cb.timer_reg[first].p_tle = p_tle;
        btu_start_timer(p_tle, type, timeout);
    }
} // no copy in retail (dead-stripped)

/* Stops p_tle and clears its timer_reg slot. */
void btu_deregister_timer(TIMER_LIST_ENT *p_tle)
{
    UINT8 i = 0;

    for (; i < BTU_MAX_REG_TIMER; ++i)
    {
        if (btu_cb.timer_reg[i].p_tle == p_tle)
        {
            btu_stop_timer(p_tle);
            btu_cb.timer_reg[i].timer_cb = NULL;
            btu_cb.timer_reg[i].p_tle = NULL;

            break;
        }
    }
} // no copy in retail (dead-stripped)

/* Registers (or, with a null callback, clears) the handler for the event range 'start'. */
void btu_register_event_range(UINT16 start, tBTU_EVENT_CALLBACK *event_cb)
{
    INT8 i = 0;
    INT8 first = -1;

    for (; i < BTU_MAX_REG_EVENT; ++i)
    {
        if (!btu_cb.event_reg[i].event_cb && first < 0)
            first = i;

        if (btu_cb.event_reg[i].event_range == start)
        {
            btu_cb.event_reg[i].event_cb = event_cb;

            if (!event_cb)
                btu_cb.event_reg[i].event_range = 0;

            first = -1;
        }
    }

    if (event_cb && first >= 0 && first < BTU_MAX_REG_EVENT)
    {
        btu_cb.event_reg[first].event_range = start;
        btu_cb.event_reg[first].event_cb = event_cb;

        if (!event_cb)
        {
            // NOTE: never reached, as event_cb must be both non-null and null
            btu_cb.event_reg[first].event_range = 0;
        }
    }
} // no copy in retail (dead-stripped)

/* Clears the handler of the event range 'range'. */
void btu_deregister_event_range(UINT16 range)
{
    btu_register_event_range(range, NULL);
} // no copy in retail (dead-stripped)
