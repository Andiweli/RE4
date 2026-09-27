// game/esp1b.cpp: effect id 0x1B, a spline (curved ribbon) sprite. The generator's Vec0..Vec2 are
// the curve control offsets (scaled by esp1b_scale), Work8[0] + 4 the number of points; the
// geometry is built and drawn by Esp1b_SpTrans in esp_sub.cpp.

#include "atari.h"
#include "esp.h"
#include <string.h>


typedef struct tagESP1B_WK {
    int div;   // 0x00 number of points
    Vec Vec0;  // 0x04
    Vec Vec1;  // 0x10
    Vec Vec2;  // 0x1C
} ESP1B_WK;

static f32 esp1b_scale = 0.005f;

// Spline sprite (drawn by Esp1b_SpTrans in esp_sub.cpp).
class cEsp1b : public cEsp {
public:
    ESP1B_WK m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

// EspCreateTbl[0x1B] factory.
cEsp* Esp1b_Create()
{
    return new cEsp1b;
}

// Standard sprite update; released when the animation ends.
void cEsp1b::move()
{
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

// Point count = Work8[0] + 4 clamped to 2..0x40 (out of range is reported), control vectors from
// Vec0..Vec2 x esp1b_scale; sets m_Flg 0x10 (spline sprite) for the trans function.
int cEsp1b::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    ESP1B_WK* w = &m_Free;
    int n;

    n = (s8)pSeq->Work8[0] + 4;
    if (n <= 1) {
        pLog->err(0, 0, "ESP1B : Wk0[%d] Invalid.", (s8)pSeq->Work8[0]);
        n = 2;
    }
    if (n > 0x40) {
        pLog->err(0, 0, "ESP1B : Wk0[%d] Invalid.", (s8)pSeq->Work8[0]);
        n = 0x40;
    }
    m_Flg |= 0x10;
    w->div = n;
    // Vec0..Vec2 through byte pointers: `&w->Vec1` changes the schedule (7 words)
    memcpy((u8*)w + 4, &pSeq->Vec0.x, sizeof(Vec));
    memcpy((u8*)w + 0x10, &pSeq->Vec1.x, sizeof(Vec));
    memcpy((u8*)w + 0x1C, &pSeq->Vec2.x, sizeof(Vec));
    PSVECScale(&w->Vec0, &w->Vec0, esp1b_scale);
    PSVECScale(&w->Vec1, &w->Vec1, esp1b_scale);
    PSVECScale(&w->Vec2, &w->Vec2, esp1b_scale);
    return 1;
}

// The split object's .sdata is padded to 8 bytes (the following unit is 8-aligned).
asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
