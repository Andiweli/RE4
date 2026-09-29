// game/light05: light type 5, path-animated brightness (D:/Bio4/Prog/light05.cpp): plays a light
// path (byte sequence 0..200 = brightness in 1/200, 0xFF = end) from the room's light path data.
#include "light.h"

// LightFuncTbl[5]: Rno0 0 binds the path (Id/Flag from the work), 1 steps it each frame and
// scales Col by value/200 into DispCol; the light is destroyed when the path ends.
// Path light: follows a light path; the path returns the brightness (0..200) or 0xFF at the end.
void Light05_Move(cLight* pLi)
{
    cLightPath* path = (cLightPath*)pLi->work;
    LIT05_MOVE_FREE* w = (LIT05_MOVE_FREE*)pLi->work;
    int v;
    f32 rate;

    switch (pLi->Rno0) {
    case 0:
        path->setPath(LightMgr.getPathPtr(w->Id), w->Flag);
        pLi->Rno0 = 1;
    case 1:
        v = path->movePath();
        if (v == 0xff) {
            LightMgr.destroy(pLi);
            return;
        }
        rate = (f32)v * 0.005f;
        pLi->DispCol.r = (u8)(rate * pLi->Col.r);
        pLi->DispCol.g = (u8)(rate * pLi->Col.g);
        pLi->DispCol.b = (u8)(rate * pLi->Col.b);
        pLi->DispCol.a = (u8)(rate * pLi->Col.a);
        break;
    }
}
