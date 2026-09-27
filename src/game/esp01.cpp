// game/esp01.cpp: effect id 0x01, a motion-trail strip that the trans re-integrates from pos0 every
// frame. EspStrip_draw_poly is the quad emitter shared with esp02.

#include "atari.h"
#include "light.h"
#include "gx.h"
#include "global.h"
#include "math_sub.h"
#include "esp.h"

#define ESP_STRIP_PTS_MAX 16

typedef struct tagESP01_WK {
    u16 Wari_num;      // 0x00 number of strip segments (15 - gen->Work8[0], clamped)
    u16 Long_num;  // 0x02 frames between two trail points (gen->Work8[1])
    u32 x4;        // 0x04
    Vec BasePos;      // 0x08 position at the time the sprite left its parent
} ESP01_WK;

// Motion trail strip: replays the speed/acceleration backwards to get the last positions and
// draws them as a textured strip.
class cEsp01 : public cEsp {
public:
    ESP01_WK m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

extern "C" {
void EspStrip01_setup(cEsp01* esp);
void esp01Trans_sub(cEsp01* esp);
}

// EspCreateTbl[0x01] factory.
cEsp* Esp01_Create()
{
    return new cEsp01;
}

// Own update: on release from the parent captures pos0 in world space; applies scale / colour
// fade / life / animation but leaves m_Pos untouched (the trail is rebuilt from the speed).
// Never Z-culled.
void cEsp01::move()
{
    ESP01_WK* w = &m_Free;

    if (parent != pEffParentWorld && m_Release_time != 0xFF && m_Release_time <= m_Life_time) {
        ApplyMatrix(parent->mat);
        w->BasePos = m_Pos;
        parent = pEffParentWorld;
    }
    if (m_Size_start_cnt <= m_Life_time) {
        m_Size_mul += m_Size_plus;
        m_Size_plus *= m_D_size_plus;
        if (m_Size_mul <= 0.0f) {
            PushEsp(this);
            return;
        }
    }
    if (ColorUpdate()) {
        if (m_Life_max != 0 && m_Life_max <= m_Life_time) {
            PushEsp(this);
            return;
        }
        m_Life_time++;
        if (!AnmMove()) {
            PushEsp(this);
            return;
        }
        m_Radius = 100000000.0f;
        m_Flg |= 2;
    }
}

// EspTransTbl[0x01]: GX setup then the strip.
extern "C" void Esp01_Trans(cEsp01* esp)
{
    EspStrip01_setup(esp);
    esp01Trans_sub(esp);
}

// Builds m_Mat = view * parent * Rot(m_Ang) * Trans(pos0), binds texture pattern, blend mode and
// the position + texcoord vertex format. Screen-mode Parts_no releases the effect.
void EspStrip01_setup(cEsp01* pEsp)
{
    ESP01_WK* w = &pEsp->m_Free;
    Mtx id;
    Mtx m;

    CameraCurrentProjection();
    if ((s8)pEsp->m_Parts_no >= -8 && (s8)pEsp->m_Parts_no <= -3) {
        pLog->err(0, 0, "ESP_STRIP : SCREEN MODE is invalid.");
        PushEsp(pEsp);
        return;
    }
    PSMTXIdentity(pEsp->m_Mat);
    RotMatrix(pEsp->m_Mat, &pEsp->m_Ang);
    TransMatrix(pEsp->m_Mat, &w->BasePos);
    PSMTXConcat(pG->Camera.v_mat, pEsp->parent->mat, m);
    PSMTXConcat(m, pEsp->m_Mat, pEsp->m_Mat);
    PSMTXIdentity(id);
    GXLoadPosMtxImm(id, 0);
    GXSetCurrentMtx(0);
    EspTexSet(pEsp->m_Tex_id, pEsp->m_Ptn_no);
    pEsp->ChannelSet();
    GXSetBlendMode(pEsp->m_Blend_mode, pEsp->m_Src_factor, pEsp->m_Dst_factor, pEsp->m_Logic_op);
    pEsp->CommonStateSet();
    GXClearVtxDesc();
    GXSetVtxDesc(0, 1);
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
}

// Computes the trail points by replaying the motion in m_Mat space (a shared 48-entry table
// when the sampled frame range fits, else per point), then builds each segment's quad
// perpendicular to the view and draws it with the texture's t split across the segments.
void esp01Trans_sub(cEsp01* pEsp)
{
    static Vec tmp_poss[48];
    static int tmp_n[ESP_STRIP_PTS_MAX];
    ESP01_WK* w = &pEsp->m_Free;
    Vec spd;
    Vec acc;
    Vec org;
    Vec pts[ESP_STRIP_PTS_MAX];
    Vec s;
    int max;
    int min;
    int i;
    int j;

    PSMTXMultVecSR(pEsp->m_Mat, &pEsp->m_Speed, &spd);
    PSMTXMultVecSR(pEsp->m_Mat, &pEsp->m_Speed_plus, &acc);
    org.x = 0.0f;
    org.y = 0.0f;
    org.z = 0.0f;
    PSMTXMultVec(pEsp->m_Mat, &org, &org);
    max = 0;
    min = 999999;
    for (i = 0; i < w->Wari_num + 1; i++) {
        int n;

        n = pEsp->m_Life_time - i * (w->Long_num + 1);
        if (n < 0) {
            n = 0;
        }
        tmp_n[i] = n;
        if (n > max) {
            max = n;
        }
        if (n < min) {
            min = n;
        }
    }
    if (max - min <= 47) {
        Vec p;

        p = org;
        tmp_poss[0] = p;
        s = spd;
        for (j = 1; j <= max; j++) {
            p.x += s.x;
            p.y += s.y;
            p.z += s.z;
            if (j >= min) {
                tmp_poss[j - min] = p;
            }
            s.x += acc.x;
            s.y += acc.y;
            s.z += acc.z;
            PSVECScale(&s, &s, pEsp->m_D_speed);
        }
        for (i = 0; i < w->Wari_num + 1; i++) {
            pts[i] = tmp_poss[tmp_n[i] - min];
        }
    } else {
        for (i = 0; i < w->Wari_num + 1; i++) {
            Vec s2;
            int n;

            s2 = spd;
            n = pEsp->m_Life_time - i * (w->Long_num + 1);
            pts[i] = org;
            if (n < 0) {
                n = 0;
            }
            for (j = 0; j < n; j++) {
                pts[i].x += s2.x;
                pts[i].y += s2.y;
                pts[i].z += s2.z;
                s2.x += acc.x;
                s2.y += acc.y;
                s2.z += acc.z;
                PSVECScale(&s2, &s2, pEsp->m_D_speed);
            }
        }
    }
    for (i = 0; i < w->Wari_num; i++) {
        Vec d;
        Vec tmp;
        Vec cross;
        Vec q[2];
        Vec v[4];
        f32 rate;
        f32 half;

        PSVECSubtract(&pts[i + 1], &pts[i], &d);
        tmp = pts[i];
        PSVECCrossProduct(&d, &tmp, &cross);
        if (cross.x == 0.0f && cross.y == 0.0f && cross.z == 0.0f) {
            continue;
        }
#line 379 "D:/Bio4/Prog/esp01.cpp"
        VECNormalize(&cross, &cross);
        rate = (f32)i / (f32)(w->Wari_num - 1);
        half = (rate * pEsp->m_Size_base_y + (1.0f - rate) * pEsp->m_Size_base_x) * pEsp->m_Size_mul;
        PSVECScale(&cross, &q[0], half);
        PSVECScale(&cross, &q[1], -half);
        if (i == 0) {
            PSVECAdd(&pts[i], &q[0], &v[0]);
            PSVECAdd(&pts[i], &q[1], &v[1]);
        } else {
            v[0] = v[2];
            v[1] = v[3];
        }
        PSVECAdd(&pts[i + 1], &q[0], &v[2]);
        PSVECAdd(&pts[i + 1], &q[1], &v[3]);
        EspStrip_draw_poly(pEsp, i, v, w->Wari_num, 0);
    }
}

// Emits segment `no` of a strip as a 4-vertex quad (v[0], v[1], v[3], v[2]) with texture slice
// no / texRepeat along t (flag 0) or s (flag 1); Tool_flg bit1 mirrors s, bit2 mirrors t.
void EspStrip_draw_poly(cEsp* esp, int no, Vec* v, u8 texRepeat, int flag)
{
    cAnm* anm;
    f32 s;
    f32 t;
    f32 sw;
    f32 tw;

    if (!EspGetAnmAddr(esp->m_Tex_id, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", esp->m_Tex_id);
        return;
    }
    if (flag) {
        if (esp->m_Tool_flg & 4) {
            sw = 1.0f / (f32)texRepeat;
            tw = 1.0f;
            s = 1.0f - 1.0f / texRepeat * no;
            t = 0.0f;
        } else {
            sw = 1.0f / (f32)texRepeat;
            tw = 1.0f;
            s = 1.0f / texRepeat * no;
            t = 0.0f;
        }
    } else {
        if (esp->m_Tool_flg & 4) {
            sw = 1.0f;
            tw = 1.0f / (f32)texRepeat;
            s = 0.0f;
            t = 1.0f - 1.0f / texRepeat * no;
        } else {
            sw = 1.0f;
            tw = 1.0f / (f32)texRepeat;
            s = 0.0f;
            t = 1.0f / texRepeat * no;
        }
    }
    GXBegin(0x80, 0, 4);
    if (flag) {
        if (esp->m_Tool_flg & 2) {
            if (esp->m_Tool_flg & 4) {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s + sw, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s + sw, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32(s, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s, t);
            } else {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s + sw, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s + sw, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32(s, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s, t + tw);
            }
        } else {
            if (esp->m_Tool_flg & 4) {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32((f32)(s16)(s + sw), t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s + sw, t + tw);
            } else {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32(s + sw, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s + sw, t);
            }
        }
    } else {
        if (esp->m_Tool_flg & 2) {
            if (esp->m_Tool_flg & 4) {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s + sw, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32(s, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s + sw, t);
            } else {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s + sw, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32(s, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s + sw, t + tw);
            }
        } else {
            if (esp->m_Tool_flg & 4) {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s + sw, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32(s + sw, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s, t);
            } else {
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[0].x, v[0].y, v[0].z);
                GXTexCoord2f32(s, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[1].x, v[1].y, v[1].z);
                GXTexCoord2f32(s + sw, t);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[3].x, v[3].y, v[3].z);
                GXTexCoord2f32(s + sw, t + tw);
                GXMatrixIndex1u8(0);
                GXPosition3f32(v[2].x, v[2].y, v[2].z);
                GXTexCoord2f32(s, t + tw);
            }
        }
    }
}

// Segment count 15 - Work8[0] (2 when Work8[0] > 12, max 15), point interval Work8[1], and the
// spawn position as pos0.
int cEsp01::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    ESP01_WK* w = &m_Free;

    w->BasePos = m_Pos;
    w->Wari_num = (s8)pSeq->Work8[0];
    w->Long_num = (s8)pSeq->Work8[1];
    if (w->Wari_num > 12) {
        w->Wari_num = 2;
    } else {
        w->Wari_num = 15 - w->Wari_num;
    }
    if (w->Wari_num > 15) {
        w->Wari_num = 15;
    }
    return 1;
}
