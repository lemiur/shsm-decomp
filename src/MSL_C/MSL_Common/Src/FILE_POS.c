/* Unit name unconfirmed: ftell and fseek internals, MSL FILE_POS.c by recall; the filename is a guess. */
#include <stdio.h>
#include <errno.h>
#include <MSL_C/MSL_Common/buffer_io.h>

/*
 * _ftell
 *
 * Current stream position, counting a newline as one byte more for text
 * streams (the buffer holds the translated form).
 */
long _ftell(FILE* file) {
    int unget_count = 0;
    long position;
    unsigned char* p;
    int n;
    unsigned char kind = file->file_mode.file_kind;

    if (!(kind == __disk_file || kind == __console_file) || file->file_state.error) {
        errno = EFPOS;
        return -1;
    }

    if (file->file_state.io_state == __neutral) {
        return file->position;
    }

    position = file->buffer_pos + (file->buffer_ptr - file->buffer);

    if (file->file_state.io_state >= __rereading) {
        unget_count = file->file_state.io_state - __reading;
        position -= unget_count;
    }

    if (!file->file_mode.binary_io) {
        p = file->buffer;

        for (n = (file->buffer_ptr - file->buffer) - unget_count; n; n--) {
            if (*p++ == '\n') {
                position++;
            }
        }
    }

    return position;
} // 0x801990D8

long ftell(FILE* file) {
    return _ftell(file);
} // 0x80199180

/*
 * _fseek
 *
 * Repositions a disk stream. A target inside the current read buffer only
 * moves the buffer pointer; everything else goes through the position
 * procedure.
 */
int _fseek(FILE* file, fpos_t offset, int mode) {
    if (file->file_mode.file_kind != __disk_file || file->file_state.error) {
        errno = EFPOS;
        return -1;
    }

    if (file->file_state.io_state == __writing && __flush_buffer(file, NULL)) {
        file->file_state.error = 1;
        file->buffer_len = 0;
        errno = EFPOS;
        return -1;
    }

    if (mode == SEEK_CUR) {
        mode = SEEK_SET;
        offset += _ftell(file);
    }

    if (mode != SEEK_END && file->file_mode.io_mode != __read_write &&
        (file->file_state.io_state == __reading || file->file_state.io_state == __rereading)) {
        if (offset >= file->position || offset < file->buffer_pos) {
            file->file_state.io_state = __neutral;
        } else {
            file->buffer_ptr = file->buffer + (offset - file->buffer_pos);
            file->buffer_len = file->position - offset;
            file->file_state.io_state = __reading;
        }
    } else {
        file->file_state.io_state = __neutral;
    }

    if (file->file_state.io_state == __neutral) {
        if (file->position_proc) {
            if ((*file->position_proc)(file->handle, &offset, mode, file->idle_proc)) {
                file->file_state.error = 1;
                file->buffer_len = 0;
                errno = EFPOS;
                return -1;
            }
        }

        file->file_state.eof = 0;
        file->position = offset;
        file->buffer_len = 0;
    }

    return 0;
} // 0x80199184
