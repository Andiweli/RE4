#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"

// Room 3-1d (D:/Bio4/Prog/r31d.cpp): a small island room with a single shelf item event (item 0x85,
// area 7); nothing else is scripted.

struct R31dWork {
    u8 dummy;
};

static R31dWork* r31d_work;

static void r31d_ShelfOpen(int no);
static void r31d_ShelfOpened();

// Room init: one shelf item event (item 0x85 at area 7).
void R31dInit()
{
#line 29 "D:/Bio4/Prog/r31d.cpp"
    r31d_work = (R31dWork*) MEM_CALLOC(sizeof(R31dWork), 1, 0xd);
    SceSetItemEvent(7, 0x85, 0, 2, r31d_ShelfOpen, (void (*)(int)) r31d_ShelfOpened, (void*) 0x35, 0);
}

// Per-frame room main: nothing.
void R31dMain()
{
}

// Item-event opener: the shelf (objects 0x35/0x36, type 0x1A) swings open.
static void r31d_ShelfOpen(int no)
{
    OpenBoxMain(0, 0, 0x1A, 0x35, 0x36, -1);
}

// Item-event "already opened": the shelf posed open.
static void r31d_ShelfOpened()
{
    OpenBoxMain(0, 1, 0x1A, 0x35, 0x36, -1);
}
