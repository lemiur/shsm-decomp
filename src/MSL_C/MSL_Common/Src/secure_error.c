// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/src/MSL_C/MSL_Common/secure_error.c
/* Unit name unconfirmed: MSL secure_error.c, as in the public decomps; no DOL string names the file. */

#include <stddef.h>
#include <MSL_C/MSL_Common/secure_error.h>

static constraint_handler_t __msl_constraint_handler;

/*
 * __msl_runtime_constraint_violation_s
 *
 * Reports a broken runtime constraint of a "_s" function to the installed
 * handler; with none installed the violation is ignored.
 */
void __msl_runtime_constraint_violation_s(const char* msg, void* ptr, int error) {
    if (__msl_constraint_handler != NULL) {
        __msl_constraint_handler(msg, ptr, error);
    }
} // 0x801A1D80
