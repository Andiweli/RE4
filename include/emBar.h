#ifndef EMBAR_H
#define EMBAR_H

#include "types.h"
#include "vec.h"
#include "em.h"

// Work of the wooden bar (window board) enemy (game/emBar.cpp), overlaid on cEm from 0x3E0.
struct FREE_EMBAR {
    u32 Be_flg;            // 0x000 (0x3E0)
    int Timer;            // 0x004 (0x3E4)
    u8 pad_8[0x110 - 0x8];
    Vec size;             // 0x110 (0x4F0)  yarare box size
    void* Mot;         // 0x11C (0x4FC)  player escape motion (setMotion)
    u8 Eff_id;               // 0x120 (0x500)  break effect no, 0xFF = none (setEff)
    u8 Act_ck;          // 0x121 (0x501)  the player is climbing through
    u8 Etc_no;            // 0x122 (0x502)  etc flag that remembers the broken bar
};

#define EMBAR_WK(em) ((FREE_EMBAR*) (((cEmBar*) (em))->free))

class cEmBar : public cEm {
public:
    u8 free[0xDE0 - 0x3E0];   // 0x3E0  this class's own work (EMBAR_WK)
    virtual void move();

    void setEff(u8 eff_id);
    void setMotion(void* mot);
};

cEmBar* SetBar(void* bin, void* tpl, Vec* pos, Vec* rot, int flagNo);
void emBarDmCk(cEmBar* em);
void emBarSetBreak(cEmBar* pEm, u32 type);
void emBar_R0_Init(cEmBar* pEm);
void emBar_R0_Move(cEmBar* pEm);
void emBar_R1_Set(cEmBar* pEm);
void emBar_R1_Break(cEmBar* pEm);
void emBarActEscape(cEmBar* ptr);
void emBarYarareInit(cEmBar* pEm);
int emBarHitCk(cEmBar* pEm);

#endif
