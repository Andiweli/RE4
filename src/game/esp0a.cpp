// game/esp0a.cpp: effect id 0x0A, motion-trail sprites drawn along a path the effect predicts by
// simulating itself 50 frames ahead.

#include "atari.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"


struct Esp0aWork {
    u8 Type;   // 0x00 0: spawner (copies itself 50 times), 1: rim-lit sprite
    u8 Base_alpha;  // 0x01 base alpha
};

static f32 RIMIT1 = 0.9f;
static f32 RIMIT2 = 0.6f;
static f32 RIMIT3 = 0.9f;
static f32 RIMIT4 = 0.6f;

// Rim highlight sprite: the alpha fades in when the sprite's direction (spd) points toward
// or away from the camera while the sprite is in front of it.
class cEsp0a : public cEsp {
public:
    Esp0aWork m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

extern "C" {
void Esp0a_Trans(cEsp0a* esp);
void Esp0a_Trans2(cEsp* esp);
}


// Reference read of the world parent: an unflagged MEM that stays below the stores through e.
static inline cCoord* RefCoordP(cCoord*& p)
{
    return p;
}

// EspCreateTbl[0x0A] factory.
cEsp* Esp0a_Create()
{
    return new cEsp0a;
}

// Type 0 releases itself (its copies were made in SetFreeWork). Type 1 computes the view-space
// speed direction and position and fades m_Col_a from Base_alpha when the sprite is near the view
// axis (vpos.z < -0.9) and moving along it (|dot| beyond 0.6).
void cEsp0a::move()
{
    Esp0aWork* w = &m_Free;
    Vec dir;
    Vec vpos;
    Mtx m;
    f32 dot;

    switch (w->Type) {
    case 0:
        PushEsp(this);
        break;
    case 1:
        PSMTXConcat(pG->Camera.v_mat, parent->mat, m);
        PSMTXMultVecSR(m, &m_Speed, &dir);
        PSMTXMultVec(m, &m_Pos, &vpos);
        if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f) {
            dir.y = 1.0f;
        }
        if (vpos.x == 0.0f && vpos.y == 0.0f && vpos.z == 0.0f) {
            vpos.y = 1.0f;
        }
#line 51 "D:/Bio4/Prog/esp0a.cpp"
        VECNormalize(&dir, &dir);
        VECNormalize(&vpos, &vpos);
        dot = PSVECDotProduct(&dir, &vpos);
        if (vpos.z < -RIMIT1 && dot < -RIMIT2) {
            m_Col_a = (f32)w->Base_alpha * (1.0f - ((-dot - RIMIT2) / (1.0f - RIMIT2)) * (-vpos.z - RIMIT1) / (1.0f - RIMIT1));
        } else if (vpos.z < -RIMIT3 && dot > RIMIT4) {
            m_Col_a = (f32)w->Base_alpha * (1.0f - ((dot - RIMIT4) / (1.0f - RIMIT4)) * (-vpos.z - RIMIT3) / (1.0f - RIMIT3));
        } else {
            m_Col_a = (f32)w->Base_alpha;
        }
        break;
    }
}

// EspTransTbl[0x0A]: Type 1 pulls a scratch copy, steps it with CommonMove 50 times and queues a
// one-frame id-0 ghost sprite (Esp0a_Trans2) at each position, Z-sorted by world position.
void Esp0a_Trans(cEsp0a* pEsp)
{
    Esp0aWork* w = &pEsp->m_Free;

    switch (w->Type) {
    case 0:
        break;
    case 1: {
        cEsp* base;
        cEsp* e;
        u32 i = 0;

        if (PullEsp(&base, 0)) {
            *base = *pEsp;
            base->m_Id = 0;
            for (; i < 50; i++) {
                if (PullEsp(&e, 0)) {
                    Vec wpos;
                    *e = *base;
                    int ot = 8;
                    e->m_Speed.x = e->m_Speed.y = e->m_Speed.z = 0.0f;
                    e->m_Size_plus = 0.0f;
                    e->m_Col_d_r = 1.0f;
                    e->m_Col_d_g = 1.0f;
                    e->m_Col_d_b = 1.0f;
                    e->m_Col_d_a = 1.0f;
                    e->m_Col_max_cnt = 0;
                    e->m_Col_start_cnt = 0;
                    e->m_Pos_start_cnt = 0;
                    e->m_Size_start_cnt = 0;
                    e->m_Life_max = 1;
                    e->m_Life_time = 0;
                    if (e->parent != pEffParentWorld) {
                        PSMTXMultVec(e->parent->mat, &e->m_Pos, &wpos);
                    } else {
                        wpos = e->m_Pos;
                    }
                    AddOtWorldPos(e, Esp0a_Trans2, &wpos, ot, 0.0f);
                }
                if (!base->CommonMove()) {
                    break;
                }
            }
            if (base->m_Be_flg & 1) {
                PushEsp(base);
            }
        }
        break;
    }
    }
}

// Draw callback of the ghost copies: plain EspCommonTrans.
void Esp0a_Trans2(cEsp* esp)
{
    EspCommonTrans(esp);
}

// Type 0: pulls a scratch copy and lays down up to 50 frozen id-0 sprites (life Work8[0]) along
// its simulated path. Type 1: Tool_flg 0x400 (pre-world layer) and m_Flg bit1. Work8[1] must be 0.
int cEsp0a::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    Esp0aWork* w = &m_Free;

    if (pSeq->Work8[1] != 0) {
        pLog->err(0, 0, "ESP0a : WK1 not 0!!");
    }
    w->Type = pSeq->Work8[2];
    w->Base_alpha = m_Col_start_a;
    switch (w->Type) {
    case 0: {
        cEsp* base;
        cEsp* p;
        u32 i = 0;

        if (PullEsp(&base, 0)) {
            *base = *this;
            base->m_Id = 0;
            for (; i < 50; i++) {
                if (PullEsp(&p, 0)) {
                    *p = *base;
                    p->m_Speed.x = p->m_Speed.y = p->m_Speed.z = 0.0f;
                    p->m_Size_plus = 0.0f;
                    p->m_Col_d_r = 1.0f;
                    p->m_Col_d_g = 1.0f;
                    p->m_Col_d_b = 1.0f;
                    p->m_Col_d_a = 1.0f;
                    p->m_Col_max_cnt = 0;
                    p->m_Col_start_cnt = 0;
                    p->m_Pos_start_cnt = 0;
                    p->m_Size_start_cnt = 0;
                    p->m_Life_time = 0;
                    p->m_Life_max = (s8)pSeq->Work8[0];
                }
                if (!base->CommonMove()) {
                    break;
                }
            }
            if (base->m_Be_flg & 1) {
                PushEsp(base);
            }
        }
        break;
    }
    case 1:
        m_Tool_flg |= 0x400;
        m_Flg |= 2;
        break;
    }
    return 1;
}
