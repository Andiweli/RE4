// game/esp03.cpp: effect id 0x03, a line trail drawn from a ring buffer of the last positions.

#include "atari.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"

typedef struct tagESP03_WK {
    s8 maxPoints;         // 0x00 number of trail points (2..6), 10: single camera-facing quad
    u8 flag;   // 0x01 bit0: stop at walls (gen->Work8[1])
    u8 pad_2[7];
    u8 nPos;       // 0x09 ring buffer index of the next point
    u16 Width;    // 0x0A line width
    Vec* pBeforePos;     // 0x0C current point
    Vec Pos[6];   // 0x10 position history (only 4 are used)
} ESP03_WK;

// Line trail: keeps the last positions in a ring buffer and draws them as a line strip.
class cEsp03 : public cEsp {
public:
    ESP03_WK m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

void Esp03_HitWall(cEsp03* esp);

// EspCreateTbl[0x03] factory.
cEsp* Esp03_Create()
{
    return new cEsp03;
}

// Own update: release from parent, optional wall check, integrates speed into the next ring
// buffer slot, applies scale and colour fades and life, and advances the ring index (mod 4).
void cEsp03::move()
{
    ESP03_WK* w = &m_Free;
    Vec* p;

    if (parent != pEffParentWorld && m_Release_time != 0xFF && m_Release_time <= m_Life_time) {
        ApplyMatrix(parent->mat);
        parent = pEffParentWorld;
    }
    if (w->flag & 1) {
        Esp03_HitWall(this);
    }
    if (m_Pos_start_cnt <= m_Life_time) {
        p = &w->Pos[w->nPos];
        PSVECAdd(w->pBeforePos, &m_Speed, p);
        PSVECAdd(&m_Speed, &m_Speed_plus, &m_Speed);
        PSVECScale(&m_Speed, &m_Speed, m_D_speed);
        w->pBeforePos = p;
    }
    if (m_Size_start_cnt <= m_Life_time) {
        m_Size_mul += m_Size_plus;
        m_Size_plus *= m_D_size_plus;
        if (m_Size_mul <= 0.0f) {
            PushEsp(this);
            return;
        }
    }
    w->Width = (u16)(m_Size_base_x * m_Size_mul / (100.0f / 3.0f));
    if (ColorUpdate()) {
        if (m_Life_max != 0 && m_Life_max <= m_Life_time) {
            PushEsp(this);
            return;
        }
        m_Life_time++;
        w->nPos = (w->nPos + 1) & 3;
    }
}

// EspTransTbl[0x03]: GX line state in the parent * local matrix; draws the diamond quad
// (maxPoints 10) or the line strip through the last maxPoints history points, newest first.
void Esp03_Trans(cEsp03* esp)
{
    ESP03_WK* w = &esp->m_Free;
    Vec* p;
    Vec* v;
    int idx;
    int i;

    GXSetZMode(1, 3, 0);
    GXSetCullMode(0);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    CameraCurrentProjection();
    if ((s8)esp->m_Parts_no >= -8 && (s8)esp->m_Parts_no <= -3) {
        pLog->err(0, 0, "ESP_03 : SCREEN MODE is invalid.");
        PushEsp(esp);
        return;
    }
    {
        Mtx m;

        PSMTXIdentity(esp->m_Mat);
        RotMatrix(esp->m_Mat, &esp->m_Ang);
        TransMatrix(esp->m_Mat, &esp->m_Pos);
        PSMTXConcat(pG->Camera.v_mat, esp->parent->mat, m);
        PSMTXConcat(m, esp->m_Mat, esp->m_Mat);
    }
    GXLoadPosMtxImm(esp->m_Mat, 0);
    GXSetCurrentMtx(0);
    esp->ChannelSet();
    GXSetTevOp(0, 4);
    GXSetAlphaCompare(4, 1, 1, 4, 1);
    GXSetBlendMode(esp->m_Blend_mode, esp->m_Src_factor, esp->m_Dst_factor, esp->m_Logic_op);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    if (w->maxPoints == 10) {
        Vec d;
        Vec up;
        Vec q[4];

        p = &w->Pos[(w->nPos - 1) & 3];
        v = q;
#line 187 "D:/Bio4/Prog/esp03.cpp"
        VECNormalize(&pG->Camera.Up, &up);
        PSVECScale(&up, &up, esp->m_Size_base_x * 0.5f);
        PSVECSubtract(p, &pG->Camera.param.Campos, &d);
        PSVECCrossProduct(&up, &d, &d);
#line 193 "D:/Bio4/Prog/esp03.cpp"
        VECNormalize(&d, &d);
        PSVECScale(&d, &d, esp->m_Size_base_x * 0.5f);
        PSVECAdd(p, &d, &q[0]);
        PSVECAdd(p, &up, &q[1]);
        PSVECSubtract(p, &d, &q[2]);
        PSVECSubtract(p, &up, &q[3]);
        GXBegin(0x80, 0, 4);
        GXPosition3f32(v->x, v->y, v->z);
        GXColor4u8((u8)esp->m_Col_r, (u8)esp->m_Col_g, (u8)esp->m_Col_b, (u8)esp->m_Col_a);
        v++;
        GXPosition3f32(v->x, v->y, v->z);
        GXColor4u8((u8)esp->m_Col_r, (u8)esp->m_Col_g, (u8)esp->m_Col_b, (u8)esp->m_Col_a);
        v++;
        GXPosition3f32(v->x, v->y, v->z);
        GXColor4u8((u8)esp->m_Col_r, (u8)esp->m_Col_g, (u8)esp->m_Col_b, (u8)esp->m_Col_a);
        v++;
        GXPosition3f32(v->x, v->y, v->z);
        GXColor4u8((u8)esp->m_Col_r, (u8)esp->m_Col_g, (u8)esp->m_Col_b, (u8)esp->m_Col_a);
    } else {
        idx = w->nPos;
        GXSetLineWidth((u8)w->Width, 0);
        GXBegin(0xB0, 0, (u16)w->maxPoints);
        for (i = 0; i < w->maxPoints; i++) {
            idx--;
            idx &= 3;
            p = &w->Pos[idx];
            GXPosition3f32(p->x, p->y, p->z);
            GXColor4u8((u8)esp->m_Col_r, (u8)esp->m_Col_g, (u8)esp->m_Col_b, (u8)esp->m_Col_a);
        }
        GXSetLineWidth(6, 0);
    }
}

// Point count from Work8[0] (10 = quad mode), wall flag Work8[1] (0/1), never Z-culled.
int cEsp03::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    ESP03_WK* w = &m_Free;
    int n;

