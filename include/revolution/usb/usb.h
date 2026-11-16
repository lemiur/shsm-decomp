// Reference: PrimeDecomp/sdk include/revolution/usb.h
// Reference: zeldaret/tp libs/revolution/include/revolution/usb.h
#ifndef REVOLUTION_USB_USB_H
#define REVOLUTION_USB_USB_H

#include <revolution/types.h>
#include <revolution/ios/iostypes.h>
#include <revolution/ipc/ipcclt.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Byte swaps between the CPU's big-endian order and USB's little-endian wire order. */
#define SWAP32(val)                                                                                                    \
    ((u32)((((u32)(val) & (u32)0x000000ffUL) << 24) | (((u32)(val) & (u32)0x0000ff00UL) << 8) |                        \
           (((u32)(val) & (u32)0x00ff0000UL) >> 8) | (((u32)(val) & (u32)0xff000000UL) >> 24)))

#define SWAP16(val) ((u16)((((u16)(val) & (u16)0x00ffUL) << 8) | (((u16)(val) & (u16)0xff00) >> 8)))

/*
 * USB_DevDescr
 *
 * The standard USB device descriptor, as the device sends it (little-endian fields).
 */
typedef struct {
    u8 bLength;            // 0x00
    u8 bDescriptorType;    // 0x01
    u16 bcdUSB;            // 0x02
    u8 bDeviceClass;       // 0x04
    u8 bDeviceSubClass;    // 0x05
    u8 bDeviceProtocol;    // 0x06
    u8 bMaxPacketSize0;    // 0x07
    u16 idVendor;          // 0x08
    u16 idProduct;         // 0x0A
    u16 bcdDevice;         // 0x0C
    u8 iManufacturer;      // 0x0E
    u8 iProduct;           // 0x0F
    u8 iSerialNumber;      // 0x10
    u8 bNumConfigurations; // 0x11
} USB_DevDescr; // size 0x12

/*
 * USBDeviceInfo
 *
 * One entry of the device list IOS returns for a host controller.
 */
typedef struct USBDeviceInfo {
    IOSFd fd; // 0x00
    u16 vid;  // 0x04
    u16 pid;  // 0x06
} USBDeviceInfo; // size 0x08

/*
 * IsoTransfer
 *
 * An isochronous transfer: one buffer split into up to eight packets.
 */
typedef struct IsoTransfer {
    void* buf;       // 0x00
    u8 numPackets;   // 0x04
    u16* packets;    // 0x08
} IsoTransfer; // size 0x0C

typedef void (*USBCallbackFunc)(IOSError result, void* cbArg);
typedef void (*USBIsoCallbackFunc)(IOSError result, IsoTransfer* xfer, void* cbArg);

IOSError IUSB_OpenLib(void);
IOSError IUSB_CloseLib(void);
IOSError IUSB_OpenDeviceIds(const char* did, u16 vid, u16 pid, IOSFd* fd);
IOSError IUSB_OpenDeviceIdsAsync(const char* did, u16 vid, u16 pid, USBCallbackFunc cb, void* cbArg);
IOSError IUSB_CloseDevice(IOSFd fd);
IOSError IUSB_CloseDeviceAsync(IOSFd fd, USBCallbackFunc cb, void* cbArg);
IOSError IUSB_GetDeviceList(const char* path, USBDeviceInfo* deviceList, u8 maxDev, u8 deviceClass, u8* numDev);
IOSError IUSB_ReadIntrMsg(IOSFd fd, u32 ep, u32 buflen, char* buf);
IOSError IUSB_ReadIntrMsgAsync(IOSFd fd, u32 ep, u32 buflen, char* buf, USBCallbackFunc cb, void* cbArg);
IOSError IUSB_WriteIntrMsg(IOSFd fd, u32 ep, u32 buflen, char* buf);
IOSError IUSB_WriteIntrMsgAsync(IOSFd fd, u32 ep, u32 buflen, char* buf, USBCallbackFunc cb, void* cbArg);
IOSError IUSB_ReadBlkMsg(IOSFd fd, u32 ep, u32 buflen, char* buf);
IOSError IUSB_ReadBlkMsgAsync(IOSFd fd, u32 ep, u32 buflen, char* buf, USBCallbackFunc cb, void* cbArg);
IOSError IUSB_WriteBlkMsg(IOSFd fd, u32 ep, u32 buflen, char* buf);
IOSError IUSB_WriteBlkMsgAsync(IOSFd fd, u32 ep, u32 buflen, char* buf, USBCallbackFunc cb, void* cbArg);
IOSError IUSB_ReadCtrlMsg(IOSFd fd, u8 reqType, u8 request, u16 value, u16 index, u16 buflen, char* buf);
IOSError IUSB_ReadCtrlMsgAsync(IOSFd fd, u8 reqType, u8 request, u16 value, u16 index, u16 buflen, char* buf,
                               USBCallbackFunc cb, void* cbArg);
IOSError IUSB_WriteCtrlMsg(IOSFd fd, u8 reqType, u8 request, u16 value, u16 index, u16 buflen, char* buf);
IOSError IUSB_WriteCtrlMsgAsync(IOSFd fd, u8 reqType, u8 request, u16 value, u16 index, u16 buflen, char* buf,
                                USBCallbackFunc cb, void* cbArg);
IOSError IUSB_GetAsciiStr(IOSFd fd, u8 ep, u16 index, u16 langId, char* buf, u16 buflen);
IOSError IUSB_GetAsciiStrAsync(IOSFd fd, u8 ep, u16 index, u16 langId, char* buf, u16 buflen, USBCallbackFunc cb,
                               void* cbArg);
IOSError IUSB_GetDevDescr(IOSFd fd, USB_DevDescr* des);
IOSError IUSB_GetDevDescrAsync(IOSFd fd, USB_DevDescr* des, USBCallbackFunc cb, void* cbArg);
IOSError IUSB_SuspendDevice(IOSFd fd);
IOSError IUSB_ResumeDevice(IOSFd fd);
IOSError IUSB_ResetDevice(IOSFd fd);
IOSError IUSB_GetRhDesca(IOSFd fd, u32* desca);
IOSError IUSB_GetRhPortStatus(IOSFd fd, u8 port, u32* stat);
IOSError IUSB_SetRhPortStatus(IOSFd fd, u8 port, u32 stat);
IOSError IUSB_DeviceRemovalNotifyAsync(IOSFd fd, IOSIpcCb cb, void* cbArg);
IOSError IUSB_IsoMsgAsync(IOSFd fd, u8 ep, IsoTransfer* xfer, USBIsoCallbackFunc cb, void* cbArg);
IOSError IUSB_DeviceInsertionNotifyAsync(const char* path, u16 vid, u16 pid, USBCallbackFunc cb, void* cbArg);
IOSError IUSB_DeviceClassInsertionNotifyAsync(const char* path, u8 devClass, USBCallbackFunc cb, void* cbArg);

#ifdef __cplusplus
}
#endif

#endif
