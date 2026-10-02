#ifndef HERMITE_H
#define HERMITE_H

#include "types.h"

// game/hermite.cpp: 1-D cubic Hermite curve.
struct _HERMITE_1_POINT {
    f32 T;     // 0x00  key time
    f32 Q;     // 0x04  value
    f32 dQ[2]; // 0x08  tangents: dQ[0] leaving this key, dQ[1] arriving at this key
};
typedef _HERMITE_1_POINT HERMITE_1_POINT;

struct HERMITE_1_PTR {
    s32 nPoint;               // 0x00
    HERMITE_1_POINT Point[1]; // 0x04  nPoint entries
};

// Fixed 64-key curve (0x404 bytes): the event fog/focus data, the id_sys screen widget curves.
struct HERMITE_1_FIX {
    s32 nPoint;                // 0x00
    HERMITE_1_POINT Point[64]; // 0x04
};

void Hermite_1Clear(HERMITE_1_PTR* pCurve);
int Hermite_1CurveRight(HERMITE_1_PTR* pCurve, f32 frame);
int Hermite_1CurveCalc(HERMITE_1_PTR* pCurve, f32 frame, f32* pS);
void Hermite_1Scale(HERMITE_1_PTR* pScurve, f32 Hscale, f32 Vscale);
void Hermite_1Trans(HERMITE_1_PTR* pScurve, f32 Xoffset, f32 Yoffset);
void Hermite_1Reverse(HERMITE_1_PTR* pScurve);
void Hermite_1(HERMITE_1_POINT* pH0, HERMITE_1_POINT* pH1, f32 t, f32* pP);
void Hermite_1_dt(HERMITE_1_POINT* pH0, HERMITE_1_POINT* pH1, f32 t, f32* pT);

// C++ overload (Hermite_1CurveCalc__FP8Hermite1f): evaluate the curve, 0.0f when t is outside.
f32 Hermite_1CurveCalc(HERMITE_1_PTR* pCurve, f32 frame);

#endif
