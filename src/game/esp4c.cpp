// game/esp4c.cpp: effect id 0x4C, the invisible controller for est generator 45 (Espgen45, weather
// particles around the camera). It feeds its parameters through the Estgen45Set* interface.

#include "atari.h"
#include "light.h"
#include "esp.h"
#include "espgen.h"

// Weather (est generator 45) controller: pushes its color/size into the generator every frame.
class cEsp4c : public cEsp {
public:
    ESP4C_WK m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
    virtual void Destruct();
};

// EspCreateTbl[0x4C] factory.
cEsp* Esp4c_Create()
{
    return new cEsp4c;
}

// Runs the base update but restores the colour (the generator does its own fading), then pushes
// target position (m_Pos x/z; y only with Tool_flg bit1), size, colour + fade steps and, with
// Tool_flg bit0, the ESP4C_WK parameter block into Espgen45.
void cEsp4c::move()
{
    ESP4C_WK* w = &m_Free;
    f32 r = m_Col_r;
    f32 g = m_Col_g;
    f32 b = m_Col_b;
    f32 a = m_Col_a;

    if (CommonMove()) {
        m_Col_r = r;
        m_Col_g = g;
        m_Col_b = b;
        m_Col_a = a;
        Estgen45SetTargetCamera(0);
        Estgen45SetTargetPos(m_Pos.x, m_Pos.z);
        if (m_Tool_flg & 2) {
            Estgen45SetTargetHeight(1);
            Estgen45SetHeight(m_Pos.y);
        } else {
            Estgen45SetTargetHeight(0);
        }
        Estgen45SetSizeOverWrite(1);
        Estgen45SetSize(m_Size_base_x * m_Size_mul);
        Estgen45SetColorMul(1);
        Estgen45SetColor((u8)m_Col_r, (u8)m_Col_g, (u8)m_Col_b, (u8)m_Col_a, m_Col_d_r, m_Col_d_g, m_Col_d_b, m_Col_d_a);
        if (m_Tool_flg & 1) {
            Estgen45SetColorOverWrite(1);
            Estgen45SetParamOverWrite(1);
            Estgen45SetParam(w);
        } else {
            Estgen45SetColorOverWrite(0);
            Estgen45SetParamOverWrite(0);
        }
    }
}

// Nothing to release.
void cEsp4c::Destruct()
{
}

// EspTransTbl[0x4C]: draws nothing.
void Esp4c_Trans()
{
}

// Fills ESP4C_WK from the record: Type Work8[0] (2 = spread/damp from Work8[1..2]), specular
// texture Tex_id, shimmer powers prm 0xCE/0xD2, reflection type Work8[3], angles (degrees ->
// radians), mask texture when Tool_flg 0x4000; then applies the first frame at once.
int cEsp4c::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    ESP4C_WK* w = &m_Free;

    w->wave_ratio_base = pSeq->WorkSp8[2];
    w->Type = pSeq->Work8[0];
    if (w->Type == 2) {
        w->Prm_a = 0.5f - (f32)(s8)pSeq->Work8[1] * 0.005f;
        if (w->Prm_a > 0.5f) {
            w->Prm_a = 0.5f;
        }
        if (w->Prm_a < 0.0f) {
            w->Prm_a = 0.0f;
        }
        w->Prm_dmp = 0.99f - pSeq->Work8[2] * 0.001f;
    }
    w->Spec_Tex = pSeq->Tex_id;
    w->Shimmer_pow1 = pSeq->prm.h.xCE;
    w->Shimmer_pow2 = pSeq->prm.h.xD2;
    w->Refrect_type = pSeq->Work8[3];
    w->ang = pSeq->Ang;
    PSVECScale(&w->ang, &w->ang, 3.14 / 180);
    if (pSeq->Tool_flg & 0x4000) {
        w->flag |= 2;
        w->Mask_Tex = pSeq->MaskTex_id;
        w->flag |= 1;
    }
    move();
    return 1;
}