    w->pBeforePos = &w->Pos[0];
    w->nPos = 1;
    n = (s8)pSeq->Work8[0];
    if (n == 10) {
        w->maxPoints = n;
    } else {
        w->maxPoints = 4 - pSeq->Work8[0];
        if (w->maxPoints <= 1) {
            w->maxPoints = 2;
        } else if (w->maxPoints > 6) {
            w->maxPoints = 6;
        }
    }
    w->flag = pSeq->Work8[1];
    if (w->flag > 1) {
        pLog->err(0, 0, "ESP_03 : WK1 invalid.");
        return 0;
    }
    m_Radius = 100000000.0f;
    m_Flg |= 2;
    return 1;
}

// Casts the next step (current point + speed) against the wall collision (EatMgr); on a hit the
// life is set to expire this frame.
void Esp03_HitWall(cEsp03* pEsp)
{
    ESP03_WK* w = &pEsp->m_Free;
    Vec hit;
    Vec next2;
    Vec nrm;
    Vec next;

    PSVECAdd(w->pBeforePos, &pEsp->m_Pos, &next);
    PSVECAdd(&next, &pEsp->m_Speed, &next2);
    if (EatMgr.hitCheck(&next, &next2, &hit, &nrm, 0, 0)) {
        pEsp->m_Life_max = 1;
        pEsp->m_Life_time = 1;
    }
}
