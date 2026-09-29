// game/light02: light type 2, sine pulse (D:/Bio4/Prog/light02.cpp): brightness rate = base + amp *
// sin(phase), phase advancing freq cycles per second (30 fps).
#include "light.h"
#include "math_sub.h"

// LightFuncTbl[2]: pulsing light; DispCol = Col * rate clamped to 0..255.
// Pulsing light: brightness rate = base + amp * sin(phase), clamped to [0, 255] per channel.
void Light02_Move(cLight* pLi)
{
    LIT02_MOVE_FREE* w = (LIT02_MOVE_FREE*)pLi->work;
    f32 rate;
    f32 r;
    f32 g;
    f32 b;

    if (w->Speed <= 0.0f) {
        w->Speed = 0.00001f;
    }
    w->Radian += w->Speed * 6.2831855f / 30.0f;
    w->Radian = LIMIT_ANGLE(w->Radian);
    rate = w->Center + w->Range * sinf(w->Radian);

    r = rate * pLi->Col.r;
    if (r < 0.0f) {
        r = 0.0f;
    } else if (r > 255.0f) {
        r = 255.0f;
    }
    pLi->DispCol.r = (u8)r;
    g = rate * pLi->Col.g;
    if (g < 0.0f) {
        g = 0.0f;
    } else if (g > 255.0f) {
        g = 255.0f;
    }
    pLi->DispCol.g = (u8)g;
    b = rate * pLi->Col.b;
    if (b < 0.0f) {
        b = 0.0f;
    } else if (b > 255.0f) {
        b = 255.0f;
    }
    pLi->DispCol.b = (u8)b;
    pLi->DispCol.a = pLi->Col.a;
}
