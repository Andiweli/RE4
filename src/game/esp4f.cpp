// game/esp4f: effect id 0x4F, area-gated sprite (D:/Bio4/Prog/esp4f.cpp). A plain animated sprite
// that is only queued for drawing while its world position is inside the effect area number given
// by the record (SstArea / EffAreaCheckNo). Entry points: Esp4f_Create, cEsp4f::move / SetFreeWork.
#include "atari.h"
#include "esp.h"

struct Esp4fWork {
    u8 area_no;  // 0x00
};

// Effect that is only drawn while inside a given room area.
class cEsp4f : public cEsp {
public:
    Esp4fWork m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

// Create entry of the EffSetId function table for effect id 0x4F.
cEsp* Esp4f_Create()
{
    return new cEsp4f;
}

// Per-frame move: common update, texture animation, then PushEsp (queue for EspCommonTrans) only when
// the animation has not ended or the world position lies inside effect area m_Free.area_no.
void cEsp4f::move()
{
    Esp4fWork* w = &m_Free;
    Vec wpos;

    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else {
            if (parent == pEffParentWorld) {
                wpos = m_Pos;
            } else {
                PSMTXMultVec(parent->mat, &m_Pos, &wpos);
            }
            if (EffAreaCheckNo(&wpos, w->area_no)) {
                PushEsp(this);
            }
        }
    }
}

// Work8[0] of the record is the effect area number the sprite is gated on.
int cEsp4f::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    m_Free.area_no = pSeq->Work8[0];
    return 1;
}
