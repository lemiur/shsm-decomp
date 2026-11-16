// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/bte/uusb_ppc.c (Broadcom BTE, Apache-2.0)

/* Original source: Broadcom's USB transport for the Wii's IOS USB client. The reference names no bluedroid
 * counterpart; the closest in purpose is hci/src/userial.c.
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

#include <stddef.h>
#include <string.h>

#include <revolution/bte/data_types.h>
#include <revolution/bte/gki_target.h>

#include <revolution/bte.h>
#include <revolution/bte/bte.h>
#include <revolution/bte/gki.h>
#include <revolution/bte/hci.h>
#include <revolution/bte/l2c_int.h>
#include <revolution/bte/uusb.h>

#include <revolution/ipc/ipcclt.h>
#include <revolution/os.h>
#include <revolution/os/OSContext.h>
#include <revolution/os/OSError.h>
#include <revolution/usb/usb.h>

// This unit compares pointers against a parenthesised void pointer null (retail: li r0, 0; cmplw),
// not the plain 0 of <stddef.h>.
#undef NULL
#define NULL ((void *)(0))

// The retail build has no HCISU trace strings, so the trace output below is compiled out as in the
// project's other BTE glue (NDEBUG).
#if !defined(NDEBUG)
#define UUSB_DBG 1
#else
#define UUSB_DBG 0
#endif

#if UUSB_DBG
#define UUSBDBG(param, ...)               \
    do                                    \
    {                                     \
        if (usb.trace_state)              \
            OSReport(param, __VA_ARGS__); \
    } while (FALSE)

#define UUSBDBG0(param)      \
    do                       \
    {                        \
        if (usb.trace_state) \
            OSReport(param); \
    } while (FALSE)
#else
#define UUSBDBG(param, ...) do {} while (FALSE)
#define UUSBDBG0(param) do {} while (FALSE)
#endif

typedef struct
{
    IOSError fd;
    char pad0_[0x10 - (0x00 + 0x04)];
    u8 bulkEndpoint1;
    u8 bulkEndpoint2;
    u8 intrEndpoint1;
    char intrEndpoint2; /* name invented; cleared with the other endpoints, never used */
    int vid;
    int pid;
    UINT8 cmd_buffer_pool;
    UINT8 acl_buffer_pool;
    char pad2_[0x20 - (0x1d + 0x01)];
    tUUSB_CBACK *cback;
    tUUSB *p_uusb_cfg;
    UINT8 trace_state;
    UINT8 state;
    char pad4_[1];
    UINT8 reading_intr_data;
    BUFFER_Q bulk_buffer_q;
    UINT8 bulk_writes_pending; /* name invented from the trace text "writes outstanding" */
    BUFFER_Q intr_buffer_q;
    UINT8 ctrl_writes_pending; /* name invented from the trace text "writes outstanding" */
    char pad8_[3];
} tUUSB_CB;

static void uusb_ReleaseCmdBufferPool(void);
static void uusb_ReleaseAclBufferPool(void);
static void uusb_CloseDeviceCB(s32 result, void *p_data);
static void uusb_ReadIntrDataCB(s32 ret, void *p_args);
static void uusb_ReadBulkDataCB(s32 ret, void *p_args);
static void uusb_issue_bulk_read(void);
static void uusb_issue_intr_read(void);
static void uusb_WriteCtrlDataCB(s32 result, void *p_data);
static void uusb_WriteBulkDataCB(s32 result, void *p_data);
static s32 uusb_get_devId(int vid, int pid);

static tUUSB_CB usb;
unsigned char __uusb_ppc_stack1[0x1000] ATTRIBUTE_ALIGN(32);
unsigned char __uusb_ppc_stack2[0x1000] ATTRIBUTE_ALIGN(32);

UINT32 volatile wait4hci = TRUE;
static s32 __ntd_ios_file_descriptor = -1;

static int __ntd_pid;
static int __ntd_vid;
static BOOLEAN __ntd_pid_vid_specified;
static UINT8 uusb_g_usb_devid_found = FALSE;
static UINT8 uusb_g_trace_state_initialized = FALSE;
UINT32 __ntd_ohci = 0;
UINT32 __ntd_ohci_init_flag = FALSE;

