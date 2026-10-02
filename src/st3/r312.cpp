#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"

// Room 3-12 (D:/Bio4/Prog/r312.cpp): a small island room with a single treasure chest item event
// (item 0x82, area 6); nothing else is scripted.

struct R312Work {
    u8 dummy;
};

static R312Work* r312_work;

static void OpenBoxTreasure(int id);
static void OpenedBoxTreasure(int id);

// Room init: one treasure chest item event (item 0x82 at area 6).
void R312Init()
{
#line 44 "D:/Bio4/Prog/r312.cpp"
    r312_work = (R312Work*) MEM_CALLOC(sizeof(R312Work), 1, 0xd);
    SceSetItemEvent(6, 0x82, 2, 5, OpenBoxTreasure, OpenedBoxTreasure, (void*) 0x82, 0);
}

// Per-frame room main: nothing.
void R312Main()
{
}

// Item-event "already opened": the chest (object 0x38, type 0x1C) posed open.
static void OpenedBoxTreasure(int id)
{
    if (id == 0x82) {
        OpenBoxMain(1, 1, 0x1c, 0x38, -1, -1);
    }
}

// Item-event opener: the chest lid swings open.
static void OpenBoxTreasure(int id)
{
    if (id == 0x82) {
        OpenBoxMain(1, 0, 0x1c, 0x38, -1, -1);
    }
}
