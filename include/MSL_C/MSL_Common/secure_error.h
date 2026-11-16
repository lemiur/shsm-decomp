// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/include/PowerPC_EABI_Support/MSL_C/MSL_Common/secure_error.h
#ifndef _MSL_SECURE_ERROR_H
#define _MSL_SECURE_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Runtime-constraint handler of the C11 Annex K ("_s") interfaces. */
typedef void (*constraint_handler_t)(const char* msg, void* ptr, int error);

void __msl_runtime_constraint_violation_s(const char* msg, void* ptr, int error);

#ifdef __cplusplus
}
#endif

#endif