/* Selects which OHCI controller ("oh0" or "oh1") the USB Bluetooth device is opened on. */
void __ntd_set_ohci(UINT8 ohci)
{
    if (ohci == 0)
    {
        __ntd_ohci = 0;
        __ntd_ohci_init_flag = TRUE;
    }
    else if (ohci == 1)
    {
        __ntd_ohci = 1;
        __ntd_ohci_init_flag = TRUE;
    }
} // no copy in retail (dead-stripped)

/* Overrides the vendor and product id of the USB device to open. */
void __ntd_set_pid_vid(int pid, int vid)
{
    __ntd_vid = vid;
    __ntd_pid = pid;
    __ntd_pid_vid_specified = TRUE;
} // no copy in retail (dead-stripped)

/* Returns the IOS file descriptor of the opened USB device. */
int __ntd_get_usb_file_descriptor(void)
{
    return __ntd_ios_file_descriptor;
} // no copy in retail (dead-stripped)

/* Returns the memory the BTE stack allocates. */
UINT32 __ntd_get_allocated_mem_size(void)
{
    UINT32 size = 0;

    size += 0x07620; // byte counts as written in the reference; the allocations they stand for are
    size += 0x10000; // not identified
    size -= 0x02BA8; // (all three)

    return size;
} // no copy in retail (dead-stripped)

/* Sets the trace flag of the USB transport. */
void uusb_set_trace_state(UINT8 state)
{
    usb.trace_state = state;

    uusb_g_trace_state_initialized = TRUE;
} // no copy in retail (dead-stripped or inlined)

/* Returns the trace flag of the USB transport. */
UINT8 uusb_get_trace_state(void)
{
    return usb.trace_state;
} // no copy in retail (dead-stripped)

/* Deletes the command (interrupt pipe) buffer pool. */
static void uusb_ReleaseCmdBufferPool(void)
{
    GKI_delete_pool(usb.cmd_buffer_pool);

    usb.cmd_buffer_pool = GKI_INVALID_POOL;
}

/* Deletes the ACL (bulk pipe) buffer pool. */
static void uusb_ReleaseAclBufferPool(void)
{
    GKI_delete_pool(usb.acl_buffer_pool);

    usb.acl_buffer_pool = GKI_INVALID_POOL;
}

/* IOS close completion: forgets the device and reports the close to the registered callback. */
static void uusb_CloseDeviceCB(s32 result, void *p_data)
{
    usb.bulkEndpoint1 = 0;
    usb.bulkEndpoint2 = 0;
    usb.intrEndpoint1 = 0;
    usb.intrEndpoint2 = 0;

    usb.fd = 0;

    uusb_g_usb_devid_found = 0;

    UUSB_Unregister();

    if (usb.cback)
    {
        (*usb.cback)(4, result);
    }
} // 0x8024CB90

/* Interrupt pipe completion (HCI events): copies the event into an HCI pool buffer, hands it to the
 * BTA through the fibre and reissues the read. */
