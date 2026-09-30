// Sscrn/ss_item_draw: ordering-table primitive helpers of the sub screen (textures, 3D lines and
// tiles drawn from OT callbacks; ss_pzzl draws the case with them). A separate object after ss_item
// (its functions follow ss_item's end-of-file blocks); the real file name is unknown.
#include "types.h"
#include "global.h"
#include "gx.h"
#include "tpl.h"
#include "trans.h"
#include "trans_ot.h"
#include "camera.h"
#include "main_sub.h"

void ss_Draw_tpl(void* tpl, u32 id, int x, int y, int w, int h, int ot, int prio);
void ss_Draw_tpl_local(TEXPalette* tpl, u32 id, int x, int y, int w, int h);
void ss_Draw_line3d(Vec* a, Vec* b, u32 color, int width, int blend, int zupd, int ot, int prio);
void ss_Draw_line3d_local(Vec* a, Vec* b, Mtx mtx, u32 color, u32 blend, int zupd);
void ss_Draw_tile3d(Vec* a, Vec* b, Vec* c, Vec* d, u32 color, int x34, int blend, int ot, u16 prio);
void ss_Draw_tile3d_local(Vec* a, Vec* b, Vec* c, Vec* d, Mtx mtx, u32 color, u32 blend, int zupd);

// Ordering-table primitives of the sub screen (drawn from the OT callbacks).
struct SsTplPrim {
    void* tpl;  // 0x00
    u32 id;     // 0x04
    int x;      // 0x08
    int y;      // 0x0C
    int w;      // 0x10
    int h;      // 0x14
    u16 ot;     // 0x18
    u16 prio;   // 0x1A
};

struct SsLinePrim {
    Vec a;      // 0x00
    Vec b;      // 0x0C
    u32 color;  // 0x18
    int width;  // 0x1C
    int blend;  // 0x20
    int zupd;   // 0x24
    u16 ot;     // 0x28
    u16 prio;   // 0x2A
};

struct SsTilePrim {
    Vec a;      // 0x00
    Vec b;      // 0x0C
    Vec c;      // 0x18
    Vec d;      // 0x24
    u32 color;  // 0x30
    int x34;    // 0x34
    int blend;  // 0x38
    u16 ot;     // 0x3C
    u16 prio;   // 0x3E
};

static void ss_Draw_tpl_trans(SsTplPrim* p);
static void ss_Draw_line3d_trans(SsLinePrim* p);
static void ss_Draw_tile3d_trans(SsTilePrim* p);

// Queues a 2D texture blit (TPL image `id`, screen rect x/y/w/h) into ordering table `ot` at `prio`;
// drawn later by ss_Draw_tpl_trans.
void ss_Draw_tpl(void* tpl, u32 id, int x, int y, int w, int h, int ot, int prio)
{
    SsTplPrim* p = (SsTplPrim*) GetPrimBuff(sizeof(SsTplPrim));

    p->tpl = tpl;
    p->id = id;
    p->x = x;
    p->y = y;
    p->w = w;
    p->h = h;
    p->ot = ot;
    p->prio = prio;
    AddOtDirect(ot, p, (void (*)()) ss_Draw_tpl_trans, prio, 0x1000, 0, 0.0f);
}

// OT callback of ss_Draw_tpl.
static void ss_Draw_tpl_trans(SsTplPrim* p)
{
    ss_Draw_tpl_local((TEXPalette*) p->tpl, p->id, p->x, p->y, p->w, p->h);
}

// Draws TPL image `id` immediately with DrawTexture (CI formats 8/9 load their palette first). Every
// pointer is range checked against MRAM (0x80000000..0x82FFFFFF) because the data may still be in
// the ARAM-swapped area.
void ss_Draw_tpl_local(TEXPalette* tpl, u32 id, int x, int y, int w, int h)
{
    GXTexObj tex;
    GXTlutObj tlut;
    TEXDescriptor* d;
    TEXHeader* th;

    // main-memory range checks (the sub screen's data lives in the ARAM-swapped area)
    if ((u32) tpl - 0x80000000 > 0x02FFFFFF) {
        return;
    }
    calcTplAddr(tpl);
    d = TEXGet(tpl, id);
    if ((u32) d < 0x80000000) {
        return;
    }
    if ((u32) d > 0x82FFFFFF) {
        return;
    }
    th = d->textureHeader;
    if ((u32) th < 0x80000000) {
        return;
    }
    if ((u32) th > 0x82FFFFFF) {
        return;
    }
    if (th->format == 8 || th->format == 9) {
        CLUTHeader* ch;
        GXInitTexObjCI(&tex, th->data, th->width, th->height, th->format, 0, 0, 0, 0);
        ch = d->CLUTHeader;
        if ((u32) ch < 0x80000000) {
            return;
        }
        if ((u32) ch > 0x82FFFFFF) {
            return;
        }
        GXInitTlutObj(&tlut, ch->data, ch->format, ch->numEntries);
        GXLoadTlut(&tlut, 0);
    } else {
        GXInitTexObj(&tex, th->data, th->width, th->height, th->format, 0, 0, 0);
    }
    DrawTexture(&tex, x, y, 1, w, h);
}

