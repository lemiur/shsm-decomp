// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Export/mslsupp.c

#include <stddef.h>
#include "TRK/dsmsg.h"
#include "TRK/msghndlr.h"
#include "TRK/target_options.h"
#include "TRK/targsupp.h"

/* The MSL console and file hooks of the nub: stdin and stdout travel to the host debugger as file requests.
 * The file functions are defined after their callers, so the callers reach them out of line. */
DSIOResult __read_file(u32 handle, u8* buffer, size_t* count, void* ref_con);
DSIOResult __write_file(u32 handle, u8* buffer, size_t* count, void* ref_con);
DSIOResult __close_file(u32 handle);
DSIOResult __access_file(u32 handle, u8* buffer, size_t* count, void* ref_con, MessageCommandID cmd);

/* Reads from the debugger's stdin. */
DSIOResult __read_console(u32 handle, u8* buffer, size_t* count, void* ref_con) {
    if (GetUseSerialIO() == 0) {
        return DS_IOError;
    }
    return __read_file(DS_Stdin, buffer, count, ref_con);
} // 0x801942F4

/* Writes to the debugger's stdout. */
DSIOResult __TRK_write_console(u32 handle, u8* buffer, size_t* count, void* ref_con) {
    if (GetUseSerialIO() == 0) {
        return DS_IOError;
    }
    return __write_file(DS_Stdout, buffer, count, ref_con);
} // 0x8019435C

/* Closes an MSL stream by closing its file on the host. */
DSIOResult __close_console(u32 handle) {
    return __close_file(handle);
} // 0x801943C4

/* Reads from a host file. */
DSIOResult __read_file(u32 handle, u8* buffer, size_t* count, void* ref_con) {
    return __access_file(handle, buffer, count, ref_con, DSMSG_ReadFile);
} // 0x801943C8

/* Writes to a host file. */
DSIOResult __write_file(u32 handle, u8* buffer, size_t* count, void* ref_con) {
    return __access_file(handle, buffer, count, ref_con, DSMSG_WriteFile);
} // 0x801943D0

/* Closes a host file; the host reports the IO result. */
DSIOResult __close_file(u32 handle) {
    u32 result;

    if (GetTRKConnected() == 0) {
        return DS_IOError;
    }

    result = TRKCloseFile(DSMSG_CloseFile, handle);
    switch ((u8)result) {
    case DS_IONoError:
        return DS_IONoError;
    case DS_IOEOF:
        return DS_IOEOF;
    }
    return DS_IOError;
} // 0x801943D8

/* Sends a read or write request for a host file; *count carries the byte count in and the bytes moved out. */
DSIOResult __access_file(u32 handle, u8* buffer, size_t* count, void* ref_con, MessageCommandID cmd) {
    size_t countTemp;
    u32 result;

    if (GetTRKConnected() == 0) {
        return DS_IOError;
    }

    countTemp = *count;
    result = TRKAccessFile(cmd, handle, &countTemp, buffer);
    *count = countTemp;

    switch ((u8)result) {
    case DS_IONoError:
        return DS_IONoError;
    case DS_IOEOF:
        return DS_IOEOF;
    }
    return DS_IOError;
} // 0x80194448
