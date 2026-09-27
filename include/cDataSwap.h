#ifndef CDATASWAP_H
#define CDATASWAP_H

#include "types.h"

// Parks a memory range (MRAM heap or ARAM) so heap 11 can reuse it, and restores it later
// (game/cDataSwap.cpp; used by card/mercenaries/sce_com).
class cDataSwap {
private:
    u32 m_be_flag;  // 0x00  1: copy in MRAM (mram), 2: copy in ARAM (aram)
    u32 m_SwapMaddr;  // 0x04  range start (heap 11 start)
    u32 m_SwapAaddr;  // 0x08  ARAM copy address
    u32 m_SwapSize;  // 0x0C
    u32 m_CurHeapNo;  // 0x10  heap that was current at SwapOut
    void* m_alloc_addr;// 0x14  MRAM copy (mem_alloc)

public:
    cDataSwap();
    ~cDataSwap();
    int SwapOut(u32 maddr, u32 size, u32 aaddr);
    void SwapIn();
};

#endif
