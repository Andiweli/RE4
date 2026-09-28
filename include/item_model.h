#ifndef ITEM_MODEL_H
#define ITEM_MODEL_H

#include "types.h"

// One registered item model: model (.bin) and texture (.tpl) addresses.
struct ItmSysWork {
    void* bin;  // 0x00
    void* tpl;  // 0x04
};

// Item model registry (game/item_model.cpp), 0x800 bytes allocated by ItemModelRoomInit.
class cItmSys {
public:
    ItmSysWork work[0x100];  // 0x00

private:
    void WorkClear();
public:
    void Init();
    int DataLoad(u32 data_addr);
    int ItmRegist(void* bin, void* tpl, u8 no);
    int GetBinAddr(u8 id, void** pBin_addr);
    int GetTplAddr(u8 id, void** pTpl_addr);
};

extern cItmSys* g_pItemModelSys;

void ItemModelInit();
void ItemModelRoomInit();
int ItemModelDataLoad(void* data);
int ItemGetBinAddr(u8 id, void** pBin_addr);
int ItemGetTplAddr(u8 id, void** pTpl_addr);
int ItemGetBinTplAddr(u8 id, void** pBin_addr, void** pTpl_addr);

#endif
