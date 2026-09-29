#ifndef CAM_MOTION_H
#define CAM_MOTION_H

#include "types.h"
#include "vec.h"
#include "cam_extra.h"
#include "motion.h"

// Keyframed camera motion (game/cam_motion.cpp). Derives from cCamera (vptr at 0xF8).
class CameraMotion : public cCamera {
private:
    s32 m_state;                       // 0xFC  1 when the motion has finished
    MOTION_INFO m_info;                 // 0x100 motion work (getMotionInfoPtr): the same PS2
                                         // struct as a model's own motion work (model.h), reused
                                         // here for a camera's pos/at/roll/fovy tracks
    Mtx* m_p_base_mat;                 // 0x1D0

public:
    CameraMotion(void* data, int hokan, int flags, f32 frame);
    virtual ~CameraMotion();
    virtual void move();
    s32 getState() { return m_state; }
    MOTION_INFO* getInfoPtr() { return &m_info; }
    void setBaseMatPtr(Mtx* p_mat) { m_p_base_mat = p_mat; }
};

u32 CameraSequenceCtrl(MOTION_INFO* w);

#endif
