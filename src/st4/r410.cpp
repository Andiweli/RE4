#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"

// Room 4-10 (D:/Bio4/Prog/r410.cpp): an Assignment Ada room with two item box events (items 0x82 /
// 0x80); nothing else is scripted.

struct R410Work {
    u8 dummy;
};

static R410Work* r410_work;

static void r410_ItemBoxOpen(int id);
static void r410_ItemBoxOpened(int id);

// Room init (Assignment Ada): two item box events (items 0x82 / 0x80).
void R410Init()
{
#line 28 "D:/Bio4/Prog/r410.cpp"
    r410_work = (R410Work*) MEM_CALLOC(sizeof(R410Work), 1, 0xd);
    SceSetItemEvent(2, 0x82, 0, 3, r410_ItemBoxOpen, r410_ItemBoxOpened, (void*) 0xF, 0);
    SceSetItemEvent(3, 0x80, 1, 4, r410_ItemBoxOpen, r410_ItemBoxOpened, (void*) 0x14, 0);
}

// Per-frame room main: nothing.
void R410Main()
{
}

// Item-event opener: the box of item `id` (0xF double door, else a duralumin case lid up +Z) opens.
static void r410_ItemBoxOpen(int id)
{
    if (id == 0xF) {
        OpenBoxMain(0, 0, 6, 0xF, 0x10, -1);
    } else {
        OpenBoxMain(OpenBoxPartsUpZP, 0, 0x18, id, 0xFFFFFFFF, -1);
    }
}

// Item-event "already opened": the box of item `id` posed open.
static void r410_ItemBoxOpened(int id)
{
    if (id == 0xF) {
        OpenBoxMain(0, 1, 6, 0xF, 0x10, -1);
    } else {
        OpenBoxMain(OpenBoxPartsUpZP, 1, 0x18, id, 0xFFFFFFFF, -1);
    }
}
