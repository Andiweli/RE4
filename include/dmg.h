#ifndef DMG_H
#define DMG_H

#include "types.h"
#include "vec.h"
#include "cManager.h"
#include "main_mem.h"

// Damage volume kind (PS2 DMG_TYPE): cDmgMgr::set `type` / cDmg::kind, returned by hitCheck.
enum DMG_TYPE {
    DMG_TYPE_NO_HIT = 0,
    DMG_TYPE_FIRE = 1,
    DMG_TYPE_GRENADE_BLAST = 2,
    DMG_TYPE_PUSH = 3,
    DMG_TYPE_FLAME = 4,
    DMG_TYPE_LAMP = 5,
    DMG_TYPE_ENV_LIGHT = 6,
    DMG_TYPE_ENV_FIRE = 7,
    DMG_TYPE_GRENADE = 8,
    DMG_TYPE_GIRL = 9
};

// Damage volume (game/dmg.cpp): a cylinder (id 0) or an XZ quad (id 1) that hurts whatever
// stands in it for `timer` frames.
class cDmg : public cUnit {
public:
    u32 m_Id;      // 0x0C  construct id (cDmgMgr::ID): ID_CYLINDER, ID_POINT4
    DMG_TYPE m_Type;    // 0x10  returned by hitCheck (FIRE/FLAME/LAMP/ENV_FIRE break the item enemies)
    int m_Time;   // 0x14  frames left

    virtual ~cDmg() {}
    virtual void beginEvent(u32 flag);
    virtual int hitCheck(Vec* pPos, Vec* pFrom) = 0;
};

class cDmgCyl : public cDmg {
public:
    Vec m_Pos;  // 0x18
    f32 m_Radius;    // 0x24
    f32 m_Height;    // 0x28  half height

    virtual int hitCheck(Vec* pPos, Vec* pFrom);
};

class cDmgP4 : public cDmg {
public:
    Vec m_Pos[4];  // 0x18
    f32 m_Height;      // 0x48

    virtual int hitCheck(Vec* pPos, Vec* pFrom);
};

// Damage volume manager (game/dmg.cpp `DmgMgr`, 0x34 bytes, work size 0x118).
#line 95 "D:/Bio4/Prog/dmg.h"
class cDmgMgr : public cManager<cDmg> {
public:
    cDmgMgr();
    virtual void* memAlloc(u32 size) { return MEM_ALLOC(size, 1, 13); }
    virtual void memFree(void* p) { Mem_free(p); }
    virtual void memClear(cDmg* p, u32 size) { memclr_asm(p, size); }
    // Construct ids (PS2 cDmgMgr::ID): cDmg::m_Id.
    enum ID {
        ID_CYLINDER = 0,
        ID_POINT4 = 1
    };

    virtual int construct(cDmg* pSat, u32 room_no);
    int construct(cDmg* pSat, int room_no);

    void move();
    // Registers a cylinder volume: kind, frames, centre, radius, half height. Returns 1 when a
    // volume was created (int result: the call's set of r3 changes the haifa depend counts, obj10 dmgSet).
    int set(int type, int time, Vec* pPos, f32 radius, f32 height);
    // Registers a quad volume: kind, frames, the 4 XZ corners, half height.
    int set(int type, int time, Vec* pPos4, f32 height);
    // Damage volume containing `pos`: its kind, 0 when none; `out` gets the volume's centre.
    int hitCheck(Vec* pPos, Vec* pFrom);
};

extern cDmgMgr DmgMgr;

#endif
