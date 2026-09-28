#ifndef EMROCK_H
#define EMROCK_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "embarrel.h"

class cPlayer;
struct EmAtkInfo;

// Work of the rolling rock enemy (game/emrock.cpp), overlaid on cEm from 0x3E0.
struct FREE_EMROCK {
    u32 Be_flg;            // 0x000 (0x3E0)  bit0: setParent flag (no matrix normalisation), bit1: transparent mode
    int Timer;            // 0x004 (0x3E4)
    int Timer2;           // 0x008 (0x3E8)  Fall / Throw: frames before the rock stops
    u32 TmpU32;              // 0x00C (0x3EC)  Drop2: Rnd() & 1 (action button variant)
    f32 Radius;           // 0x010 (0x3F0)  collision radius (scale.x * 265 or 450)
    f32 Gravity;             // 0x014 (0x3F4)  gravity per frame
    u8 pad_18[0xC];
    int Fall_wait;        // 0x024 (0x404)
    int Bound_wait;       // 0x028 (0x408)  Roll: frames before the floor check starts
    cEm* pEm_oya;         // 0x02C (0x40C)  model the rock hangs on (setParent)
    cEm* pEm_old;         // 0x030 (0x410)  pParent at the time of setFall / setThrow
    u32 oya_parts;          // 0x034 (0x414)  parts of pParent
    u32 seid_throw;            // 0x038 (0x418)  SndCall handle of the always sound
    int Roll_wait;           // 0x03C (0x41C)  Roll: start delay
    void* Mot_wait;           // 0x040 (0x420)  Drop motions (setDropMot)
    void* Mot_drop;           // 0x044 (0x424)
    void* Mot_pldie;           // 0x048 (0x428)  player death motion (plemDropDie)
    void* Mot_subdie;           // 0x04C (0x42C)  sub character death motion (subemDropDie)
    void* Mot_plesc;           // 0x050 (0x430)  setDropMot2: player escape motion (plemDropEscape)
    void* Mot_plfind;           // 0x054 (0x434)  player find motion (plemDropFind)
    Vec spd[3];           // 0x058 (0x438)  unused by this file
    Vec throw_v;          // 0x07C (0x45C)
    u8 seFall[4];         // 0x088 (0x468)  SndCall blk / no / vol of the landing (setSeFall), 0xFF = none
    u8 fall_type;         // 0x08C (0x46C)
    u8 seHit[3];          // 0x08D (0x46D)  SndCall of the player hit (emRockAtkCk), 0xFF = none
    u8 seDamage[3];       // 0x090 (0x470)
    u8 seThrow[3];        // 0x093 (0x473)  SndCall of the flying sound, 0xFF = none
    u8 throw_se_wait;     // 0x096 (0x476)  frames between the flying sound calls (4)
    u8 seWall[3];         // 0x097 (0x477)
    u8 effFall[2];        // 0x09A (0x47A)  EstSet id / type when the rock lands (setEffFall), 0xFF = none
    u8 effHit[2];         // 0x09C (0x47C)  EmPlBloodSet2 arguments when the player is hit, 0xFF = none
    u8 effDamage[2];      // 0x09E (0x47E)
    u8 EffKindId;         // 0x0A0 (0x480)  EspPullCoreKind at creation
    u8 Catch_ck;          // 0x0A1 (0x481)
    u8 pad_A2[2];
    u32 Rock_route;         // 0x0A4 (0x484)  current EMI route point (type 6) of the rolling rock
    EmiEntry* pRoute;     // 0x0A8 (0x488)
    u8 Roll_flag;           // 0x0AC (0x48C)  Set: the roll started
    u8 First_bound;               // 0x0AD (0x48D)  Roll: room 104 flag
    u8 Act_ck;               // 0x0AE (0x48E)  Drop2 / escape: the player escaped / died
    u8 pad_AF;
    u32 Seid;           // 0x0B0 (0x490)  Roll: rolling sound handle
    void* Mot_tbl[16];      // 0x0B4 (0x494)  player motions of the roll escape (setPlMotion)
    struct EmAtkInfo* pAtk;  // 0x0F4 (0x4D4)  attack parameters of the flying rock (emRockAtkCk)
    YARARE_INFO YarareTbl[5];  // 0x0F8 (0x4D8)  unused by this file
    class cSat* pSat;     // 0x1FC (0x5DC)  scenario piece of the room 11E rock (emRockSatSet)
    class cSat* pEat;     // 0x200 (0x5E0)  unused by this file
};