static void uusb_ReadIntrDataCB(s32 ret, void *p_args)
{
    HC_BT_HDR *p_buf1;
    HC_BT_HDR *p_recv_buf = p_args;
    int len = 0;
    unsigned long cp_len = 0;
#if UUSB_DBG
    UINT8 *p;
    int i;
#endif

    UUSBDBG0("HCISU_LOG: uusb_ReadIntrDataCB called\n");
    usb.reading_intr_data = FALSE;

    if (usb.state != 2)
    {
        UUSBDBG0("HCISU_ERR: uusb_ReadIntrDataCB: usb_state != UUSB_PPC_OPENED_ST\n");
        UUSBDBG0("HCISU_ERR: ************\n* uusb_ReadIntrDataCB(): usb_state != UUSB_PPC_OPENED_ST - stop reading\n************\n");
        GKI_freebuf(p_recv_buf);
        p_recv_buf = NULL;
        uusb_ReleaseCmdBufferPool();

        return;
    }

    if (p_recv_buf == NULL)
    {
        goto end;
    }

    if (ret <= 0)
    {
        UUSBDBG("HCISU_ERR: uusb_ReadIntrDataCB(): Got Error code %d\n", ret);
        GKI_freebuf(p_recv_buf);
        p_recv_buf = NULL;
        goto end;
    }

    p_buf1 = GKI_getpoolbuf(GKI_POOL_ID_2);
    if (!p_buf1)
    {
        GKI_freebuf(p_recv_buf);
        p_recv_buf = NULL;
        goto end;
    }

    p_recv_buf->event = MSG_HC_TO_STACK_HCI_EVT;
    len = ret;
    p_recv_buf->len = len;

    cp_len = sizeof *p_recv_buf + p_recv_buf->len + p_recv_buf->offset;
    cp_len = (cp_len >> 2 << 2) + 4; // round up to 4
    memcpy(p_buf1, p_recv_buf, cp_len);

#if UUSB_DBG
    UUSBDBG("HCISU_LOG: uusb_ReadIntrDataCB: len(%d) offset (%d) cp_len(%d) data = ",
        p_buf1->len,
        p_buf1->offset,
        cp_len);
    p = (UINT8 *)(p_buf1 + 1) + p_buf1->offset;
    for (i = 0; i < ret; ++i)
    {
        UUSBDBG("%02x ", p[i]);
    }
    UUSBDBG0("\n");
#endif

    OSSwitchFiberEx((u32)p_buf1, 0, 0, 0, (u32)&bta_ci_hci_msg_handler, (u32)(__uusb_ppc_stack1 + sizeof __uusb_ppc_stack1));

    GKI_freebuf(p_recv_buf);
    p_recv_buf = NULL;

end:
    uusb_issue_intr_read();
} // 0x8024CC30

/* Bulk-in pipe completion (ACL data): copies the packet into an ACL pool buffer, passes complete L2CAP
 * packets to the BTA through the fibre and reissues the read. */
static void uusb_ReadBulkDataCB(s32 ret, void *p_args)
{
    HC_BT_HDR *p_buf1 = NULL;
    HC_BT_HDR *p_recv_buf = p_args;
    unsigned long cp_len = 0;
#if UUSB_DBG
    UINT8 *p;
    int i;
#endif

    if (usb.state != 2)
    {
        UUSBDBG0("HCISU_ERR: uusb_readBulkDataCB: usb_state != UUSB_PPC_OPENED_ST\n");
        UUSBDBG0("HCISU_ERR: ****\n* uusb_readBulkDataCB(): usb_state != UUSB_PPC_OPENED_ST - stop reading\n****\n");
        GKI_freebuf(p_recv_buf);
        p_recv_buf = NULL;
        uusb_ReleaseAclBufferPool();

        return;
    }

    if (ret <= 0)
    {
        UUSBDBG("HCISU_ERR: uusb_ReadBulkDataCB(): Got Error code %d\n", ret);
        GKI_freebuf(p_recv_buf);
        p_recv_buf = NULL;
        goto end;
    }

    p_recv_buf->len = ret;

    p_buf1 = GKI_getpoolbuf(GKI_POOL_ID_3);
    if (!p_buf1)
    {
        GKI_freebuf(p_recv_buf);
        p_recv_buf = NULL;
        goto end;
    }

    cp_len = sizeof *p_recv_buf + p_recv_buf->len + p_recv_buf->offset;
    cp_len = (cp_len >> 2 << 2) + 4;
    memcpy(p_buf1, p_recv_buf, cp_len);

#if UUSB_DBG
    UUSBDBG("HCISU_LOG: uusb_ReadBulkDataCB: len(%d) offset(%d) cp_len(%d) data = ",
        p_buf1->len,
        p_buf1->offset,
        cp_len);
    p = (UINT8 *)(p_buf1 + 1) + p_buf1->offset;
    for (i = 0; i < p_buf1->len; ++i)
    {
        UUSBDBG("%02x ", p[i]);
    }
    UUSBDBG0("\n");
#endif

    p_buf1 = (HC_BT_HDR *)l2cap_link_chk_pkt_start((BT_HDR *)p_buf1);

    if (p_buf1 != NULL && l2cap_link_chk_pkt_end())
    {
        OSSwitchFiberEx((u32)p_buf1, 0, 0, 0, (u32)&bta_ci_hci_msg_handler, (u32)(__uusb_ppc_stack2 + sizeof __uusb_ppc_stack2));
        p_buf1 = 0;
    }

    GKI_freebuf(p_recv_buf);
    p_recv_buf = NULL;

end:
    uusb_issue_bulk_read();
} // 0x8024CDE0

