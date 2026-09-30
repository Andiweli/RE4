#ifndef MOTION_H
#define MOTION_H

// game/motion.cpp: key-frame motion playback for cModel hierarchies (Em, Obj, player, ...).

#include "types.h"
#include "vec.h"
#include "model.h"
#include "cam_ctrl.h"

// SEQUENCE_DATA / MotionData / MOTION_INFO are defined in model.h (cModel::Motion at 0x1D8).

// MotionParts (cParts::motParts, 0x174) is defined in model.h.

// HermiteInterpolation parameter block.
struct _HERMITE_SET {
    f32 Frame;      // 0x00
    f32 Frame_max;  // 0x04
    u32 Attr;       // 0x08  bit0: search backwards, bit1: reverse, bit2: loop, bit3: ignore the key history
    u8 Data_fmt;    // 0x0C  Fcc type
    u8 pad_D[3];
    u8* pData;      // 0x10
};
typedef _HERMITE_SET HERMITE_SET;

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
int HermiteInterpolation(HERMITE_SET* prm, Vec* out, u16* hist);
int Fcc_next_axis_addr(int fmt, int n);
void IKInit(cModel* pEm, MOTION_INFO* pInfo);
void InverseKinematics(cModel* pEm, int arm_flag);
void MotionSetCore(cModel* m, void* w, void* data, void* seq, int hokan, int flags, int frame);

#endif
