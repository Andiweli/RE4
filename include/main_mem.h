#ifndef MAIN_MEM_H
#define MAIN_MEM_H

#include "types.h"

// game/main_mem.cpp heap. Callers pass their source location.
void* mem_alloc(u32 size, const char* file, int line, int a, int b);

void* mem_calloc(u32 size, const char* file, int line, int a, int b);

#define MEM_ALLOC(size, a, b) mem_alloc(size, __FILE__, __LINE__, a, b)
#define MEM_CALLOC(size, a, b) mem_calloc(size, __FILE__, __LINE__, a, b)

// Address inside the console main memory (MEM1: 0x80000000 to 0x82FFFFFF).
#define VALID_PTR(p) ((u32) (p) >= 0x80000000 && (u32) (p) <= 0x82FFFFFF)
// Size / address rounded up to the 32-byte DMA and cache-line unit.
#define ALIGN32(x) (((x) + 0x1F) & ~0x1F)

// Debug heap (CurrentDbgHeap). Debug_free is the out-of-line copy owned by main_mem.
void* Debug_alloc(u32 size, int release_flag);
void Debug_free(void* addr);
// Free to a given heap (MEM_HEAP_CURRENT = the current one); datactrl calls them directly.
void Mem_free_h(void* pAddr, int heap_no);
void Debug_free_h(void* addr, int heap_no);

extern "C" {
// game/memset_2.s
void memclr_asm(void* dst, u32 n);
void memset_asm(void* dst, int c, u32 n);
}

// Dolphin OSAlloc structures (the SDK header pulls in the CodeWarrior libc).
struct OSHeapCell {
    OSHeapCell* prev;  // 0x00
    OSHeapCell* next;  // 0x04
    s32 size;          // 0x08  including this 0x20-byte header
    u8 pad_C[0x20 - 0x0C];
};

struct OSHeapDescriptor {
    s32 size;               // 0x00
    OSHeapCell* free;       // 0x04
    OSHeapCell* allocated;  // 0x08
};

// Heap table entry (main_mem.cpp `Heap[13]`).
struct HEAP_TBL {
    s32 handle;  // 0x00  OSAlloc heap handle, -1 = none
    u32 start;   // 0x04
    u32 end;     // 0x08
    u8 status;   // 0x0C  1 = suspended
    u8 pad_D[3];
};

#define MEM_HEAP_NUM 13
#define MEM_HEAP_CURRENT 13  // heap argument meaning "the current heap"

// Only Heap is declared here: GCC 2.95 lays out uninitialized globals in first-declaration order,
// and main_mem.cpp's statics sit between the others (declare arenaLo/arenaHi/CurrentHeap/
// pMemTile/heap_backup `extern` locally where needed).
extern HEAP_TBL Heap[MEM_HEAP_NUM];
extern OSHeapCell* cell_main;
extern OSHeapCell* cell_game;
extern OSHeapCell* cell_dll;
extern u32 _epy_base;

void SystemMemInit();
void memInitHeapTbl();
void MemSuspendHeap(int heap_no);
void MemSignalHeap(int heap_no);
int memGetHeapSattus(int heap_no);
int memCheckHeapActive(int heap_no);
int MemSetCurrentHeap(int heap_no);
int MemSetCurrentDbgHeap(int heap_no);
u8 MemGetCurrentHeap();
u8 MemGetCurrentDbgHeap();
u32 MemGetHeapStartAddr(int heap_no);
u32 MemGetHeapEndAddr(int heap_no);
u32 MemCheckHeapEnd(int heap_no);
int MemCreateHeap(int no, u32 start, u32 end);
int MemDestroyHeap(int heap_no);
int MemReplaceHeap(int old_heap, int new_heap);
void MemClearAllHeap();
void Mem_free(void* pAddr);
void SetDebugAlloc();
void ResetDebugAlloc();
void* MemAlloc(u32 size, int release_flag);
void MemFree(void* addr);
extern "C" void MemCheckUsedHeap();  // C linkage in the DOL (debug.cpp calls `bl MemCheckUsedHeap`)

#endif
