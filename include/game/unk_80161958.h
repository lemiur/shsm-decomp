#ifndef GAME_UNK_80161958_H
#define GAME_UNK_80161958_H

#include <game/unk_8015F670.h>

/*
 * The base of the puzzle behaviours (vtable lbl_803F9318, size 0x158, constructor 0x80161958,
 * destructor 0x80161C68; unit not yet reconstructed): CButtonPuzzle, CCardObjectController,
 * CDeadSleepingPuzzle, CLoveAndMarriage, CMemento, CPasswordPuzzleController,
 * CPickupObjectController, CProjectorPuzzle, CSplinePuzzle, CTimeTablePuzzle and CTwistPuzzle.
 * Its inline slots have their only copies in the first unit that emits a puzzle vtable
 * (CButtonPuzzle). Slot 0x150 is pure. The object is 0x5BC bytes; its own data is padding here
 * except the fields this code reads. Slot signatures are known only where recovered code calls them.
 */
class unk_80161958 : public unk_8015F670 {
public:
                    unk_80161958( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~unk_80161958();  // override
    virtual float   fn_80160584( float t ) {  // override
                        float rate;
                        if ( field_0x1A8 == NULL ) {
                            rate = fn_80024A58();
                        } else {
                            rate = field_0x1A8->field_0xC;
                        }
                        float lo = field_0x23C;
                        float hi = field_0x240;
                        float start = lo * rate;
                        float extent = rate * ( hi - lo );
                        return start + extent * t;
                    }
    virtual SClassInfo* fn_8015AEC8() { return CAttributeHandler::mClassInfo; }
    virtual CZone*  fn_8015AEC0() { return CAttributeHandler::mZone; }
    virtual void    fn_80164064();
    virtual void    fn_80164578();
    virtual void    fn_80164894();
    virtual void    fn_80164BE0();
    virtual void    fn_80164C54();
    virtual void    fn_80164DDC();
    virtual void    fn_80164DE8();
    virtual void    fn_80165B90( bool reset );
    virtual void    fn_8015DBD4() { fn_80165B90( false ); }
    // the position of the behaviour's frame
    virtual void    fn_8015AEA0( unk_RwV3d* out ) {
                        *out = RwFrameGetMatrix( field_0x30 )->pos;
                    }
    virtual u32     fn_8015AE1C( u32* out ) = 0;
    virtual void    fn_8015A9A8( float t ) {
                        fn_8016537C();
                        fn_80160614( fn_80160584( t ) );
                    }

    // defined in 0x801650EC, 0x8016537C, 0x80164F08 and 0x80165AFC, unit not yet reconstructed
    u32             fn_801650EC();
    void            fn_8016537C();
    void            fn_80164F08( float from, float to, float step );
    void            fn_80165AFC( u32 state );

    float           field_0x23C;
    float           field_0x240;
    u8              field_0x244[0xC];
    s32             field_0x250;
    u8              field_0x254[0x290];
    bool            field_0x4E4;
    bool            field_0x4E5;
    bool            field_0x4E6;
    u8              field_0x4E7[0x2];
    bool            field_0x4E9;
    u8              field_0x4EA[0x2];
    bool            field_0x4EC;
    u8              field_0x4ED[0xCB];
    s32             field_0x5B8;
};

#endif
