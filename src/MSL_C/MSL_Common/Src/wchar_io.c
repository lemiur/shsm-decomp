#include <MSL_C/MSL_Common/wchar_io.h>

/*
 * fwide
 *
 * Sets or queries a stream's orientation. A stream stays unoriented until
 * the first call with a nonzero mode fixes it to wide (> 0) or byte (< 0).
 */
int fwide(FILE* file, int mode) {
    int orientation;

    if (file == NULL || file->file_mode.file_kind == __closed_file) {
        return 0;
    }

    orientation = file->file_mode.file_orientation;

    switch (orientation) {
    case __unoriented:
        if (mode > 0) {
            file->file_mode.file_orientation = __wide_oriented;
        } else if (mode < 0) {
            file->file_mode.file_orientation = __char_oriented;
        }
        break;
    case __wide_oriented:
        mode = 1;
        break;
    case __char_oriented:
        mode = -1;
        break;
    }

    return mode;
} // 0x801A1C30
