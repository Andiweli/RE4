// game/esp12.cpp: effect id 0x12, a textured ribbon trail through the effect's recent world
// positions, which live in an esp3f vector buffer.

#include "atari.h"
#include "light.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"

typedef struct tagESP12_WK {
    u32 Num;         // 0x00 number of trail points
    cEsp3f* pBuf;   // 0x04 vector buffer (esp3f)
} ESP12_WK;

// Ribbon trail: keeps the last n positions in an esp3f buffer and draws a textured strip
// through them.
class cEsp12 : public cEsp {
public:
    ESP12_WK m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
    virtual void Destruct();
};

// EspCreateTbl[0x12] factory.
cEsp* Esp12_Create()
{
    return new cEsp12;
}

// Base update/animation, then shifts the position history by one and stores the current world
// position at index 0. Never Z-culled (huge m_Radius).
void cEsp12::move()
{
    ESP12_WK* w = &m_Free;
    Vec wpos;
    Vec* dst;
    Vec* src;
    u32 i;

    if (CommonMove()) {
        if (!AnmMove()) {
            PushEsp(this);
        } else {
            m_Radius = 100000000.0f;
            if (parent == pEffParentWorld) {
                wpos = m_Pos;
            } else {
                PSMTXMultVec(parent->mat, &m_Pos, &wpos);
            }
            for (i = w->Num - 1; i != 0; i--) {
                dst = Esp3f_GetVecPtr(w->pBuf, i);
                src = Esp3f_GetVecPtr(w->pBuf, i - 1);
                *dst = *src;
            }
            *Esp3f_GetVecPtr(w->pBuf, 0) = wpos;
        }
    }
}

