#ifndef EMHIT_H
#define EMHIT_H

#include "types.h"
#include "vec.h"
#include "em.h"

// Work of the hit-only enemy (game/emhit.cpp), overlaid on cEm from 0x3E0.
struct EmHitWork {
    u32 Be_flg;            // 0x000 (0x3E0)
    int Timer;            // 0x004 (0x3E4)  beetle: frames before it fades out
    u8 pad_8[4];
    int Status;           // 0x00C (0x3EC)  1 = damaged this frame (ckStatus / ckDmgWeapon)
    cModel* pParent;      // 0x010 (0x3F0)  model the hit follows (setParent)
    int oya_parts;          // 0x014 (0x3F4)
    int noNormalize;      // 0x018 (0x3F8)  setParent 3rd argument: keep the parent's scale
    u8 pad_1C[0x224 - 0x1C];
    Vec spd;              // 0x224 (0x604)  beetle fly-away speed
    Vec size;             // 0x230 (0x610)  yarare box size
    void* mot0;           // 0x23C (0x61C)  beetle motions (setBeetle)
    void* mot1;           // 0x240 (0x620)
    void* mot2;           // 0x244 (0x624)
    u8 x248;              // 0x248 (0x628)  0xFF
};

#define EMHIT_WK(em) ((EmHitWork*) (((cEmHit*) (em))->free))

// Hit-only enemy: a cEm that exists to receive weapon damage for an object (bell, ...) or to
// follow a parent model's parts (setParent); type 3 is the beetle that flies off when shot.
class cEmHit : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMHIT_WK)
    virtual void move();

    int ckStatus();                                 // EmHitWork::status
    int ckDmgWeapon();                              // weapon id of the damage taken this frame, 0 = none
    void setParent(cModel* pCoord, int oya_parts, int mode);   // 0x8010539C
    void setBeetle(void* mot0, void* mot1, void* mot2);
};

cEmHit* SetEmHit(void* bin, void* tpl, Vec* pos, Vec* rot, int type);
void emHitDmCk(cEmHit* pEm);
void emHit_R0_Init(cEmHit* pEm);
void emHit_R0_Move(cEmHit* pEm);
void emHit_R1_Set(cEmHit* pEm);
void emHit_R1_Parent(cEmHit* pEm);
void emHit_R1_Break(cEmHit* pEm);
void emHit_R1_Beetle(cEmHit* pEm);
void emHitYarareInit(cEmHit* pEm);
// Hit box setup (game/at_mod.cpp): the model's own box (em->hitInfo) and extra boxes chained to it.
// The parts number / flags come last: the callers' `li` argument loads are scheduled after the
// float moves (emhit, obj14, obj15 ...).
void YarareInit(cEm* pEm, f32 x, f32 y, f32 z, f32 r, f32 h, s16 pno, u16 flag);
void YarareInitCube(cEm* em, f32 x, f32 y, f32 z, f32 w, f32 h, f32 extent, s16 no, u16 flags);
void YarareAdd(cEm* pEm, YARARE_INFO* pAt, f32 x, f32 y, f32 z, f32 r, f32 h, s16 pno, u16 flag);
void YarareAddCube(cEm* em, YARARE_INFO* box, f32 x, f32 y, f32 z, f32 w, f32 h, f32 extent, s16 no, u16 flags);
int EmGetDmPos(cEm* pEm, Vec* pPos, Vec* pAng);                                     // em_sub.cpp
void EmDmBloodSet2(cEm* pEm, u32 est_id, u32 type, u32 mode, u16 esp_core_flg, u32 core_kind);               // em_sub.cpp
int VehicleAdjust(Vec* pPos);                                                     // em_sub.cpp: rides `pos` along the trolley (room 21B)

// Player damage motion kind (PS2 PL_DM_TYPE): ATK_INFO::dm_type, PlSetDamage `type`. Bit0 back, +2 middle,
// +4 down (EmAtkSetDamagePL computes it from the hit height); AUTO picks from the hit direction.
enum PL_DM_TYPE {
    PL_DM_UP_FRONT = 0,
    PL_DM_UP_BACK = 1,
    PL_DM_MIDDLE_FRONT = 2,
    PL_DM_MIDDLE_BACK = 3,
    PL_DM_DOWN_FRONT = 4,
    PL_DM_DOWN_BACK = 5,
    PL_DM_FRONT = 6,
    PL_DM_BACK = 7,
    PL_DM_AUTO = 8,
    PL_DM_AUTO_SML = 9
};

// Attack parameters handed to EmAtkSetDamagePL (obj15 Obj15_atk_info_tbl: {100.0, 8, 600, 0, 10, 0}).
struct ATK_INFO {
    f32 radius;    // 0x00
    PL_DM_TYPE dm_type;  // 0x04
    u16 power;    // 0x08
    u16 flag;     // 0x0A  bit2: LifeDownSet2 keep, bit3: pl_life = 0
    u16 dm_cnt;     // 0x0C
    u8 esp_attr;  // 0x0E  unused by this file
    u8 padding;   // 0x0F
};

void EmPlBloodSet2(cModel* pEm, Vec* pos, u32 a, u8 eff_id, u8 type);                 // em_sub.cpp
// Line `a`-`b` against the player's hit boxes: the hit box or NULL; hit point / normal and the
// scenario attribute out.
YARARE_INFO* EmAtkLineHitCk(Vec* pPos, Vec* pPos2, Vec* pCross, Vec* pNorm, u32* pAttr);              // em_sub.cpp
void EmAtkSetDamagePL(YARARE_INFO* pAt, ATK_INFO* pAtk, Vec* pPos, Vec* pPos2);                 // em_sub.cpp

void PlSetDamage(int damage_type, int damage_val, int flag);                                   // em_sub.cpp (C++ linkage; obj10 hitCkPl)

#endif
