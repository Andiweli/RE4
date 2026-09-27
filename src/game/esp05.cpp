// game/esp05.cpp: effect id 0x05, a fluttering sprite (falling leaf, feather, ash): the position
// wobbles by Pow (Vec0.x) along sin / cos of a random phase advanced by Spd (Vec0.y x 0.05) with
// random speed jitter; Vec0.z randomises both by up to 10% per unit.

#include "light.h"
#include "atari.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"

struct Esp05Work {
    f32 Pow;     // 0x00 wobble amplitude
    f32 Spd;  // 0x04
    f32 Theta;     // 0x08
};

// Fluttering sprite (falling leaf / feather): wobbles the position with sin/cos of a random angle.
class cEsp05 : public cEsp {
public:
    Esp05Work m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

// EspCreateTbl[0x05] factory.
cEsp* Esp05_Create()
{
    return new cEsp05;
}

// Base update, then the wobble: x by Pow sin(Theta), z by Pow cos(1.7 Theta), y by 0.3 Pow
// cos(1.9 Theta), Theta advancing by Spd x (0.2 .. 1.2). Released when the animation ends.
void cEsp05::move()
{
    Esp05Work* w = &m_Free;

    if (CommonMove()) {
        m_Pos.x += w->Pow * sinf(w->Theta);
        m_Pos.z += w->Pow * cosf(w->Theta * 1.7f);
        m_Pos.y += w->Pow * 0.3f * cosf(w->Theta * 1.9f);
        w->Theta += w->Spd * (fRand0_1() + 0.2f);
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

// Amplitude / speed from Vec0.x / Vec0.y (x 0.05), each randomised by Vec0.z x 10%, random phase.
int cEsp05::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    Esp05Work* w = &m_Free;

    w->Pow = pSeq->Vec0.x;
    w->Spd = pSeq->Vec0.y * 0.05f;
    w->Pow += w->Pow * fRand0_1() * (pSeq->Vec0.z * 0.1f);
    w->Spd += w->Spd * fRand0_1() * (pSeq->Vec0.z * 0.1f);
    w->Theta = fRand0_1() * PI * 2.0f;
    return 1;
}
