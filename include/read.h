#ifndef READ_H
#define READ_H

#include "types.h"
#include "main_sub.h"
#include "cFlag.h"

// MODULE_DAT::ctrl_flag bits.
enum MODULE_CTRL {
    MODULE_CTRL_DLL_MALLOC = 0,    // dll copied to its own block
    MODULE_CTRL_DLL_LINK = 1,      // linked
    MODULE_CTRL_DATA_MALLOC = 2,   // data allocated by DvdReadN
    MODULE_CTRL_DATA_DMALLOC = 3   // on the debug heap
};

// A linked DLL with its data archive (game/read.cpp). The first 0x80 bytes are the module's bss area.
struct MODULE_DAT {
    u32 pBss[32];                // 0x00
    u16 id;                      // 0x80
    cFlag<u16, MODULE_CTRL> ctrl_flag;  // 0x82
    void* pData;                 // 0x84
    OSModuleHeader* pDll;        // 0x88
    u32 DataSize;                // 0x8C
    u32 DllSize;                 // 0x90  size of the part after the data (dll + bss)
    void* EmInitFunc;            // 0x94  set by the dll prolog, copied to/from game/em.cpp's EmInitFunc

    MODULE_DAT() { ctrl_flag.reset(); }
};

extern MODULE_DAT EmReadModule[4];
extern MODULE_DAT PlReadModule;
extern MODULE_DAT WepReadModule;

void CoreDataRead();
void OptionDataRead();
// Loads enemy module `id` (the rooms preload the enemies of their events); the read address
void* EmReadSearch(int id, void* data_addr, u32 malloc_size);
// Runs the module's prolog (the rooms re-link an enemy module after swapping event data into it).
void InitModule(MODULE_DAT* m);
// Room archive, player and weapon module reads / releases (game.cpp, main.cpp, the player units).
void ReadAreaData();
void ReadPlayerData(int type, int costume);
void ReleasePlData();
void ReadWepData(u32 no, u32 type);
void ReleaseWepData();
void ContinueWepData();

// Clears the enemy module list (r106 before the chapter-end event reloads them).
void EmReadInit();

// The enemy module entry of enemy `id` (the rooms swap event data into the boss module's block).
MODULE_DAT* SearchEmModule(int id);

#endif
