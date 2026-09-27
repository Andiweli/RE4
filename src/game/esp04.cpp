// game/esp04.cpp: effect id 0x04, a screen-space tiled texture overlay for rain sheets, dust and
// static. The sprite must be a screen sprite, and the tiling makes a scrolling m_Pos wrap seamlessly.

#include "atari.h"
#include "light.h"
#include "gx.h"
#include "rnd.h"
#include "esp.h"

typedef struct tagESP04_WK {
    Vec base_pos;      // 0x00 initial position
    u8 flag;   // 0x0C bit0: repeat horizontally, bit1: repeat vertically (gen->Work8[0])
    s8 rand_x;      // 0x0D random jitter in x (gen->Work8[1])
    s8 rand_y;      // 0x0E random jitter in y (gen->Work8[2])
    s8 a_rate;   // 0x0F alpha change per frame (gen->prm byte 0xCF)
    u8 a_wait;  // 0x10 frames before the alpha starts changing (gen->Work8[3])
} ESP04_WK;

// Screen-space tiled texture (rain / dust overlay): a 2D quad grid drawn in an orthographic
// projection; scrolls with pos and wraps around the 512x448 screen.
class cEsp04 : public cEsp {
public:
    ESP04_WK m_Free;  // 0xF8

    virtual void move();
    virtual int SetFreeWork(cEspSeqTbl* gen, u32* seed);
};

extern "C" {
void move00(cEsp04* esp);
void move10(cEsp04* esp);
}

static void (*func_tbl[])(cEsp04*) = { move00, move10 };

// EspCreateTbl[0x04] factory.
cEsp* Esp04_Create()
{
    return new cEsp04;
}

// Colour fade and life only (no speed integration), then the Rno0 step from func_tbl.
void cEsp04::move()
{
    if (ColorUpdate()) {
        if (m_Life_max != 0 && m_Life_max <= m_Life_time) {
            PushEsp(this);
        } else {
            m_Life_time++;
            func_tbl[m_Rno0](this);
        }
    }
}

// Rno0 == 0: records the spawn position as the jitter centre and moves to Rno0 1.
void move00(cEsp04* pEsp)
{
    pEsp->m_Free.base_pos = pEsp->m_Pos;
    pEsp->m_Rno0 = 1;
}

// Rno0 == 1: wraps m_Pos back into the screen by whole tiles, applies the random x/y jitter
// around base_pos, ramps the alpha by a_rate per frame after a_wait frames, and advances the
// animation (released when it ends).
void move10(cEsp04* esp)
{
    ESP04_WK* w = &esp->m_Free;
    f32 v;
    f32 x;
    f32 y;
    f32 lim;

    // Wrap the position back into the screen. The original keeps the loaded coordinate (x/y)
    // and the loop variable (v) in separate registers, which GCC's cse only does when the
    // loaded variable's last mention is later than v's (see the trailing reloads below).
    x = esp->m_Pos.x;
    v = x;
    lim = 512.0f;
    if (x >= lim) {
        while (v >= 0.0f) {
            esp->m_Pos.x = v - esp->m_Size_base_x;
            v = esp->m_Pos.x;
        }
    } else if (x < -esp->m_Size_base_x) {
        if (x < lim - esp->m_Size_base_x) {
            v = x;
            while (v < lim - esp->m_Size_base_x) {
                v += esp->m_Size_base_x;
            }
            esp->m_Pos.x = v;
        }
    }
    y = esp->m_Pos.y;
    v = y;
    lim = 448.0f;
    if (y >= lim) {
        while (v >= 0.0f) {
            esp->m_Pos.y = v - esp->m_Size_base_y;
            v = esp->m_Pos.y;
        }
    } else if (y < -esp->m_Size_base_y) {
        if (y < lim - esp->m_Size_base_x) {
            // The original's loop step lives in f0 and its `v = y` copy is issued after the
            // hoisted step/bound copies (y stays live past it, so the loop bound cannot take
            // y's f12): value pin + keep-alive.
            register f32 s asm("fr0");  // COMPILER-DIFF: #17 (FPR value pin)
            s = esp->m_Size_base_y;
            v = y;
            while (v < lim - esp->m_Size_base_x) {
                v += s;
            }
            esp->m_Pos.y = v;
            asm("" : "=m"(esp->m_Pos.y) : "f"(y));  // COMPILER-DIFF: #13 (keep-alive)
        }
    }
    x = esp->m_Pos.x;  // dead: keeps x/y alive past v for cse (see above)
    y = esp->m_Pos.y;

    if (w->rand_x != 0) {
        esp->m_Pos.x = w->base_pos.x + (Rnd() % (w->rand_x * 2)) - (f32)w->rand_x;
        if (esp->m_Pos.x < -esp->m_Size_base_x) {
            esp->m_Pos.x += esp->m_Size_base_x + 512.0f;
        }
        if (esp->m_Pos.x > esp->m_Size_base_x + 512.0f) {
            esp->m_Pos.x -= esp->m_Size_base_x + 512.0f;
        }
    }
    if (w->rand_y != 0) {
        esp->m_Pos.y = w->base_pos.y + (Rnd() % (w->rand_y * 2)) - (f32)w->rand_y;
        if (esp->m_Pos.y < -esp->m_Size_base_y) {
            esp->m_Pos.y += esp->m_Size_base_y + 448.0f;
        }
        if (esp->m_Pos.y > esp->m_Size_base_y + 448.0f) {
            esp->m_Pos.y -= esp->m_Size_base_y + 448.0f;
        }
    }

    if (w->a_wait != 0) {
        w->a_wait--;
    } else {
        if (w->a_rate > 0) {
            if ((int)esp->m_Col_a + w->a_rate > 255) {
                esp->m_Col_a = 255.0f;
            } else {
                esp->m_Col_a += (f32)w->a_rate;
            }
        } else if (w->a_rate < 0) {
            if ((int)esp->m_Col_a + w->a_rate < 0) {
                esp->m_Col_a = 0.0f;
            } else {
                esp->m_Col_a += (f32)w->a_rate;
            }
        }
    }
    if (!esp->AnmMove()) {
        PushEsp(esp);
    }
}

