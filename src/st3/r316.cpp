#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "event.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "em.h"
#include "em_set.h"
#include "em_wrap.h"
#include "emdoor.h"
#include "emwindow.h"
#include "etc_model.h"
#include "player.h"
#include "esp.h"
#include "est.h"
#include "fade.h"
#include "sscrn.h"
#include "room_data.h"

// Room 3-16 (D:/Bio4/Prog/r316.cpp): the s00 event (Ashley taken away), the enemy waves after
// it, the heat effect in front of the furnace and the item that falls off the shelf.

struct R316Work {
    u8 dummy;
};

// The item object's per-object work (obj19 leaves it to the room): the fall speed at cObj+0x32C.
struct R316ItemView {
    u8 pad_0[0x32C];
    f32 fallSpd;
};

static R316Work* r316_work;

void r316_openShelf_main(int no, int mode);
static void r316_openedShelf(int no);
static void r316_openShelf(int no);
static void r316_checkFallItem();
static void r316_checkHeatEffect();
static void r316_exitDoorTo317();
static void r316_checkEmReset();
static void R316EventS00();
static void R316EventSXX();
extern "C" void Evt_R316S00_Func(Event* e);

// Room init: until the event has run (Room_flg bit 0) it runs at once, else the wave refills run until
// the room is left through door 1. Also sets up the heat and falling-item watchers and the furnace.
void R316Init()
{
    cEmDoor* door;
    cEmWindow* win;

    SysFlagOff(pG, SYS_SCREEN_STOP);
    DbgFlagOn(pG, DBG_CAST_ERR_NO_DISP);
#line 51 "D:/Bio4/Prog/r316.cpp"
    r316_work = (R316Work*) MEM_CALLOC(sizeof(R316Work), 1, 0xd);
    if (pG->JumpPoint == 1) {
        RsfSet(G_ROOM_ID, 0);
        RsfSet(G_ROOM_ID, 1);
    }
    StaFlagOff(pG, STA_SUB_ASHLEY);
    EvtMgr.SetFunc("evt_r316s00_func", (void*) Evt_R316S00_Func);
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        EvtMgr.EvtReadAram("event/evd/r316s00.evd", (u8) GetEmIdFromList(0), 0, 1, 0);
        SceExec(0x12, (TaskFunc) R316EventS00, 0, 2, 2, 0);
    } else if (RsfCheck(G_ROOM_ID, 2) == 0) {
        SceExec(0x12, (TaskFunc) r316_checkEmReset, 0, 0, 2, 0);
        SceAtSetDoorFunc(1, (TaskFunc) r316_exitDoorTo317, 0);
    }
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        SceAtDataSet_exec(2, 0x12, 0, (TaskFunc) R316EventSXX, 0, 1);
        if (getRoomEtcDoor(0xD, &door, 1)) {
            door->setCloseLock();
        }
    }
    SceExec(0x12, (TaskFunc) r316_checkHeatEffect, 0, 0, 2, 0);
    SceExec(0x12, (TaskFunc) r316_checkFallItem, 0, 0, 2, 0);
    if (getRoomEtcWindow(0x10, &win, 1)) {
        win->LightInfo.SelectMask &= ~0x10;
    }
    if (getRoomEtcWindow(0x11, &win, 1)) {
        win->LightInfo.SelectMask &= ~0x10;
    }
    if (getRoomEtcWindow(0x12, &win, 1)) {
        win->LightInfo.SelectMask &= ~0x10;
    }
    if (getRoomEtcWindow(0x18, &win, 1)) {
        win->LightInfo.SelectMask &= ~0x10;
    }
    if (getRoomEtcWindow(0x19, &win, 1)) {
        win->LightInfo.SelectMask &= ~0x10;
    }
    if (getRoomEtcWindow(0x1A, &win, 1)) {
        win->LightInfo.SelectMask &= ~0x10;
    }
    SmdSetTrans(0x3D, 1);
    SmdSetTrans(0x3E, 0);
    SceSetItemEvent(7, 0x84, 3, 2, r316_openShelf, r316_openedShelf, 0, 0);
}

// Per-frame room main: nothing.
void R316Main()
{
}

// Shelf 0 (object 0x3F, locker type 2) opens (mode 1: snap).
void r316_openShelf_main(int no, int mode)
{
    if (no == 0) {
        OpenBoxMain(2, mode, 0x1C, 0x3F, 0xFFFFFFFF, -1);
    }
}

// Item-event "already opened": the shelf posed open.
static void r316_openedShelf(int no)
{
    r316_openShelf_main(no, 1);
}

// Item-event opener: animate the shelf open.
static void r316_openShelf(int no)
{
    r316_openShelf_main(no, 0);
}

// The item on the shelf stays put until etc flag 6 (the shelf tipped over).
static void r316_checkFallItem()
{
    cModel* m;

    SceAtSetEnable(0x83, 1);
    m = SceAtItemModelPtr(0x83);
    if (m) {
        Vec pos = m->pos;

        while ((*GetEtcFlgPtr(6, G_ROOM_ID) & 1) == 0) {
            ((R316ItemView*) m)->fallSpd = 0.0f;
            m->setPos(&pos);
            SceSleep(1);
        }
    }
}

// Heat haze in front of the furnace while the player stands in area 6.
static void r316_checkHeatEffect()
{
    int kind = EspPullCoreKind();
    int on = 0;
    void* zero = 0;

    for (;;) {
        if (on == 0) {
            if (SceAtHitCheck(6) == 1) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 0, 1, (u8) kind, zero, zero);
                on = 1;
                SceSleep(30);
            }
        } else {
            if (SceAtHitCheck(6) == 0) {
                EffectEspDelete(0, (u8) kind, 0, 0);
                EffectEspgenDelete(0, (u8) kind, 0);
                EffectEfmDelete(0, (u8) kind, 0);
                on = 0;
                EstSet(0, -1, 0, 0, EFF_ROOM, 1, 1, ESP_CORE_KIND_NONE, 0, 0);
                SceSleep(30);
            }
        }
        SceSleep(1);
    }
}

