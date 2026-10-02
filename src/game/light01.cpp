// game/light01: light type 1, flicker (D:/Bio4/Prog/light01.cpp). Light types are the per-frame
// move entries of LightFuncTbl (game.cpp) run by cLightMgr::move on every alive cLight; each reads
// its parameters from the light's `work` bytes set by the room light data.
#include "light.h"
#include "rnd.h"

// Nothing beyond the cLight constructor.
cLight01::cLight01()
{
}

// LightFuncTbl[1]: torches and candles: DispCol = Col plus one random offset in [-range, range)
// applied to r, g and b each frame.
// Flicker light: adds a random offset in [-range, range) to every color channel.
void Light01_Move(cLight* pLi)
{
    LIT01_MOVE_FREE* w = (LIT01_MOVE_FREE*)pLi->work;
    int r;
    int c;

    if (w->ColFlick != 0) {
        r = Rnd() % (w->ColFlick + w->ColFlick) - w->ColFlick;
    } else {
        r = 0;
    }
    c = pLi->Col.r + r;
    if (c < 0) {
        c = 0;
    } else if (c > 255) {
        c = 255;
    }
    pLi->DispCol.r = c;
    c = pLi->Col.g + r;
    if (c < 0) {
        c = 0;
    } else if (c > 255) {
        c = 255;
    }
    pLi->DispCol.g = c;
    c = pLi->Col.b + r;
    if (c < 0) {
        c = 0;
    } else if (c > 255) {
        c = 255;
    }
    pLi->DispCol.b = c;
    pLi->DispCol.a = pLi->Col.a;
}
