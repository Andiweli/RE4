// wep10 module entry: the semi-auto rifle (class in objHkSniper.cpp, routines wep/pl_rifle.cpp).
// _prolog registers Wep10_init as the WeaponInitFunc and PlRifleMove as the WeaponMoveFunc.

#include "wep_mod.h"
#include "light.h"
#include "esp.h"

void PlRifleMove(cPlayer* pl);   // wep/pl_rifle.cpp

void ObjHkSniper_init(cObj* obj);   // wep10/objHkSniper.cpp

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the cObjHkSniper as Wep->m_pWep
// (NULL is stored too), inits it on the player, installs its motions, loads the effects (archive
// 0x8 as group 0x44) and points the debug preview PlWepMot at motion 0xE.
void Wep10_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_HKSNIPER);

    if (obj == 0) {
        pLog->err(0, 0, "Wep10_init() cObjWep CREATE FAILED");
        pl->Wep->m_pWep = obj;
        return;
    }
    pl->Wep->m_pWep = obj;
    obj->init(pl);
    obj->setMotion(pl);
    EspDataLoad((u32) WEP_ARC_PTR(0x8), EFF_WEP10, 1);
    PlWepMot[0] = WEP_ARC_PTR(0xE);
    PlWepMot[1] = WEP_ARC_PTR(0xE);
    PlWepMot[2] = WEP_ARC_PTR(0xE);
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep10_init;
    WeaponMoveFunc = PlRifleMove;
    ObjInitFunc[0x30] = ObjHkSniper_init;
    OSReport("Wep10 HK-SNIPER prolog Ok\n");
}

// REL exit: frees the object constructor slot.
extern "C" void _epilog()
{
    ObjInitFunc[0x30] = 0;
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