// Door 1's exit hook (toward r317): Room_flg bit 2, every Ganado removed, ESL entries 3/8/9 marked alive again.
static void r316_exitDoorTo317()
{
    RsfSet(G_ROOM_ID, 2);
    SceDestroyEm(0x10, 0x20);
    EmListSetAlive(3, 1);
    EmListSetAlive(8, 1);
    EmListSetAlive(9, 1);
}

// Refills the Ganado waves from the side the player is on.
static void r316_checkEmReset()
{
    int a = 1;

    SceSleep(30);
    int b = 1;
    int tbl[4] = {0, 1, 2, 5};
    int tbl2[2] = {4, 6};
    u32 i = 0;
    int c = 0;

    while (1) {
        if (SceAtHitCheck(3) == 1) {
            a = 0;
            b = 1;
        }
        if (SceAtHitCheck(4) == 1) {
            a = 1;
            b = 0;
        }
        if (SceAtHitCheck(5) == 1) {
            a = 1;
            b = 1;
        }
        if ((u32) SceCountEmAlive(0x10, 0x20) <= 3) {
            if (a == 1 && i <= 1) {
                setEm(tbl2[i], -1, 1, 1, 1);
                i++;
            }
            if (b == 1 && c == 0) {
                setEm(7, -1, 1, 1, 1);
                c = 1;
            }
            if (i > 1 && c != 0) {
                break;
            }
        }
        SceSleep(1);
    }
}

// Once (Room_flg bit 0): System_flg 0x400, event r316s00 (Ashley is taken away by Saddler's men),
// chapter 5-2 ends (SceSetChapterEnd(CHAPTER_5_2)), the wave refills start, fade-in and the inventory opens.
static void R316EventS00()
{
    if (RsfCheck(G_ROOM_ID, 0) == 0) {
        RsfSet(G_ROOM_ID, 0);
        SysFlagOn(pG, SYS_SCREEN_STOP);
        EvtMgr.EvtReadExec("event/evd/r316s00.evd", (u8) GetEmIdFromList(0), EvtReadFlagNone);
        SceSetChapterEnd(0xF, -1);
        SceExec(0x12, (TaskFunc) r316_checkEmReset, 0, 0, 2, 0);
        FadeSetW(1, 0, 0, 0);
        SubScreenOpen(2, 0);
    }
}

// Leaving through door 2: fade out and jump to r30a.
static void R316EventSXX()
{
    if (RsfCheck(G_ROOM_ID, 1) == 0) {
        RsfSet(G_ROOM_ID, 1);
        SceEventStart(0);
        pPL->setNoSuspend(1);
        FadeSetW(2, 60, 0, 0);
        FadeWait(2);
        SceEventEnd(0);
        ScfFlagOn(pG, SCF_R316_TO_R30A_CUTBACK_EVENT);
        {
            Vec pos = {0.0f, 0.0f, 0.0f};
            Vec rot = {0.0f, 0.0f, 0.0f};

            SceAtExecRoomJump(0x30A, &pos, &rot, 0);
        }
        SysFlagOn(pG, SYS_SCREEN_STOP);
    }
    // COMPILER-DIFF: candidate (gcse table size). Five dead insns (folded by cse2, no code) grow the
    // PRE hash table from 45 to 47/49 buckets, which numbers the hoisted `&pos` (fp+0x10, hash
    // 13305) before `&rot` (fp+0x20, 13321) and gives them the target's r29/r28.
    {
        int z = 0;
        do { } while (0);
        if (z > 128) {
            z = z * 77 + 1;
        }
    }
}

// Event r316s00 callback: swaps the door objects 0x3D/0x3E (closed / open) on cuts 0 and 5/0xB, sets the
// event models' flags per cut; the end restores the room.
void Evt_R316S00_Func(Event* e)
{
    switch (e->GetFuncType()) {
    case 0:
        break;
    case 1: {
        void* mod;

        switch (e->GetNowCut()) {
        case 0:
            if (e->GetNowFrame() == 0) {
                SmdSetTrans(0x3D, 1);
                SmdSetTrans(0x3E, 0);
            }
            break;
        case 5:
        case 0xB:
            if (e->GetNowFrame() == 0) {
                SmdSetTrans(0x3D, 0);
                SmdSetTrans(0x3E, 1);
            }
            break;
        }
        switch (e->GetNowCut()) {
        case 0:
        case 1:
            if (e->GetMod(&mod, "obm5500", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 1, 0);
            }
            break;
        default:
            if (e->GetMod(&mod, "obm5500", 0, 0) == 1) {
                ModelInfoSetTrans((cModel*) mod, 1, 1);
            }
            break;
        }
        switch (e->GetNowCut()) {
        case 0xB:
            if (e->GetNowFrame() == 0) {
                SmdSetTrans(1, 0);
            }
            break;
        case 0xA:
        case 0xC:
            if (e->GetNowFrame() == 0) {
                SmdSetTrans(1, 1);
            }
            break;
        case 0x12:
            if (e->GetNowFrame() == 0) {
                SmdSetTrans(6, 0);
            }
            break;
        case 0x11:
        case 0x13:
            if (e->GetNowFrame() == 0) {
                SmdSetTrans(6, 1);
            }
            break;
        }
        break;
    }
    case 2:
        SmdSetTrans(0x3D, 1);
        SmdSetTrans(0x3E, 0);
        SmdSetTrans(1, 1);
        SmdSetTrans(6, 1);
        break;
    }
}
