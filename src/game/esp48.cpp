// game/esp48.cpp: effect id 0x48, a sprite whose rotation oscillates: each axis swings by a sine of
// its own amplitude (Vec0/1/2.x in 10ths) and frequency (Vec0/1/2.y) around the base rotation.

#include "atari.h"
#include "rnd.h"
#include "math_sub.h"
#include "esp.h"

typedef struct tagESP48_WK {
    f32 dist_x;    // 0x00
    f32 time_x;   // 0x04
    f32 dist_y;    // 0x08
    f32 time_y;   // 0x0C
    f32 dist_z;    // 0x10
    f32 time_z;   // 0x14
    f32 timer;    // 0x18
    Vec add_ang;   // 0x1C rotation added this frame
} ESP48_WK;

// Effect whose rotation swings on a sine wave around the base rotation.
class cEsp48 : public cEsp {
public:
    ESP48_WK m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

// EspCreateTbl[0x48] factory.
cEsp* Esp48_Create()
{
    return new cEsp48;
}

// Removes last frame's swing from m_Ang, runs the base update, advances the timer by 0.01 and adds
// the new per-axis sine offsets. Released when the animation ends.
void cEsp48::move()
{
    ESP48_WK* w = &m_Free;

    PSVECSubtract(&m_Ang, &w->add_ang, &m_Ang);
    if (CommonMove()) {
        w->timer += 0.01f;
        w->add_ang.x = w->dist_x * sinf(w->timer * w->time_x);
        w->add_ang.y = w->dist_y * sinf(w->timer * w->time_y);
        w->add_ang.z = w->dist_z * sinf(w->timer * w->time_z);
        PSVECAdd(&m_Ang, &w->add_ang, &m_Ang);
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

// Amplitudes (x 0.1) and frequencies per axis from Vec0..Vec2; the timer starts at a random phase.
int cEsp48::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    ESP48_WK* w = &m_Free;

    w->dist_x = pSeq->Vec0.x * 0.1f;
    w->time_x = pSeq->Vec0.y;
    w->dist_y = pSeq->Vec1.x * 0.1f;
    w->time_y = pSeq->Vec1.y;
    w->dist_z = pSeq->Vec2.x * 0.1f;
    w->time_z = pSeq->Vec2.y;
    w->timer = fRandSeed1_1(pRand_seed) * 2.0f * PI;
    return 1;
}
