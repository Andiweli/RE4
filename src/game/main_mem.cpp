// game/main_mem: heap management over OSAlloc (D:/Bio4/Prog/main_mem.cpp), with thirteen logical
// heaps in a fixed main RAM map. operator new/delete route here.
#include "types.h"
#include "global.h"
#include "main_mem.h"
#include "db_log.h"
#include "joy.h"
#include "eprintf.h"
#include "file.h"
#include "libgpu.h"
#include "ref_access.h"
#include <string.h>
#include <stdio.h>
#include <dolphin/os/OSReboot.h>
#include <dolphin/os.h>
#include <dolphin/os/OSAlloc.h>
#include "main.h"


// Fixed memory map of the debug build.
struct SystemMemMap {
    u32 x0;         // 0x00
    u32 elf_end;    // 0x04
    u32 dvd;        // 0x08
    u32 sound;      // 0x0C
    u32 fifo;       // 0x10
    u32 xfb;        // 0x14
    u32 core;       // 0x18
    u32 option;     // 0x1C
    u32 player;     // 0x20
    u32 weapon;     // 0x24  start of the main heap
    u32 heap_end;   // 0x28
    u32 arena_lo;   // 0x2C  OSGetArenaLo() at boot
    u32 usb;        // 0x30
    u32 debug;      // 0x34
};


HEAP_TBL Heap[MEM_HEAP_NUM];
static SystemMemMap SysMem;
OSHeapDescriptor heap_backup[MEM_HEAP_NUM];  // OSAlloc descriptors of suspended heaps

OSHeapCell* cell_main = NULL;
OSHeapCell* cell_game = NULL;
static OSHeapCell* cell_stage = NULL;
OSHeapCell* cell_dll = NULL;
static u8 Dalloc_flg = 0;
u32 _epy_base = 40;

static OSHeapDescriptor* HeapHead;
u32 arenaLo;
u32 arenaHi;
u8 CurrentHeap;
static u8 CurrentDbgHeap;
void* pMemTile;
static u32 _epy;

// Zeroed allocation from the current heap.
void* operator new(unsigned int size)
{
    return mem_calloc(size, "operator new", 0, 1, MEM_HEAP_CURRENT);
}

// Zeroed array allocation from the current heap.
void* operator new[](unsigned int size)
{
    return mem_calloc(size, "operator new", 0, 1, MEM_HEAP_CURRENT);
}

// Frees to the current heap.
void operator delete(void* p)
{
    Mem_free(p);
}

// Frees to the current heap.
void operator delete[](void* p)
{
    Mem_free(p);
}

// Boot: fixes the memory map, inits OSAlloc over [weapon archive end, heap_end/arena hi), creates
// heap 0 over the whole range and allocates the reset-keep block pRK (a 0x40-byte save region
// tagged "_reset_keep_" that survives soft resets).
void SystemMemInit()
{
    SysMem.heap_end = 0x817F4000;
    SysMem.elf_end = 0x80350000;
    SysMem.dvd = 0x80370000;
    SysMem.sound = 0x803F0000;
    SysMem.fifo = 0x80460000;
    SysMem.xfb = 0x80578000;
    SysMem.core = 0x807AC000;
    SysMem.option = 0x807EC000;
    SysMem.player = 0x80904000;
    SysMem.weapon = 0x80974000;
    SysMem.usb = 0x81800000;
    SysMem.debug = 0x8181FB00;
    SysMem.arena_lo = (u32) OSGetArenaLo();
    if (SysMem.arena_lo > 0x8034FFFF) {
        OSReport("ELF size overflow\n");
#line 100 "D:/Bio4/Prog/main_mem.cpp"
        HALT();
    }
    arenaLo = SysMem.weapon;
    arenaHi = (u32) OSGetArenaHi();
    if (SysMem.heap_end > arenaHi) {
        arenaHi = SysMem.heap_end;
    }
    arenaLo = (arenaLo + 0x1F) & ~0x1F;
    arenaHi &= ~0x1F;
    HeapHead = (OSHeapDescriptor*) arenaLo;
    arenaLo = (u32) OSInitAlloc((void*) arenaLo, (void*) arenaHi, MEM_HEAP_NUM);
    OSSetArenaLo((void*) arenaLo);
    OSSetArenaHi((void*) arenaHi);
    memInitHeapTbl();
    MemCreateHeap(0, (void*) arenaLo, (void*) SysMem.heap_end);
    MemSetCurrentHeap(0);
    pMemTile = NULL;
#line 145
    pRK = (RESET_KEEP_WORK*) MEM_ALLOC(0x40, 1, MEM_HEAP_CURRENT);
    OSSetSaveRegion(pRK, (u8*) pRK + 0x40);
    if (strcmp(pRK->head, "_reset_keep_") != 0) {
        memclr_asm(pRK, 0x40);
        strcpy(pRK->head, "_reset_keep_");
        OSReport("RESET_KEEP_WORK memory clear...\n");
    }
}

