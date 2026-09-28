// game/hermite: 1-D Hermite curves (D:/Bio4/Prog/hermite.cpp). A HERMITE_1_PTR is a list of points
// (T, Q, in/out tangents); the event fog/focus curves and camera paths evaluate them with
// Hermite_1CurveCalc, and the tool editors scale/translate/reverse them.
#include "types.h"
#include "hermite.h"
#include "main_mem.h"

// Empties the curve.
void Hermite_1Clear(HERMITE_1_PTR* pCurve)
{
    int i;

    for (i = 0; i < pCurve->nPoint; i++) {
        memclr_asm(&pCurve->Point[i], sizeof(HERMITE_1_POINT));
    }
    pCurve->nPoint = 0;
}

// 1 when t lies within the curve's key range.
int Hermite_1CurveRight(HERMITE_1_PTR* pCurve, f32 frame)
{
    if (pCurve == NULL || pCurve->nPoint <= 0) {
        return 0;
    }
    if (frame < pCurve->Point[0].T) {
        return 0;
    }
    if (frame > pCurve->Point[pCurve->nPoint - 1].T) {
        return 0;
    }
    return 1;
}

// Evaluates the curve at t into *out; 0 (no value) when t is outside the key range.
int Hermite_1CurveCalc(HERMITE_1_PTR* pCurve, f32 frame, f32* pS)
{
    if (pS == NULL) {
        return 0;
    }
    if (!Hermite_1CurveRight(pCurve, frame)) {
        return 0;
    }
    *pS = Hermite_1CurveCalc(pCurve, frame);
    return 1;
}

// Evaluates the curve at t (0 when no segment contains t).
f32 Hermite_1CurveCalc(HERMITE_1_PTR* pCurve, f32 frame)
{
    int num = pCurve->nPoint;
    HERMITE_1_POINT* k0 = NULL;
    HERMITE_1_POINT* k1 = NULL;
    int found = 0;
    int i;
    f32 result;

    for (i = 0; i < num - 1; i++) {
        k0 = &pCurve->Point[i];
        k1 = &pCurve->Point[i + 1];
        if (frame >= k0->T && frame <= k1->T) {
            found = 1;
            break;
        }
    }
    if (found) {
        Hermite_1(k0, k1, frame, &result);
    } else {
        result = 0.0f;
    }
    return result;
}

// Scales the curve in time (about the first key) by sx and in value by sy, adjusting tangents.
void Hermite_1Scale(HERMITE_1_PTR* pScurve, f32 Hscale, f32 Vscale)
{
    int i;
    f32 base;

    base = pScurve->Point[0].T;
    for (i = 0; i < pScurve->nPoint; i++) {
        pScurve->Point[i].T = (pScurve->Point[i].T - base) * Hscale + base;
        pScurve->Point[i].dQ[0] /= Hscale;
        pScurve->Point[i].dQ[1] /= Hscale;
    }
    base = pScurve->Point[0].Q;
    for (i = 0; i < pScurve->nPoint; i++) {
        pScurve->Point[i].Q = (pScurve->Point[i].Q - base) * Vscale + base;
        pScurve->Point[i].dQ[0] *= Vscale;
        pScurve->Point[i].dQ[1] *= Vscale;
    }
}

// Moves the curve so its first key is at (tx, ty).
void Hermite_1Trans(HERMITE_1_PTR* pScurve, f32 Xoffset, f32 Yoffset)
{
    int i;

    Xoffset -= pScurve->Point[0].T;
    for (i = 0; i < pScurve->nPoint; i++) {
        pScurve->Point[i].T += Xoffset;
    }
    Yoffset -= pScurve->Point[0].Q;
    for (i = 0; i < pScurve->nPoint; i++) {
        pScurve->Point[i].Q += Yoffset;
    }
}

// Reverses the curve in time (keys mirrored, tangents swapped and negated).
void Hermite_1Reverse(HERMITE_1_PTR* pScurve)
{
    int num = pScurve->nPoint;
    HERMITE_1_POINT* tmp = (HERMITE_1_POINT*) Debug_alloc(num * sizeof(HERMITE_1_POINT), 1);
    f32 t0, t1;
    int i;

    for (i = 0; i < num; i++) {
        tmp[i] = pScurve->Point[i];
    }
    t0 = pScurve->Point[0].T;
    t1 = pScurve->Point[num - 1].T;
    for (i = 0; i < num; i++) {
        pScurve->Point[i].T = t1 - tmp[num - 1 - i].T + t0;
        pScurve->Point[i].Q = tmp[num - 1 - i].Q;
        pScurve->Point[i].dQ[0] = -tmp[num - 1 - i].dQ[1];
        pScurve->Point[i].dQ[1] = -tmp[num - 1 - i].dQ[0];
    }
    Debug_free(tmp);
}

// Cubic Hermite interpolation between two keys at time t.
void Hermite_1(HERMITE_1_POINT* pH0, HERMITE_1_POINT* pH1, f32 t, f32* pP)
{
    f32 dt = pH1->T - pH0->T;
    f32 s = (t - pH0->T) / dt;
    f32 s2 = s * s;
    f32 s3 = s * s2;
    f32 h01 = -(s3 + s3) + 3.0f * s2;
    f32 h11 = s3 - s2;
    f32 h10 = h11 - s2 + s;
    f32 h00 = -h01 + 1.0f;

    *pP = h00 * pH0->Q + h01 * pH1->Q + dt * (h10 * pH0->dQ[0] + h11 * pH1->dQ[1]);
}

// Derivative of the Hermite segment at time t.
void Hermite_1_dt(HERMITE_1_POINT* pH0, HERMITE_1_POINT* pH1, f32 t, f32* pT)
{
    f32 dt = pH1->T - pH0->T;
    f32 s = (t - pH0->T) / dt;
    f32 s2 = s * s;
    f32 dh11 = 3.0f * s2 - 2.0f * s;
    f32 dh10 = 3.0f * s2 - 2.0f * s - 2.0f * s + 1.0f;
    f32 dh00 = dh10 + dh11 - 1.0f;
    f32 dh01 = -dh00;

    *pT = dh00 * pH0->Q + dh01 * pH1->Q + dt * (dh10 * pH0->dQ[0] + dh11 * pH1->dQ[1]);
}
