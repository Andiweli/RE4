// game/se_at: the room's ambient sound emitters from the "ESE" sub-file of the room archive. The
// room scripts switch single emitters with SeAtSetOnOff / SeAtSndCall.
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "snd.h"
#include "global.h"
#include "db_log.h"
#include "rnd.h"
#include "db_menu.h"

void* GetDataExt(void* arc, const char* tag, int no);   // game/read.cpp

// Room start: takes the room's ESE emitter list (version 0x100) into Snd.se_at / se_at_list.
void SeAtInit()
{
    SND_WORK* s = &Snd;

    s->pSeAtHeader = (SeAtHead*) GetDataExt(pG->pRoom, "ESE", 0);
    if (s->pSeAtHeader == 0) {
        return;
    }
    if (s->pSeAtHeader->version != 0x100) {
        pLog->err(0, 0, "SeAt DATA IS OLD VERSION");
        s->pSeAtHeader = 0;
        s->pSeAtData = 0;
        return;
    }
    s->pSeAtData = (SE_AT_DATA*) (s->pSeAtHeader + 1);
}

// Per frame in the game routine (Rno0 3, not while Stop_flg 0x800): each enabled emitter waits
// its `wait` frames, then plays its SE (positioned unless flags2 bit0) and reloads `cnt` with the
// fixed interval or rnd_base + random(rnd_range); `repeat` counts the plays down (1 = last, -1 done).
void SeAtCheck()
{
    SND_WORK* s = &Snd;
    SE_AT_DATA* at;
    Vec* pos;
    int i;

    if (SpfFlagChk(pG, SPF_SE_CALC)) {
        return;
    }
    if (DbgFlagChk(pG, DBG_TEST_MODE) && DebugMenuSelected != 0x18) {
        return;
    }
    if (pG->Rno0 != 3) {
        return;
    }
    if (s->pSeAtHeader == 0) {
        return;
    }
    for (i = 0; i < s->pSeAtHeader->num; i++) {
        at = &s->pSeAtData[i];
        if ((at->be_flg & 1) == 0) {
            continue;
        }
        if (at->call_num < 0) {
            continue;
        }
        if (at->delay == 0) {
            if (at->ctr == 0) {
                pos = &at->pos;
                if (at->flag & 1) {
                    pos = 0;
                }
                if (SndCall(at->blk, at->se_no, pos, 0, 0, 0) == 0) {
                    continue;
                }
                if (at->call_num == 1) {
                    at->call_num = -1;
                    continue;
                }
                if (at->call_num != 0) {
                    at->call_num--;
                }
                if (at->interval == 0) {
                    at->ctr = at->rnd_base + Rnd() % at->rnd_interval;
                } else {
                    at->ctr = at->interval;
                }
            } else {
                at->ctr--;
            }
        } else {
            at->delay--;
        }
    }
}

// Room script: enables / disables emitter `no` (flags bit0). 0 when not found.
int SeAtSetOnOff(int no, int sw)
{
    SE_AT_DATA* at = GetSeAtPtr(no);

    if (at == 0) {
        if (sw == 1) {
            pLog->err(0, 0, "SeAtSetEnable() : AT DATA NOT FOUND");
        } else {
            pLog->err(0, 0, "SeAtSetDisable() : AT DATA NOT FOUND");
        }
        return 0;
    }
    if (sw == 1) {
        at->be_flg |= 1;
    } else {
        at->be_flg &= ~1;
    }
    return 1;
}

// The emitter record numbered `no`, or 0.
SE_AT_DATA* GetSeAtPtr(int no)
{
    SE_AT_DATA* at;
    u32 i;

    if (Snd.pSeAtHeader == 0) {
        return 0;
    }
    for (i = 0; i < Snd.pSeAtHeader->num; i++) {
        at = &Snd.pSeAtData[i];
        if (at->at_no == no) {
            return at;
        }
    }
    return 0;
}

// Plays emitter `no`'s SE once now; returns the SndCall handle (0 when not found).
u32 SeAtSndCall(int no)
{
    SE_AT_DATA* at = GetSeAtPtr(no);

    if (at != 0) {
        if (at->flag & 1) {
            return SndCall(at->blk, at->se_no, 0, 0, 0, 0);
        }
        return SndCall(at->blk, at->se_no, &at->pos, 0, 0, 0);
    }
    pLog->err(0, 0, "SeAtSeCall() : AT DATA NOT FOUND");
    return 0;
}

