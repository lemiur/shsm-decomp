#ifndef GAME_UNK_803E8E64_H
#define GAME_UNK_803E8E64_H

#include <revolution/types.h>
#include <game/Vec4.h>

/*
 * The polymorphic interface at 0x34 of unk_8009D2E4 (vtable lbl_803E8E64, size 0x4C, destructor
 * 0x8009D3DC; seventeen slots, destructor first; its object is the vptr and one word). Name from
 * its vtable address (invented); slot names are the addresses of the functions its vtable lists.
 */
class unk_803E8E64 {
public:
    virtual         ~unk_803E8E64();
    virtual void    fn_8000F5E0();
    virtual void    fn_8000F5DC();
    virtual void    fn_8009ECB0( unk_Vec4* out );
    virtual bool    fn_8001754C();
    virtual bool    fn_80017544();  // true here
    virtual bool    fn_8001753C();
    virtual bool    fn_80017534();
    virtual bool    fn_8001752C();
    virtual bool    fn_80017524();
    virtual bool    fn_8001751C();
    virtual void    fn_80017518();
    virtual void    fn_80017510();
    virtual void    fn_80017514();
    virtual void    fn_8001A0A0();
    virtual void    fn_8001750C();
    virtual bool    fn_80017504();

    s32             field_0x4;
};

#endif
