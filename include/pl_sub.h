#ifndef PL_SUB_H
#define PL_SUB_H

#include "types.h"
#include "vec.h"

class cPlayer;

// game/pl_sub.cpp: player / partner helpers called from the rest of the game (the joy* helpers,
// PlSetCostume and PlChangeData are declared in player.h).
void PlSelect(int type);
void PlGachaInit();
void PlGachaMove();
int PlGachaGet();
void PlSetDamageSe(int se_no);
u32 PlGetStatus();
void PlSetCrouch();
void PlSetHand(int mode, int flag);
void SubCharSetHand(int type);
void SetPlDamage(cEm* em, void (*func)(cPlayer*));
void EndPlDamage();
void SetSubAux(void (*ft)(cEm*), void (*ftdm)(cEm*));
void SetSubBulldozer(void (*ft)(cEm*), void (*ftdm)(cEm*));
void EndSubDamage();
void SubCharInit(int type, Vec* pos, f32 ang_y);
enum SCC_MODE {
    SCC_STAY = 0,
    SCC_CHASE = 1,
    SCC_KILL = 2,
    SCC_SLEEP = 3,
    SCC_BEHIND = 4,
    SCC_AUX_MOT = 5,
    SCC_RESET = 6,
    SCC_STOP = 7
};

void SubCharCtrl(int mode, int sccf);
int SubCharCheckCtrl();
void SubCharCtrlHide(Vec* pos, int type);
void SubCharMoveTo(f32 x, f32 y, f32 z, f32 ry, int mode);
void PlSetLadder(Vec* pos, f32 ang, int level);
void PlSetNeck(int mode);
void PlEndCamera();
void PlRegistMotion(void* m0, void* m1, void* m2, void* m3, void* m4, void* m5, void* m6, void* m7,
                    void* m8, void* m9, void* m10, void* m11);
void SubCharRegistMotion(void* m0, void* m1);
void PlRegistRoomEff(struct PlRoomEff* er);
void PlReloadBullet();
void PlWaterProc(cPlayer* pEm);
void PlMotionReset();
int SubCharCheckHealing();
int SubCharMotionReset();
void PlSetEyeMode(u8 mode);
f32 PlGetDirY();
void PlRegistBoss(void* a, void* b);
int PlIsArmor();
int PlSetWhistle();
int PlGetWeaponNo();
void PlSetFace(int type);
void SubCharSetFace(int type);
void PlDataRelease();
void SetSubDamage(cEm* em, void (*ft)());

int PlSetCostume();
void PlChangeData();

#endif
