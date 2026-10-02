// game/light08: light type 8, character-tracking spot (D:/Bio4/Prog/light08.cpp): aims the spot at
// a parts of an enemy chosen by the work (emId, partsNo).
#include "light.h"
#include "em.h"

// Nothing beyond the cLight constructor.
cLight08::cLight08()
{
}

// LightFuncTbl[8]: type 0 aims the spot target at parts partsNo of enemy emId every frame.
// Spot light that tracks an enemy model part.
void Light08_Move(cLight* pLi)
{
    LIT08_MOVE_FREE* w = (LIT08_MOVE_FREE*)pLi->work;

    if (w->type == 0) {
        cEm* em = EmMgr.getEmPtr(w->id, 0);
        if (em) {
            cParts* parts = em->getPartsPtr(w->pno);
            if (parts) {
                pLi->setSpotTarget(&parts->world);
            }
        }
    }
}
