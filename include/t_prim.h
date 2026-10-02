#ifndef T_PRIM_H
#define T_PRIM_H

#include "types.h"
#include "vec.h"
#include "gx.h"

void TprimInitEnv2D3D(f32* view, MtxPtr proj, MtxPtr view_mtx);
void TprimDraw3D(u32 mode);
void TprimSetBlend(u32 mode);
// The tool modules link the full t_prim (tools/t_prim.cpp); the DOL link dead-stripped these.
void TprimDraw2D(u32 mode);
void TprimDrawPolyFn(Vec* v, GXColor* c, u16 n);
void TprimDrawCursor(Vec* pos, f32 z, GXColor* c);
void TprimDrawMtxDirection(Mtx mat, GXColor* c0, GXColor* c1);

// The full build (Tools REL, tools/t_prim.cpp with TPRIM_FULL): 2D-only environment, line strips, tiles,
// the hit marker (Htr) and its cone, and the s16 vertex variants.
struct S16Vec {
    s16 x, y, z;
};

void TprimInitEnv2D(f32* rect);
void TprimDrawLineFn(Vec* v, GXColor* c, u16 n);
void TprimDrawTile2D(f32* v, f32 z, GXColor* c);
void TprimDrawHtr(Vec* pos, GXColor* col);
void TprimDrawHtrCone(Vec* pos, GXColor* col);
void TprimDrawFrameFn_s16(S16Vec* v, GXColor* c, u16 n);
void TprimDrawPolyFn_s16(S16Vec* v, GXColor* c, u16 n);

#endif