// Queues a world-space line a-b (RGBA `color`, GX line width, blend 0 opaque / 1 alpha / 2 add /
// 3 additive-no-alpha, zupd = z write) into ordering table `ot` at `prio`.
void ss_Draw_line3d(Vec* a, Vec* b, u32 color, int width, int blend, int zupd, int ot, int prio)
{
    SsLinePrim* p = (SsLinePrim*) GetPrimBuff(sizeof(SsLinePrim));

    p->a = *a;
    p->b = *b;
    p->color = color;
    p->width = width;
    p->blend = blend;
    p->zupd = zupd;
    p->ot = ot;
    p->prio = prio;
    AddOtDirect(ot, p, (void (*)()) ss_Draw_line3d_trans, prio, 0x1000, 0, 0.0f);
}

// OT callback of ss_Draw_line3d: sets the line width, draws with the current camera view matrix,
// restores width 6.
static void ss_Draw_line3d_trans(SsLinePrim* p)
{
    GXSetLineWidth(p->width, 0);
    ss_Draw_line3d_local(&p->a, &p->b, pG->Camera.v_mat, p->color, p->blend, p->zupd);
    GXSetLineWidth(6, 0);
}

// Draws the line a-b now through view matrix `mtx` with the given blend mode (see ss_Draw_line3d)
// and z write flag.
void ss_Draw_line3d_local(Vec* a, Vec* b, Mtx mtx, u32 color, u32 blend, int zupd)
{
    u8 cr, cg, cb, ca;

    switch (blend) {
    case 0:
        GXSetBlendMode(1, 1, 0, 0);
        break;
    case 1:
        GXSetBlendMode(1, 4, 5, 0);
        break;
    case 2:
        GXSetBlendMode(1, 4, 1, 0);
        break;
    case 3:
        GXSetBlendMode(1, 1, 1, 0);
        break;
    }
    CameraCurrentProjection();
    GXSetCullMode(0);
    if (zupd) {
        GXSetZMode(1, 3, 0);
    } else {
        GXSetZMode(0, 3, 0);
    }
    cr = color >> 24;
    cg = color >> 16;
    cb = color >> 8;
    ca = color;
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    GXLoadPosMtxImm(mtx, 0);
    GXSetCurrentMtx(0);
    GXBegin(0xB0, 0, 2);
    GXPosition3f32(a->x, a->y, a->z);
    GXColor4u8(cr, cg, cb, ca);
    GXPosition3f32(b->x, b->y, b->z);
    GXColor4u8(cr, cg, cb, ca);
}

// Queues a world-space quad a-b-c-d (RGBA `color`, blend mode as ss_Draw_line3d) into ordering table
// `ot` at `prio`; x34 is stored but unused. ss_pzzl draws the case grid cells with it.
void ss_Draw_tile3d(Vec* a, Vec* b, Vec* c, Vec* d, u32 color, int x34, int blend, int ot, u16 prio)
{
    SsTilePrim* p = (SsTilePrim*) GetPrimBuff(sizeof(SsTilePrim));

    p->a = *a;
    p->b = *b;
    p->c = *c;
    p->d = *d;
    p->color = color;
    p->x34 = x34;
    p->blend = blend;
    p->ot = ot;
    p->prio = prio;
    AddOtDirect(ot, p, (void (*)()) ss_Draw_tile3d_trans, prio, 0x1000, 0, 0.0f);
}

// OT callback of ss_Draw_tile3d (no z write).
static void ss_Draw_tile3d_trans(SsTilePrim* p)
{
    ss_Draw_tile3d_local(&p->a, &p->b, &p->c, &p->d, pG->Camera.v_mat, p->color, p->blend, 0);
}

// Draws the quad a-b-c-d now through view matrix `mtx` with the given blend mode and z write flag.
void ss_Draw_tile3d_local(Vec* a, Vec* b, Vec* c, Vec* d, Mtx mtx, u32 color, u32 blend, int zupd)
{
    u8 cr, cg, cb, ca;

    switch (blend) {
    case 0:
        GXSetBlendMode(1, 1, 0, 0);
        break;
    case 1:
        GXSetBlendMode(1, 4, 5, 0);
        break;
    case 2:
        GXSetBlendMode(1, 4, 1, 0);
        break;
    case 3:
        GXSetBlendMode(1, 1, 1, 0);
        break;
    }
    CameraCurrentProjection();
    GXSetCullMode(0);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    if (zupd) {
        GXSetZMode(1, 3, 0);
    } else {
        GXSetZMode(0, 3, 0);
    }
    cr = color >> 24;
    cg = color >> 16;
    cb = color >> 8;
    ca = color;
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetTevOp(0, 4);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    GXLoadPosMtxImm(mtx, 0);
    GXSetCurrentMtx(0);
    GXBegin(0x98, 0, 4);
    GXPosition3f32(a->x, a->y, a->z);
    GXColor4u8(cr, cg, cb, ca);
    GXPosition3f32(b->x, b->y, b->z);
    GXColor4u8(cr, cg, cb, ca);
    GXPosition3f32(c->x, c->y, c->z);
    GXColor4u8(cr, cg, cb, ca);
    GXPosition3f32(d->x, d->y, d->z);
    GXColor4u8(cr, cg, cb, ca);
}

// The split object's .rodata is 4 bytes longer than the three pools: the next unit's .rodata (ss_main,
// vtables) starts 8-aligned in the REL.
asm(".section .rodata; .balign 8; .section .text");