/* Queues an asynchronous read on the bulk-in (ACL) pipe into a fresh pool buffer. */
static void uusb_issue_bulk_read(void)
{
    HC_BT_HDR *p_buf = NULL;
    void *p_buffer;
    IOSError ret;

#if UUSB_DBG
    for (;;)
    {
        p_buf = GKI_getpoolbuf(usb.acl_buffer_pool);
        if (p_buf != NULL)
        {
            break;
        }
        UUSBDBG0("HCISU_ERR: ****\n* uusb_issue_bulk_read(): unable to get buffer - try again\n****\n");
    }
#else
    do
    {
        p_buf = GKI_getpoolbuf(usb.acl_buffer_pool);
    } while (!p_buf);
#endif

    p_buf->event = MSG_HC_TO_STACK_HCI_ACL;
    p_buf->len = 0;

    p_buffer = (void *)OSRoundUp32B((UINT8 *)(p_buf + 1));

    p_buf->offset = (UINT8 *)p_buffer - (UINT8 *)(p_buf + 1);

    ret = IUSB_ReadBlkMsgAsync(usb.fd, usb.bulkEndpoint2, GKI_get_buf_size(p_buf) - 0x28 - p_buf->offset, p_buffer, uusb_ReadBulkDataCB, p_buf);

    if (ret != 0)
    {
        UUSBDBG("HCISU_ERR: uusb_issue_bulk_read: IUSB_ReadBlkMsgAsync failed with err = %d\n", ret);
        UUSBDBG("HCISU_ERR: ****\n* uusb_issue_bulk_read: IUSB_ReadBlkMsgAsync failed with err = %d\n****\n", ret);
        GKI_freebuf(p_buf);
    }
}

/* Queues an asynchronous read on the interrupt-in (HCI event) pipe into a fresh pool buffer. */
static void uusb_issue_intr_read(void)
{
    HC_BT_HDR *p_buf = NULL;
    void *p_buffer = NULL;
    IOSError ret;

#if UUSB_DBG
    for (;;)
    {
        p_buf = GKI_getpoolbuf(usb.cmd_buffer_pool);
        if (p_buf != NULL)
        {
            break;
        }
        UUSBDBG0("HCISU_ERR: ****\n* uusb_issue_intr_read(): unable to get buffer - try again\n****\n");
    }
#else
    do
    {
        p_buf = GKI_getpoolbuf(usb.cmd_buffer_pool);
    } while (!p_buf);
#endif

    p_buf->event = MSG_HC_TO_STACK_HCI_EVT;
    p_buf->len = 0;

    p_buffer = (void *)OSRoundUp32B((UINT8 *)(p_buf + 1));

    p_buf->offset = (UINT8 *)p_buffer - (UINT8 *)(p_buf + 1);

    ret = IUSB_ReadIntrMsgAsync(usb.fd, usb.intrEndpoint1, GKI_get_buf_size(p_buf) - 0x28 - p_buf->offset, p_buffer, uusb_ReadIntrDataCB, p_buf);

    if (ret != 0)
    {
        UUSBDBG("HCISU_ERR: IUSB_ReadIntrMsgAsync failed with err = %d\n", ret);
        UUSBDBG("HCISU_ERR: ****\n* uusb_issue_intr_read(): IUSB_ReadIntrMsgAsync failed with err = %d\n****\n", ret);
        GKI_freebuf(p_buf);
    }

    usb.reading_intr_data = TRUE;
}

