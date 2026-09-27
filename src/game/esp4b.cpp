// game/esp4b.cpp: effect id 0x4B, a sprite frozen on one animation pattern: Work8[0] selects the
// pattern (0xFF = random) and the animation is never advanced.

#include "atari.h"
#include "light.h"
#include "rnd.h"
#include "esp.h"

// Static sprite showing one selected (or random) animation pattern.
class cEsp4b : public cEsp {
public:
    u32 xF8;

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

// EspCreateTbl[0x4B] factory.
cEsp* Esp4b_Create()
{
    return new cEsp4b;
}

// Base motion/colour/life only; the pattern chosen at spawn stays.
void cEsp4b::move()
{
    CommonMove();
}

// Sets m_Ptn_no from Work8[0] (0xFF: random pattern of the texture animation). Fails (0) when the
// texture id has no animation data or the pattern is out of range.
int cEsp4b::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    cAnm* anm;
    u32 ptn;

    if (!EspGetAnmAddr(m_Tex_id, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", m_Tex_id);
        return 0;
    }
    ptn = pSeq->Work8[0];
    if (ptn == 0xff) {
        m_Ptn_no = (Rnd() + Rnd()) % anm->Frames;
    } else {
        if (ptn >= anm->Frames) {
            pLog->err(0, 0, "ESP4B : PtnNo[%d >= %d] over", ptn, anm->Frames);
            return 0;
        }
        m_Ptn_no = ptn;
    }
    return 1;
}