// Clears the 13 heap slots and the suspend backups.
void memInitHeapTbl()
{
    int i;

    for (i = 0; i < MEM_HEAP_NUM; i++) {
        Heap[i].handle = -1;
        Heap[i].start = 0;
        Heap[i].end = 0;
        Heap[i].status = 0;
    }
    memclr_asm(heap_backup, sizeof(heap_backup));
}

// Suspends heap no: saves its OSAlloc descriptor and empties the live one (allocations from it fail).
void MemSuspendHeap(int heap_no)
{
    int h = Heap[heap_no].handle;

    if (memGetHeapSattus(heap_no) == 0 && h >= 0) {
        OSHeapDescriptor* hh = HeapHead;

        heap_backup[h] = hh[h];
        hh[h].free = NULL;
        hh[h].allocated = NULL;
        Heap[heap_no].status = 1;
    }
}

// Resumes a suspended heap from its backup.
void MemSignalHeap(int heap_no)
{
    int h = Heap[heap_no].handle;

    if (memGetHeapSattus(heap_no) == 1 && h >= 0) {
        OSHeapDescriptor* hh = HeapHead;
        OSHeapDescriptor* bk = heap_backup;

        *(OSHeapDescriptor*) (h * sizeof(OSHeapDescriptor) + (u32) hh) =
            *(OSHeapDescriptor*) (h * sizeof(OSHeapDescriptor) + (u32) bk);
        Heap[heap_no].status = 0;
    }
}

// 1 when the heap is suspended.
int memGetHeapSattus(int heap_no)
{
    return Heap[heap_no].status;
}

// 1 when the heap is not suspended.
int memCheckHeapActive(int heap_no)
{
    return memGetHeapSattus(heap_no) == 0;
}

// Makes heap no the current allocation heap (and the debug heap); 0 when it is missing/suspended.
int MemSetCurrentHeap(int heap_no)
{
    if (memCheckHeapActive(heap_no) && Heap[heap_no].handle >= 0) {
        CurrentHeap = heap_no;
        OSSetCurrentHeap(Heap[CurrentHeap].handle);
        CurrentDbgHeap = CurrentHeap;
        return 1;
    }
    return 0;
}

// Points the debug heap at the current heap (no must be active).
int MemSetCurrentDbgHeap(int heap_no)
{
    if (memCheckHeapActive(heap_no) && Heap[heap_no].handle >= 0) {
        CurrentDbgHeap = CurrentHeap;
        return 1;
    }
    return 0;
}

// Current heap number.
u8 MemGetCurrentHeap()
{
    return CurrentHeap;
}

// Current debug heap number.
u8 MemGetCurrentDbgHeap()
{
    return CurrentDbgHeap;
}

// Start address of heap no.
void* MemGetHeapStartAddr(int heap_no)
{
    return Heap[heap_no].start;
}

// End address of heap no.
void* MemGetHeapEndAddr(int heap_no)
{
    return Heap[heap_no].end;
}

