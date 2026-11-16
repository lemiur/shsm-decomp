#ifndef HBM_AX_SOUND_H
#define HBM_AX_SOUND_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/HBMAxSound.h
// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/revolution/hbm/homebutton/HBMAxSound.hpp

#include <revolution/types.h>

namespace homebutton {

void PlaySeq(int num);
void InitAxSound(const void* soundData, void* mem, u32 memSize);
void ShutdownAxSound();
void AxSoundMain();
void StopAllSeq();
void SetVolumeAllSeq(f32 gain);
void SetSoundMode(u32 mode);

} // namespace homebutton

#endif
