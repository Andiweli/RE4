// game/esp17.cpp: effect id 0x17, a sprite positioned in camera (view) space. `work` holds the
// view-space position; each frame it is moved there, then converted back to world space through
// the inverse view matrix so the sprite follows the camera.

#include "global.h"
#include "esp.h"

// Screen-space effect: keeps its own copy of the position and converts it into view space.
class cEsp17 : public cEsp {
public:
    Vec work;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

// EspCreateTbl[0x17] factory.
cEsp* Esp17_Create()
{
    return new cEsp17;
}

// Restores the view-space position, runs the base motion on it, saves it back, then converts it to
// world space (z negated, inverse of Cam.v_mat) for drawing. Released when the animation ends.
void cEsp17::move()
{
    Mtx inv;

    m_Pos = work;
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else {
            work = m_Pos;
            m_Pos.z = -m_Pos.z;
            PSMTXInverse(pG->Camera.v_mat, inv);
            PSMTXMultVec(inv, &m_Pos, &m_Pos);
        }
    }
}

// Takes the generator position as the initial view-space position.
int cEsp17::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    work = m_Pos;
    return 1;
}