// EspTransTbl[0x12]: draws min(Life_time + 1, Num) history points as a strip of 2 vertices each,
// widened perpendicular to the segment and the camera direction, width interpolated head -> tail,
// in view space rotated by m_Ang.
void Esp12_Trans(cEsp12* esp)
{
    ESP12_WK* w = &esp->m_Free;
    cAnm* anm;
    Mtx inv;
    Vec camPos;
    Vec v0;
    Vec v1;
    Vec dir;
    Vec toCam;
    Vec nrm;
    Vec cross;
    Vec* p;
    Vec* next;
    u32 n;
    u32 i;
    f32 t;
    f32 tstep;
    f32 r;
    f32 wid;
    register f32 z asm("fr12");  // COMPILER-DIFF: #13
    u32 magic;                   // COMPILER-DIFF: #13 (hoisted conversion constant)

    if (esp->m_Life_time < w->Num) {
        n = esp->m_Life_time + 1;
    } else {
        n = w->Num;
    }
    if (!EspGetAnmAddr(esp->m_Tex_id, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", esp->m_Tex_id);
        return;
    }
    CameraCurrentProjection();
    EspTexSet(esp->m_Tex_id, esp->m_Ptn_no);
    esp->ChannelSet();
    GXSetBlendMode(esp->m_Blend_mode, esp->m_Src_factor, esp->m_Dst_factor, esp->m_Logic_op);
    esp->CommonStateSet();
    // The original holds the `(f32) w->n` conversion's 0x43300000 word in a callee-saved
    // register loaded at the top of this block (three refs, so local-alloc does not move the
    // single-use constant next to its store): a variable cse substitutes into the conversion,
    // kept alive by the codeless asm after GXBegin.
    magic = 0x43300000;  // COMPILER-DIFF: #13 (hoisted conversion constant)
    // Same block shape as Esp16_Trans: the zero in f12 copied into t (`lfs f12; fmr f30,f12`),
    // the dead three-load test splits the block at sched time so `lbz partsNo` and the 1.0 high
    // are scheduled after the zero load.
    z = 0.0f;  // COMPILER-DIFF: #13
    if (esp->m_Life_time + w->Num + esp->m_Tex_id == 99) {  // COMPILER-DIFF: candidate (sched block split)
        t = z;
    }
    t = z;
    tstep = 1.0f;
    if ((s8)esp->m_Parts_no >= -8 && (s8)esp->m_Parts_no <= -3) {
        pLog->err(0, 0, "ESP_12 : Parent is screen.");
        return;
    }
    PSMTXIdentity(esp->m_Mat);
    RotMatrix(esp->m_Mat, &esp->m_Ang);
    PSMTXConcat(pG->Camera.v_mat, esp->m_Mat, esp->m_Mat);
    PSMTXInverse(esp->m_Mat, inv);
    PSMTXTranspose(inv, inv);
    GXLoadNrmMtxImm(inv, 0);
    GXLoadPosMtxImm(esp->m_Mat, 0);
    GXSetCurrentMtx(0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xA, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xA, 0, 1, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    tstep = tstep / (f32)w->Num;
    camPos = pG->Camera.param.Campos;
    nrm.x = 0.0f;
    nrm.y = 0.0f;
    nrm.z = 0.0f;
    GXBegin(0x98, 0, n * 2);
    asm("" : "=m"(nrm.x) : "r"(magic));  // COMPILER-DIFF: #13 (keep-alive)
    p = Esp3f_GetVecPtr(w->pBuf, 0);
    next = NULL;
    for (i = 0; i < n; i++) {
        if (i != n - 1) {
            next = Esp3f_GetVecPtr(w->pBuf, i + 1);
            PSVECSubtract(next, p, &dir);
        }
        PSVECSubtract(p, &camPos, &toCam);
        PSVECCrossProduct(&dir, &toCam, &cross);
        if (!(cross.x == 0.0f && cross.y == 0.0f && cross.z == 0.0f)) {
#line 208 "D:/Bio4/Prog/esp12.cpp"
            VECNormalize(&cross, &nrm);
        }
        r = (f32)i / (f32)(n - 1);
        wid = (r * esp->m_Size_base_y + (1.0f - r) * esp->m_Size_base_x) * esp->m_Size_mul;
        PSVECScale(&nrm, &v0, wid);
        PSVECScale(&nrm, &v1, -wid);
        PSVECAdd(&v0, p, &v0);
        PSVECAdd(&v1, p, &v1);
        p = next;
        GXPosition3f32(v0.x, v0.y, v0.z);
        GXNormal3s8(0, 1, 0);
        GXTexCoord2f32(0.0f, t);
        t += tstep;
        GXPosition3f32(v1.x, v1.y, v1.z);
        GXNormal3s8(0, 1, 0);
        GXTexCoord2f32(1.0f, t);
    }
}

// Releases the esp3f history buffer.
void cEsp12::Destruct()
{
    cEsp* b = (cEsp*)m_Free.pBuf;

    if (b != NULL && (b->m_Be_flg & 1)) {
        PushEsp(b);
    }
}

// Num = Work8[0] + 2 (Work8[0] <= 123), allocates the esp3f buffer and fills every slot with the
// current world position; never Z-culled (m_Flg bit1). Fails when the buffer cannot be pulled.
int cEsp12::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    ESP12_WK* w = &m_Free;
    Vec wpos;
    int i;

    if (pSeq->Work8[0] > 0x7B) {
        pLog->err(0, 0, "ESP_12 : WK0 > 123.");
        return 0;
    }
    w->Num = (s8)pSeq->Work8[0] + 2;
    if (!Esp3f_Alloc(sizeof(Vec), w->Num, &w->pBuf, &info)) {
        pLog->err(0, 0, "ESP_12 : Buf alloc failed.");
        return 0;
    }
    m_Radius = 100000000.0f;
    m_Flg |= 2;
    if (parent == pEffParentWorld) {
        wpos = m_Pos;
    } else {
        PSMTXMultVec(parent->mat, &m_Pos, &wpos);
    }
    for (i = w->Num - 1; i >= 0; i--) {
        *Esp3f_GetVecPtr(w->pBuf, i) = wpos;
    }
    return 1;
}
