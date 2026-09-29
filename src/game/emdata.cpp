// game/emdata.cpp: enemy effect data swap. Room events that need the effect tables for other data
// release an enemy module's ".EFF" data with EspEmDataSwapPush and restore it with EspEmDataSwapPop.

#include "atari.h"
#include "light.h"
#include "em.h"
#include "db_log.h"
#include "read.h"
#include "esp.h"

// read.cpp enemy module entry (SearchEmModule); only the archive pointer is used here.
struct EmModule {
    u8 pad_0[0x84];
    void* pArc;   // 0x84
};

void* GetDataExt(void* arc, const char* tag, int no);   // game/read.cpp

// Effect owner id of enemy module `id` (enemy 0x11..0x20 -> EM10 owner 0x10, 0x2B -> 0x23, ...);
// -1 with an error for enemies without effect data.
int GetEmEffId(int em_id)
{
    int ret = -1;

    switch (em_id) {
    case 0x11 ... 0x20:
        ret = 0x10;
        break;
    case 0x2B:
        ret = 0x23;
        break;
    case 0x2F:
        ret = 0x27;
        break;
    case 3:
        ret = 4;
        break;
    case 4:
        ret = 7;
        break;
    case 0x22:
        ret = 0x1A;
        break;
    case 0x2C:
        ret = 0x24;
        break;
    case 0x2D:
        ret = 0x25;
        break;
    case 0x36:
        ret = 0x2D;
        break;
    case 0x30:
        ret = 0x28;
        break;
    case 0x31:
        ret = 0x29;
        break;
    case 0x32:
        ret = 0x2A;
        break;
    case 0x39:
        ret = 0x2F;
        break;
    default:
        pLog->err(0, 0, "GetEmEffId(): ID[%x] invalid.", em_id);
        break;
    }
    return ret;
}

// Releases (reference-counted) the effect data registered by enemy `id`'s module.
void EspEmDataSwapPush(int em_id)
{
    int eff = GetEmEffId(em_id);

    if (eff == -1) {
        pLog->err(0, 0, "EspEmDataSwapPush(): ID[%x] invalid.", em_id);
    } else {
        EspDataRelease(eff, 1, 0);
    }
}

// Re-registers enemy `id`'s effect data from the "EFF" entry of its module archive.
void EspEmDataSwapPop(int em_id)
{
    int eff = GetEmEffId(em_id);
    void* data;

    if (eff == -1) {
        pLog->err(0, 0, "EspEmDataSwapPop(): ID[%x] invalid.", em_id);
        return;
    }
    data = GetDataExt(SearchEmModule(em_id)->pData, "EFF", 0);
    if (data == 0) {
        pLog->err(0, 0, "EspEmDataSwapPop(): ID[%x] '.EFF' not found.", em_id);
        return;
    }
    EspDataLoad((u32) data, eff, 0);
}
