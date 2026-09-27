// game/flr_at: floor attribute areas from the room's "FSE" block (D:/Bio4/Prog/flr_at.cpp), used
// for footstep sounds and effects, sound situations and puddles.
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "flr_at.h"
#include "area.h"
#include "global.h"
#include "db_log.h"
#include "main_mem.h"
#include <string.h>

void* GetDataExt(void* arc, const char* tag, int no);   // game/read.cpp

FLR_AT_SYS FlrAt_sys;
FLR_AT_SYS* pFlrSys;

// Room init: binds the room archive's "FSE" block (version 0x103) as the floor attribute list.
void FlrAtInit()
{
    FLR_AT_HEADER* p;

    pFlrSys = &FlrAt_sys;
    memclr_asm(&FlrAt_sys, sizeof(FlrAt_sys));
    p = (FLR_AT_HEADER*) GetDataExt(pG->pRoom, "FSE", 0);
    if (p == 0) {
        return;
    }
    if (strcmp((char*) p, "FSE") != 0) {
        return;
    }
    if (p->version != 0x103) {
        pLog->warn(0, 0, "FlrAt DATA IS OLD VERSION");
        return;
    }
    pFlrSys->pHead = p;
    pFlrSys->pData = (FLR_AT_DATA*) (p + 1);
}

// Returns the enabled floor attribute of `type` whose area contains pos (+300 y) and whose group
// matches the current group (0xFF = any); NULL outside the main game step (Rno0 != 3), during
// Status_flg[0] 0x10000000, or when none hits.
FLR_AT_DATA* FlrAtCheck(int id, Vec* pos, int flag)
{
    Vec p;
    FLR_AT_DATA* at;
    u32 i;
    int hit = 0;

    if (pG->Rno0 != 3) {
        return 0;
    }
    if (StaFlagChk(pG, STA_MOVIE_ON)) {
        return 0;
    }
    if (pFlrSys == 0) {
        return 0;
    }
    if (pFlrSys->pHead == 0) {
        return 0;
    }
    p.x = pos->x;
    p.y = pos->y + 300.0f;
    p.z = pos->z;
    for (i = 0; i < ((FLR_AT_HEADER*) pFlrSys->pHead)->num; i++) {
        at = &pFlrSys->pData[i];
        if ((at->flag & 1) == 0) {
            continue;
        }
        if (at->group != pFlrSys->cur_group && pFlrSys->cur_group != 0xFF) {
            continue;
        }
        if (at->type != id) {
            continue;
        }
        if (AreaHitCheck(&at->area, &p) != 1) {
            continue;
        }
        if (id != 0) {
            hit = 1;
        } else if (at->se.use_kind & flag) {
            hit = 1;
        }
        if (hit == 1) {
            return at;
        }
    }
    return 0;
}

// Enables floor attribute entry `no` (flag bit 0).
// Dead-stripped by the original linker (only their strings survive in .rodata).
static int FlrAtSetEnable(int no)
{
    if (pFlrSys->pHead == 0) {
        pLog->err(0, 0, "FlrAtSetEnable() : AT DATA NOT FOUND");
        return 0;
    }
    pFlrSys->pData[no].flag |= 1;
    return 1;
}

// Disables floor attribute entry `no`.
static int FlrAtSetDisable(int no)
{
    if (pFlrSys->pHead == 0) {
        pLog->err(0, 0, "FlrAtSetDisable() : AT DATA NOT FOUND");
        return 0;
    }
    pFlrSys->pData[no].flag &= ~1;
    return 1;
}

// Sets the default footstep SE set and effect number for group `no` (0..0x3F; 0xFF = the default slot 0x40).
int FlrAtSetDefVal(u32 group, u8 foot_se_set, u8 eff_no)
{
    if (group != 0xFF && group > 0x3F) {
        pLog->err(0, 0, "FlrAt : group %d Illegal No.", group);
        return 0;
    }
    group = group == 0xFF ? 0x40 : group;
    pFlrSys->def_se_set[group] = foot_se_set;
    pFlrSys->def_eff_set[group] = eff_no;
    return 1;
}
