// wep09 module entry: the bolt-action rifle, with its class in objSniper.cpp and its routines in
// wep/pl_rifle.cpp.

#include "wep_mod.h"
#include "light.h"
#include "esp.h"

void PlRifleMove(cPlayer* pl);   // wep/pl_rifle.cpp
cObjWep* equipWeapon(cPlayer* pl);
void ObjSniper_init(cObj* obj);  // wep09/objSniper.cpp

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the rifle object as Wep->m_pWep,
// installs its motions, loads the effects (archive 0x8 as group 0x3D) and points the debug preview
// PlWepMot at the scope idle 0x15. (The error string still says Wep02.)
void Wep09_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep02_init() wep model init failed.");
    } else {
        pl->Wep->m_pWep = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x8), EFF_WEP09, 1);
        PlWepMot[0] = WEP_ARC_PTR(0x15);
        PlWepMot[1] = WEP_ARC_PTR(0x15);
        PlWepMot[2] = WEP_ARC_PTR(0x15);
    }
}

// Creates the cObjSniper (ObjMgr id 0x28) and inits it on the player; NULL (also stored as the
// player's weapon) when the work is full.
cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_SNIPER);

    if (obj == 0) {
        pLog->err(0, 0, "Wep09_init() cObjWep CREATE FAILED");
        pl->Wep->m_pWep = obj;
        return 0;
    }
    obj->init(pl);
    return obj;
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep09_init;
    WeaponMoveFunc = PlRifleMove;
    ObjInitFunc[0x28] = ObjSniper_init;
    OSReport("Wep09 SNIPER prolog Ok\n");
}

// REL exit: frees the object constructor slot.
extern "C" void _epilog()
{
    ObjInitFunc[0x28] = 0;
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
