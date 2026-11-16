#ifndef GAME_UNK_HUDCONTROLLER_H
#define GAME_UNK_HUDCONTROLLER_H

// unit name unconfirmed: header and type names invented.

#include <revolution/types.h>

class unk_HUDRequest;

/*
 * A part of a HUD element that plays named transitions on its children (fn_801016A4 finds the
 * child by the string hash of its name). Defined in 0x801016A4 (unit not yet reconstructed).
 * Name invented.
 */
class unk_HUDPanel {
public:
    /* Plays the named transition (CDrawingTest passes "No" with the arguments of fn_800E4148). */
    void    fn_801016A4( const char* transition, int a, int b, int c, float speed );
};

/* A HUD element, as fn_800E4148 returns it; only the member read so far is known. Name invented. */
struct unk_HUDElement {
    /* Plays the named transition on the element's panel, when it has one. Name invented. */
    void            PlayPanelTransition( const char* transition, int a, int b, int c, float speed ) {
                        if ( field_0x5C != NULL ) {
                            field_0x5C->fn_801016A4( transition, a, b, c, speed );
                        }
                    }

    u8              field_0x0[0x5C];
    unk_HUDPanel*   field_0x5C;     // 0x5C, NULL when the element has none
};

/*
 * The object at 0x6C4 of *lbl_80529D20 that shows and hides HUD elements by index (0x40 is
 * the key HUD prompt), defined in 0x800E3188-0x800E6590 (units not yet reconstructed).
 * Parameter names and the place of the float among the parameters are invented.
 */
class unk_HUDController {
public:
    /* Shows element id with the named transition; returns the element, or NULL. */
    unk_HUDElement* fn_800E4148( int id, const char* transition, int a, int b, float speed );
    /* Hides element id. */
    void    fn_800E588C( int id, int a, int b );
    /* Defined in 0x800E5490: CDrawingTest calls it with (3, 0) when the painting is done. */
    void    fn_800E5490( int id, int a );
    /* Shows text as a prompt of the given kind until request is done. */
    bool    fn_800E50E8( const char* text, int kind, unk_HUDRequest* request );
    /* Shows a modal message with a title, a body and an optional image. */
    void    fn_800E611C( const char* title, const char* body, void* image );
};

extern char lbl_803E6400[];     // "TransitionIN"; defined elsewhere, unit not yet reconstructed

#endif