#define EMROCK_WK(em) ((FREE_EMROCK*) (((cEmRock*) (em))->free))

// Rolling rock enemy (game/emrock.cpp): the boulders that chase the player, hang on a parent
// model, fall, get thrown by El Gigante or drop on the player.
class cEmRock : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMROCK_WK)
    virtual void beginEvent(u32 flag);
    virtual void move();

    void setParent(cEm* pEm, u32 oya_parts, u32 mode);
    void setFall(EmAtkInfo* atk);
    void setThrow(Vec* spd, EmAtkInfo* atk);
    void setThrow2(Vec* spd, EmAtkInfo* atk);
    void setSeFall(u8 se_id, u8 se_no, u8 em_id);
    void setEffFall(u8 eff_id, u8 est_id);
    void setEffAlways(u8 eff_id, u8 est_id);
    void setYarareCube(f32 w, f32 h, f32 d, Vec* pOfs);
    void setTransMode(int mode);
    void setPlMotion(void** pTbl);
    void setScale(f32 mag);
    void setDropMot(void* a, void* b, void* c, void* d);
    void setDropMot2(void* a, void* b, void* c, void* d, void* e, void* f, void* g);
    void setBreakR11E();
};

cEmRock* SetRock(void* bin, void* tpl, Vec* pos, Vec* rot, u8 type);

extern "C" {
void emRockDmCk(cEmRock* pEm);
void emRock_R0_Init(cEmRock* pEm);
void emRock_R1_Set(cEmRock* pEm);
void emRock_R1_Parent(cEmRock* pEm);
void emRock_R1_Fall(cEmRock* pEm);
void emRock_R1_Throw(cEmRock* pEm);
void emRock_R1_Throw2(cEmRock* pEm);
void emRock_R1_Roll(cEmRock* pEm);
void emRock_R1_Drop(cEmRock* pEm);
void emRock_R1_Drop2(cEmRock* pEm);
void plemDropEscAction(cEmRock* ptr);
void plemDropFind(cPlayer* pEm);
void plemDropEscape(cPlayer* pEm);
int emRockRollHitCk(cEmRock* pEm);
void emRockAtkScrCk(cEmRock* pEm);
int emRockSetRollRoute(cEmRock* pEm);
int emRockSetRollSpd(cEmRock* pEm);
int emRockRollStartCk(cEmRock* pEm);
void plemRockEscape(cPlayer* pEm);
int plemRockSetEscapeRoute();
int plemRockEscapeCk(cPlayer* pEm);
void plemRockEscAction(cEmRock* ptr);
void plemRockEscapeCamMove(cPlayer* pEm, f32 rate);
void plemRockEscapeCamMove2(cPlayer* pEm, int mode);
void plemRockDropDieCamMove(cEmRock* pEm);
void emRockPushCamMove(cEmRock* pEm);
void emRockPushCamMove2(cEmRock* pEm);
void emRockDropCamMove(cEmRock* em);
void emRockRunDownCk(cEmRock* pEm);
int emRockAtkCk(cEmRock* em, struct EmAtkInfo* atk, int type, f32 r);
void emRockPushCk(cEmRock* pEm, int frame);
int emRockDropHitCk(cEmRock* pEm);
int emRockDropHitCkSub(cEmRock* pEm);
int emRockDropHitCkEm2b(cEmRock* pEm);
void plemDropDie(cPlayer* pEm);
void subemDropDie();
void emRockSatClear(cEmRock* pEm);
void emRockSatSet(cEmRock* pEm);
}

#endif
