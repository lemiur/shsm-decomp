#ifndef GAME_UNK_80054F0C_H
#define GAME_UNK_80054F0C_H

#include <revolution/types.h>

/*
 * Class name invented from its vtable address lbl_803E0AF0 (no string names it). Its
 * constructor is at 0x80054728 (another unit); its destructor has no body.
 */
class unk_80054F0C {
public:
                    ~unk_80054F0C();                    // 0x80020AC4
    virtual void    fn_8001B5DC();
    virtual void    fn_8001B778();
    virtual void    fn_8001FC18();
    virtual void    fn_8001FB08();
    virtual void    fn_8001FBCC();
    virtual void    fn_8001FBAC();
    virtual void    fn_8001FBEC();
    virtual void    fn_8001FB00();
    virtual void    fn_8001FAF8();
    virtual void    fn_8001FB30();
    virtual void    fn_8001FB0C();
    virtual void    fn_8001FAF4();
    virtual void    fn_8001B764();
    virtual void    fn_8001B664();
    virtual void    fn_8001D580();
    virtual void    fn_8001C42C();
    virtual void    fn_8001C9B8();
};

#endif
