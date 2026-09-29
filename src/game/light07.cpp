// game/light07: light type 7, spinning directional light (D:/Bio4/Prog/light07.cpp): the direction
// is rebuilt from angles advanced by a per-frame speed (both in the work, radians).
#include "light.h"
#include "math_sub.h"

// Nothing beyond the cLight constructor.
cLight07::cLight07()
{
}

// LightFuncTbl[7]: Rno0 0 copies the colour, 1 spins the angles and sets normal = the far point
// (100000 units) in that direction.
// Rotating directional light: the direction is rebuilt from spinning angles every frame.
void Light07_Move(cLight* pLi)
{
    Vec* n = &pLi->normal;
    Vec* ang = &((LIT07_MOVE_FREE*)pLi->work)->Rot;
    Vec* spd = &((LIT07_MOVE_FREE*)pLi->work)->Speed;

    switch (pLi->Rno0) {
    case 0:
        pLi->DispCol = pLi->Col;
        pLi->Rno0 = 1;
    case 1:
        PSVECAdd(ang, spd, ang);
        ang->x = LIMIT_ANGLE(ang->x);
        ang->y = LIMIT_ANGLE(ang->y);
        ang->z = LIMIT_ANGLE(ang->z);
        n->x = cosf(ang->x) * sinf(ang->y) * 100000.0f;
        n->y = sinf(ang->x) * 100000.0f;
        n->z = cosf(ang->x) * cosf(ang->y) * 100000.0f;
        break;
    }
}
