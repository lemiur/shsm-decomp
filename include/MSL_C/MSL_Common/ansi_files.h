#ifndef _MSL_ANSI_FILES_H
#define _MSL_ANSI_FILES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __ungetc_buffer_size 2

#define __no_io_error 0
#define __io_error 1
#define __io_EOF 2

enum __file_kinds { __closed_file, __disk_file, __console_file, __unavailable_file };
enum __open_modes { __must_exist, __create_if_necessary, __create_or_truncate };
enum __io_modes { __read = 1, __write = 2, __read_write = 3, __append = 4 };
enum __file_orientations { __unoriented, __char_oriented, __wide_oriented };
enum __io_states { __neutral, __writing, __reading, __rereading };
enum __buffer_modes { __no_buffer, __line_buffered, __fully_buffered };

typedef unsigned long __file_handle;
typedef unsigned long fpos_t;

typedef struct {
    unsigned int open_mode : 2;
    unsigned int io_mode : 3;
    unsigned int buffer_mode : 2;
    unsigned int file_kind : 3;
    unsigned int file_orientation : 2;
    unsigned int binary_io : 1;
} __file_modes;

typedef struct {
    unsigned int io_state : 3;
    unsigned int free_buffer : 1;
    unsigned char eof;
    unsigned char error;
} __file_state;

typedef int (*__pos_proc)(__file_handle file, fpos_t* position, int mode, void* idle_proc);
typedef int (*__io_proc)(__file_handle file, unsigned char* buff, size_t* count, void* idle_proc);
typedef int (*__close_proc)(__file_handle file);

typedef struct _FILE {
    __file_handle handle;
    __file_modes file_mode;
    __file_state file_state;
    unsigned char is_dynamically_allocated;
    unsigned char char_buffer;
    unsigned char char_buffer_overflow;
    unsigned char ungetc_buffer[__ungetc_buffer_size];
    wchar_t ungetc_wide_buffer[__ungetc_buffer_size];
    unsigned long position;
    unsigned char* buffer;
    unsigned long buffer_size;
    unsigned char* buffer_ptr;
    unsigned long buffer_len;
    unsigned long buffer_alignment;
    unsigned long saved_buffer_len;
    unsigned long buffer_pos;
    __pos_proc position_proc;
    __io_proc read_proc;
    __io_proc write_proc;
    __close_proc close_proc;
    void* idle_proc;
    struct _FILE* next_file_struct;
} FILE;

extern FILE __files[4];

void __close_all(void);
int __flush_all(void);

#ifdef __cplusplus
}
#endif

#endif
