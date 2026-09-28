#ifndef MOTION_H
#define MOTION_H

// game/motion.cpp: key-frame motion playback for cModel hierarchies (Em, Obj, player, ...).

#include "types.h"
#include "vec.h"
#include "model.h"
#include "cam_ctrl.h"

// MotionSeqKey / MotionData / MotionWork are defined in model.h (cModel::Motion at 0x1D8).

// The motion-driven model view (cMotBase::set(cMotModel*), MOTION(m)): cModel carries the work
// itself now, so this adds nothing.
class cMotModel : public cModel {
public:
};

// MotionParts (cParts::motParts, 0x174) is defined in model.h.

#define MOTION(m) (&((cMotModel*)(m))->Motion)

// HermiteInterpolation parameter block.
struct HermitePrm {
    f32 frame;     // 0x00
    f32 maxFrame;  // 0x04
    u32 flags;     // 0x08  bit0: search backwards, bit1: reverse, bit2: loop, bit3: ignore the key history
    u8 type;       // 0x0C  Fcc type
    u8 pad_D[3];
    u8* key;       // 0x10
};

void PartsWorldPosCalc(cModel* pMod);
void MotionBlendOff(cModel* pEm);
void MotionPause(cModel* pEm);
void MotionClear(cModel* pEm, int flag);
u32 MotionMove(cModel* pEm, CAMERA* pCamera);
u16 MotionMoveSub(cModel* pEm, MOTION_INFO* w);
void MotionMoveCore(cModel* pEm, MOTION_INFO* w, CAMERA* pCamera);
void MotionHokan(cModel* m, MOTION_INFO* w);
void MotionGetSpeed(cModel* pEm, MOTION_INFO* w, int flg, Vec* Pos_move, Vec* Ang_move);
void MotionAddSpeed(cModel* pEm, MOTION_INFO* w, Vec* Pos_move, Vec* Ang_move);
void MotionGetPosition(cModel* pEm, Vec* pPos, Vec* pAng);
u16 MotionSequenceCtrl(MOTION_INFO* w);
u16 FcvGetMaxFrame(u16* pData);
f32 MotionGetMaxFrame(MOTION_INFO* w);
f32 MotionGetCurrentFrame(MOTION_INFO* w);
int MotionCheckCrossFrame(MOTION_INFO* w, f32 frame);
int MotionGetState(cModel* m);
int HermiteInterpolation(HermitePrm* prm, Vec* out, u16* hist);
int Fcc_next_axis_addr(int fmt, int n);
void IKInit(cModel* pEm, MOTION_INFO* pInfo);
void InverseKinematics(cModel* pEm, int arm_flag);
void MotionSetCore(cModel* m, void* w, void* data, void* seq, int hokan, int flags, int frame);

#endif