// EspTransTbl[0x04]: ortho projection, then draws the quad grid (542 / sx + 2 columns and / or
// 448 / sy + 2 rows when repeating, starting one tile off screen) with the whole texture per tile.
extern "C" void Esp04_Trans(cEsp04* esp)
{
    ESP04_WK* w = &esp->m_Free;
    cAnm* anm;
    Mtx44 proj;
    int sx;
    int sy;
    int nx;
    int ny;
    int i;
    int j;
    s16 x;
    s16 y;
    s16 xx;
    f32 u;
    f32 uw;
    s16 z;

    if (!EspGetAnmAddr(esp->m_Tex_id, &anm)) {
        pLog->err(0, 0, "ESP : TexId[%x] no data", esp->m_Tex_id);
        return;
    }
    z = 0;
    PSMTXIdentity(esp->m_Mat);
    C_MTXOrtho(proj, 0.0f, 448.0f, 0.0f, 512.0f, 0.0f, -100.0f);
    GXSetProjection(proj, 1);
    GXLoadPosMtxImm(esp->m_Mat, 0);
    GXSetCurrentMtx(0);
    EspTexSet(esp->m_Tex_id, esp->m_Ptn_no);
    esp->ChannelSet();
    GXSetBlendMode(esp->m_Blend_mode, esp->m_Src_factor, esp->m_Dst_factor, esp->m_Logic_op);
    esp->CommonStateSet();
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 3, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);

    sx = (int)esp->m_Size_base_x;
    sy = (int)esp->m_Size_base_y;
    u = 0.0f;
    uw = 1.0f;
    if (w->flag & 1) {
        nx = 542 / sx + 2;
        x = (s16)esp->m_Pos.x;
        if (sx != 0) {
            while (x > 0) {
                x -= sx;
            }
        }
    } else {
        nx = 1;
        x = (s16)esp->m_Pos.x;
    }
    if (w->flag & 2) {
        ny = 448 / sy + 2;
        y = (s16)esp->m_Pos.y;
        if (sy != 0) {
            while (y > 0) {
                y -= sy;
            }
        }
    } else {
        ny = 1;
        y = (s16)esp->m_Pos.y;
    }
    for (j = 0; j < ny; j++) {
        xx = x;
        for (i = 0; i < nx; i++) {
            GXBegin(0x80, 0, 4);
            GXPosition3s16(xx, y, z);
            GXTexCoord2f32(u, u);
            GXPosition3s16(xx + sx, y, z);
            GXTexCoord2f32(u + uw, u);
            GXPosition3s16(xx + sx, y + sy, z);
            GXTexCoord2f32(u + uw, u + uw);
            GXPosition3s16(xx, y + sy, z);
            GXTexCoord2f32(u, u + uw);
            xx += sx;
        }
        y += sy;
    }
}

// Repeat flags, jitter ranges, alpha delay and rate from the record; tile sizes are clamped to
// at least 0.1. Warns when the parent is not a screen layer.
int cEsp04::SetFreeWork(cEspSeqTbl* pSeq, u32* pRand_seed)
{
    ESP04_WK* w = &m_Free;

    w->flag = pSeq->Work8[0];
    w->rand_x = pSeq->Work8[1];
    w->rand_y = pSeq->Work8[2];
    w->a_wait = pSeq->Work8[3];
    w->a_rate = pSeq->prm.b.xCF;
    if (m_Size_base_x < 0.1f) {
        m_Size_base_x = 0.1f;
    }
    if (m_Size_base_y < 0.1f) {
        m_Size_base_y = 0.1f;
    }
    if (!((s8)m_Parts_no >= -8 && (s8)m_Parts_no <= -3)) {
        pLog->warn(0, 0, "ESP04: Parent is no SCREEN.");
    }
    return 1;
}