// Highest address in use by heap no, or 0 when inactive.
// OPEN (-4): the original places `li r3,0` between the compare and the branch and reloads
// d->allocated for the loop init after the if/else join, where ours forwards it.
u32 MemCheckHeapEnd(int heap_no)
{
    int h = Heap[heap_no].handle;
    OSHeapDescriptor* d;
    OSHeapCell* cell;
    u32 end;

    if (!memCheckHeapActive(heap_no) || h < 0) {
        return 0;
    }
    d = HeapHead;
    d += h;
    // `d = HeapHead; d += h;` loads HeapHead straight into d, a global pseudo. The two-statement
    // then-arm keeps jump1 from hoisting `end = 0` above the branch, so the loop init reloads
    // `d->allocated` and jump2 later puts the `li r3,0` between the compare and the branch.
    if (d->allocated == NULL) {
        cell = d->free;
        end = (u32) cell;
    } else {
        end = 0;
    }
    for (cell = d->allocated; cell != NULL; cell = cell->next) {
        if (end < (u32) cell + cell->size) {
            end = (u32) cell + cell->size;
        }
    }
    return end;
}

// Creates (or recreates) heap no over [start, end).
int MemCreateHeap(int no, void* start, void* end)
{
    if (!memCheckHeapActive(no)) {
        return 0;
    }
    if (Heap[no].handle >= 0) {
        MemDestroyHeap(no);
    }
    OSReport("-- MemCreateHeap %d %08x - %08x  ", no, start, end);
    Heap[no].handle = OSCreateHeap(start, end);
    if (Heap[no].handle >= 0) {
        Heap[no].start = start;
        Heap[no].end = end;
        OSReport("succeed!!\n");
        return 1;
    }
    OSReport("failed!!\n");
    return 0;
}

// Destroys heap no (resuming it first if suspended).
int MemDestroyHeap(int heap_no)
{
    if (!memCheckHeapActive(heap_no)) {
        MemSignalHeap(heap_no);
    }
    OSReport("-- MemDestroyHeap %d  ", heap_no);
    if (Heap[heap_no].handle >= 0) {
        OSDestroyHeap(Heap[heap_no].handle);
        Heap[heap_no].handle = -1;
        OSReport("succeed!!\n");
        return 1;
    }
    OSReport("failed!!\n");
    return 0;
}

// Shrinks heap `from` to what it has in use and creates heap `to` over the freed tail (or over
// `to`'s previous range when `from` does not exist); records the current heap's allocation list
// head in cell_main/game/stage/dll for the checker. 0 on failure.
int MemReplaceHeap(int old_heap, int new_heap)
{
    u32 start;
    u32 end;
    OSHeapDescriptor* hd;  // function scope: a global pseudo, allocated after the blocks' local qtys (lis/cell r9)

    if (CurrentHeap == 0) {
        hd = HeapHead + Heap[CurrentHeap].handle;
        cell_main = hd->allocated;
    }
    if (CurrentHeap == 1) {
        hd = HeapHead + Heap[CurrentHeap].handle;
        cell_game = hd->allocated;
    }
    if (CurrentHeap == 2) {
        hd = HeapHead + Heap[CurrentHeap].handle;
        cell_stage = hd->allocated;
    }
    if (CurrentHeap == 3) {
        hd = HeapHead + Heap[CurrentHeap].handle;
        cell_dll = hd->allocated;
    }
    if (!memCheckHeapActive(old_heap)) {
        return 0;
    }
    if (!memCheckHeapActive(new_heap)) {
        return 0;
    }
    if (Heap[old_heap].handle >= 0) {
        start = MemCheckHeapEnd(old_heap);
        end = (u32) Heap[old_heap].end;
        MemDestroyHeap(old_heap);
    } else {
        start = (u32) Heap[new_heap].start;
        end = (u32) Heap[new_heap].end;
    }
    if (start == 0) {
        return 0;
    }
    return MemCreateHeap(new_heap, (void*) start, (void*) end);
}

