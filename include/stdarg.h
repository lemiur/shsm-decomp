#ifndef _MSL_STDARG_H
#define _MSL_STDARG_H

#ifdef __cplusplus
extern "C" {
#endif

/* PowerPC EABI variable-argument state (see __va_arg.c). */
typedef struct __va_list_struct {
    char gpr;               /* next general register argument, 0-8 */
    char fpr;               /* next floating register argument, 0-8 */
    char reserved[2];
    char* input_arg_area;   /* arguments passed on the stack */
    char* reg_save_area;    /* r3-r10 then f1-f8, saved by the prologue */
} __va_list[1];

typedef __va_list va_list;

/* Argument classes passed to __va_arg by the va_arg macro. */
#define arg_ARGPOINTER  0
#define arg_WORD        1
#define arg_DOUBLEWORD  2
#define arg_REAL        3

void* __va_arg(va_list v_list, int type);

#define va_start(ap, fmt) ((void)fmt, __builtin_va_info(&ap))
#define va_arg(ap, t) (*((t*)__va_arg(ap, _var_arg_typeof(t))))
#define va_end(ap) ((void)0)

#ifdef __cplusplus
}
#endif

#endif
