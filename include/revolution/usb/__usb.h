// Reference: zeldaret/tp libs/revolution/src/usb/__usb.h
//
// The USB client library's private request context.
#ifndef REVOLUTION_USB___USB_H
#define REVOLUTION_USB___USB_H

#include <revolution/usb/usb.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Rounds a size up to the 32-byte IPC buffer granule. */
#define ROUNDUP(sz) (((u32)(sz) + 32 - 1) & ~(u32)(32 - 1))

/*
 * IntBlkCtrlReq
 *
 * The data buffer of an interrupt, bulk or control transfer, kept for the completion callback.
 */
typedef struct {
    char* data;  // 0x00
    u16 wLength; // 0x04
} IntBlkCtrlReq; // size 0x08

/*
 * iusb_ctxt
 *
 * One asynchronous request: the caller's callback and the IPC buffers to free when IOS completes it.
 */
typedef struct iusb_Ctxt {
    USBCallbackFunc cb;     // 0x00
    USBIsoCallbackFunc icb; // 0x04
    void* cbArg;            // 0x08
    void* xfer;             // 0x0C
    void* spare;            // 0x10
    void* clean[8];         // 0x14
    u32 nclean;             // 0x34

    union {
        char path[ROUNDUP(64)];
        USB_DevDescr des;
        IntBlkCtrlReq reqBuf;
    } u ATTRIBUTE_ALIGN(32); // 0x40
} iusb_ctxt; // size 0x80

#ifdef __cplusplus
}
#endif

#endif
