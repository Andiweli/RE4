// game/esp49.cpp: effect id 0x49, a sprite that sinks into water. Once its position is more than
// `fade_height` (Vec0.z) below the water surface its alpha fades, and below `del_height`
// (Vec0.x) it dies, optionally (Work8[2]) spawning est Work8[1] of owner Work8[0] there.

#include "atari.h"
#include "light.h"
#include "esp.h"

typedef struct tagESP49_WK {
    f32 del_height;     // 0x00 depth below the water surface at which the effect dies
    f32 fade_height; // 0x04 depth over which the alpha fades
    f32 Base_alpha;     // 0x08 base alpha
    u8 EstOwner;      // 0x0C spawn an est when the effect dies underwater
    u8 EstNo;      // 0x0D
    u8 estPrm;     // 0x0E
} ESP49_WK;

// Effect that fades out and dies when it sinks below the water surface.
class cEsp49 : public cEsp {
public:
    ESP49_WK m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

// EspCreateTbl[0x49] factory.
cEsp* Esp49_Create()
{
    return new cEsp49;
}

// Restores the unfaded alpha, runs the base update/animation, then compares the depth below the
// water surface (GetWaterHeight at m_Pos): deeper than del_height releases the effect (after the
// optional EstSet), between del_height and fade_height scales m_Col_a linearly.
void cEsp49::move()
{
    ESP49_WK* w = &m_Free;
    Vec wpos;
    Vec r;
    Vec ep;
    f32 h;

    m_Col_a = w->Base_alpha;
    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
            return;
        }
        w->Base_alpha = m_Col_a;
        if (parent != pEffParentWorld && (m_Release_time == 0xff || m_Release_time <= m_Life_time)) {
            PSMTXMultVec(parent->mat, &m_Pos, &wpos);
        } else {
            wpos = m_Pos;
        }
        if (GetWaterHeight(&m_Pos, &h)) {
            f32 d = h - m_Pos.y;
            if (d < w->del_height) {
                if (w->EstOwner & 1) {
                    r.x = r.y = r.z = 0.0f;
                    ep = m_Pos;
                    EstSet(0, -1, &ep, &r, w->EstNo, w->estPrm, info.Core_flg, info.Core_kind, info.Core_pEm, 0);
                }
                PushEsp(this);
            } else if (d < w->fade_height) {
                m_Col_a *= (d - w->del_height) / w->fade_height;
            }
        }
    }
}

// Depth thresholds from Vec0.x / Vec0.z (fade never below delete), est owner/id from Work8[0..1],
// est enable Work8[2] (0/1, else fails); remembers the initial alpha.
int cEsp49::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    ESP49_WK* w = &m_Free;

    w->del_height = pSeq->Vec0.x;
    w->fade_height = pSeq->Vec0.z;
    if (w->del_height > w->fade_height) {
        w->fade_height = w->del_height;
    }
    w->EstNo = pSeq->Work8[0];
    w->estPrm = pSeq->Work8[1];
    w->EstOwner = pSeq->Work8[2];
    if (w->EstOwner > 1) {
        pLog->err(0, 0, "ESP_49 : FLAG[%d] invalid.", w->EstOwner);
        return 0;
    }
    w->Base_alpha = m_Col_a;
    return 1;
}