// Destroys every heap (soft reset).
void MemClearAllHeap()
{
    u32 i;

    for (i = 0; i < MEM_HEAP_NUM; i++) {
        if (Heap[i].handle >= 0) {
            MemDestroyHeap(i);
        }
    }
}

// Allocates size (rounded to 32) from `heap` (MEM_HEAP_CURRENT = current) with a 32-byte "MAD"
// tag holding "file(line)" after the block; flag 1 logs allocation failures. NULL when size is 0,
// the heap is suspended or full.
void* mem_alloc(u32 size, const char* file, int line, int flag, int heap)
{
    char str[64] = "";
    u8* p;
    char* name;
    u8* tag;

    if (size == 0) {
        return NULL;
    }
    if (heap == MEM_HEAP_CURRENT) {
        heap = CurrentHeap;
    }
    if (!memCheckHeapActive(heap)) {
        return NULL;
    }
    size = (size + 0x1F) & ~0x1F;
    if (file == NULL) {
        p = (u8*) OSAllocFromHeap(Heap[heap].handle, size);
    } else {
        p = (u8*) OSAllocFromHeap(Heap[heap].handle, size + 0x20);
        name = strrchr(file, '/');
        if (name == NULL) {
            name = (char*) file;
        } else {
            name++;
        }
        sprintf(str, "%s(%d)", name, line);
        str[0x1B] = 0;
        if (p != NULL) {
            tag = p + size;
            tag[0] = 0;
            tag[1] = 'M';
            tag[2] = 'A';
            tag[3] = 'D';
            strcpy((char*) tag + 4, str);
        }
    }
    if (flag == 1 && p == NULL) {
        pLog->err(0, 0, "alloc[%x]:free[%x] %s", size, OSCheckHeap(Heap[heap].handle), str);
    }
    return p;
}

// mem_alloc plus zero fill.
void* mem_calloc(u32 size, const char* file, int line, int flag, int heap)
{
    void* p = mem_alloc(size, file, line, flag, heap);

    if (p != NULL) {
        memclr_asm(p, size);
    }
    return p;
}

// Frees a block to the current heap.
void Mem_free(void* pAddr)
{
    Mem_free_h(pAddr, CurrentHeap);
}

// Frees a block (to the OS current heap) when heap `heap` is active.
void Mem_free_h(void* pAddr, int heap_no)
{
    if (heap_no == MEM_HEAP_CURRENT) {
        heap_no = CurrentHeap;
    }
    if (memCheckHeapActive(heap_no)) {
        OSFreeToHeap(__OSCurrHeap, pAddr);
    }
}

// Tool mode: Debug_alloc(flag 1) blocks are tagged "toolmem" so ResetDebugAlloc can free them.
void SetDebugAlloc()
{
    Dalloc_flg = 1;
}

// The cell header fields the debug code reads (the real OSHeapCell is 0x20 bytes).
struct MemCellHead {
    OSHeapCell* prev;  // 0x00
    OSHeapCell* next;  // 0x04
    s32 size;          // 0x08
};

// Frees every "toolmem" block of the debug heap and leaves tool mode.
void ResetDebugAlloc()
{
    OSHeapCell* cell;
    MemCellHead tmp;

    Dalloc_flg = 0;
    {
        // pointer + index (the pointer is the first `add` operand, tied to HeapHead's register)
        OSHeapDescriptor* hd = HeapHead + Heap[CurrentDbgHeap].handle;
        cell = hd->allocated;
    }
    for (; cell != NULL; cell = tmp.next) {
        tmp = *(MemCellHead*) cell;
        if (strcmp((char*) cell + cell->size - 8, "toolmem") == 0) {
            Debug_free((u8*) cell + 0x20);
        }
    }
}

