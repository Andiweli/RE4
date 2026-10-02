#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "esp.h"

// Room 3-1a (D:/Bio4/Prog/r31a.cpp): the upper floor of the elevator from r318 (the laser corridor):
// sce_com's SceElevator with the arrive / leave data tables, area 0 rides back down.

struct R31aWork {
    u8 dummy;
};


static R31aWork* r31a_work;

static ElevatorParam r31a_elvArrive = {Ele1FArrival, 0, {3250.0f, -578.0f, 0.0f}, {3085.0f, 0.0f, -100.0f}, {0.0f, 1.35f, 0.0f}, 1, 0, 2, 0, 1, {27850.0f, 826.0f, 4380.0f}, {0.0f, -1.48f, 0.0f}, 0x318};
static ElevatorParam r31a_elvLeave = {Ele1FStarting, 0, {3250.0f, -578.0f, 0.0f}, {3085.0f, 0.0f, -100.0f}, {0.0f, 1.35f, 0.0f}, 1, 0, 0, 0, 1, {27850.0f, 826.0f, 4380.0f}, {0.0f, -1.48f, 0.0f}, 0x318};

// Room init: no water splashes; area 0 = the elevator back down to r318 (SceElevator, action colour);
// arriving from r318 by a normal transition plays the elevator's arrival ride.
void R31aInit()
{
#line 63 "D:/Bio4/Prog/r31a.cpp"
    r31a_work = (R31aWork*) MEM_CALLOC(sizeof(R31aWork), 1, 0xd);
    Espgen42SetNoWater(1);
    SceAtDataSet_exec(0, 0x12, 0, (TaskFunc) SceElevator, &r31a_elvLeave, 1);
    SceAtSetActColor(0, 1);
    if (SysFlagChk(pG, SYS_LOAD_GAME) == 0 && pG->room_id_prev == 0x318) {
        SceExec(0x12, (TaskFunc) SceElevator, &r31a_elvArrive, 0, 2, 0);
    }
}

// Per-frame room main: nothing.
void R31aMain()
{
}
