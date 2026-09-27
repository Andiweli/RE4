// game/esp4d: effect id 0x4D, water ripple source (D:/Bio4/Prog/esp4d.cpp). Has no sprite of its
// own: every frame it feeds AddWaterPower into the water surface in five rings around m_Pos.
// Entry points: Esp4d_Create (EffSetId table), cEsp4d::move / SetFreeWork.
#include "atari.h"
#include "light.h"
#include "math_sub.h"
#include "esp.h"


// Water ripple: pushes the water surface in rings around the effect position.
class cEsp4d : public cEsp {
public:
    u8 type;  // 0xF8 0: push down, 1: pull up

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

// Create entry of the EffSetId function table for effect id 0x4D.
cEsp* Esp4d_Create()
{
    return new cEsp4d;
}

// Per-frame move: after the common life/position update, pushes (type 0) or pulls (type 1) the water
// surface on 5 rings of radius (i+1)/4 * m_Size_base_x*m_Size_mul, 3+i points per ring, the force
// m_Col_a/1000 fading linearly outwards; m_Tool_flg bit 0x20000 multiplies the force by 4.
void cEsp4d::move()
{
    Vec p;
    f32 sign;
    f32 fade;
    f32 r;
    f32 ang;
    u32 nRing;
    u32 i;
    u32 j;
    u32 n;

    if (CommonMove()) {
        fade = 1.0f;
        sign = (type == 0) ? 1.0f : -1.0f;
        if (m_Tool_flg & 0x20000) {
            sign *= 4.0f;
        }
        nRing = 5;
        for (i = 0; i < nRing; i++) {
            n = i + 3;
            r = m_Size_base_x * m_Size_mul * ((f32)(i + 1) * 0.25f);
            ang = 0.0f;
            for (j = 0; j < n; j++) {
                p = m_Pos;
                p.x = SINF(ang) * r + p.x;
                p.z = COSF(ang) * r + p.z;
                AddWaterPower(p, m_Col_a * 0.001f * fade * sign);
                ang += 6.2831855f / (f32)n;
            }
            fade -= 1.0f / (f32)nRing;
        }
    }
}

// Trans entry of the function table: nothing to draw (the ripple only moves water).
void Esp4d_Trans()
{
}

// Reads the effect-record parameter Work8[0] as the push (0) / pull (1) type; other values log an
// error and fall back to 0. Always succeeds (returns 1).
int cEsp4d::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    type = pSeq->Work8[0];
    if (type > 1) {
        pLog->err(0, 0, "ESP4D : Type[%d] is invalid.", type);
        type = 0;
    }
    return 1;
}