// Zeroed allocation from the debug heap (flag 1 in tool mode adds the "toolmem" tag).
void* Debug_alloc(u32 size, int release_flag)
{
    u8* p;

    if (size == 0) {
        return NULL;
    }
    if (!memCheckHeapActive(CurrentDbgHeap)) {
        return NULL;
    }
    if (Dalloc_flg == 1 && release_flag == 1) {
        size += 8;
    }
    size = (size + 0x1F) & ~0x1F;
    p = (u8*) OSAllocFromHeap(Heap[CurrentDbgHeap].handle, size);
    if (p != NULL) {
        memclr_asm(p, size);
        if (Dalloc_flg == 1 && release_flag == 1) {
            strcpy((char*) p + size - 8, "toolmem");
        }
    }
    return p;
}

// Frees a debug heap block.
void Debug_free(void* addr)
{
    Debug_free_h(addr, CurrentDbgHeap);
}

// Frees a debug block after wiping it (poison for stale pointers).
void Debug_free_h(void* addr, int heap_no)
{
    if (heap_no == MEM_HEAP_CURRENT) {
        heap_no = CurrentDbgHeap;
    }
    if (memCheckHeapActive(heap_no)) {
        memclr_asm(addr, ((OSHeapCell*) ((u8*) addr - 0x20))->size - 0x20);
        OSFreeToHeap(Heap[CurrentDbgHeap].handle, addr);
    }
}

// Game allocation, or a debug-heap allocation when Debug_flg[3] 0x200000 (tool memory mode).
void* MemAlloc(u32 size, int release_flag)
{
    void* p;

    if (!DbgFlagChk(pG, DBG_APP_USE_DBMEM)) {
#line 646
        p = MEM_ALLOC(size, 1, MEM_HEAP_CURRENT);
    } else {
        p = Debug_alloc(size, release_flag);
    }
    return p;
}

// Counterpart of MemAlloc.
void MemFree(void* addr)
{
    if (DbgFlagChk(pG, DBG_APP_USE_DBMEM)) {
        Debug_free(addr);
    } else {
        Mem_free(addr);
    }
}

// Debug heap display (debug page 4): one TILE per allocated cell. Matches with the same shape as
// datactrl's dispDebug. `x = 498` is a function-scope local so that reload rematerialises it before
// each `sth x0`, which frees the code constant's register.
struct MemTile {
    u32 tag;       // 0x00
    u32 code;      // 0x04
    u8 r, g, b, cd;  // 0x08
    s16 x0, y0;    // 0x0C
    s16 w, h;      // 0x10
    s16 z0;        // 0x14
    u8 pad_16[0x20 - 0x16];
};

struct SysFlagsView {
    u32 Config_flg;  // 0x00
};
extern SysFlagsView* pSysView asm("pSys");

struct DvdFreeSizeView {
    u32 freeSize;  // 0x00  cDvd::freeSize
    u8 pad_4[0x10];  // (keeps the extern out of small data)
};
extern DvdFreeSizeView DvdView asm("Dvd");


#define MEM_TAG_OK(tag) ((tag)[0] == 0 && (tag)[1] == 'M' && (tag)[2] == 'A' && (tag)[3] == 'D')

