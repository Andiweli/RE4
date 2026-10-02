// game/light06: light type 6, linear fade (D:/Bio4/Prog/light06.cpp): the brightness rate starts at
// `start` and moves by `speed` per frame, clamped to 0..1.
#include "light.h"

// LightFuncTbl[6]: Rno0 0 loads the start rate, 1 fades and writes DispCol = Col * rate.
// Fade light: brightness rate moves from `start` by `speed` and is clamped to [0, 1].
void Light06_Move(cLight* pLi)
{
    LIT06_MOVE_FREE* w = (LIT06_MOVE_FREE*)pLi->work;

    switch (pLi->Rno0) {
    case 0:
        w->m_Fade = w->m_Start;
        pLi->Rno0 = 1;
    case 1:
        w->m_Fade += w->m_Speed;
        if (w->m_Speed > 0.0f) {
            if (w->m_Fade > 1.0f) {
                w->m_Fade = 1.0f;
            }
        } else {
            if (w->m_Fade < 0.0f) {
                w->m_Fade = 0.0f;
            }
        }
        pLi->DispCol.r = (u8)(w->m_Fade * pLi->Col.r);
        pLi->DispCol.g = (u8)(w->m_Fade * pLi->Col.g);
        pLi->DispCol.b = (u8)(w->m_Fade * pLi->Col.b);
        break;
    }
}
