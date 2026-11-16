#include <stdarg.h>

#define ALIGN(addr, amount) (((unsigned int)(addr) + ((amount) - 1)) & ~((amount) - 1))

/*
 * __va_arg
 *
 * Returns the address of the next variable argument of the given class,
 * taking it from the register save area until the eight registers of its
 * kind are used up, then from the stack.
 */
void* __va_arg(va_list v_list, int type) {
    char* addr;
    char* reg = &(v_list->gpr);
    int g_reg = v_list->gpr;
    int maxsize = 8;
    int size = 4;
    int increment = 1;
    int even = 0;
    int fpr_offset = 0;
    int regsize = 4;

    if (type == arg_REAL) {
        reg = &(v_list->fpr);
        g_reg = v_list->fpr;
        size = 8;
        fpr_offset = 32;
        regsize = 8;
    }

    if (type == arg_DOUBLEWORD) {
        /* A long long starts in an odd-numbered register pair. */
        size = 8;
        maxsize--;
        if (g_reg & 1) {
            even = 1;
        }
        increment = 2;
    }

    if (g_reg < maxsize) {
        g_reg += even;
        addr = v_list->reg_save_area + fpr_offset + (g_reg * regsize);
        *reg = g_reg + increment;
    } else {
        *reg = 8;
        addr = v_list->input_arg_area;
        addr = (char*)ALIGN(addr, size);
        v_list->input_arg_area = addr + size;
    }

    if (type == arg_ARGPOINTER) {
        addr = *((char**)addr);
    }

    return addr;
} // 0x801A6248
