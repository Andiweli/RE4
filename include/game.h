#ifndef GAME_H
#define GAME_H

#include "types.h"

// game/game.cpp: save data front end.

// Copy of the GLOBAL_WK tail (0x4F80 .. 0x8678) kept in the save data (cGameSave::save / load).
struct GameSaveBlock {
    u8 pad_0[4];              // 0x4F80
    s32 point;                // 0x4F84  pG->point (GamePointBossReset refreshes it)
    u8 pad_8[0x36F8 - 0x8];
};

// Save data image (cGameSave::alloc). The section pointers are stored as offsets from the image
// start while it travels (calcOffset) and turned back into addresses by calcAddr; `base` is 0 in
// the offset form.
struct SAVE_DATA_HEAD {
    SAVE_DATA_HEAD* base;     // 0x00  the image's own address (0 = offsets)
    u32 size;                 // 0x04
    GameSaveBlock* pGlobal;   // 0x08  offset 0x40
    void* pItm;              // 0x0C  cItemMgr::save/load
    void* pRm;              // 0x10  cRoomData::save/load (0x3740)
    u32* pSscrn;              // 0x14  SscrnDataSave/Load
    void* pMr;          // 0x18  MerchantDataSave/Load
};

class cGameSave {
public:
    u8 pad_0;

    SAVE_DATA_HEAD* alloc();
    bool load(SAVE_DATA_HEAD* head);
    bool save(SAVE_DATA_HEAD* data, int mode);
    void checkAddr(SAVE_DATA_HEAD* head);
    void calcOffset(SAVE_DATA_HEAD* head, u32 headaddr);
    void calcAddr(SAVE_DATA_HEAD* head);
};

extern SAVE_DATA_HEAD* pSaveData;  // .sbss order: pSaveData before GameSave
extern cGameSave GameSave;

// Died demo task parameter (DiedemoExec -> gameDiedemo).
struct DIEDEMO_WORK {
    int exec_frame;   // 0x00  frames before the demo starts
    int demo_type;   // 0x04  0 normal, 1 with the sub character alive, 2 (flags_54 bit31)
};

extern "C" {
void GameTask();
void primInit();
void primFree();
void GameLoad();
void GameContinue(int option_flag);
void GamePointInit(u32 type);
enum LVADD {
    LVADD_UPDATE = 0,
    LVADD_DIE = 1,
    LVADD_PL_DAMAGE = 2,
    LVADD_PL_BIG_DAMAGE = 3,
    LVADD_MISS_HANDGUN = 4,
    LVADD_MISS_SHOTGUN = 5,
    LVADD_MISS_GRENADE = 6,
    LVADD_MISS_SNIPER = 7,
    LVADD_MISS_MACHINEGUN = 8,
    LVADD_CRITICALHIT = 9,
    LVADD_RECOVERY = 10,
    LVADD_ESCAPEATTACK = 11,
    LVADD_EM_DAMAGE = 12,
    LVADD_EM_DIE = 13,
    LVADD_TIMECOUNT = 14,
    LVADD_NUM = 15
};

void GameAddPoint(int type);
void GamePointBossReset();
void PrimDispWorkNum(int x, int y, int page);
void DiedemoExec(int time, int type);
void gameDiedemoCheck();
void gameDiedemo(DIEDEMO_WORK* pDw);
}
void GameStopModeEnd();

// game.cpp also owns the collision profile counters g_at2_cnt[20], g_at2_cyc[20], g_at2_total,
// g_at2_total_cyc (uninitialised: a header extern reorders game.cpp's .bss); atari.cpp declares them locally.

#endif
