/* Unit name unconfirmed: fclose and fflush, MSL file_io.c by recall; the filename is a guess. */
#include <stdio.h>
#include <stdlib.h>
#include <MSL_C/MSL_Common/buffer_io.h>

/*
 * fclose
 *
 * Flushes the stream, closes its handle and releases a buffer the library
 * allocated. EOF if either the flush or the close failed.
 */
int fclose(FILE* file) {
    int flush_result, close_result;

    if (file == NULL) {
        return EOF;
    }

    if (file->file_mode.file_kind == __closed_file) {
        return 0;
    }

    flush_result = fflush(file);
    close_result = (*file->close_proc)(file->handle);

    file->handle = 0;
    file->file_mode.file_kind = __closed_file;

    if (file->file_state.free_buffer) {
        free(file->buffer);
    }

    return (flush_result || close_result) ? EOF : 0;
} // 0x80198EE8

/*
 * fflush
 *
 * Writes out a stream's pending output; a null stream flushes every open
 * stream. A stream that was being read just forgets its buffered input.
 */
int fflush(FILE* file) {
    int position;

    if (file == NULL) {
        return __flush_all();
    }

    if (file->file_state.error || file->file_mode.file_kind == __closed_file) {
        return EOF;
    }

    if (file->file_mode.io_mode == __read) {
        return 0;
    }

    if (file->file_state.io_state >= __rereading) {
        file->file_state.io_state = __reading;
    }

    if (file->file_state.io_state == __reading) {
        file->buffer_len = 0;
    }

    if (file->file_state.io_state != __writing) {
        file->file_state.io_state = __neutral;
        return 0;
    }

    if (file->file_mode.file_kind != __disk_file) {
        position = 0;
    } else {
        position = ftell(file);
    }

    if (__flush_buffer(file, NULL)) {
        file->file_state.error = 1;
        file->buffer_len = 0;
        return EOF;
    }

    file->file_state.io_state = __neutral;
    file->position = position;
    file->buffer_len = 0;

    return 0;
} // 0x80198FA4
