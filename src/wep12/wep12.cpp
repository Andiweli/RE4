// wep12 module: the Thompson, with the weapon class in wep12/objTompson.cpp and the routines in
// wep/pl_machine.cpp. Wep12_init creates the cObjTompson as the player's weapon.

#include "wep_mod.h"
#include "light.h"
#include "esp.h"

void PlMachineMove(cPlayer* pl);   // wep/pl_machine.cpp
cObjWep* equipWeapon(cPlayer* pl);

// WeaponInitFunc (cPlayer::weaponInit with the player): creates the weapon object as Wep->m_pWep,
// installs its motions, loads the muzzle-flash effects (archive 0x4 as group 0x46) and points the
// debug preview PlWepMot at the aim idles 0x1B/0x1F/0x21. (The error string still says Wep11.)
void Wep12_init(cModel* m)
{
    cPlayer* pl = (cPlayer*) m;
    cObjWep* obj = equipWeapon(pl);

    if (!VALID_PTR(obj)) {
        pLog->err(0, 0, "Wep11_init() wep model init failed.");
    } else {
        pl->Wep->m_pWep = obj;
        obj->setMotion(pl);
        EspDataLoad((u32) WEP_ARC_PTR(0x4), EFF_WEP12, 1);
        PlWepMot[0] = WEP_ARC_PTR(0x1B);
        PlWepMot[1] = WEP_ARC_PTR(0x1F);
        PlWepMot[2] = WEP_ARC_PTR(0x21);
    }
}

// Creates the cObjTompson (ObjMgr id 0x25) and inits it on the player; NULL when the work is full.
cObjWep* equipWeapon(cPlayer* pl)
{
    cObjWep* obj = (cObjWep*) ObjMgr.createBack(cObjMgr::ID_WEP_TOMPSON);

    if (obj == 0) {
        pLog->err(0, 0, "Wep12_init() cObjWep CREATE FAILED");
        return 0;
    }
    obj->init(pl);
    return obj;
}

// REL entry: registers the weapon init / move routines and the object constructor slot.
extern "C" void _prolog()
{
    WeaponInitFunc = Wep12_init;
    WeaponMoveFunc = PlMachineMove;
    ObjInitFunc[0x25] = ObjTompson_init;
    OSReport("Wep12 tompson prolog Ok\n");
}

// REL exit: frees the object constructor slot.
extern "C" void _epilog()
{
    ObjInitFunc[0x25] = 0;
}

// Target of every unresolved cross-module branch (snmakerel patches them to `bl _unresolved`).
extern "C" void _unresolved()
{
}
