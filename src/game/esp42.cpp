// game/esp42.cpp: effect id 0x42, a sprite with an explicit GX blend mode: Work8[0] / Work8[1]
// pick the source / destination blend factors (bl1 / bl2 tables, GXBlendFactor order).

#include "atari.h"
#include "esp.h"

// Effect that selects its blend mode from two lookup tables.
class cEsp42 : public cEsp {
public:
    u32 xF8;

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

// EspCreateTbl[0x42] factory.
cEsp* Esp42_Create()
{
    return new cEsp42;
}

// Standard sprite update; released when the animation ends.
void cEsp42::move()
{
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        }
    }
}

// Stores GX_BM_BLEND with the source factor bl1[Work8[0]] and destination factor bl2[Work8[1]] in
// m_Blend_mode..m_Logic_op (read by EspCommonTrans). Fails when either index is above 8.
int cEsp42::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    static u32 bl1[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    static u32 bl2[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };

    if ((s8)pSeq->Work8[0] > 8) {
        pLog->err(0, 0, "ESP42 : WK0[%d] invalid", (s8)pSeq->Work8[0]);
        return 0;
    }
    if ((s8)pSeq->Work8[1] > 8) {
        pLog->err(0, 0, "ESP42 : WK1[%d] invalid", (s8)pSeq->Work8[1]);
        return 0;
    }
    m_Blend_mode = 1;
    m_Src_factor = bl1[(s8)pSeq->Work8[0]];
    m_Dst_factor = bl2[(s8)pSeq->Work8[1]];
    m_Logic_op = 0;
    return 1;
}
