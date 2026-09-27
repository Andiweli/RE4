// game/esp40.cpp: effect id 0x40, a sprite riding the water surface (Espgen42 water). Each frame
// the world position is re-projected onto the water height plus `Ofs_y` (generator Pos.y) plus
// a bobbing offset integrated from the y speed; the parent-space position is cached in Base_Pos.

#include "atari.h"
#include "light.h"
#include "esp.h"

struct Esp40Work {
    Vec Base_Pos;      // 0x00 position kept in parent space
    f32 Ofs_y;     // 0x0C height above the water surface
    f32 Pos_y;     // 0x10 bobbing offset
    f32 Speed_y;     // 0x14
    f32 Speed_plus_y;     // 0x18
};

// Effect floating on the water surface of its parent.
class cEsp40 : public cEsp {
public:
    Esp40Work m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

// EspCreateTbl[0x40] factory.
cEsp* Esp40_Create()
{
    return new cEsp40;
}

// While attached to a parent: restores Base_Pos, runs the base update, integrates the bob
// (Pos_y / Speed_y / Speed_plus_y, damped by m_D_speed) and sets the world y to water height +
// Ofs_y + Pos_y, converting back into parent space. Released when the animation ends.
void cEsp40::move()
{
    Vec wpos;
    Mtx inv;
    f32 h;

    if (parent != pEffParentWorld && (m_Release_time == 0xff || m_Release_time - 1 > m_Life_time)) {
        m_Pos = m_Free.Base_Pos;
    }
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else if (parent != pEffParentWorld && (m_Release_time == 0xff || m_Release_time > m_Life_time)) {
            Esp40Work* w = &m_Free;
            w->Base_Pos = m_Pos;
            if (m_Pos_start_cnt == 0 || m_Pos_start_cnt <= m_Life_time) {
                w->Pos_y += w->Speed_y;
                w->Speed_y += w->Speed_plus_y;
                w->Speed_y *= m_D_speed;
            }
            PSMTXMultVec(parent->mat, &m_Pos, &wpos);
            if (GetWaterHeight(&wpos, &h)) {
                wpos.y = h + w->Ofs_y + w->Pos_y;
                PSMTXInverse(parent->mat, inv);
                PSMTXMultVec(inv, &wpos, &m_Pos);
            }
        }
    }
}

// A parent with Release_time 0 is dropped to world space at once. Attached effects record Ofs_y,
// Base_Pos and the y speeds; world-space effects are simply placed at water height + Pos.y.
int cEsp40::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    f32 h;

    if (parent != pEffParentWorld && m_Release_time == 0) {
        ApplyMatrix(parent->mat);
        parent = pEffParentWorld;
    }
    if (parent != pEffParentWorld && (m_Release_time == 0xff || m_Release_time <= m_Life_time)) {
        Esp40Work* w = &m_Free;
        w->Ofs_y = pSeq->Pos.y;
        w->Base_Pos = m_Pos;
        w->Speed_y = m_Speed.y;
        w->Speed_plus_y = m_Speed_plus.y;
    } else if (GetWaterHeight(&m_Pos, &h)) {
        m_Pos.y = h + pSeq->Pos.y;
    }
    return 1;
}
