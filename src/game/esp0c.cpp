// game/esp0c.cpp: effect id 0x0C, an est (effect set) spawner that never draws. On its first move
// it starts est id Work8[1] of owner Work8[0] at its position, then releases itself.

#include "atari.h"
#include "light.h"
#include "main_mem.h"
#include "esp.h"

struct Esp0cWork {
    u8 EstNo;      // 0x00 est number (gen->Work8[0])
    u8 EstOwner_wt;     // 0x01 (gen->Work8[1])
    u8 EstNo_wt;     // 0x02 est number used when the sprite hit water (gen->prm byte 0xCF)
    u8 estPrm2;    // 0x03 (gen->prm byte 0xD3)
    u32 onWater;   // 0x04 1: the position was raised to the water surface
};

// Est work passed to EstSet (0x1C bytes).
struct EstSetWork {
    u8 x0;      // 0x00
    u8 flag1;   // 0x01
    u8 flag2;   // 0x02
    u8 x3;      // 0x03
    Vec spd;    // 0x04
    f32 sizeX;  // 0x10
    f32 sizeY;  // 0x14
    u8 col[4];  // 0x18
};

// Est (effect set) trigger: on its first move it starts an est at its position and dies.
class cEsp0c : public cEsp {
public:
    Esp0cWork m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(EspGenWork* gen, u32* seed);
};


// EspCreateTbl[0x0C] factory.
cEsp* Esp0c_Create()
{
    return new cEsp0c;
}

// First (and only) update: builds the ESPSEQ_CONTROL (Add_flg speed, Mul_flg size + colour) from
// the sprite's own parameters, calls EstSet with the normal or on-water est owner/id, and pushes
// itself.
void cEsp0c::move()
{
    Esp0cWork* w = &m_Free;
    EstSetWork est;

    memclr_asm(&est, sizeof(EstSetWork));
    est.flag2 |= 1;
    est.flag1 |= 6;
    est.sizeX = m_Size_base_x * 0.005f;
    est.sizeY = m_Size_base_y * 0.005f;
    est.col[0] = m_Col_start_r;
    est.col[1] = m_Col_start_g;
    est.col[2] = m_Col_start_b;
    est.col[3] = m_Col_start_a;
    est.spd = m_Speed;
    if (w->onWater == 1) {
        EstSet(0, -1, &m_Pos, &m_Ang, w->EstNo_wt, w->estPrm2, info.Core_flg, info.Core_kind, info.Core_pEm, &est);
    } else {
        EstSet(0, -1, &m_Pos, &m_Ang, w->EstNo, w->EstOwner_wt, info.Core_flg, info.Core_kind, info.Core_pEm, &est);
    }
    PushEsp(this);
}

// EspTransTbl[0x0C]: never expected to run (the effect dies in its first move); logs an error.
extern "C" void Esp0c_Trans(cEsp* esp)
{
    pLog->err(0, 0, "ESP0C : Invalid Trans.");
}

// Reads the est owner/id pairs, detaches from the parent into world space, applies the Work8[2]
// floor / water snap (+65 units, + Vec0.y) and the Work8[3] in-room check; unknown modes fail.
int cEsp0c::SetFreeWork(EspGenWork* pSeq, u32* pRand_seed)
{
    Esp0cWork* w = &m_Free;
    u32 attr;
    f32 h;

    w->EstNo = pSeq->Work8[0];
    w->EstOwner_wt = pSeq->Work8[1];
    w->EstNo_wt = pSeq->prm.b.xCF;
    w->estPrm2 = pSeq->prm.b.xD3;
    if (parent != pEffParentWorld) {
        ApplyMatrix(parent->mat);
        parent = pEffParentWorld;
    }
    switch ((s8)pSeq->Work8[2]) {
    case 0:
        break;
    case 1:
        m_Pos.y = SatMgr.getFloor(&m_Pos, &attr, 600.0f, 100000.0f, 0) + 65.0f + pSeq->Vec0.y;
        break;
    case 2:
        m_Pos.y = SatMgr.getFloor(&m_Pos, &attr, 600.0f, 100000.0f, 0) + 65.0f;
        if (GetWaterHeight(&m_Pos, &h)) {
            if (m_Pos.y < h + pSeq->Vec0.y) {
                m_Pos.y = h + pSeq->Vec0.y;
                w->onWater = 1;
            }
        }
        break;
    default:
        pLog->err(0, 0, "ESP0C : Type[%d] Invalid Trans.", (s8)pSeq->Work8[2]);
        return 0;
    }
    if ((s8)pSeq->Work8[3] == 0) {
    } else if ((s8)pSeq->Work8[3] == 1) {
        if (EffAreaCheckInRoom(&m_Pos) == 1) {
            PushEsp(this);
        }
    } else {
        pLog->err(0, 0, "ESP0C : Type[%d] Invalid Trans.", (s8)pSeq->Work8[3]);
        return 0;
    }
    return 1;
}
