#ifndef REVOLUTION_HARDWARE_H
#define REVOLUTION_HARDWARE_H

#include <revolution/types.h>

/*
 * Memory-mapped hardware register blocks. The header name and __ACRRegs
 * are invented: no SDK name is known for them.
 */

/* The C dialect's absolute-address arrays are not C++; C++ units (the HBM) only include the headers. */
#ifndef __cplusplus

/* Processor interface: interrupt cause (0) and mask (1). */
vu32 __PIRegs[] : 0xCC003000; // volatile: hardware registers

/* Video interface: 16-bit registers, indexed by halfword. */
vu16 __VIRegs[] : 0xCC002000; // volatile: hardware registers

/* Memory interface: 16-bit registers, indexed by halfword. */
vu16 __MEMRegs[] : 0xCC004000; // volatile: hardware registers

/* DSP interface: 16-bit registers, indexed by halfword. */
vu16 __DSPRegs[] : 0xCC005000; // volatile: hardware registers

/* Serial interface: channel command/input words, poll, comm control and status. */
vu32 __SIRegs[] : 0xCD006400; // volatile: hardware registers

/* DVD interface: 32-bit registers. */
vu32 __DIRegs[] : 0xCD006000; // volatile: hardware registers

/* External interface: five words per channel, three channels. */
vu32 __EXIRegs[] : 0xCD006800; // volatile: hardware registers

/* Audio interface. */
vu32 __AIRegs[] : 0xCD006C00; // volatile: hardware registers

/* Hollywood registers through the mapping at physical 0x0D000000; name invented. */
vu32 __ACRRestrictedRegs[] : 0xCD000000; // volatile: hardware registers

/* Hollywood registers through the Broadway-side (unrestricted) mapping. */
vu32 __ACRRegs[] : 0xCD800000; // volatile: hardware registers

#endif /* __cplusplus */

#endif
