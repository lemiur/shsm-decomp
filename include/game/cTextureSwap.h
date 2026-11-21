#ifndef GAME_CTEXTURESWAP_H
#define GAME_CTEXTURESWAP_H

#include <game/CBaseBehaviour.h>
#include <climax/unk_801A92A8.h>
#include <rwsdk/batextur.h>

class cTextureSwap;

// One owner of a texture swap entry: the behaviour that was loaded under its name. Names invented.
struct unk_TexSwapOwner {
    u32             field_0x0;
    cTextureSwap*   behaviour;      // 0x4
};

struct unk_TexSwapOwnerNode {
    unk_TexSwapOwner*       owner;  // 0x0
    unk_TexSwapOwnerNode*   next;   // 0x4
};

// One named texture swap entry in the registry. Names invented.
struct unk_TexSwapEntry {
    const char*             name;   // 0x0
    u8                      field_0x4[0xC];
    unk_TexSwapOwnerNode*   owners; // 0x10
};

struct unk_TexSwapNode {
    unk_TexSwapEntry*       entry;  // 0x0
    unk_TexSwapNode*        next;   // 0x4
};

// The texture swap registry (lbl_8046A668, 0x20 bytes). Name invented.
struct unk_TexSwapRegistry {
    u8                      field_0x0[0x14];
    unk_TexSwapNode*        head;   // 0x14
    u8                      field_0x18[0x8];
};

extern unk_TexSwapRegistry lbl_8046A668;
extern u32 lbl_80529710;            // class-name hash of "cTextureSwap"

/*
 * A behaviour that loads a texture from a texture stream under its name ("SwapTex_%s.TexStream")
 * and swaps it into the scene. Vtable lbl_803E8718, size 0x54. Member names invented.
 */
class cTextureSwap : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetTextureName,     // text: copied as a pointer into textureName
        CMD_SetName,            // text: copied as a pointer into name
        CMD_SetFlag,            // word: non-zero sets field_0x40
        CMD_SetEvent,           // text: the event name passed to fn_801AA218
        CMD_SetSaveDataA,       // text: copied to saveDataA
        CMD_SetSaveDataB        // text: copied to saveDataB
    };

    virtual         ~cTextureSwap();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual void    fn_801A7A88();

    const char*     name;           // 0x34, points into the packet; names the swap
    const char*     textureName;    // 0x38, points into the packet
    RwTexture*      texture;        // 0x3C
    bool            field_0x40;     // 0x40
    unk_801A92A8    id;             // 0x44
    char*           saveDataA;      // 0x4C, owned copy; first argument of fn_80206348
    char*           saveDataB;      // 0x50, owned copy; second argument of fn_80206348
};

#endif
