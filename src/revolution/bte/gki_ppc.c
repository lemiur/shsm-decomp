// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/bte/gki_ppc.c (Broadcom BTE, Apache-2.0)

/* Original source:
 * bluedroid <android.googlesource.com/platform/external/bluetooth/bluedroid>
 * gki/ulinux/gki_ulinux.c
 */

/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
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

#include <string.h>

#include <revolution/bte/data_types.h>
#include <revolution/bte/gki_common.h>
#include <revolution/bte/gki_int.h>
#include <revolution/bte/gki_target.h>

#include <revolution/os/OSError.h> // ASSERTLINE
#include <revolution/os/OSInterrupt.h>
#include <revolution/wud.h> // App_MEMalloc, App_MEMfree

static UINT8 GKI_init_btu_task(UINT8 task_id);

tGKI_CB gki_cb;

/* Clears the control block, initialises the buffer pools and timers, and registers the PPC task. */
void GKI_init(void)
{
    memset(&gki_cb, 0, sizeof gki_cb);

    gki_buffer_init();
    gki_timers_init();

    GKI_init_btu_task(PPC_TASK);

    gki_cb.os.int_index = 0;
    gki_cb.os.int_state[gki_cb.os.int_index] = OSEnableInterrupts();
    ++gki_cb.os.int_index;
} // 0x8024C1C0

/* Marks the PPC task dead and unwinds every nested GKI_disable. */
void GKI_shutdown(void)
{
    GKI_exit_task(PPC_TASK);

    while (gki_cb.os.int_index > 0)
    {
        GKI_enable();
    }
} // 0x8024C250

/* Readies a task's slot in the control block; expanded into GKI_init. */
static UINT8 GKI_init_btu_task(UINT8 task_id)
{
    if (task_id >= GKI_MAX_TASKS)
    {
        return GKI_FAILURE;
    }

    gki_cb.com.OSRdyTbl[task_id] = TASK_READY;
    gki_cb.com.OSTName[task_id] = NULL;
    gki_cb.com.OSWaitTmr[task_id] = 0;
    gki_cb.com.OSWaitEvt[task_id] = 0;

    return GKI_SUCCESS;
}

UINT8 GKI_create_task(TASKPTR task_entry, UINT8 task_id, INT8 *taskname,
                      UINT16 *stack, UINT16 stacksize)
{
    return GKI_SUCCESS;
} // no copy in retail (dead-stripped)

/* The BTU task is driven by the alarm in bte_main, so there is no scheduler loop. */
void GKI_run(void *p_task_id)
{
} // 0x8024C300

UINT8 GKI_suspend_task(UINT8 task_id)
{
    return GKI_SUCCESS;
} // no copy in retail (dead-stripped)

UINT8 GKI_resume_task(UINT8 task_id)
{
    return GKI_SUCCESS;
} // no copy in retail (dead-stripped)

/* Marks a task dead with interrupts disabled. */
void GKI_exit_task(UINT8 task_id)
{
    GKI_disable();

    gki_cb.com.OSRdyTbl[task_id] = TASK_DEAD;

    GKI_enable();
} // no copy in retail (dead-stripped or inlined)

void GKI_sched_lock(void)
{
} // 0x8024C310

void GKI_sched_unlock(void)
{
} // 0x8024C320

/* Collects pending mailbox events and returns those selected by flag. */
UINT16 GKI_wait(UINT16 flag, UINT32 timeout)
{
    UINT16 evt;
    UINT8 rtask = GKI_get_taskid();

    gki_cb.com.OSWaitForEvt[rtask] = flag;

    if (gki_cb.com.OSTaskQFirst[rtask][0])
    {
        gki_cb.com.OSWaitEvt[rtask] |= TASK_MBOX_0_EVT_MASK;
    }

    if (gki_cb.com.OSTaskQFirst[rtask][1])
    {
        gki_cb.com.OSWaitEvt[rtask] |= TASK_MBOX_1_EVT_MASK;
    }

    if (gki_cb.com.OSTaskQFirst[rtask][2])
    {
        gki_cb.com.OSWaitEvt[rtask] |= TASK_MBOX_2_EVT_MASK;
    }

    if (gki_cb.com.OSTaskQFirst[rtask][3])
    {
        gki_cb.com.OSWaitEvt[rtask] |= TASK_MBOX_3_EVT_MASK;
    }

    if (!(gki_cb.com.OSWaitEvt[rtask] & flag))
    {
        gki_cb.com.OSWaitTmr[rtask] = timeout;
        gki_adjust_timer_count(timeout);
    }

    gki_cb.com.OSWaitForEvt[rtask] = 0;

    evt = gki_cb.com.OSWaitEvt[rtask] & flag;
    gki_cb.com.OSWaitEvt[rtask] &= ~flag;

    return evt;
} // no copy in retail (dead-stripped)

