#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "event.h"
#include "light.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "em.h"
#include "emwindow.h"
#include "etc_model.h"
#include "esp.h"
#include "player.h"
#include "flr_at.h"
#include "rnd.h"

// Room 1-11 (D:/Bio4/Prog/r111.cpp): the village house exterior in the storm; the rack ranges, the
// rain effects and the thunder task.

struct R111Work {
    u8 pad[0x1C];
};

static R111Work* r111_work;

static void r111_ThunderMove();

// Room init: sets the three rack (cEmRack, etc_model) trigger ranges (moving rack 0x11 to a fixed
// position), starts the thunder task, attaches the rain effects to the player (EstSet 1/3/0x23),
// sets Status_flg[1] 0x400 (raining), pre-breaks windows 0 and 0x13, and default floor attribute 3.
void R111Init()
{
    cEmRack* rack;
    cEmWindow* win;

#line 47 "D:/Bio4/Prog/r111.cpp"
    r111_work = (R111Work*) MEM_CALLOC(sizeof(R111Work), 1, 0xd);

    if (getRoomEtcRack(0xD, &rack, 1)) {
        rack->setRange(0.0f, 1000.0f, 0.0f, 2000.0f);
    }
    if (getRoomEtcRack(0xF, &rack, 1)) {
        rack->setRange(2000.0f, 2000.0f, 0.0f, 2600.0f);
    }
    if (getRoomEtcRack(0x11, &rack, 1)) {
        rack->pos.x = 6656.0f;
        rack->pos.y = 902.0f;
        rack->pos.z = 8925.0f;
        rack->setRange(2000.0f, 800.0f, 0.0f, 4400.0f);
    }
    SceExec(0x12, (TaskFunc) r111_ThunderMove, 0, 0, SCE_PRIO_DEF_2, 0);
    {
        void* zero = 0;

        EstSet(pPL, -1, 0, 0, EFF_ROOM, 0, 0x800, ESP_CORE_KIND_NONE, zero, zero);
        EstSet(pPL, -1, 0, 0, EFF_PL00, 1, 0x800, ESP_CORE_KIND_NONE, zero, zero);
        EstSet(pPL, -1, 0, 0, EFF_CORE, 0x23, 0x800, ESP_CORE_KIND_NONE, zero, zero);
    }
    StaFlagOn(pG, STA_ROOM_RAIN);
    if (getRoomEtcWindow(0, &win, 1)) {
        win->SetBreakModel();
    }
    if (getRoomEtcWindow(0x13, &win, 1)) {
        win->SetBreakModel();
    }
    FlrAtSetDefVal(0, 0, 3);
}

// Per-frame room main: nothing.
void R111Main()
{
}

// Thunder every 90..235 frames: the flash (indoors: the window flash), the lit rain areas, the sound.
static void r111_ThunderMove()
{
    int cnt;
    void* zero = 0;

    SceSleep(1);
    {
        u8 r = Rnd() % 30;
        cnt = r * 5 + 90;
    }
    for (;;) {
        if (cnt == 0) {
            if (!StaFlagChk(pG, STA_CAMERA_IN_ROOM)) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 2, 1, ESP_CORE_KIND_NONE, 0, 0);
            } else {
                EstSet(0, -1, 0, 0, EFF_ROOM, 0x10, 1, ESP_CORE_KIND_NONE, 0, 0);
            }
            if (EffGetAreaState(0)) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 4, 0, ESP_CORE_KIND_NONE, zero, zero);
            }
            if (EffGetAreaState(1)) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 6, 0, ESP_CORE_KIND_NONE, zero, zero);
            }
            if (EffGetAreaState(2)) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 8, 0, ESP_CORE_KIND_NONE, zero, zero);
            }
            if (EffGetAreaState(3)) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 0xA, 0, ESP_CORE_KIND_NONE, zero, zero);
            }
            if (EffGetAreaState(4)) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 0xC, 0, ESP_CORE_KIND_NONE, zero, zero);
            }
            if (EffGetAreaState(5)) {
                EstSet(0, -1, 0, 0, EFF_ROOM, 0xE, 0, ESP_CORE_KIND_NONE, zero, zero);
            }
            {
                u8 r = Rnd() % 30;
                cnt = r * 5 + 90;
            }
            SceSndCallThunder();
        }
        cnt--;
        SceSleep(1);
    }
}