/* Control-pipe write completion: frees the finished buffer and sends the next queued command. */
static void uusb_WriteCtrlDataCB(s32 result, void *p_data)
{
    HC_BT_HDR *p_buf = NULL;
    void *p_buffer = NULL;
    IOSError ret = 0;

    UUSBDBG("HCISU_LOG: uusb_WriteCtrlDataCB called with err = %d\n", result);
    if (result < 0)
    {
        UUSBDBG("HCISU_ERR: uusb_WriteCtrlDataCB(): Got error (%d)\n", result);
    }

    if (p_data != NULL)
    {
        GKI_freebuf(p_data);
        p_data = NULL;

        GKI_disable();

        --usb.ctrl_writes_pending;

        GKI_enable();
    }

    if (usb.ctrl_writes_pending >= 5)
    {
        return;
    }

    if (usb.intr_buffer_q.count == 0)
    {
        return;
    }

    p_buf = GKI_dequeue(&usb.intr_buffer_q);
    p_buffer = (UINT8 *)(p_buf + 1) + p_buf->offset;

    ret = IUSB_WriteCtrlMsgAsync(usb.fd, 0x20, 0, 0, 0, p_buf->len, p_buffer, uusb_WriteCtrlDataCB, p_buf);

    if (ret < 0)
    {
        UUSBDBG0("HCISU_ERR: uusb_WriteCtrlDataCB(): error - unable to write data packet\n");
        GKI_enqueue_head(&usb.intr_buffer_q, p_buf);
    }
    else
    {
        GKI_disable();

        ++usb.ctrl_writes_pending;

        GKI_enable();
    }
} // 0x8024CF80

/* Bulk-pipe write completion: frees the finished buffer and sends the next queued ACL packet. */
static void uusb_WriteBulkDataCB(s32 result, void *p_data)
{
    HC_BT_HDR *p_buf = NULL;
    void *p_buffer = NULL;
    IOSError ret = 0;

    UUSBDBG("HCISU_LOG: uusb_WriteBulkDataCB called with err = %d\n", result);
    if (result < 0)
    {
        UUSBDBG("HCISU_ERR: uusb_WriteBulkDataCB(): Got error (%d)\n", result);
    }

    if (p_data != NULL)
    {
        GKI_freebuf(p_data);
        p_data = NULL;

        GKI_disable();

        --usb.bulk_writes_pending;

        GKI_enable();
    }

    if (usb.bulk_writes_pending >= 5)
    {
        return;
    }

    if (usb.bulk_buffer_q.count == 0)
    {
        return;
    }

    p_buf = GKI_dequeue(&usb.bulk_buffer_q);

    p_buffer = (UINT8 *)(p_buf + 1) + p_buf->offset;
    ret = IUSB_WriteBlkMsgAsync(usb.fd, usb.bulkEndpoint1, p_buf->len, p_buffer, uusb_WriteBulkDataCB, p_buf);

    if (ret < 0)
    {
        UUSBDBG0("HCISU_ERR: uusb_WriteBulkDataCB(): error - unable to write data packet\n");
        GKI_enqueue_head(&usb.bulk_buffer_q, p_buf);
    }
    else
    {
        GKI_disable();

        ++usb.bulk_writes_pending;

        GKI_enable();
    }
} // 0x8024D070

/* Opens the device vid:pid on the selected OHCI controller; returns the IOS file descriptor or an error. */
static s32 uusb_get_devId(int vid, int pid)
{
    IOSError ret;

    if (__ntd_ohci_init_flag == TRUE)
    {
        if (__ntd_ohci == 0)
        {
            ret = IUSB_OpenDeviceIds("oh0", vid, pid, &usb.fd);
        }
        else if (__ntd_ohci == 1)
        {
            ret = IUSB_OpenDeviceIds("oh1", vid, pid, &usb.fd);
        }
    }
    else
    {
        ret = IUSB_OpenDeviceIds("oh1", vid, pid, &usb.fd);
    }

    if (ret < 0)
    {
        UUSBDBG("HCISU_ERR: ERROR from IUSB_OpenDeviceIds (%d) : Device (vendor 0x%x product 0x%x) not found\n",
            ret,
            0x057E,
            0x0305);
        UUSBDBG0("HCISU_ERR: No suitable Bluetooth Device was found. Exiting...\n");
    }
    else
    {
        __ntd_ios_file_descriptor = ret;
    }

    return ret;
}

