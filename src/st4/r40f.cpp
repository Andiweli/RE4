#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "flag_rsf.h"
#include "global.h"
#include "sce.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "scroll.h"
#include "em.h"
#include "em_wrap.h"
#include "player.h"
#include "cam_ctrl.h"
#include "snd.h"
#include "esp.h"
#include "est.h"

// Room 4-0F (D:/Bio4/Prog/r40f.cpp): the barred door with its lever switch, the guards that rush
// in when it opens and the two bomb carriers.

struct R40fWork {
    u8 pad_0[0x30];
    cEmWrap em;      // 0x30  the guard of the door event
    u8 pad_3C[0x60 - 0x3C];
    cEmWrap em0;     // 0x60  the five guards of the switch event
    cEmWrap em1;     // 0x6C
    cEmWrap em2;     // 0x78
    cEmWrap em3;     // 0x84
    cEmWrap em4;     // 0x90
    cEmWrap bomb0;   // 0x9C
    cEmWrap bomb1;   // 0xA8
};

static R40fWork* r40f_work;

static void R40fBombSet();
void R40fDoorEventEmMove();
static void R40fDoorEvent00Main();
static void R40fDoorEvent00End();
static void R40fDoorSwitchMain();
static void R40fDoorSwitchEnd();
void R40fDoorOpened(int on);

// Room init: area 9 = the guard opening the door from outside until Room_flg bit 3; area 8 = the lever;
// the door posed open (bit 5, open effect) or shut; area 0xE = the bomb carriers until bit 7; object
// 0x15 hidden.
void R40fInit()
{
#line 32 "D:/Bio4/Prog/r40f.cpp"
    r40f_work = (R40fWork*) MEM_CALLOC(sizeof(R40fWork), 1, 0xd);
    if (RsfCheck(G_ROOM_ID, 3) == 0) {
        SceAtDataSet_exec(9, SCE_LEVEL10, 0, (TaskFunc) R40fDoorEvent00Main, 0, 1);
    }
    SceAtDataSet_exec(8, SCE_LEVEL10, 0, (TaskFunc) R40fDoorSwitchMain, 0, 1);
    if (RsfCheck(G_ROOM_ID, 5)) {
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, 0x2001, ESP_CORE_KIND_ROOM00, 0, 0);
        R40fDoorOpened(1);
    } else {
        EstSet(0, -1, 0, 0, EFF_ROOM, 1, 0x2001, ESP_CORE_KIND_ROOM00, 0, 0);
        R40fDoorOpened(0);
    }
    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        SceAtDataSet_exec(0xE, SCE_LEVEL10, 0, (TaskFunc) R40fBombSet, 0, 1);
    }
    SmdSetTrans(0x15, 0);
}

// Per-frame room main: nothing.
void R40fMain()
{
}

// The two bomb carriers walk in.
static void R40fBombSet()
{
    if (RsfCheck(G_ROOM_ID, 7) == 0) {
        RsfSet(G_ROOM_ID, 7);
        SceAtSetEnable(0xE, 0);
        r40f_work->bomb0.setEm(0xAE, -1, 1, 1, 1);
        r40f_work->bomb1.setEm(0xAF, -1, 1, 1, 1);
        r40f_work->bomb0.setFlag(1);
        r40f_work->bomb1.setFlag(1);
        Vec p0 = {-6970.0f, -2000.0f, -1215.0f};
        Vec p1 = {-9220.0f, -2000.0f, -2190.0f};
        r40f_work->bomb0.setGoto(&p0, 1);
        r40f_work->bomb1.setGoto(&p1, 1);
    }
}

// The guard of the door event walks to the door.
void R40fDoorEventEmMove()
{
    Vec p = {6630.0f, 0.0f, 3660.0f};
    cEmWrap* em;
    int i;

    r40f_work->em.setEm(0xA4, -1, 1, 1, 1);
    em = &r40f_work->em;
    em->setNoSuspend(1);
    em->setGoto(&p, 4);
    while (em->ckGoto()) {
        SceSleep(1);
    }
    for (i = 0; i < 20; i++) {
        SceSleep(1);
    }
}

// Area 9 once (Room_flg bit 3, door still shut): camera cut 6 while the guard walks up and the door
// opens (effect swapped), the message camera set 9/8/5; player-cancellable.
static void R40fDoorEvent00Main()
{
    if (RsfCheck(G_ROOM_ID, 5) == 0 && RsfCheck(G_ROOM_ID, 3) == 0) {
        RsfSet(G_ROOM_ID, 3);
        SceAtSetEnable(9, 0);
        SceEventStart(1);
        SceSetEventCancel(1, (TaskFunc) R40fDoorEvent00End, 0, -1, 1);
        CamCtrl.CutCall(6);
        R40fDoorEventEmMove();
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
        EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM00, 0, 0);
        EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM00, 0);
        EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM00, 0);
        EstSet(0, -1, 0, 0, EFF_ROOM, 0, 0x2001, ESP_CORE_KIND_ROOM00, 0, 0);
        SceMesCamSndSet(9, 8, 5, 4);
        SceSetEventCancel(0, 0, 0, -1, 1);
        R40fDoorEvent00End();
    }
}

