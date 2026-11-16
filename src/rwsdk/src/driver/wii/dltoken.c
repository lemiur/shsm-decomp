// Unit name and boundary: Gormiti link map (RwCore.a dltoken); path: Gormiti DWARF (src/driver/wii/dltoken.c)
// Names: Gormiti link map
/*
 * dltoken.c: draw-sync tokens. The driver tags the GX command stream with a 16-bit token and asks
 * the pipe how far it got; the counter wraps, so a token is compared against the window between
 * the last one handed out and the last one the pipe reported.
 */
#include <revolution/gx/GXManage.h>
#include <rwsdk/driver/wii/dltoken.h>

/* The token the driver handed out last (starts at 1). */
volatile RwUInt16 _RwDlTokenCurrent = 1; // volatile: retail reloads it at every use in _rwDlTokenQueryDone

/* The token the pipe reported on the last query. */
volatile RwUInt16 _RwDlTokenLastSeen; // volatile: retail reloads it after the store in _rwDlTokenQueryDone

/* The first value GXReadDrawSync reports once a frame's tokens run out. Name invented. */
#define DL_TOKEN_LIMIT 0xE000

/*
 * Whether the pipe has passed the token: the token has been drawn when it is outside the window
 * of tokens still in flight (between the current one and the last one seen, wrapped around).
 * A pipe that reports past the token range counts as not done.
 */
RwBool _rwDlTokenQueryDone(RwUInt16 token) {
    RwBool done;

    _RwDlTokenLastSeen = GXReadDrawSync();
    if (_RwDlTokenLastSeen >= DL_TOKEN_LIMIT) {
        return FALSE;
    }

    if (_RwDlTokenCurrent >= _RwDlTokenLastSeen) {
        done = FALSE;
        if (token <= _RwDlTokenLastSeen || token > _RwDlTokenCurrent) {
            done = TRUE;
        }
    } else {
        done = FALSE;
        if (token > _RwDlTokenCurrent && token <= _RwDlTokenLastSeen) {
            done = TRUE;
        }
    }

    return done;
} // 0x802B395C
