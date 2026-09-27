// game/esp4e (D:/Bio4/Prog/esp4e.cpp): effect id 0x4E, a cloth sheet from the cloth pool that
// follows the effect and waves in sine fields driven by a global "wind" phase.
#include "atari.h"
#include "light.h"
#include "math_sub.h"
#include "rnd.h"
#include "esp.h"
#include "cloth.h"

typedef struct tagESP4E_WK {
    Cloth* pCl;      // 0x00
    GXTexObj tex;      // 0x04
    GXTlutObj tlut;    // 0x24
    f32 time;          // 0x30 wave phase along x
    f32 time2;          // 0x34 wave phase along y
    s8 time_plus;       // 0x38
    s8 time_plus2;       // 0x39
    s8 pow;           // 0x3A
    s8 pow2;           // 0x3B
    f32 range;         // 0x3C
    f32 range2;         // 0x40
    f32 offset;         // 0x44
    f32 wind_time;           // 0x48 global phase
    f32 wind_time_plus;        // 0x4C
    f32 wind_range_pow;     // 0x50
    f32 rand_ratio;           // 0x54 random factor
} ESP4E_WK;

// Cloth sheet: a Cloth grid attached to the effect position, waving with a sine field.
class cEsp4e : public cEsp {
public:
    ESP4E_WK m_Free;       // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
    virtual void Destruct();
};

extern "C" {
cEsp* Esp4e_Create();
void Esp4e_Trans();
}

// Create entry of the EffSetId function table for effect id 0x4E.
cEsp* Esp4e_Create()
{
    return new cEsp4e;
}

// Per-frame move: re-attaches the cloth to the parent coordinate when not in world space, copies the
// colour bytes, steps the cloth simulation (damping 0.98) and then adds the sine disturbance
// (pow/pow2 amplitudes, time/time2 phases advanced by time_plus/100 per frame, wind_range_pow
// scaling) plus m_Speed/10 rotated into cloth space to every grid vertex.
void cEsp4e::move()
{
    Vec pos0 = m_Pos;
    Vec sp;
    ESP4E_WK* wk = &m_Free;
    Cloth* c;
    f32 base;
    f32 rand;
    f32 stepY;
    f32 stepX;
    f32 wx;
    f32 wy;
    f32 ay;
    f32 ax;
    f32 s1;
    f32 s;
    u32 i;
    u32 j;

    if (!CommonMove()) {
        return;
    }
    m_Pos = pos0;
    c = wk->pCl;
    if (c == NULL) {
        return;
    }
    if (parent != pEffParentWorld) {
        Vec p;
        Vec r;
        Mtx m;
        PSMTXMultVec(parent->mat, &m_Pos, &p);
        low_RotMatrix(m, &m_Ang);
        PSMTXConcat(parent->mat, m, m);
        Matrix2AxisAngle(m, &r);
        c->SetPosAng(r, p);
    }
    {
        Mtx m;
        PSVECScale(&m_Speed, &sp, 0.1f);
        RotMatrix(m, &m_Ang);
        PSMTXInverse(m, m);
        PSMTXMultVec(m, &sp, &sp);
    }
    c->colR = (u8) m_Col_r;
    c->colG = (u8) m_Col_g;
    c->colB = (u8) m_Col_b;
    c->colA = (u8) m_Col_a;
    {
        static f32 DAMPING = 0.98f;
        c->calcSpeed(DAMPING);
    }
    c->move();
    c->calcNormal();

    wk->wind_time += wk->wind_time_plus;
    rand = wk->rand_ratio;
    wk->wind_time = LIMIT_ANGLE(wk->wind_time);
    s1 = SINF(wk->wind_time);
    s = wk->wind_range_pow * s1 * SINF(wk->wind_time * 0.3f) + 1.0f;
    wk->time = (f32) wk->time_plus * 0.01f + wk->time;
    wk->time2 = (f32) wk->time_plus2 * 0.01f + wk->time2;
    stepY = wk->range / c->divV;
    stepX = wk->range2 / c->divH;
    wx = (f32) wk->pow * 0.025f;
    wy = (f32) wk->pow2 * 0.025f;
    PSVECScale(&sp, &sp, s);
    ay = wk->time;
    wk->time = LIMIT_ANGLE(wk->time);
    wk->time2 = LIMIT_ANGLE(wk->time2);

    for (i = 0; i < c->divV; i++) {
        base = SINF(ay) * wx;
        ay += stepY * rand * fRand0_1() + stepY;
        ax = wk->time2;
        for (j = 0; j < c->divH; j++) {
            f32 v = SINF(ax) * wy;
            ax += stepX * rand * fRand0_1() + stepX;
            c->disturbance(j, i, (base + v) * s + wk->offset * s);
            PSVECAdd(&c->pSpd[j + c->divH * i], &sp, &c->pSpd[j + c->divH * i]);
        }
    }
}

