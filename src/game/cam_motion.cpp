// game/cam_motion.cpp: CameraMotion, a Camera driven by a camera motion file (cutscene cameras):
// the file holds Hermite key tracks for position, target, roll and fov; move() evaluates them at
// the current frame and CameraSequenceCtrl advances / loops / ends the sequence.

#include "types.h"
#include "vec.h"
#include "camera.h"
#include "cam_extra.h"
#include "cam_motion.h"
#include "main_mem.h"
#include <string.h>
#include "math_sub_decl.h"

#define CAM_HIST(w, i) ((w)->Key_hist[i])

// Binds the motion file: frame count, parts (track) table, key offsets relocated to pointers,
// key history cleared; blend frames `hokan`, flags (bit2 loop, bit3 pause) and the start frame.
CameraMotion::CameraMotion(void* data, int hokan, int flags, f32 frame)
{
    MOTION_INFO* w = &m_info;
    u32* tbl;
    int i;

    memclr_asm(w, sizeof(MOTION_INFO));
    w->pMot = (MotionData*) data;
    w->Mot_frame_max = (f32) (((MotionData*) data)->maxFrame & 0x3FFF);
    w->Mot_frame_max += 1.0f;
    w->Joint_num = w->pMot->nParts;
    w->pJoint_kind = (u16*) ((u8*) w->pMot + 3);
    w->pJoint_no = (u8*) w->pMot + (w->Joint_num * 2 + 3);
    tbl = (u32*) ((u32) w->pJoint_no + w->Joint_num);
    tbl = (u32*) (((u32) tbl + 3) & ~3);
    tbl++;
    if ((s32) tbl[0] >= 0) {
        for (i = 0; i < w->Joint_num; i++) {
            tbl[i] += (u32) w->pMot;
        }
    }
    w->pHermite_data = tbl;
    for (i = 0; i < w->Joint_num; i++) {
        CAM_HIST(w, i)[0] = CAM_HIST(w, i)[1] = CAM_HIST(w, i)[2] = 0;
    }
    w->Hokan_frame = hokan;
    w->Mot_attr = flags;
    w->Mot_frame = frame;
    w->Mot_flag = 0;
    m_p_base_mat = NULL;
    m_state = 0;
}

// Poisons the object (memset 9) so a stale pointer is caught.
CameraMotion::~CameraMotion()
{
    memset(this, 9, 0x200);
}

// Evaluates the pos / at / roll / fov tracks at the current frame (Hermite), sets the camera
// parameters (fov track in radians -> degrees), applies the optional base matrix (event
// placed in the room), and sets `end` when the sequence finished.
void CameraMotion::move()
{
    HERMITE_SET prm;
    Vec pos;
    Vec at;
    Vec roll = {0.0f, 0.0f, 0.0f};
    Vec fov;
    HERMITE_SET* pp = &prm;
    MOTION_INFO* w = &m_info;
    int i;

    pp->Frame = w->Mot_frame;
    pp->Frame_max = w->Mot_frame_max;
    pp->Attr = 2;
    for (i = 0; i < w->Joint_num; i++) {
        pp->Data_fmt = w->pJoint_kind[i] >> 12;
        pp->pData = (u8*) w->pHermite_data[i];
        switch (w->pJoint_no[i]) {
        case 0:
            HermiteInterpolation(pp, &pos, CAM_HIST(w, i));
            break;
        case 1:
            HermiteInterpolation(pp, &at, CAM_HIST(w, i));
            break;
        case 2:
            HermiteInterpolation(pp, &roll, CAM_HIST(w, i));
            break;
        case 3:
            HermiteInterpolation(pp, &fov, CAM_HIST(w, i));
            break;
        }
    }
    param.Campos = pos;
    param.Target = at;
    param.Roll = roll.y;
    param.Fovy = fov.y * 180.0f / PI;
    CameraSetOrientationRoll(this);
    if (m_p_base_mat) {
        PSMTXMultVec(*m_p_base_mat, &param.Campos, &param.Campos);
        PSMTXMultVec(*m_p_base_mat, &param.Target, &param.Target);
        PSMTXMultVec(*m_p_base_mat, &Up, &Up);
        CameraSetOrientationUp(this);
    }
    m_state = 0;
    if (CameraSequenceCtrl(&m_info) == 4) {
        m_state = 1;
    }
}

// Radians to degrees.
static f32 rad2deg(f32 r)
{
    return r * 180.0f / PI;
}

// Advances the frame unless paused (flags bit3); past the last frame either loops (flags bit2,
// state 1) or ends (state 4). Returns the state.
u32 CameraSequenceCtrl(MOTION_INFO* pInfo)
{
    if (!(pInfo->Mot_attr & 8)) {
        if (pInfo->Mot_frame >= pInfo->Mot_frame_max) {
            if (pInfo->Mot_attr & 4) {
                pInfo->Mot_flag = 1;
                pInfo->Mot_frame = 0.0f;
            } else {
                pInfo->Mot_flag = 4;
            }
        } else {
            pInfo->Mot_frame += 1.0f;
        }
    }
    return pInfo->Mot_flag;
}
