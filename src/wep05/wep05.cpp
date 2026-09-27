// wep05 module entry: the civilian handgun (class in objCivilian.cpp, routines wep/pl_handgun.cpp).
// _prolog registers Wep05_init and PlHandgunMove as the module's WeaponInitFunc and WeaponMoveFunc.

#include "wep_mod.h"
#include "light.h"
#include "esp.h"

void PlHandgunMove(cPlayer* pl);   // wep/pl_handgun.cpp
void ObjCivilian_init(cObj* obj);  // wep05/objCivilian.cpp

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the cObjCivilian as Wep->m_pWep,
// inits it on the player, installs its motions, loads the muzzle-flash effects (archive 0x4 as
// group 0x39) and points the debug preview PlWepMot at the aim motions 0x26..0x28.
void Wep05_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_CIVILIAN);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep05_init() wep model init failed.");
        return;
    }
    pl->Wep->m_pWep = obj;
    obj->init(pl);
    obj->setMotion(pl);
    EspDataLoad((u32) WEP_ARC_PTR(0x4), EFF_WEP05, 1);
    PlWepMot[0] = WEP_ARC_PTR(0x26);
    PlWepMot[1] = WEP_ARC_PTR(0x27);
    PlWepMot[2] = WEP_ARC_PTR(0x28);
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep05_init;
    WeaponMoveFunc = PlHandgunMove;
    ObjInitFunc[0x2E] = ObjCivilian_init;
    OSReport("Wep05 CIVILIAN prolog Ok\n");
}

// REL exit: frees the object constructor slot.
extern "C" void _epilog()
{
    ObjInitFunc[0x2E] = 0;
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
