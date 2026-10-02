#ifndef PL_CLOTH_H
#define PL_CLOTH_H

#include "types.h"
#include "vec.h"
#include "pendulum.h"

class cModel;

// Collision volume a cloth chain avoids (pl_cloth.cpp `*At` tables), 0x24 bytes: attached to a
// model part, sphere (p1 zero) or capsule between p0 and p1.
struct CLOTH_AT_SET {
    u16 Type;          // 0x00
    u8 P1;       // 0x02
    u8 P2;       // 0x03
    f32 Weight;        // 0x04
    f32 R;           // 0x08  radius
    Vec Ofs1;          // 0x0C
    Vec Ofs2;          // 0x18
};

// The player units pass these in this order; PlClothSet*/Move* use them as (jacket, holster, hair)
// and (skirt, hair, sweater) respectively (the original naming does not match the use).
extern CLOTH_INFO leonHair;
extern CLOTH_INFO leonJacket;
extern CLOTH_INFO leonHolster;
extern CLOTH_INFO girlHair;
extern CLOTH_INFO girlSkirt;
extern CLOTH_INFO girlSweater;

extern CLOTH_INFO luisHair;
extern CLOTH_INFO adaDress;
extern CLOTH_INFO adaHair;
extern CLOTH_INFO adaRibbon;

// Chain object (game/obj1d.cpp, obj1d.h).
class cObjChain;
cObjChain* SetChain(void* bin, void* tpl, Vec* pos, Vec* rot);

void PlClothSetLeon(cModel* pl, CLOTH_INFO* pCloth1, CLOTH_INFO* pCloth2, CLOTH_INFO* pCloth3);
void PlClothMoveLeon(cModel* pl, CLOTH_INFO* pCloth1, CLOTH_INFO* pCloth2, CLOTH_INFO* pCloth3);
void PlClothSetGirl(cModel* pl, CLOTH_INFO* pCloth1, CLOTH_INFO* pCloth2, CLOTH_INFO* pCloth3, int mode);
void PlClothMoveGirl(cModel* pl, CLOTH_INFO* pCloth1, CLOTH_INFO* pCloth2, CLOTH_INFO* pCloth3);
void PlClothSetLuis(cModel* pl, CLOTH_INFO* pCloth1);
void PlClothMoveLuis(cModel* pl, CLOTH_INFO* pCloth1);
void PlClothSetAda(cModel* pl, CLOTH_INFO* ribbon, CLOTH_INFO* dress, CLOTH_INFO* hair, int evt);
void PlClothMoveAda(cModel* pl, CLOTH_INFO* ribbon, CLOTH_INFO* dress, CLOTH_INFO* hair);
cObjChain* AdaRibbonSet(cModel* pl, CLOTH_INFO* ribbon, void* bin, void* tpl);

// game/pl_cloth.cpp: Ada's hair chain parameters (pl02 builds its costume-2 hair from them).
extern f32 adaHairMax[14];
extern f32 adaHairWindS[14];
extern f32 adaHairWindR[14];
extern CLOTH_AT_SET adaHairAt[6];

#endif