// Returns the Cloth to the pool when the effect dies.
void cEsp4e::Destruct()
{
    if (m_Free.pCl) {
        m_Free.pCl->Destroy();
    }
}

// Trans entry of the function table: the Cloth draws itself, nothing to do here.
void Esp4e_Trans()
{
}

// Builds the cloth from the effect record: texture from m_Tex_id, grid nx = size_x/200*36 (2..100)
// by ny = size_y/200*24 (2..50), cell size from Vec0, Tool_flg bit 0 clears the cloth flag, and the
// wave parameters from Work8[0..3] / prm xCC,xD0 / xD4 / WorkSp8[0..2]. Returns 0 (effect not
// created) when the texture or a cloth slot is unavailable.
int cEsp4e::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    ESP4E_WK* wk = &m_Free;
    void* tpl;
    int ci;
    int nx;
    int ny;
    f32 width;
    f32 height;
    f32 d;
    u32 t;
    int flag;

    if (!EspGetTplAddr(m_Tex_id, &tpl)) {
        pLog->err(0, 0, "ESP4e : tex init invalid.");
        return 0;
    }
    if (!PullCloth(&wk->pCl)) {
        pLog->err(0, 0, "ESP4e : init invalid.");
        return 0;
    }
    d = 60.857143f;
    ci = ClothTexSetUp(tpl, &wk->tex, 0, &wk->tlut);
    nx = (int) (m_Size_base_x / 200.0f * 36.0f);
    ny = (int) (m_Size_base_y / 200.0f * 24.0f);
    width = pSeq->Vec0.x * 0.1f + 1.0f;
    height = pSeq->Vec0.y * 0.1f + 1.0f;
    if (width == 0.0f) {
        width = 0.001f;
    }
    if (height == 0.0f) {
        height = 0.001f;
    }
    t = pSeq->Tool_flg & 1;
    flag = t == 0;
    if (nx < 2) {
        nx = 2;
    }
    if (ny < 2) {
        ny = 2;
    }
    if (nx > 100) {
        nx = 100;
    }
    if (ny > 50) {
        ny = 50;
    }
    if (ci) {
        wk->pCl->Set(m_Ang, m_Pos, nx, ny, width, &wk->tex, height * (3000.0f / d / 23.0f), NULL, d, &wk->tlut, flag);
    } else {
        wk->pCl->Set(m_Ang, m_Pos, nx, ny, width, &wk->tex, height * (3000.0f / d / 23.0f), NULL, d, NULL, flag);
    }
    if (pSeq->Blend_type) {
        wk->pCl->blendMode = 1;
    }
    wk->time_plus = pSeq->Work8[0];
    wk->pow = pSeq->Work8[1];
    wk->time_plus2 = pSeq->Work8[2];
    wk->pow2 = pSeq->Work8[3];
    wk->range = (f32) (int) (pSeq->prm.w.xCC + 1) * 0.5f;
    wk->range2 = (f32) (int) (pSeq->prm.w.xD0 + 1) * 0.5f;
    wk->offset = (f32) (int) pSeq->xD4 * 0.025f;
    wk->wind_time_plus = (f32) (pSeq->WorkSp8[0] + 1) * 0.0025f;
    wk->wind_range_pow = (f32) (pSeq->WorkSp8[1] + 1) * 0.07f;
    wk->rand_ratio = (f32) (pSeq->WorkSp8[2] + 1) * 0.2f;
    return 1;
}

asm(".section .sdata; .balign 8");
