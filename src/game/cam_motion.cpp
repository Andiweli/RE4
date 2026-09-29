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

// Binds the motion file: frame count, parts (track) table, key offsets relocated to pointers,
// key history cleared; blend frames `hokan`, flags (bit2 loop, bit3 pause) and the start frame.
CameraMotion::CameraMotion(void* data, int hokan, int flags, f32 frame)
{
    CameraMotionWork* w = &m_info;
    u32* tbl;
    int i;

    memclr_asm(w, sizeof(CameraMotionWork));
    w->data = (MotionData*) data;
    w->maxFrame = (f32) (((MotionData*) data)->maxFrame & 0x3FFF);
    w->maxFrame += 1.0f;
    w->nParts = w->data->nParts;
    w->partsInfo = (u16*) ((u8*) w->data + 3);
    w->partsNo = (u8*) w->data + (w->nParts * 2 + 3);
    tbl = (u32*) ((u32) w->partsNo + w->nParts);
    tbl = (u32*) (((u32) tbl + 3) & ~3);
    tbl++;
    if ((s32) tbl[0] >= 0) {
        for (i = 0; i < w->nParts; i++) {
            tbl[i] += (u32) w->data;
        }
    }
    w->keyTbl = tbl;
    for (i = 0; i < w->nParts; i++) {
        w->hist[i][0] = w->hist[i][1] = w->hist[i][2] = 0;
    }
    w->hokan = hokan;
    w->flags = flags;
    w->frame = frame;
    w->state = 0;
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
    CameraMotionWork* w = &m_info;
    int i;

    pp->Frame = w->frame;
    pp->Frame_max = w->maxFrame;
    pp->Attr = 2;
    for (i = 0; i < w->nParts; i++) {
        pp->Data_fmt = w->partsInfo[i] >> 12;
        pp->pData = (u8*) w->keyTbl[i];
        switch (w->partsNo[i]) {
        case 0:
            HermiteInterpolation(pp, &pos, w->hist[i]);
            break;
        case 1:
            HermiteInterpolation(pp, &at, w->hist[i]);
            break;
        case 2:
            HermiteInterpolation(pp, &roll, w->hist[i]);
            break;
        case 3:
            HermiteInterpolation(pp, &fov, w->hist[i]);
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
u32 CameraSequenceCtrl(CameraMotionWork* pInfo)
{
    if (!(pInfo->flags & 8)) {
        if (pInfo->frame >= pInfo->maxFrame) {
            if (pInfo->flags & 4) {
                pInfo->state = 1;
                pInfo->frame = 0.0f;
            } else {
                pInfo->state = 4;
            }
        } else {
            pInfo->frame += 1.0f;
        }
    }
    return pInfo->state;
}
