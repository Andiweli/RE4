// game/fade: full-screen colour fades (D:/Bio4/Prog/fade.cpp). FadeControl runs and draws the four
// fade slots every frame, with the "late" group drawn after the HUD.
#include "types.h"
#include "vec.h"
#include "gx.h"
#include "main_sub.h"
#include "main_mem.h"
#include "fade.h"

FADE_WORK Fade[4];

// Starts fade slot (no & 0x7FFFFFFF) from *start to *end over `time` frames at depth z; a negative
// `no` means "one shot" (stop drawing when done), otherwise the end colour stays on screen until
// FadeKill. late != 0 puts the fade in the late draw group.
void FadeSet(int no, GXColor* start, GXColor* end, u32 time, u32 z, int late)
{
    FADE_WORK* f = &Fade[no & 0x7FFFFFFF];
    GXColor s = *start;
    GXColor e = *end;

    if (no < 0) {
        f->flags = 1;
    } else {
        f->flags = 3;
    }
    if (late) {
        f->flags |= 4;
    }
    f->s_col = s;
    f->state = 0;
    f->e_col = e;
    f->time = time;
    f->cnt = 0;
    f->z = (f32) z;
    f->col = s;
}

// Stops and hides all four fades.
void FadeKillAll()
{
    u32 i;

    for (i = 0; i < 4; i++) {
        FadeKill(i);
    }
}

// Stops and hides fade slot no.
void FadeKill(int no)
{
    Fade[no].flags = 0;
}

// Boot init: clears the four slots.
void FadeInit()
{
    int i;
    FADE_WORK* f = Fade;

    memclr_asm(f, sizeof(Fade));
    for (i = 0; i < 4; i++, f++) {
        f->z = 0.0f;
    }
}

// Per-frame update + draw of the normal (late == 0) or late group: advances the colour
// interpolation by one frame, and when finished either keeps the end colour (flag bit 1) or clears
// the slot; every active slot is drawn with fadeDraw.
void FadeControl(int flag)
{
    int i;
    FADE_WORK* f;

    for (i = 0; i < 4; i++) {
        f = &Fade[i];
        if (flag == 0) {
            if (f->flags & 4) {
                continue;
            }
        } else {
            if (!(f->flags & 4)) {
                continue;
            }
        }
        if (f->flags & 1) {
            if (f->cnt < f->time) {
                u32 rem = f->time - f->cnt;

                f->col.r = (f->s_col.r * rem + f->e_col.r * f->cnt) / f->time;
                f->col.g = (f->s_col.g * rem + f->e_col.g * f->cnt) / f->time;
                f->col.b = (f->s_col.b * rem + f->e_col.b * f->cnt) / f->time;
                f->col.a = (f->s_col.a * rem + f->e_col.a * f->cnt) / f->time;
                f->cnt++;
            } else {
                f->col = f->e_col;
                if (f->flags & 2) {
                    f->flags &= ~1;
                } else {
                    f->flags = 0;
                }
            }
        }
        if (f->flags & 3) {
            fadeDraw(f);
        }
    }
}

// Draws the fade quad (screen-wide, between y 56 and height-56) in the slot's current colour at its
// z with alpha blending.
void fadeDraw(FADE_WORK* pF)
{
    GXRenderModeObj* rmode = &Rmode;
    Mtx44 proj;
    Mtx mv;

    GXSetBlendMode(1, 4, 5, 0);
    GXSetColorUpdate(1);
    C_MTXOrtho(proj, 0.0f, (f32) rmode->xfbHeight, 0.0f, (f32) rmode->fbWidth, 0.0f, -10000.0f);
    GXSetProjection(proj, 1);
    PSMTXIdentity(mv);
    GXLoadPosMtxImm(mv, 0);
    GXSetCurrentMtx(0);
    GXSetCullMode(0);
    GXSetZMode(1, 7, 0);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOp(0, 4);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 1, 0, 2);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
    GXBegin(0x80, 0, 4);
    GXPosition3f32(0.0f, 56.0f, pF->z);
    GXColor4u8(pF->col.r, pF->col.g, pF->col.b, pF->col.a);
    GXPosition3f32((f32) rmode->fbWidth, 56.0f, pF->z);
    GXColor4u8(pF->col.r, pF->col.g, pF->col.b, pF->col.a);
    GXPosition3f32((f32) rmode->fbWidth, rmode->xfbHeight - 56.0f + 1.0f, pF->z);
    GXColor4u8(pF->col.r, pF->col.g, pF->col.b, pF->col.a);
    GXPosition3f32(0.0f, rmode->xfbHeight - 56.0f + 1.0f, pF->z);
    GXColor4u8(pF->col.r, pF->col.g, pF->col.b, pF->col.a);
}
