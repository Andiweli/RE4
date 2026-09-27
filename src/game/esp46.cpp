// game/esp46.cpp: effect id 0x46, the screen colour filter driver. The effect is never drawn as a
// sprite; its trans function pushes its faded colour into Filter03 (full screen tint) with the
// filter type and priority from the generator's Work8 bytes.

#include "atari.h"
#include "filter.h"
#include "esp.h"

struct Esp46Work {
    int level;   // 0x00 filter03 type
    u8 priority;      // 0x04
    u8 sp_flag;      // 0x05
};

// Screen filter (filter03) driver: never drawn itself, feeds its color into the filter.
class cEsp46 : public cEsp {
public:
    Esp46Work m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

// EspCreateTbl[0x46] factory.
cEsp* Esp46_Create()
{
    return new cEsp46;
}

// Forces a huge m_Size_mul so the base scale logic can never release the effect, then runs the
// base motion/colour/life (life and colour fade drive the tint).
void cEsp46::move()
{
    m_Size_mul = 1e22f;
    CommonMove();
}

// EspTransTbl[0x46]: sets Filter03 (type `level`, priority, sp_flag) to the current colour scaled by
// alpha; nothing is drawn here.
void Esp46_Trans(cEsp46* pEsp)
{
    Esp46Work* w = &pEsp->m_Free;
    f32 a = pEsp->m_Col_a * (1.0f / 255.0f);

    Filter03SetParam(w->level, (u8)(pEsp->m_Col_r * a), (u8)(pEsp->m_Col_g * a), (u8)(pEsp->m_Col_b * a), w->priority, w->sp_flag);
}

// Reads the filter type (Work8[0]), priority (Work8[1]) and special flag (Work8[2], 0/1) and puts
// the effect on the screen-first layer (m_Parts_no 0xF8).
int cEsp46::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    Esp46Work* w = &m_Free;

    w->level = (s8)pSeq->Work8[0];
    w->priority = pSeq->Work8[1];
    if ((s8)pSeq->Work8[2] <= 1) {
        w->sp_flag = pSeq->Work8[2];
    } else {
        pLog->err(0, 0, "ESP46 : WK2[%x] invalid.", (s8)pSeq->Work8[2]);
    }
    m_Parts_no = 0xf8;
    return 1;
}
