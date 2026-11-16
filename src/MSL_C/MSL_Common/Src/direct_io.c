/* Unit name unconfirmed: __fwrite, MSL direct_io.c by recall; the filename is a guess. */
#include <stdio.h>
#include <string.h>
#include <MSL_C/MSL_Common/buffer_io.h>
#include <MSL_C/MSL_Common/misc_io.h>
#include <MSL_C/MSL_Common/wchar_io.h>

/*
 * __fwrite
 *
 * Writes count items of the given size through the file's buffer. Line
 * buffered streams are flushed after the last newline; unbuffered binary
 * streams write the remainder straight from the caller's memory.
 */
size_t __fwrite(const void* buffer, size_t size, size_t count, FILE* file) {
    unsigned char* write_ptr;
    int ioresult, always_buffer, buffered;
    size_t num_bytes, bytes_to_go, bytes_written;

    if (fwide(file, 0) == 0) {
        fwide(file, -1);
    }

    bytes_to_go = size * count;

    if (bytes_to_go == 0 || file->file_state.error || file->file_mode.file_kind == __closed_file) {
        return 0;
    }

    if (file->file_mode.file_kind == __console_file) {
        __stdio_atexit();
    }

    buffered = 1;
    always_buffer = !file->file_mode.binary_io || file->file_mode.buffer_mode == __fully_buffered;

    if (!always_buffer && file->file_mode.buffer_mode != __line_buffered) {
        buffered = 0;
    }

    if (file->file_state.io_state == __neutral) {
        if (file->file_mode.io_mode & __write) {
            if (file->file_mode.io_mode & __append) {
                if (_fseek(file, 0, SEEK_END)) {
                    return 0;
                }
            }

            file->file_state.io_state = __writing;
            __prep_buffer(file);
        }
    }

    if (file->file_state.io_state != __writing) {
        file->file_state.error = 1;
        file->buffer_len = 0;
        return 0;
    }

    write_ptr = (unsigned char*)buffer;
    bytes_written = 0;

    if (bytes_to_go && (file->buffer_ptr != file->buffer || buffered)) {
        file->buffer_len = file->buffer_size - (file->buffer_ptr - file->buffer);

        do {
            unsigned char* newline;

            num_bytes = file->buffer_len;
            newline = NULL;

            if (num_bytes > bytes_to_go) {
                num_bytes = bytes_to_go;
            }

            if (file->file_mode.buffer_mode == __line_buffered && num_bytes) {
                newline = __memrchr(write_ptr, '\n', num_bytes);

                if (newline) {
                    num_bytes = newline + 1 - write_ptr;
                }
            }

            if (num_bytes) {
                memcpy(file->buffer_ptr, write_ptr, num_bytes);
                write_ptr += num_bytes;
                bytes_to_go -= num_bytes;
                file->buffer_ptr += num_bytes;
                file->buffer_len -= num_bytes;
            }

            if (file->buffer_len == 0 || newline || file->file_mode.buffer_mode == __no_buffer) {
                if (__flush_buffer(file, NULL)) {
                    file->file_state.error = 1;
                    file->buffer_len = 0;
                    bytes_to_go = 0;
                    break;
                }
            }

            bytes_written += num_bytes;
        } while (bytes_to_go && buffered);
    }

    if (bytes_to_go && !buffered) {
        unsigned char* saved_buffer = file->buffer;
        unsigned long saved_size = file->buffer_size;

        file->buffer = write_ptr;
        file->buffer_size = bytes_to_go;
        file->buffer_ptr = write_ptr + bytes_to_go;

        ioresult = __flush_buffer(file, &num_bytes);

        if (ioresult) {
            file->file_state.error = 1;
            file->buffer_len = 0;
        } else {
            bytes_written += num_bytes;
        }

        file->buffer = saved_buffer;
        file->buffer_size = saved_size;
        __prep_buffer(file);
        file->buffer_len = 0;
    }

    if (file->file_mode.buffer_mode != __fully_buffered) {
        file->buffer_len = 0;
    }

    return bytes_written / size;
} // 0x80198BE0
