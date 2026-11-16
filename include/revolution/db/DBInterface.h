#ifndef REVOLUTION_DB_DBINTERFACE_H
#define REVOLUTION_DB_DBINTERFACE_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Low-memory block that a debugger on the host shares with the program (physical address 0x40). */
#define DB_INTERFACE_PHYS_ADDR 0x00000040

/* Physical address of the word that holds the running context's physical address. */
#define DB_CURRENT_CONTEXT_PHYS_ADDR 0x000000C0

typedef struct DBInterface {
    u32 bPresent;
    u32 exceptionMask;       /* bit n set: exception n is routed to the debugger */
    void (*exceptionHook)(void);
    void* exceptionReturn;
} DBInterface;

extern DBInterface* __DBInterface;
extern BOOL DBVerbose;

void DBInit(void);
void __DBExceptionDestinationAux(void);
void __DBExceptionDestination(void);

#ifdef __cplusplus
}
#endif

#endif
