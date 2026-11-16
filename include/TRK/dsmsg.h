// Reference: zeldaret/tp libs/PowerPC_EABI_Support/MetroTRK/trk.h (message enums)

#ifndef TRK_DSMSG_H
#define TRK_DSMSG_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IO result a file request reports back to the caller. */
typedef enum DSIOResult {
    DS_IONoError = 0,
    DS_IOError = 1,
    DS_IOEOF = 2
} DSIOResult;

/* Standard stream handles of the file requests. */
typedef enum DSFileHandle {
    DS_Stdin = 0,
    DS_Stdout = 1,
    DS_Stderr = 2
} DSFileHandle;

/* Command byte of a message. A reply's command byte is at least DSMSG_ReplyACK. */
typedef enum MessageCommandID {
    DSMSG_Ping = 0x00,
    DSMSG_Connect = 0x01,
    DSMSG_Disconnect = 0x02,
    DSMSG_Reset = 0x03,
    DSMSG_Versions = 0x04,
    DSMSG_SupportMask = 0x05,
    DSMSG_Override = 0x07,

    DSMSG_ReadMemory = 0x10,
    DSMSG_WriteMemory = 0x11,
    DSMSG_ReadRegisters = 0x12,
    DSMSG_WriteRegisters = 0x13,
    DSMSG_SetOption = 0x17,
    DSMSG_Continue = 0x18,
    DSMSG_Step = 0x19,
    DSMSG_Stop = 0x1A,

    DSMSG_ReplyACK = 0x80,

    DSMSG_NotifyStopped = 0x90,
    DSMSG_NotifyException = 0x91,

    DSMSG_WriteFile = 0xD0,
    DSMSG_ReadFile = 0xD1,
    DSMSG_OpenFile = 0xD2,
    DSMSG_CloseFile = 0xD3,
    DSMSG_PositionFile = 0xD4,

    DSMSG_ReplyNAK = 0xFF
} MessageCommandID;

/* Register classes of the register requests. */
typedef enum DSMessageRegisterOptions {
    DSREG_Default = 0,
    DSREG_FP = 1,
    DSREG_Extended1 = 2,
    DSREG_Extended2 = 3
} DSMessageRegisterOptions;

/* Step kinds of the step request. */
typedef enum DSMessageStepOptions {
    DSSTEP_IntoCount = 0x00,
    DSSTEP_IntoRange = 0x01,
    DSSTEP_OverCount = 0x10,
    DSSTEP_OverRange = 0x11
} DSMessageStepOptions;

/* Error byte of a reply. */
typedef enum DSReplyError {
    DSREPLY_NoError = 0x00,
    DSREPLY_Error = 0x01,
    DSREPLY_PacketSizeError = 0x02,
    DSREPLY_CWDSError = 0x03,
    DSREPLY_EscapeError = 0x04,
    DSREPLY_BadFCS = 0x05,
    DSREPLY_Overflow = 0x06,
    DSREPLY_SequenceMissing = 0x07,

    DSREPLY_UnsupportedCommandError = 0x10,
    DSREPLY_ParameterError = 0x11,
    DSREPLY_UnsupportedOptionError = 0x12,
    DSREPLY_InvalidMemoryRange = 0x13,
    DSREPLY_InvalidRegisterRange = 0x14,
    DSREPLY_CWDSException = 0x15,
    DSREPLY_NotStopped = 0x16,
    DSREPLY_BreakpointsFull = 0x17,
    DSREPLY_BreakpointConflict = 0x18,

    DSREPLY_OSError = 0x20,
    DSREPLY_InvalidProcessID = 0x21,
    DSREPLY_InvalidThreadID = 0x22,
    DSREPLY_DebugSecurityError = 0x23
} DSReplyError;

/* Option bits of the memory requests. */
enum {
    DSMSGMEMORY_Segmented = 0x01,   /* non-flat address space */
    DSMSGMEMORY_Extended = 0x02,    /* more than 32 data address bits */
    DSMSGMEMORY_Protected = 0x04,   /* non-user memory */
    DSMSGMEMORY_Userview = 0x08,    /* breakpoints are invisible */
    DSMSGMEMORY_Space_program = 0x00,
    DSMSGMEMORY_Space_data = 0x40,
    DSMSGMEMORY_Space_io = 0x80
};

/* Head of a request from the host (0x40 bytes). The argument fields are read according to the command. */
typedef struct CommandRequest {
    u32 length;                     /* 0x00: length of the whole message, head included */
    u8 commandID;                   /* 0x04: MessageCommandID */
    u8 unused5;                     /* 0x05 */
    u16 sequence;                   /* 0x06 */
    u8 option;                      /* 0x08: memory space flags, register class, step kind or option number */
    u8 unused9[3];                  /* 0x09 */
    union {
        u16 byteCount;              /* 0x0C: bytes a memory request covers */
        u16 firstRegister;          /* 0x0C: first register of a register request */
        u8 stepCount;               /* 0x0C: instructions a step request runs */
        u8 enable;                  /* 0x0C: value of an option request */
    } arg;
    union {
        u32 address;                /* 0x10: start of a memory request */
        u16 lastRegister;           /* 0x10: last register of a register request */
        u32 rangeStart;             /* 0x10: start of the range of a step request */
    } range;
    u32 rangeEnd;                   /* 0x14: end of the range of a step request */
    u8 unused[0x28];                /* 0x18: rest of the head */
} CommandRequest;

/* Head of a MetroTRK message (0x40 bytes). File requests fill it in; replies are read through it. */
typedef struct CommandReply {
    u32 length;                     /* 0x00: length of the whole message, head included */
    u8 commandID;                   /* 0x04: DSMsgCommand of a request, command byte of a reply */
    u8 unused5;                     /* 0x05 */
    u16 sequence;                   /* 0x06: sequence number stamped by TRKMessageSend */
    union {
        u8 b;                       /* 0x08: error byte of a reply, byte argument of an open request */
        u32 r;                      /* 0x08: file handle */
    } replyError;
    union {
        u16 count;                  /* 0x0C: byte count of a read, write or path */
        u32 offset;                 /* 0x0C: file position */
        u32 serial;                 /* 0x0C: serial number the nub stamps on its replies */
    } arg;
    union {
        u8 seekMode;                /* 0x10: seek mode of a position request */
        u32 ioResult;               /* 0x10: IO result of a reply */
    } param;
    u16 replyLength;                /* 0x14: bytes a read reply carries */
    u32 replyPosition;              /* 0x18: file position a position reply carries */
    u8 unused[0x24];                /* 0x1C: rest of the head */
} CommandReply;

#ifdef __cplusplus
}
#endif

#endif /* TRK_DSMSG_H */