void GKI_delay(UINT32 timeout)
{
} // 0x8024C330

/* Posts an event to a task with interrupts disabled. */
UINT8 GKI_send_event(UINT8 task_id, UINT16 event)
{
    if (task_id >= GKI_MAX_TASKS)
    {
        return GKI_FAILURE;
    }

    GKI_disable();

    gki_cb.com.OSWaitEvt[task_id] |= event;

    GKI_enable();

    return GKI_SUCCESS;
} // 0x8024C340

/* Interrupt-context variant of GKI_send_event; leaves interrupts alone. */
UINT8 GKI_isend_event(UINT8 task_id, UINT16 event)
{
    if (task_id >= GKI_MAX_TASKS)
    {
        return GKI_FAILURE;
    }

    gki_cb.com.OSWaitEvt[task_id] |= event;

    return GKI_SUCCESS;
} // no copy in retail (dead-stripped)

UINT8 GKI_get_taskid(void)
{
    return PPC_TASK;
} // 0x8024C3F0

/* Returns a task's name, "BAD" for an invalid id. */
INT8 *GKI_map_taskname(UINT8 task_id)
{
    if (task_id < GKI_MAX_TASKS)
    {
        return gki_cb.com.OSTName[task_id];
    }
    else if (task_id == GKI_MAX_TASKS)
    {
        return gki_cb.com.OSTName[GKI_get_taskid()];
    }
    else
    {
        return (INT8 *)"BAD";
    }
} // no copy in retail (dead-stripped)

/* Pops one nesting level and restores the interrupt state saved by the matching GKI_disable. */
void GKI_enable(void)
{
    if (gki_cb.os.int_index == 0)
    {
        ASSERTLINE(gki_cb.os.int_index == 0, 586);
    }

    --gki_cb.os.int_index;

    OSRestoreInterrupts(gki_cb.os.int_state[gki_cb.os.int_index]);
} // 0x8024C400

/* Disables interrupts, pushing the previous state onto the nesting stack. */
void GKI_disable(void)
{
    BOOL intrStatus;

    if (gki_cb.os.int_index >= MAX_INT_STATE)
    {
        ASSERTLINE(gki_cb.os.int_index > MAX_INT_STATE, 611);
    }

    intrStatus = OSDisableInterrupts();

    gki_cb.os.int_state[gki_cb.os.int_index] = intrStatus;
    ++gki_cb.os.int_index;
} // 0x8024C430

void GKI_exception(UINT16 code, char *msg)
{
} // 0x8024C480

/* Formats the tick count as "HH:MM:SS:mm:" into tbuf. */
INT8 *GKI_get_time_stamp(INT8 *tbuf)
{
    UINT32 ms_time;
    UINT32 s_time;
    UINT32 m_time;
    UINT32 h_time;
    INT8 *p_out = tbuf;

    ms_time = gki_cb.com.OSTicks * 10;
    s_time = ms_time / 100;
    m_time = s_time / 60;
    h_time = m_time / 60;

    ms_time -= s_time * 100;
    s_time -= m_time * 60;
    m_time -= h_time * 60;

    *p_out++ = h_time / 10 + '0';
    *p_out++ = h_time % 10 + '0';
    *p_out++ = ':';
    *p_out++ = m_time / 10 + '0';
    *p_out++ = m_time % 10 + '0';
    *p_out++ = ':';
    *p_out++ = s_time / 10 + '0';
    *p_out++ = s_time % 10 + '0';
    *p_out++ = ':';
    *p_out++ = ms_time / 10 + '0';
    *p_out++ = ms_time % 10 + '0';
    *p_out++ = ':';
    *p_out = '\0';

    return tbuf;
} // no copy in retail (dead-stripped)

/* Records the user memory pool in the control block. */
void GKI_register_mempool(void *p_mem)
{
    gki_cb.com.p_user_mempool = p_mem;
} // no copy in retail (dead-stripped)

/* Allocates through the WUD allocator hook; returns NULL on failure. */
void *GKI_os_malloc(UINT32 size)
{
    void *mem = NULL;

    mem = App_MEMalloc(size);

    if (mem == 0)
    {
        return NULL;
    }

    return mem;
} // 0x8024C490

void GKI_os_free(void *p_mem)
{
    App_MEMfree(p_mem);
} // 0x8024C4C0
