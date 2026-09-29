#include "types.h"
#include "vec.h"
#include "gx.h"
#include "t_prim.h"
#include "camera.h"


void set_attr_common();
void set_attr_f32();

// Never called in this build. GCC 2.95 still emits the initializer templates of local aggregates
// in inline functions at parse time, and the original t_prim.o carries exactly these 0x60 bytes
// of anonymous .rodata in front of everything else. The values are the original's; the grouping
// and the body are a guess that reproduces them.
static inline void tprim_default_env(f32* view, Vec* pos, Vec* at)
{
    f32 param[8] = {0.0f, -100.0f, 2.0f, 4.0f, 8.0f, 0.0f, 10.0f, 300.0f};
    f32 v[6] = {1200.0f, 300.0f, 1.0f, 0.0f, 1.0f, 0.0f};
    f32 look[10] = {0.0f, 0.0f, 0.0f, 900.0f, 300.0f, 0.0f, -300.0f, -300.0f, 0.0f, -300.0f};

    memcpy(view, v, sizeof(v));
    pos->x = param[0];
    pos->y = look[3];
    pos->z = look[4];
    at->x = look[6];
    at->y = look[7];
    at->z = look[9];
}

f32 Vrect[6] = {0.0f, 0.0f, 512.0f, 448.0f, 0.0f, 1.0f};
// .bss order follows the first declaration of each object.
f32 Orect[4];
u8 ToolBuffer[0x100] __attribute__((aligned(32)));
int FlipMode = 0;
static MtxPtr ProjMtx;
MtxPtr ViewMtx;

// Debug primitive environment: the view rectangle and the projection / view matrices the tool
// draws with (from TutilInitDefault).
void TprimInitEnv2D3D(f32* view, MtxPtr proj, MtxPtr view_mtx)
{
    memcpy(Orect, view, sizeof(Orect));
    memcpy(Vrect, view, sizeof(Vrect));
    ProjMtx = proj;
    ViewMtx = view_mtx;
    FlipMode = 0;
}

// GX state for 3D debug lines / points: current projection, view matrix, blend mode, vertex format.
void TprimDraw3D(u32 mode)
{
    CameraCurrentProjection();
    GXSetCurrentMtx(0);
    GXLoadPosMtxImm(ViewMtx, 0);
    TprimSetBlend(mode);
    set_attr_common();
}

// Blend mode 0 opaque, 1 alpha blend, 2 additive.
void TprimSetBlend(u32 mode)
{
    static u32 bl[3][4] = {
        {0, 1, 0, 0},
        {1, 1, 1, 0},
        {1, 0, 2, 0},
    };

    if (mode <= 2) {
        GXSetBlendMode(bl[mode][0], bl[mode][1], bl[mode][2], bl[mode][3]);
        GXSetColorUpdate(1);
    }
}

// Common GX setup for the debug primitives: no culling, z test by FlipMode bit0, one colour TEV
// stage, line width 6.
void set_attr_common()
{
    GXSetCullMode(0);
    if (FlipMode & 1) {
        GXSetZMode(1, 3, 1);
    } else {
        GXSetZMode(0, 3, 0);
    }
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOp(0, 4);
    GXSetTevOrder(0, 0xFF, 0xFF, 4);
    GXSetNumChans(1);
    GXSetChanCtrl(0, 0, 0, 1, 1, 0, 2);
    GXSetLineWidth(6, 0);
    set_attr_f32();
}

// Vertex format: f32 position + RGBA8 colour, direct.
void set_attr_f32()
{
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
}
