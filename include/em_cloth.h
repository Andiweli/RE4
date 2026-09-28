#ifndef EM_CLOTH_H
#define EM_CLOTH_H

#include "types.h"
#include "pl_cloth.h"

class cModel;

// game/em_cloth.cpp: cloth / hair chain set-up of the enemy costume models (obj18 selects them
// by type) and the em2b short rope chain object.
void Em34ClothSet1(cModel* pEm, PlCloth* pCloth);
void Em34ClothMove1(cModel* pEm, PlCloth* pCloth);
void Em34ClothReset(cModel* pEm);
void Em34ClothSet2(cModel* pEm, PlCloth* pCloth);
void Em34ClothMove2(cModel* pEm, PlCloth* pCloth);
void Em18ClothSet(cModel* pEm, PlCloth* pCloth, int mode);
void Em18ClothMove(cModel* pEm, PlCloth* pCloth);
void Em37HairSet(cModel* pEm, PlCloth* pCloth);
void Em37HairMove(cModel* pEm, PlCloth* pCloth);
void Em37ClothReset(cModel* pEm);
void Em37CoatSet(cModel* pEm, PlCloth* pCloth);
void Em37CoatMove(cModel* pEm, PlCloth* pCloth);
void Em33ClothSet(cModel* pEm, PlCloth* pCloth, int mode);
void Em33ClothMove(cModel* pEm, PlCloth* pCloth);
void Em33ClothSet2(cModel* pEm, PlCloth* pCloth, int mode);
void Em33ClothMove2(cModel* pEm, PlCloth* pCloth);
void Em33ClothReset(cModel* pEm);
cObjChain* Em2bShortRopeSet(cModel* m, PlCloth* c, void* bin, void* tpl);
void Em30ClothSet1(cModel* pEm, PlCloth* pCloth);
void Em30ClothMove1(cModel* pEm, PlCloth* pCloth);
void Em30ClothSet2(cModel* pEm, PlCloth* pCloth);
void Em30ClothMove2(cModel* pEm, PlCloth* pCloth);
void Em30ClothReset(cModel* pEm);

#endif
