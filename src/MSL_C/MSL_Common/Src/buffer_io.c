#include <MSL_C/MSL_Common/buffer_io.h>

/*
 * __prep_buffer
 *
 * Rewinds the buffer pointer and sets the room left up to the next
 * buffer-aligned file position.
 */
void __prep_buffer(FILE* file) {
    file->buffer_ptr = file->buffer;
    file->buffer_len = file->buffer_size;
    file->buffer_len -= file->position & file->buffer_alignment;
    file->buffer_pos = file->position;
} // 0x80198B00

/*
 * __flush_buffer
 *
 * Writes out the buffered bytes through the file's write procedure, reporting
 * the count written, and prepares the buffer for refilling.
 */
int __flush_buffer(FILE* file, size_t* bytes_flushed) {
    size_t buffer_len;
    int ioresult;

    buffer_len = file->buffer_ptr - file->buffer;

    if (buffer_len) {
        file->buffer_len = buffer_len;

        ioresult = (*file->write_proc)(file->handle, file->buffer, &file->buffer_len, file->idle_proc);

        if (bytes_flushed) {
            *bytes_flushed = file->buffer_len;
        }

        if (ioresult) {
            return ioresult;
        }

        file->position += file->buffer_len;
    }

    __prep_buffer(file);

    return __no_io_error;
} // 0x80198B28
