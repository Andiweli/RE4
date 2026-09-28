#ifndef EMSHIELD_H
#define EMSHIELD_H

#include "types.h"
#include "vec.h"
#include "em.h"
#include "emhit.h"

// Work of the shield enemy (game/emshield.cpp), overlaid on cEm from 0x3E0.
struct FREE_EMSHIELD {
    u32 Be_flg;            // 0x000 (0x3E0)  bit0: setParent flag (no matrix normalisation), bit1: hidden
    int Timer;            // 0x004 (0x3E4)
    u8 pad_8[0x1C - 0x8];  // Timer2, TmpU32, pMot, Motion_info, mot_attr on PS2, unused by this file
    int Parts_hp;           // 0x01C (0x3FC)  hits left before the next plank breaks off (Rnd() % 3 + 2)
    int Break_num;         // 0x020 (0x400)  planks broken off so far (4 = the shield is destroyed)
    int Fall_wait;        // 0x024 (0x404)  emShield_R1_Parent: frames until setFall
    cEm* pEm_oya;         // 0x028 (0x408)  model the shield follows (setParent)
    cEm* pEm_old;         // 0x02C (0x40C)  parent before setFall (landing sound owner)
    u32 oya_parts;          // 0x030 (0x410)
    u32 seid_throw;        // 0x034 (0x414)
    int At_no;             // 0x038 (0x418)  -1
    int Bomb_wait;         // 0x03C (0x41C)  unused by this file
    Vec always2_offset;           // 0x040 (0x420)  looping effect offset in parts effParts
    u8 always2_parts;          // 0x04C (0x42C)  0xFF = none
    u8 pad_4D;
    u16 always2_wait;          // 0x04E (0x42E)  frames between the looping effect restarts
    u16 always2_timer;         // 0x050 (0x430)
    u8 pad_52[2];
    f32 Gravity;          // 0x054 (0x434)  setFall first argument (20)
    Vec spd[3];           // 0x058 (0x438)  node speeds kept between frames (setFall initialises them)
    Vec throw_v;          // 0x07C (0x45C)  unused by this file
    u8 seFall[3];         // 0x088 (0x468)  blk, no, id of the landing sound (0xFF = none)
    u8 se_ck_fall;        // 0x08B (0x46B)
    u8 seHit[3];          // 0x08C (0x46C)
    u8 seDamage[3];       // 0x08F (0x46F)
    u8 seThrow[3];        // 0x092 (0x472)
    u8 throw_se_wait;     // 0x095 (0x475)  (4)
    u8 seWall[3];         // 0x096 (0x476)
    u8 effFall[2];        // 0x099 (0x479)  EstSet id / type when the fall ends (0xFF = none)
    u8 effHit[2];         // 0x09B (0x47B)
    u8 effDamage[2];      // 0x09D (0x47D)
    u8 effWater[2];       // 0x09F (0x47F)  EstSet id / type when the shield lands in water
    u8 effAlways[2];      // 0x0A1 (0x481)  looping effect (cEmShield::move)
    u8 EffKindId;         // 0x0A3 (0x483)  effect number deleted when the shield lands (50)
    u8 Water_ck;           // 0x0A4 (0x484)  landed in water
    u8 pad_A5[3];
    ATK_INFO* pAtk;      // 0x0A8 (0x488)  unused by this file
    // PS2 has 10 slots; this file only ever indexes 0..8 (9 planks, parts 2..10) and keeps the
    // body hit cube in cEm::hitInfo instead of using YarareTbl[0] for it.
    YARARE_INFO YarareTbl[9];     // 0x0AC (0x48C)  plank hit boxes (parts 2..10)
};

#define EMSHIELD_WK(em) ((FREE_EMSHIELD*) (((cEmShield*) (em))->free))

// Shield enemy: a wooden shield carried on a parent's parts (setParent) that loses planks when
// shot and falls as a three-node rope (setFall).
class cEmShield : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMSHIELD_WK)
    virtual void beginEvent(u32 flag);
    virtual void move();

    void setParent(cEm* pEm, u32 oya_parts, u32 mode);
    void setFall(Vec* pSpd, f32 gravity);
};

cEmShield* SetShield(void* bin, void* tpl, Vec* pos, Vec* rot);
void emShieldDmCk(cEmShield* pEm);
void emShield_R0_Init(cEmShield* pEm);
void emShield_R0_Move(cEmShield* pEm);
void emShield_R1_Set(cEmShield* pEm);
void emShield_R1_LostWait(cEmShield* pEm);
void emShield_R1_Lost(cEmShield* pEm);
void emShield_R1_Parent(cEmShield* pEm);
void emShield_R1_Fall(cEmShield* pEm);

#endif
