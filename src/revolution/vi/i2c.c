// Reference: zeldaret/tp libs/revolution/src/vi/i2c.c
//
// Bit-banged I2C master on the VI GPIO pins (video DAC control).
#include <revolution/os.h>
#include <revolution/vi.h>

#include <revolution/vi/__vi.h>

static volatile u32 __i2c_ident_flag = 1; // volatile: SDK declaration (zeldaret/tp i2c.c)
static volatile u32 __i2c_ident_first = 0; // volatile: SDK declaration (zeldaret/tp i2c.c)

/* The DAC bus hangs off the Hollywood GPIO block: word 0x30 drives the pins, 0x31 sets the directions and 0x32
   reads them back. SCL is pin 14 and SDA is pin 15. */
#define GPIO_OUT __ACRRegs[0x30]
#define GPIO_DIR __ACRRegs[0x31]
#define GPIO_IN  __ACRRegs[0x32]
#define GPIO_SCL 14
#define GPIO_SDA 15

/*
 * WaitMicroTime
 *
 * Busy-waits for usec microseconds on the system timebase.
 */
void WaitMicroTime(s32 usec) {
    OSTime t = __OSGetSystemTime();

    while (((__OSGetSystemTime() - t) * 8) / 486 < usec);
} // 0x8022A990

/* Selects the pin polarity used for the bus. */
static void VICheckI2C(void) {
    __i2c_ident_flag = 1;
}

/* Drives the clock line. */
static BOOL __VISetSCL(u32 value) {
    u32 reg;
    value &= 1;

    reg = GPIO_OUT;
    reg &= ~(1 << GPIO_SCL);
    reg = (u32)(reg | (value << GPIO_SCL));
    GPIO_OUT = reg;
    return TRUE;
}

/* Drives the data line. */
static BOOL __VISetSDA(u32 value) {
    u32 reg;
    value &= 1;
    reg = GPIO_OUT;
    reg &= ~(1 << GPIO_SDA);
    reg = (u32)(reg | (value << GPIO_SDA));
    GPIO_OUT = reg;
    return TRUE;
}

/* Samples the data line. */
static u32 __VIGetSDA(void) {
    u32 reg = GPIO_IN;
    return (u32)((reg >> GPIO_SDA) & 1);
}

/* Makes the clock an output and sets the data line direction (1 drives it, 0 releases it for the acknowledge). */
static void __VIOpenI2C(u32 dir) {
    u32 reg = GPIO_DIR;
    reg = (u32)(reg & ~(1 << GPIO_SDA));
    reg = (u32)(reg | (1 << GPIO_SCL) | (dir << GPIO_SDA));
    GPIO_DIR = reg;
}

/* Gives the slave time to release the clock. */
static int wait4ClkHigh(void) {
    WaitMicroTime(2);
    return 1;
}

/*
 * sendSlaveAddr
 *
 * Clocks out the start condition and the slave address byte, then samples the acknowledge bit.
 */
static int sendSlaveAddr(u8 slaveAddr) {
    int i;

    if (0 == __i2c_ident_flag) {
        __VISetSDA(1);
    } else {
        __VISetSDA(0);
    }

    WaitMicroTime(2);
    __VISetSCL(0);

    for (i = 0; i < 8; i++) {
        if (slaveAddr & 0x80) {
            if (0 == __i2c_ident_flag) {
                __VISetSDA(0);
            } else {
                __VISetSDA(1);
            }
        } else {
            if (0 == __i2c_ident_flag) {
                __VISetSDA(1);
            } else {
                __VISetSDA(0);
            }
        }

        WaitMicroTime(2);
        __VISetSCL(1);

        if (wait4ClkHigh() == 0) {
            return 0;
        }

        __VISetSCL(0);
        slaveAddr <<= 1;
    }

    __VIOpenI2C(0);
    WaitMicroTime(2);
    __VISetSCL(1);

    if (wait4ClkHigh() == 0) {
        return 0;
    }

    if (1 == __i2c_ident_flag) {
        if (__VIGetSDA() != 0) {
            return 0;
        }
    }

    if (0 == __i2c_ident_flag) {
        __VISetSDA(1);
    } else {
        __VISetSDA(0);
    }

    __VIOpenI2C(1);
    __VISetSCL(0);
    return 1;
} // 0x8022AA20

/*
 * __VISendI2CData
 *
 * Writes nBytes from pData to the slave at slaveAddr with interrupts disabled. Returns 1 on success.
 */
int __VISendI2CData(u8 slaveAddr, u8* pData, int nBytes) {
    s32 i;
    u8 data;
    BOOL enabled;

    if (__i2c_ident_first == 0) {
        VICheckI2C();
        __i2c_ident_first = 1;
    }

    enabled = OSDisableInterrupts();

    __VIOpenI2C(1);
    __VISetSCL(1);

    if (0 == __i2c_ident_flag) {
        __VISetSDA(0);
    } else {
        __VISetSDA(1);
    }

    WaitMicroTime(2);
    WaitMicroTime(2);

    if (sendSlaveAddr(slaveAddr) == 0) {
        OSRestoreInterrupts(enabled);
        return 0;
    }

    __VIOpenI2C(1);

    while (nBytes != 0) {
        data = *pData++;
        for (i = 0; i < 8; i++) {
            if (data & 0x80) {
                if (0 == __i2c_ident_flag) {
                    __VISetSDA(0);
                } else {
                    __VISetSDA(1);
                }
            } else {
                if (0 == __i2c_ident_flag) {
                    __VISetSDA(1);
                } else {
                    __VISetSDA(0);
                }
            }

            WaitMicroTime(2);
            __VISetSCL(1);

            if (wait4ClkHigh() == 0) {
                OSRestoreInterrupts(enabled);
                return 0;
            }

            __VISetSCL(0);
            data <<= 1;
        }

        __VIOpenI2C(0);
        WaitMicroTime(2);
        __VISetSCL(1);

        if (wait4ClkHigh() == 0) {
            OSRestoreInterrupts(enabled);
            return 0;
        }

        if (1 == __i2c_ident_flag) {
            if (__VIGetSDA() != 0) {
                OSRestoreInterrupts(enabled);
                return 0;
            }
        }

        if (0 == __i2c_ident_flag) {
            __VISetSDA(1);
        } else {
            __VISetSDA(0);
        }

        __VIOpenI2C(1);
        __VISetSCL(0);
        nBytes--;
    }

    __VIOpenI2C(1);

    if (0 == __i2c_ident_flag) {
        __VISetSDA(1);
    } else {
        __VISetSDA(0);
    }

    WaitMicroTime(2);
    __VISetSCL(1);
    WaitMicroTime(2);

    if (0 == __i2c_ident_flag) {
        __VISetSDA(0);
    } else {
        __VISetSDA(1);
    }

    OSRestoreInterrupts(enabled);
    return 1;
} // 0x8022AD70