/* Initialises the USB transport: opens the IOS USB library and the Bluetooth device, creates the command
 * and ACL buffer pools and moves to the registered state. */
void UUSB_Register(tUUSB *uusb)
{
    IOSError ret = 0;

    memset(&usb, 0, sizeof usb);

    GKI_disable();

    usb.state = 5;

    GKI_enable();

    if (!uusb_g_trace_state_initialized)
    {
        uusb_set_trace_state(0);
    }

    ret = IPCCltInit();
    if (ret != 0)
    {
        UUSBDBG0("HCISU_ERR: UUSB_Register: ERROR - failed IPCCltInit\n");
        return;
    }

    ret = IUSB_OpenLib();
    if (ret != 0)
    {
        UUSBDBG0("HCISU_ERR: UUSB_Register: ERROR - failed IUSB_OpenLib\n");
        return;
    }

    usb.p_uusb_cfg = uusb;

    if (__ntd_pid_vid_specified == TRUE)
    {
        usb.vid = __ntd_vid;
        usb.pid = __ntd_pid;
    }
    else
    {
        usb.vid = 0x057E; // 1406 - Nintendo Co., Ltd.
        usb.pid = 0x0305; //  773 - Broadcom BCM2045A Bluetooth Radio [Nintendo Wii/Wii U]
    }

    usb.bulkEndpoint1 = 0;
    usb.bulkEndpoint2 = 0;
    usb.intrEndpoint1 = 0;
    usb.intrEndpoint2 = 0;

    if (uusb_get_devId(usb.vid, usb.pid) < 0)
    {
        UUSBDBG0("HCISU_ERR: UUSB_Register: Error returned from get_devId()\n");
        return;
    }

    UUSBDBG("HCISU_LOG: UUSB_Register: uusb_get_dev_id returned with usb.devId = %d\n",
        usb.fd);

    usb.bulkEndpoint1 = 2;
    usb.bulkEndpoint2 = 130;
    usb.intrEndpoint1 = 129;
    usb.intrEndpoint2 = 0;

    GKI_init_q(&usb.bulk_buffer_q);
    usb.bulk_writes_pending = 0;

    GKI_init_q(&usb.intr_buffer_q);
    usb.ctrl_writes_pending = 0;

    usb.cmd_buffer_pool = GKI_create_pool(660, 45, 1, NULL);
    usb.acl_buffer_pool = GKI_create_pool(1800, 30, 1, NULL);

    if (usb.cmd_buffer_pool == GKI_INVALID_POOL
        || usb.acl_buffer_pool == GKI_INVALID_POOL)
    {
        UUSBDBG0("HCISU_LOG: UUSB_Register: Unable to create memory pools\n");
        return;
    }

    UUSBDBG("HCISU_LOG: UUSB_Register: hci_cmd_usb_pool_id (%d) hci_acl_usb_pool_id(%d)\n",
        usb.cmd_buffer_pool,
        usb.acl_buffer_pool);

    GKI_disable();

    usb.state = 4;

    GKI_enable();

    wait4hci = TRUE;
} // 0x8024D150

/* Starts the transport once registered: installs the callback, starts both read pipes and releases
 * BTE_BringUpStack's wait. */
void UUSB_Open(tUUSB *uusb, tUUSB_CBACK *cback)
{
    if (usb.state != 4)
    {
        UUSBDBG0("HCISU_LOG: UUSB_Open: USB is not in registered state, unable to open\n");
        return;
    }

    if (usb.cmd_buffer_pool == GKI_INVALID_POOL
        || usb.acl_buffer_pool == GKI_INVALID_POOL)
    {
        UUSBDBG0("HCISU_LOG: UUSB_Open: USB Buffer Pool's Invalid\n");
        UUSBDBG("HCISU_LOG: UUSB_Open: hci_cmd_usb_pool_id = %d\n",
            usb.cmd_buffer_pool);
        UUSBDBG("HCISU_LOG: UUSB_Open: hci_acl_usb_pool_id = %d\n",
            usb.acl_buffer_pool);
        return;
    }

    GKI_disable();

    usb.cback = cback;
    usb.state = 2;

    GKI_enable();

    uusb_issue_intr_read();
    uusb_issue_bulk_read();

    wait4hci = FALSE;
} // 0x8024D360

