// game/tv_mode: boot-time TV mode selection — picks the VI mode for the TV format (NTSC / PAL /
// EURGB60, interlaced or progressive from the saved setting) and, on a progressive-capable TV with
// B held (or progressive already on), asks "display in progressive mode?" before the card check.
#include "types.h"
#include "global.h"
#include "gx.h"
#include "os_vi.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "card.h"
#include "main.h"
#include "main_mem.h"
#include "main_sub.h"
#include "joy.h"
#include "scheduler.h"
#include "fade.h"
#include "mes.h"
#include "tv_mode.h"

#line 22 "D:/Bio4/Prog/tv_mode.cpp"

TV_MODE* pTv;
u8 tv_mode_cnt;

// Boot: sets the render mode's VI / XFB mode for the console's TV format (progressive when saved
// in pRK) and starts the mode-check task in slot 1.
#line 26
void SetTvMode(GXRenderModeObj* pRmode)
{
    pTv = (TV_MODE*) MEM_CALLOC(sizeof(TV_MODE), 1, 0xD);
    pTv->pRmode = pRmode;
    switch (VIGetTvFormat()) {
    case 0:
    case 2:
        if (pRK->tv_mode == 0) {
            pRmode->viTVmode = VI_TVMODE_NTSC_INT;
            pRmode->xFBmode = VI_XFBMODE_DF;
        } else {
            pRmode->viTVmode = VI_TVMODE_NTSC_PROG;
            pRmode->xFBmode = VI_XFBMODE_SF;
        }
        break;
    case 1:
        pRmode->viTVmode = VI_TVMODE_PAL_INT;
        break;
    case 5:
        pRmode->viTVmode = VI_TVMODE_EURGB60_INT;
        break;
    default:
#line 46
        OSPanic(__FILE__, __LINE__, "invalid TV format\n");
        break;
    }
    pTv->Status = 1;
    TaskExec(1, tvModeCheckTask, 0);
}

// Task: black fade, then the state machine (trigger -> progressive menu -> exit).
void tvModeCheckTask()
{
    static void (*tvModeFuncTbl[3])(TV_MODE*) = {tvModeTrigger, tvModeMenu_progressive, tvModeExit};
    u32 c0 = 0x00000000;
    u32 c1 = 0x000000FF;

    FadeSet(0, (GXColor*) &c0, (GXColor*) &c1, 0, 0, 0);
    tv_mode_cnt = 0;
    pTv->Rno0 = 0;
    while (1) {
        tvModeFuncTbl[pTv->Rno0](pTv);
        TaskSleep(1);
    }
}

// State 0: with a progressive TV and the check not done yet (waits up to 30 frames for the pad),
// B held or progressive already on opens the menu; else straight to exit. Marks the check done.
void tvModeTrigger(TV_MODE* pTv)
{
    if (VIGetDTVStatus() != 0 && pRK->tv_mode_select == 0) {
        if (Joy[0].err == -3 || Joy[0].err == -2) {
            tv_mode_cnt++;
            if (tv_mode_cnt <= 29) {
                return;
            }
        }
        if (OSGetProgressiveMode() == 1 || (Joy[0].on & JOY_B)) {
            systemVISetBlack(0);
            pTv->Rno0 = 1;
        } else {
            pTv->Rno0 = 2;
        }
    } else {
        pTv->Rno0 = 2;
    }
    pRK->tv_mode_select = 1;
}

// State 1: the yes / no message (system layout, message 0; the cursor's choice after 300 idle
// frames), applies the mode (VI reconfigured behind a black screen), then the confirmation
// message (1 progressive / 2 interlaced) until A.
void tvModeMenu_progressive(TV_MODE* pTv)
{
    u32 timer;
    s8 sel;
    u8 old;

    switch (pTv->Rno1) {
    case 0:
        MesData.registData(pTv->Rno1, (u8*) (pG->pCore->ofs_70 + (u32) pG->pCore));
        cMes.setLayout(0, LAYOUT_SYSTEM);
        cMes.MesSet(0, 100, 220, 0x1000051, 0, 0, 1);
        timer = 0;
        if ((sel = cMes.GetSelectMessage(0)) == 0) {
            do {
                timer++;
                if (Joy[0].trg & 0x30003) {
                    timer = 0;
                }
                if (timer > 300) {
                    sel = cMes.GetSelectCursor(0) + 1;
                    break;
                }
                TaskSleep(1);
            } while ((sel = cMes.GetSelectMessage(0)) == 0);
        }
        old = pRK->tv_mode;
        if (sel == 1) {
            pRK->tv_mode = 1;
            OSSetProgressiveMode(1);
        } else {
            pRK->tv_mode = 0;
            OSSetProgressiveMode(0);
        }
        if (pRK->tv_mode != old) {
            if (pRK->tv_mode == 1) {
                pTv->pRmode->viTVmode = VI_TVMODE_NTSC_PROG;
                pTv->pRmode->xFBmode = VI_XFBMODE_SF;
            } else {
                pTv->pRmode->viTVmode = VI_TVMODE_NTSC_INT;
                pTv->pRmode->xFBmode = VI_XFBMODE_DF;
            }
            systemVISetBlack(1);
            VIFlush();
            VIConfigure(pTv->pRmode);
            VIFlush();
            TaskSleep(100);
            systemVISetBlack(0);
        }
        pTv->Rno1++;
        break;
    case 1:
        if (OSGetProgressiveMode() == 1) {
            cMes.MesSet(1, 100, 220, 0x1000051, 0, 0, 1);
        } else {
            cMes.MesSet(2, 100, 220, 0x1000051, 0, 0, 1);
        }
        if (Key.trg & 0x80000000) {
            cMes.Clear();
            pTv->Rno0 = 2;
            pTv->Rno1 = 0;
        }
        break;
    }
}

// State 2: done — runs the memory card first check and ends the task.
void tvModeExit(TV_MODE* pTv)
{
    pTv->Status = 0;
    CardFirstCheck();
}
