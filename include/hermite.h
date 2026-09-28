#ifndef HERMITE_H
#define HERMITE_H

#include "types.h"

// game/hermite.cpp: 1-D cubic Hermite curve.
struct HermiteKey {
    f32 t;    // 0x00  key time
    f32 v;    // 0x04  value
    f32 out;  // 0x08  tangent leaving this key
    f32 in;   // 0x0C  tangent arriving at this key
};

struct Hermite1 {
    s32 num;             // 0x00
    HermiteKey key[1];   // 0x04  num entries
};

void Hermite_1Clear(Hermite1* pCurve);
int Hermite_1CurveRight(Hermite1* pCurve, f32 frame);
int Hermite_1CurveCalc(Hermite1* pCurve, f32 frame, f32* pS);
void Hermite_1Scale(Hermite1* pScurve, f32 Hscale, f32 Vscale);
void Hermite_1Trans(Hermite1* pScurve, f32 Xoffset, f32 Yoffset);
void Hermite_1Reverse(Hermite1* pScurve);
void Hermite_1(HermiteKey* pH0, HermiteKey* pH1, f32 t, f32* pP);
void Hermite_1_dt(HermiteKey* pH0, HermiteKey* pH1, f32 t, f32* pT);

// C++ overload (Hermite_1CurveCalc__FP8Hermite1f): evaluate the curve, 0.0f when t is outside.
f32 Hermite_1CurveCalc(Hermite1* pCurve, f32 frame);

#endif
