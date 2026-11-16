// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/bte/bte_main.c (Broadcom BTE, Apache-2.0)

/* Original source:
 * bluedroid <android.googlesource.com/platform/external/bluetooth/bluedroid>
 * main/bte_main.c
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

#include <stddef.h> // NULL

#include <revolution/bte/bte.h>

#include <revolution/bte/data_types.h>

#include <revolution/bte/bta_api.h>
#include <revolution/bte/btu.h>
#include <revolution/bte/gki.h>
#include <revolution/bte/hci.h>
#include <revolution/bte/uusb.h> // wait4hci

#include <revolution/os/OSAlarm.h>
#include <revolution/os/OSContext.h>
#include <revolution/os/OSError.h>
#include <revolution/os/OSTime.h>

static void BTE_InitSys(void);
static void bta_register_os_callbacks(void);

static void BTUInterruptHandler(OSAlarm *alarm, OSContext *context);
static void BTE_BringUpStack(void);

static OSAlarm _bte_alarm;
unsigned char __BTUInterruptHandlerStack[0x1000] ATTRIBUTE_ALIGN(32);

static tHCI_CFG bte_hcisu_h2_cfg =
{
    2652, 8449
};

static tBTA_APP_INFO_CBACK *_bte_app_info;
#if !defined(NDEBUG)
static BOOLEAN enable_dbg_msg;
#endif
tBTE_TARGET_MODE volatile bte_target_mode;

/* Clears the application info callback. */
static void BTE_InitSys(void)
{
    _bte_app_info = NULL;
}

/* Starts the periodic alarm (every 2 ms) that runs the BTU task through BTUInterruptHandler. */
static void bta_register_os_callbacks(void)
{
    OSCreateAlarm(&_bte_alarm);

    OSSetPeriodicAlarm(&_bte_alarm, OSGetTime(), OSMillisecondsToTicks(2),
                       &BTUInterruptHandler);
}

#if !defined(NDEBUG)
void __btu_interrupt_handler_debug_msg(BOOLEAN enable)
{
    enable_dbg_msg = enable;
}
#endif

/* Alarm handler: switches to the BTU task's private stack and runs its message handler. */
static void BTUInterruptHandler(OSAlarm *alarm, OSContext *context)
{
#if !defined(NDEBUG)
    OSTime start;
    OSTime end;
    OSTick elapsed;

    start = __OSGetSystemTime();
#endif

    OSSwitchFiber((u32)btu_task_msg_handler,
                  (u32)(__BTUInterruptHandlerStack
                        + sizeof __BTUInterruptHandlerStack));

#if !defined(NDEBUG)
    end = __OSGetSystemTime();

    if (enable_dbg_msg)
    {
        elapsed = OSTicksToMicroseconds(OSDiffTick(end, start));
        if (elapsed > 100)
        {
            OSReport(
                "BTUInterruptHandler: Warning BTU consumes %d microseconds\n",
                elapsed);
        }
    }
#endif
} // 0x8024DAB0

/* Selects the USB H2 HCI transport, waits for the controller, then starts the BTU task and its timer alarm. */
static void BTE_BringUpStack(void)
{
    p_hcisu_if = (tHCI_IF *)&hcisu_h2;
    p_hcisu_cfg = &bte_hcisu_h2_cfg;

    bte_hcisu_task(0);

    while (wait4hci)
        GKI_delay(100);

    btu_task_init();
    bta_register_os_callbacks();
}

/* Brings up the whole Bluetooth stack and blocks until the device is up. */
tBTA_STATUS BTA_Init(void)
{
    BTE_InitSys();

    GKI_init();
    GKI_enable();

    BTE_Init();
    BTE_BringUpStack();

    GKI_run(NULL);

    while (!BTA_DmIsDeviceUp())
        GKI_delay(2000);

    return 0;
} // 0x8024DAD0

/* Stops the BTU alarm, closes the HCI transport and shuts the GKI down; p_cb is notified of the USB close. */
void BTA_CleanUp(tBTA_APP_INFO_CBACK *p_cb)
{
    _bte_app_info = p_cb;

    OSCancelAlarm(&_bte_alarm);
    bte_hcisu_close();

    GKI_shutdown();
} // 0x8024DBB0

/* Reports the result of the USB close to the application callback: 0 on success, 1 on failure. */
void bta_usb_close_evt(INT8 status)
{
    if (_bte_app_info)
    {
        if (status >= 0)
            (*_bte_app_info)(0);
        else
            (*_bte_app_info)(1);
    }
} // 0x8024DBF0
