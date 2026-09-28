#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "flag_rsf.h"
#include "player.h"
#include "esp.h"

// Room 1-1a (D:/Bio4/Prog/r11a.cpp): the lake shore path. R11aInit attaches the water ripple / splash
// effects to the player and registers the water hit-effect table; R11aMain keeps the player's footstep
// type on the water variant. No events or enemies are scripted here.

struct R11aWork {
    u8 pad[1];
};

static R11aWork* r11a_work;

// Hit effects of attribute type 2 (water)
static const AtEffInfo r11a_eff_info = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};

// Room init: two persistent player effects (EstSet types 3/1: the water ripple / splash attached to
// pPL) and the water hit-effect table for attribute type EAT_ET_WATER.
void R11aInit()
{
#line 46 "D:/Bio4/Prog/r11a.cpp"
    r11a_work = (R11aWork*) MEM_CALLOC(1, 1, 0xd);

    EstSet(pPL, -1, 0, 0, EFF_PL00, 2, 0, ESP_CORE_KIND_NONE, pPL, 0);
    EstSet(pPL, -1, 0, 0, EFF_ROOM, 0, 0, ESP_CORE_KIND_NONE, pPL, 0);
    EatMgr.registEffInfo(EAT_ET_WATER, (AtEffInfo*) &r11a_eff_info);
}

// Per frame: keep the player's footstep/OT type switched to the water variant while in the lake room.
void R11aMain()
{
    setPlWaterOtType();
}
