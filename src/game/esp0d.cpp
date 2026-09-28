// game/esp0d.cpp: effect id 0x0D, a sprite attracted to its parent coordinate. Within `Dist`
// (Work8[0] x 100 units) of the target it accelerates toward it with strength `Pow`
// (Work8[1] x 0.00005) proportional to the remaining gap; Tool_flg bit0 keeps the pull horizontal.

#include "atari.h"
#include "math_sub.h"
#include "esp.h"

typedef struct tagESP0D_WK {
    u32 Type;        // 0x00
    f32 Dist;        // 0x04
    f32 Pow;       // 0x08
    cParts* pParts;  // 0x0C
} ESP0D_WK;

// Attractor effect: pulls the sprite toward its parent while inside `dist`.
class cEsp0d : public cEsp {
public:
    ESP0D_WK m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

// EspCreateTbl[0x0D] factory.
cEsp* Esp0d_Create()
{
    return new cEsp0d;
}

// Base update and animation, then (Type 0) adds (Dist - len) * Pow along the unit vector from the
// target's world position to the sprite into m_Speed while len < Dist.
void cEsp0d::move()
{
    ESP0D_WK* w = &m_Free;
    Vec v;
    f32 len;

    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else if (w->Type == 0) {
            PSVECSubtract(&m_Pos, &w->pParts->world, &v);
            len = PSVECMag(&v);
            if (len == 0.0f) {
                v.x = 0.0f;
                v.y = 1.0f;
                v.z = 0.0f;
            } else {
#line 77 "D:/Bio4/Prog/esp0d.cpp"
                VECNormalize(&v, &v);
            }
            if (len < w->Dist) {
                PSVECScale(&v, &v, (w->Dist - len) * w->Pow);
                if (m_Tool_flg & 1) {
                    v.y = 0.0f;
                }
                PSVECAdd(&m_Speed, &v, &m_Speed);
            }
        }
    }
}

// Range / strength from Work8[0..1], Type from WorkSp8[0] (only 0 is valid); the target is the
// attached model's parts (Tool_flg 0x20) or the parent coordinate.
int cEsp0d::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    ESP0D_WK* w = &m_Free;

    w->Dist = (f32)(s8)pSeq->Work8[0] * 100.0f;
    w->Pow = (f32)(s8)pSeq->Work8[1] * 0.00005f;
    w->Type = pSeq->WorkSp8[0];
    if (m_Tool_flg & 0x20) {
        w->pParts = m_pMod->getPartsPtr(m_Parts_no);
    } else {
        w->pParts = (cParts*) parent;
    }
    if (w->Type != 0) {
        pLog->err(0, 0, "ESP0D : Type[%x] invalid.", w->Type);
        return 0;
    }
    return 1;
}