// End of the door event (also its cancel path): the door snapped open with its effect, the guard may
// suspend, camera back, SceEventEnd, task exit.
static void R40fDoorEvent00End()
{
    R40fDoorOpened(1);
    EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM00, 0, 0);
    EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM00, 0);
    EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM00, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 0, 0x2001, ESP_CORE_KIND_ROOM00, 0, 0);
    r40f_work->em.setNoSuspend(0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// The lever: choosing to pull it opens the door and lets the guards in.
static void R40fDoorSwitchMain()
{
    int i;

    if (RsfCheck(G_ROOM_ID, 5) == 0) {
        SceUpCut(6, -1, -1, UP_CUT_ATTR_CUT_FIX);
    } else {
        SceEventStart(1);
        SceMesCamSndSet(5, -1, -1, 4);
        if (SceMesGetSelection() != 1) {
            CamCtrl.Comeback(0);
            SceEventEnd(0);
            SceExit();
        } else {
            EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM00, 0, 0);
            EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM00, 0);
            EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM00, 0);
            EstSet(0, -1, 0, 0, EFF_ROOM, 1, 0x2001, ESP_CORE_KIND_ROOM00, 0, 0);
            SndCall(6, 7, 0, 0, 0, 0);
            SceMesCamSndSet(0xA, 8, 6, 4);
            r40f_work->bomb0.destroy();
            r40f_work->bomb1.destroy();
            if (RsfCheck(G_ROOM_ID, 6) == 0) {
                r40f_work->em0.setEm(0xA8, -1, 1, 1, 1);
                r40f_work->em1.setEm(0xA9, -1, 1, 1, 1);
                r40f_work->em2.setEm(0xAA, -1, 1, 1, 1);
                r40f_work->em3.setEm(0xAB, -1, 1, 1, 1);
                r40f_work->em4.setEm(0xAC, -1, 1, 1, 1);
                r40f_work->em0.setNoSuspend(1);
                r40f_work->em1.setNoSuspend(1);
                r40f_work->em2.setNoSuspend(1);
                r40f_work->em3.setNoSuspend(1);
                r40f_work->em4.setNoSuspend(1);
            }
            SceSetEventCancel(1, (TaskFunc) R40fDoorSwitchEnd, 0, -1, 1);
            CamCtrl.CutCall(7);
            r40f_work->em0.setNoSuspend(1);
            r40f_work->em1.setNoSuspend(1);
            r40f_work->em2.setNoSuspend(1);
            r40f_work->em3.setNoSuspend(1);
            r40f_work->em4.setNoSuspend(1);
            r40f_work->em0.setFlag(1);
            r40f_work->em1.setFlag(1);
            r40f_work->em2.setFlag(1);
            r40f_work->em3.setFlag(1);
            r40f_work->em4.setFlag(1);
            Vec p = {14280.0f, 0.0f, 3170.0f};
            r40f_work->em0.setGoto(&p, 1);
            r40f_work->em1.setGoto(&p, 1);
            r40f_work->em2.setGoto(&pPL->pos, 1);
            r40f_work->em3.setGoto(&pPL->pos, 1);
            r40f_work->em4.setGoto(&pPL->pos, 1);
            for (i = 0; i < 20; i++) {
                SceSleep(1);
            }
            while (CamCtrl.IsMotionEnd() == 0) {
                SceSleep(1);
            }
            if (RsfCheck(G_ROOM_ID, 6) == 0) {
                int j;

                for (j = 0; j < 30; j++) {
                    SceSleep(1);
                }
            }
            SceSetEventCancel(0, 0, 0, -1, 1);
            R40fDoorSwitchEnd();
        }
    }
}

// End of the lever event (also its cancel path): the first time (Room_flg bit 6) the five guards are
// alerted and released; the door shut with its effect, camera back, SceEventEnd.
static void R40fDoorSwitchEnd()
{
    if (RsfCheck(G_ROOM_ID, 6) == 0) {
        RsfSet(G_ROOM_ID, 6);
        r40f_work->em0.setFlag(1);
        r40f_work->em1.setFlag(1);
        r40f_work->em2.setFlag(1);
        r40f_work->em3.setFlag(1);
        r40f_work->em4.setFlag(1);
        r40f_work->em0.setNoSuspend(0);
        r40f_work->em1.setNoSuspend(0);
        r40f_work->em2.setNoSuspend(0);
        r40f_work->em3.setNoSuspend(0);
        r40f_work->em4.setNoSuspend(0);
    }
    R40fDoorOpened(0);
    EffectEspDelete(0x2001, ESP_CORE_KIND_ROOM00, 0, 0);
    EffectEspgenDelete(0x2001, ESP_CORE_KIND_ROOM00, 0);
    EffectEfmDelete(0x2001, ESP_CORE_KIND_ROOM00, 0);
    EstSet(0, -1, 0, 0, EFF_ROOM, 1, 0x2001, ESP_CORE_KIND_ROOM00, 0, 0);
    CamCtrl.Comeback(0);
    SceEventEnd(0);
    SceExit();
}

// Snap the barred door open (area 0xD on, area 1 off, Room_flg bit 5) or shut (the reverse).
void R40fDoorOpened(int on)
{
    if (on == 1) {
        SceAtSetEnable(0xD, 1);
        SceAtSetEnable(1, 0);
        RsfSet(G_ROOM_ID, 5);
    } else {
        SceAtSetEnable(0xD, 0);
        SceAtSetEnable(1, 1);
        RsfClear(G_ROOM_ID, 5);
    }
}