// Debug display (debug_mode 4): draws the heap map (tiles per heap) and lists the allocated cells
// with their "file(line)" tags; C-stick scrolls, Z+Y writes the list to a host file.
void MemCheckUsedHeap()
{
    char* p = NULL;
    char* buf = p;
    int full = 0;
    int write = 0;
    int rest;
    u32 end;
    u32 start;
    u32 heapEnd;
    u32 size;
    int ey;
    int cnt;
    OSHeapCell* cell;
    OSHeapCell* next;
    OSHeapDescriptor* hd;
    MemTile* mt;
    int r;
    int n;
    u32 y0;
    u32 y1;
    int x;
    static int ey_base = 30;
    static MemTile tile[2];

    x = 498;
    if (pG->debug_mode == 4 && (Joy[0].on & 0x10) && (Joy[0].trg & 0x400)) {
        write = 1;
    }
    if (write == 1) {
        buf = (char*) Debug_alloc(0x2000, 1);
        p = buf;
    }
    if (!memCheckHeapActive(CurrentHeap)) {
        return;
    }
    rest = OSCheckHeap(Heap[CurrentHeap].handle) - 0x10000;
    if (rest >= 0) {
        end = MemCheckHeapEnd(CurrentHeap);
    } else {
        end = SysMem.heap_end;
    }
    start = (u32) Heap[CurrentHeap].start;
    heapEnd = (u32) Heap[CurrentHeap].end;
    eprintf2(8, 16, 440, 404, 0, 0, "%X", rest);
    r = OSCheckHeap(Heap[CurrentDbgHeap].handle);
    if (r >= 0) {
        eprintf2(8, 16, 440, 420, 0, 0, "%X", r);
    } else {
        eprintf2(8, 16, 440, 420, 2, 0, "%X", r);
    }
    if (Joy[0].rep2 & 0x400000) {
        ey_base -= 16;
    }
    if (Joy[0].rep2 & 0x800000) {
        ey_base += 16;
    }
    if (ey_base >= -2000) {
        n = ey_base;
        if (n > 2000) {
            n = 2000;
        }
    } else {
        n = -2000;
    }
    ey_base = n;
    ey = ey_base;
    size = heapEnd - start;
    mt = (MemTile*) pMemTile;
    cnt = 0;
    hd = HeapHead + Heap[CurrentHeap].handle;
    for (cell = hd->allocated; cell != NULL; cell = cell->next) {
        y0 = (u32) ((f32) ((u32) cell - start) * 400.0f / (f32) size);
        y1 = (u32) ((f32) ((u32) cell + cell->size - start) * 400.0f / (f32) size) + 1;

        u8* tag = (u8*) cell + cell->size - 0x20;
        if ((s32) tag >= 0 || (u32) tag > 0x82FFFFFF) {
            break;
        }
        if (MEM_TAG_OK(tag)) {
            eprintf2(8, 14, 230, ey, 0, 4, "%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
        } else {
            eprintf2(8, 14, 230, ey, 0, 4, "unknown           %6x %08x", cell->size - 0x20, cell);
        }
        if (write == 1 && cell->size - 0x20 > 0x1000) {
            p += sprintf(p, "%6x %18s\n", cell->size - 0x20, tag + 4);
        }
        ey += 14;
        next = cell->next;
        if (next != NULL && next->next != NULL &&
            ((s32) next->next >= 0 || (u32) next->next > 0x82FFFFFF)) {
            pLog->err(0, 0, "heap next err:%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
            pLog->err(0, 0, "next addr    : %08x", cell->next);
#line 770
            HALT();
        }
        if (pMemTile != NULL && full == 0) {
            mt->code = 4;
            mt->x0 = x;
            mt->y0 = y0 + 30;
            mt->w = 5;
            mt->h = y1 - y0;
            mt->z0 = full;
            if (rest >= 0) {
                mt->r = 0x60;
                mt->g = 0x60;
                mt->b = 0x80;
            } else {
                mt->r = 0xFF;
                mt->g = 0x20;
                mt->b = 0x20;
            }
            mt->cd = 0xFF;
            if (CfgFlagChk(pSysView, CFG_WIDE_MODE)) {
                mt->y0 = (s16) ((f32) mt->y0 / 1.3333334f + 56.0f);
                mt->h = (s16) ((f32) mt->h / 1.3333334f);
            }
            AddPrim(&MainOt[1], (u32*) mt);
            mt++;
            if (cnt++ == 0x1FF) {
                full = 1;
            }
        }
    }
    if (pMemTile == NULL || full == 1) {
        mt = &tile[0];
        y1 = (u32) ((f32) (end - start) * 400.0f / (f32) size);
        mt->code = 4;
        mt->x0 = x;
        mt->y0 = 30;
        mt->z0 = 0;
        mt->w = 5;
        mt->h = y1;
        if (rest >= 0) {
            mt->r = 0x60;
            mt->g = 0x60;
            mt->b = 0x80;
        } else {
            mt->r = 0xFF;
            mt->g = 0x20;
            mt->b = 0x20;
        }
        mt->cd = 0xFF;
        if (CfgFlagChk(pSysView, CFG_WIDE_MODE)) {
            mt->y0 = (s16) ((f32) mt->y0 / 1.3333334f + 56.0f);
            mt->h = (s16) ((f32) mt->h / 1.3333334f);
        }
        AddPrim(&MainOt[1], (u32*) mt);
    }
    if (CurrentHeap == 4) {
        for (cell = cell_dll; cell != NULL; cell = cell->next) {
            u8* tag = (u8*) cell + cell->size - 0x20;
            if ((s32) tag >= 0 || (u32) tag > 0x82FFFFFF) {
                break;
            }
            if (MEM_TAG_OK(tag)) {
                eprintf2(8, 14, 230, ey, 22, 4, "%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
            } else {
                eprintf2(8, 14, 230, ey, 22, 4, "unknown           %6x %08x", cell->size - 0x20, cell);
            }
            ey += 14;
            if (write == 1 && cell->size - 0x20 > 0x1000) {
                p += sprintf(p, "%6x %18s\n", cell->size - 0x20, tag + 4);
            }
            next = cell->next;
            if (next != NULL && next->next != NULL &&
                ((s32) next->next >= 0 || (u32) next->next > 0x82FFFFFF)) {
                pLog->err(0, 0, "heap next err:%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
                pLog->err(0, 0, "next addr    : %08x", cell->next);
#line 870
                HALT();
            }
        }
        for (cell = cell_stage; cell != NULL; cell = cell->next) {
            u8* tag = (u8*) cell + cell->size - 0x20;
            if ((s32) tag >= 0 || (u32) tag > 0x82FFFFFF) {
                break;
            }
            if (MEM_TAG_OK(tag)) {
                eprintf2(8, 14, 230, ey, 22, 4, "%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
            } else {
                eprintf2(8, 14, 230, ey, 22, 4, "unknown           %6x %08x", cell->size - 0x20, cell);
            }
            ey += 14;
            if (write == 1 && cell->size - 0x20 > 0x1000) {
                p += sprintf(p, "%6x %18s\n", cell->size - 0x20, tag + 4);
            }
            next = cell->next;
            if (next != NULL && next->next != NULL &&
                ((s32) next->next >= 0 || (u32) next->next > 0x82FFFFFF)) {
                pLog->err(0, 0, "heap next err:%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
                pLog->err(0, 0, "next addr    : %08x", cell->next);
#line 904
                HALT();
            }
        }
        for (cell = cell_game; cell != NULL; cell = cell->next) {
            u8* tag = (u8*) cell + cell->size - 0x20;
            if ((s32) tag >= 0 || (u32) tag > 0x82FFFFFF) {
                break;
            }
            if (MEM_TAG_OK(tag)) {
                eprintf2(8, 14, 230, ey, 18, 4, "%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
            } else {
                eprintf2(8, 14, 230, ey, 18, 4, "unknown           %6x %08x", cell->size - 0x20, cell);
            }
            if (write == 1 && cell->size - 0x20 > 0x1000) {
                p += sprintf(p, "%6x %18s\n", cell->size - 0x20, tag + 4);
            }
            ey += 14;
            next = cell->next;
            if (next != NULL && next->next != NULL &&
                ((s32) next->next >= 0 || (u32) next->next > 0x82FFFFFF)) {
                pLog->err(0, 0, "heap next err:%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
                pLog->err(0, 0, "next addr    : %08x", cell->next);
#line 938
                HALT();
            }
        }
        for (cell = cell_main; cell != NULL; cell = cell->next) {
            u8* tag = (u8*) cell + cell->size - 0x20;
            if ((s32) tag >= 0 || (u32) tag > 0x82FFFFFF) {
                break;
            }
            if (MEM_TAG_OK(tag)) {
                eprintf2(8, 14, 230, ey, 20, 4, "%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
            } else {
                eprintf2(8, 14, 230, ey, 20, 4, "unknown           %6x %08x", cell->size - 0x20, cell);
            }
            if (write == 1 && cell->size - 0x20 > 0x1000) {
                p += sprintf(p, "%6x %18s\n", cell->size - 0x20, tag + 4);
            }
            ey += 14;
            next = cell->next;
            if (next != NULL && next->next != NULL &&
                ((s32) next->next >= 0 || (u32) next->next > 0x82FFFFFF)) {
                pLog->err(0, 0, "heap next err:%-18s %6x %08x", tag + 4, cell->size - 0x20, cell);
                pLog->err(0, 0, "next addr    : %08x", cell->next);
#line 974
                HALT();
            }
        }
    }
    {
        mt = &tile[1];
        mt->code = 4;
        mt->x0 = x;
        mt->y0 = 30;
        mt->z0 = 0;
        mt->w = 5;
        mt->h = 400;
        mt->b = mt->g = mt->r = 0x20;
        mt->cd = 0xFF;
        if (CfgFlagChk(pSysView, CFG_WIDE_MODE)) {
            mt->y0 = 78;
            mt->h = 300;
        }
        AddPrim(&MainOt[1], (u32*) mt);
    }
    eprintf2(10, 16, 30, 0x38, 0, 4, "elf_end   %8x", SysMem.arena_lo);
    eprintf2(10, 16, 30, 0x58, 0, 4, "DVD       %8x", SysMem.elf_end);
    eprintf2(10, 16, 30, 0x68, 0, 4, "SOUND     %8x", SysMem.dvd);
    eprintf2(10, 16, 30, 0x78, 0, 4, "FIFO      %8x", SysMem.sound);
    eprintf2(10, 16, 30, 0x88, 0, 4, "XFB       %8x", SysMem.fifo);
    eprintf2(10, 16, 30, 0x98, 0, 4, "CORE      %8x", SysMem.xfb);
    eprintf2(10, 16, 30, 0xA8, 0, 4, "OPTION    %8x", SysMem.core);
    eprintf2(10, 16, 30, 0xB8, 0, 4, "PLAYER    %8x", SysMem.option);
    eprintf2(10, 16, 30, 0xC8, 0, 4, "WEAPON    %8x", SysMem.player);
    eprintf2(10, 16, 30, 0xD8, 0, 4, "HEAP_TOP  %8x", SysMem.weapon);
    eprintf2(10, 16, 30, 0xE8, 0, 4, "HEAP_SIZE %x", SysMem.heap_end - SysMem.weapon);
    eprintf2(10, 16, 30, 0x108, 0, 4, "NOW_HEAP  %8x", start);
    eprintf2(10, 16, 30, 0x118, 0, 4, " SIZE     %x", size);
    eprintf2(10, 16, 30, 0x128, 0, 4, " REST     %x", rest);
    eprintf2(10, 16, 30, 0x148, 0, 4, "FST_SIZE  %x", DvdView.freeSize);
    eprintf2(10, 16, 30, 0x168, 0, 4, "USB       %8x", SysMem.usb);
    eprintf2(10, 16, 30, 0x178, 0, 4, "DEBUG     %8x", SysMem.debug);
    if (Joy[0].rep2 & 0x400000) {
        _epy_base -= 16;
    }
    if (Joy[0].rep2 & 0x800000) {
        _epy_base += 16;
    }
    _epy = _epy_base;
    if (write == 1) {
        HDWrite("d:\\bio4\\prog\\memlog.txt", buf, p - buf);
        Debug_free(buf);
    }
}

// Counts memset errors (leftover debug hook).
// Dead-stripped helper: only its string and statics survive (STRIP_UNUSED).
static void memSetCheck()
{
    static int memSetErrCnt = 0;

    memSetErrCnt++;
    OSReport("memset error: ");
}

// main_sub's .bss starts 8-aligned; the split object carries the 4-byte pad.
asm(".section .bss; .balign 8");