/* Reading is callback driven (uusb_Read*DataCB); this entry point never returns data. */
UINT16 UUSB_Read(UINT8 port, void *p_data, UINT16 len)
{
    return 0;
} // 0x8024D4F0

/* Sends an HCI command (port 0, control pipe) or ACL packet (port 2, bulk pipe), queueing it when five
 * writes are already outstanding. */
UINT16 UUSB_Write(UINT8 port, void *p_data, UINT16 len, void *p_hci_buf /* name invented */)
{
    IOSError ret = 0;
    void *p_buffer = NULL;
    HC_BT_HDR *p_buf = NULL;
#if UUSB_DBG
    UINT16 i = 0;
#endif

    if (usb.state != 2)
    {
        UUSBDBG0("HCISU_ERR: UUSB_Write: usb_state != UUSB_PPC_OPENED_ST\n");
        return 0;
    }

#if UUSB_DBG
    UUSBDBG("HCISU_LOG: buffer size = %d\n",
        GKI_get_buf_size(p_hci_buf) - sizeof(HC_BT_HDR));
    UUSBDBG("HCISU_LOG: UUSB_Write: ep(%d) len(%d) data = ", port, len);
    for (i = 0; i < len; ++i)
    {
        UUSBDBG("%02x ", *((UINT8 *)p_data + i));
    }
    UUSBDBG0("\n");
#endif

    switch (port)
    {
    case 0:
        p_buf = GKI_getpoolbuf(usb.cmd_buffer_pool);

        if (p_buf == NULL)
        {
            UUSBDBG0("HCISU_ERR: UUSB_Write: Unable to get buffer from hci_cmd_usb_pool\n");
            return 0;
        }

        p_buf->len = len;

        p_buffer = (void *)OSRoundUp32B((UINT8 *)(p_buf + 1));

        p_buf->offset = (UINT8 *)p_buffer - (UINT8 *)(p_buf + 1);

        memcpy(p_buffer, p_data, len);

        if (usb.ctrl_writes_pending < 5 && usb.intr_buffer_q.count == 0)
        {
            ret = IUSB_WriteCtrlMsgAsync(usb.fd, 0x20, 0, 0, 0, len, p_buffer, uusb_WriteCtrlDataCB, p_buf);
        }
        else
        {
            GKI_enqueue(&usb.intr_buffer_q, p_buf);
            UUSBDBG("HCISU_ERR: UUSB_WRITE(): Ctrl Queue count =  %d\n",
                usb.intr_buffer_q.count);

            return 0;
        }

        if (ret != 0)
        {
            UUSBDBG0("HCISU_ERR: *************************************************************************************\n");
            UUSBDBG0("HCISU_ERR: *************************************************************************************\n");
            UUSBDBG("HCISU_ERR: * UUSB_Write(): IUSB_WriteCtrlMsgAsync error...  (%d), writes outstanding (%d)\n",
                ret,
                usb.ctrl_writes_pending);
            if (ret != -22)
            {
                UUSBDBG("HCISU_ERR: * UUSB_Write(): IUSB_WriteCtrlMsgAsync error... IOS_ERROR_FAIL_ALLOC  (%d), writes outstanding (%d)\n",
                    ret,
                    usb.bulk_writes_pending);
            }

            if (ret == -8)
            {
                UUSBDBG("HCISU_ERR: * UUSB_Write(): IUSB_WriteCtrlMsgAsync error... IOS_ERROR_QFULL  (%d), writes outstanding (%d)\n",
                    ret,
                    usb.bulk_writes_pending);
            }

            UUSBDBG0("HCISU_ERR: *************************************************************************************\n");
            UUSBDBG0("HCISU_ERR: *************************************************************************************\n");

            GKI_freebuf(p_buf);
            p_buf = NULL;
        }
        else
        {
            GKI_disable();

            ++usb.ctrl_writes_pending;

            GKI_enable();
        }

        break;

    case 2:
        p_buf = GKI_getpoolbuf(usb.acl_buffer_pool);

        if (p_buf == NULL)
        {
            UUSBDBG0("HCISU_ERR: UUSB_Write: Unable to get buffer from hci_acl_usb_pool\n");
            return 0;
        }

        p_buf->len = len;

        p_buffer = (void *)OSRoundUp32B((UINT8 *)(p_buf + 1));

        p_buf->offset = (UINT8 *)p_buffer - (UINT8 *)(p_buf + 1);

        memcpy(p_buffer, p_data, len);

        if (len > 190)
        {
            UUSBDBG0("HCISU_ERR: UUSB_Write - writing mem data\n");
        }

        if (usb.bulk_writes_pending < 5 && usb.bulk_buffer_q.count == 0)
        {
            ret = IUSB_WriteBlkMsgAsync(usb.fd, usb.bulkEndpoint1, len, p_buffer, uusb_WriteBulkDataCB, p_buf);
        }
        else
        {
            GKI_enqueue(&usb.bulk_buffer_q, p_buf);
            UUSBDBG("HCISU_ERR: UUSB_WRITE(): Bulk Queue count =  %d\n",
                usb.bulk_buffer_q.count);

            return 0;
        }

        if (ret != 0)
        {
            UUSBDBG0("HCISU_ERR: *************************************************************************************\n");
            UUSBDBG0("HCISU_ERR: *************************************************************************************\n");
            UUSBDBG("HCISU_ERR: * UUSB_Write(): IUSB_WriteBlkMsgAsync error...  (%d), writes outstanding (%d)\n",
                ret,
                usb.bulk_writes_pending);
            if (ret != -22)
            {
                UUSBDBG("HCISU_ERR: * UUSB_Write(): IUSB_WriteBlkMsgAsync error... IOS_ERROR_FAIL_ALLOC  (%d), writes outstanding (%d)\n",
                    ret,
                    usb.bulk_writes_pending);
            }

            if (ret == -8)
            {
                UUSBDBG("HCISU_ERR: * UUSB_Write(): IUSB_WriteBlkMsgAsync error... IOS_ERROR_QFULL  (%d), writes outstanding (%d)\n",
                    ret,
                    usb.bulk_writes_pending);
            }

            UUSBDBG0("HCISU_ERR: *************************************************************************************\n");
            UUSBDBG0("HCISU_ERR: *************************************************************************************\n");

            GKI_freebuf(p_buf);
            p_buf = NULL;
        }
        else
        {
            GKI_disable();

            ++usb.bulk_writes_pending;

            GKI_enable();
        }

        break;

    case 3:
        break;
    }

    return ret;
} // 0x8024D500

/* Closes the transport: drops the queued writes and closes the IOS device; uusb_CloseDeviceCB finishes. */
void UUSB_Close(void)
{
    GKI_disable();

    usb.state = 0;

    GKI_enable();

    if (usb.bulk_buffer_q.count)
    {
        while (usb.bulk_buffer_q.count)
        {
            GKI_freebuf(GKI_dequeue(&usb.bulk_buffer_q));
        }
    }

    if (usb.intr_buffer_q.count)
    {
        while (usb.intr_buffer_q.count)
        {
            GKI_freebuf(GKI_dequeue(&usb.intr_buffer_q));
        }
    }

    IUSB_CloseDeviceAsync(usb.fd, uusb_CloseDeviceCB, NULL);
} // 0x8024D710

/* Closes the IOS USB library and returns the transport to its initial state. */
void UUSB_Unregister(void)
{
    IUSB_CloseLib();

    GKI_disable();

    usb.state = 5;
    usb.trace_state = 0;

    uusb_g_trace_state_initialized = FALSE;

    GKI_enable();
} // 0x8024D7C0
