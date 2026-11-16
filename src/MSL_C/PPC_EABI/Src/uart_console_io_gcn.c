// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/PPC_EABI/Src/uart_console_io_gcn.c
/* Unit name unconfirmed: MSL uart_console_io_gcn.c, as in the public decomps; no DOL string names the file. */

#include <MSL_C/MSL_Common/console_io.h>
#include <revolution/euart.h>
#include <revolution/os/OS.h>

/* OSGetConsoleType bit set on a development target with a debugger attached (no UART output then). */
#define CONSOLE_TYPE_TDEV 0x20000000

/*
 * __write_console
 *
 * Writes the console stream to the UART, opening it at 57600 baud on first
 * use; on a development target the text goes to the debugger instead.
 * Returns 1 when the UART fails, 0 otherwise.
 */
int __write_console(__file_handle handle, unsigned char* buffer, size_t* count, void* idle_proc) {
    static int initialized;

    if ((OSGetConsoleType() & CONSOLE_TYPE_TDEV) == 0) {
        int uart_init = 0;

        if (initialized == 0) {
            uart_init = InitializeUART(kBaud57600);

            if (uart_init == 0) {
                initialized = 1;
            }
        }

        if (uart_init != 0) {
            return 1;
        }

        if (WriteUARTN(buffer, *count) != 0) {
            *count = 0;
            return 1;
        }
    }

    __TRK_write_console(handle, buffer, count, idle_proc);
    return 0;
} // 0x801A1CB0
