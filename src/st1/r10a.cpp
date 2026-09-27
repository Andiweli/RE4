#include "types.h"
#include "main_mem.h"
#include "st_room.h"
#include "atari.h"
#include "light.h"
#include "flag_rsf.h"
#include "global.h"
#include "db_log.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "em_set.h"
#include "em_wrap.h"
#include "emrock.h"
#include "esp.h"
#include "snd.h"

// Room 1-0a (D:/Bio4/Prog/r10a.cpp): the boulder path; the falling rock ("IWA") and its player
// motions, the Ganado waves (normal / reinforcement A / B) and the battle stream.

struct R10aWork {
    int x0;
    cEmWrap em[20];    // 0x04  0..9 normal, 10..14 zouen A, 15..19 zouen B
    cEmRock* rock;     // 0xF4
};

static R10aWork* r10a_work;

// Hit effects of attribute type 2 (water)
static const AtEffInfo r10a_eff_info = {
    1, {1, 0x2C}, {1, 0x2F}, {1, 0x2E}, {1, 0x2D}, {1, 0x20}, {1, 0x20}, {1, 0x2B}, {1, 0x2F},
};

static void r10a_StrStart();
static void r10a_zouen_ck();

// Spawn the base Ganado wave: enemy list entries 0xB1..0xBA into em[0..9].
extern "C" void EmSetNormal()
{
    r10a_work->em[0].setEm(0xB1, 0, 0, 1, 1);
    r10a_work->em[1].setEm(0xB2, 0, 0, 1, 1);
    r10a_work->em[2].setEm(0xB3, 0, 0, 1, 1);
    r10a_work->em[3].setEm(0xB4, 0, 0, 1, 1);
    r10a_work->em[4].setEm(0xB5, 0, 0, 1, 1);
    r10a_work->em[5].setEm(0xB6, 0, 0, 1, 1);
    r10a_work->em[6].setEm(0xB7, 0, 0, 1, 1);
    r10a_work->em[7].setEm(0xB8, 0, 0, 1, 1);
    r10a_work->em[8].setEm(0xB9, 0, 0, 1, 1);
    r10a_work->em[9].setEm(0xBA, 0, 0, 1, 1);
}

// Spawn reinforcement wave A (list 0xAE, 0xC4..0xC7) into em[10..14]; recorded in Room_flg bit 0.
extern "C" void EmSetZouenA()
{
    r10a_work->em[10].setEm(0xAE, 0, 0, 1, 1);
    r10a_work->em[11].setEm(0xC4, 0, 0, 1, 1);
    r10a_work->em[12].setEm(0xC5, 0, 0, 1, 1);
    r10a_work->em[13].setEm(0xC6, 0, 0, 1, 1);
    r10a_work->em[14].setEm(0xC7, 0, 0, 1, 1);
}

// Spawn reinforcement wave B (list 0x72, 0x73, 0x85, 0x86, 0x9F) into em[15..19]; Room_flg bit 1.
extern "C" void EmSetZouenB()
{
    r10a_work->em[15].setEm(0x72, 0, 0, 1, 1);
    r10a_work->em[16].setEm(0x73, 0, 0, 1, 1);
    r10a_work->em[17].setEm(0x85, 0, 0, 1, 1);
    r10a_work->em[18].setEm(0x86, 0, 0, 1, 1);
    r10a_work->em[19].setEm(0x9F, 0, 0, 1, 1);
}

// Room init. Unless the rock already fell (Room_flg bit 2), creates the cEmRock boulder at the top
// of the slope with three event Ganados pushing it. Then spawns the base wave and any reinforcement
// waves already triggered.
void R10aInit()
{
#line 92 "D:/Bio4/Prog/r10a.cpp"
    r10a_work = (R10aWork*) MEM_CALLOC(sizeof(R10aWork), 1, 0xd);

    EatMgr.registEffInfo(EAT_ET_WATER, (AtEffInfo*) &r10a_eff_info);
    SceAtDataSet_exec(4, SCE_LEVEL10, 0, (TaskFunc) r10a_StrStart, 0, 1);
    SceAtDataSet_exec(5, SCE_LEVEL10, 0, (TaskFunc) r10a_StrStart, 0, 1);
    SceAtDataSet_exec(2, SCE_LEVEL10, 0, (TaskFunc) r10a_zouen_ck, 0, 1);
    if (RsfCheck(G_ROOM_ID, 2) == 0) {
        Vec pos;
        Vec rot;
        void* tpl;
        cEmRock* rock;

        pos.x = -43779.0f;
        pos.y = 18285.0f;
        pos.z = 67184.0f;
        rot.x = 0.0f;
        rot.y = 0.39f;
        rot.z = 0.0f;
        EspDataLoad((u32) ROOM_ARC_PTR(pG->pRoom, 0x2E), EFF_OBM1F, 0);
        if (EspGetEfmTplAddr(0x20, &tpl) == 0) {
            pLog->err(0, 0, "IWA init: EFM[%02x] TPL not regist.", 0x20);
            return;
        }
        rock = SetRock(ROOM_ARC_PTR(pG->pRoom, 0x20), tpl, &pos, &rot, 1);
        r10a_work->rock = rock;
        if (rock != 0) {
            void* mot[16];

            mot[0] = ROOM_ARC_PTR(pG->pRoom, 0x21);
            mot[1] = ROOM_ARC_PTR(pG->pRoom, 0x22);
            mot[2] = ROOM_ARC_PTR(pG->pRoom, 0x23);
            mot[3] = ROOM_ARC_PTR(pG->pRoom, 0x24);
            mot[4] = ROOM_ARC_PTR(pG->pRoom, 0x25);
            mot[5] = ROOM_ARC_PTR(pG->pRoom, 0x26);
            mot[6] = ROOM_ARC_PTR(pG->pRoom, 0x27);
            mot[7] = ROOM_ARC_PTR(pG->pRoom, 0x28);
            mot[8] = ROOM_ARC_PTR(pG->pRoom, 0x29);
            mot[9] = ROOM_ARC_PTR(pG->pRoom, 0x2A);
            mot[10] = ROOM_ARC_PTR(pG->pRoom, 0x2B);
            mot[11] = ROOM_ARC_PTR(pG->pRoom, 0x2C);
            mot[12] = ROOM_ARC_PTR(pG->pRoom, 0x2D);
            mot[13] = ROOM_ARC_PTR(pG->pRoom, 0x2F);
            mot[14] = ROOM_ARC_PTR(pG->pRoom, 0x30);
            mot[15] = ROOM_ARC_PTR(pG->pRoom, 0x31);
            rock->setPlMotion(mot);
            rock->setScale(4.2f);
        }
        {
            EmListData d;

            d.id = 0x12;
            d.type = 1;
            d.set = 0x1C;
            d.flag = 0;
            d.pos[0] = -0x1163;
            d.pos[1] = 0x63D;
            d.pos[2] = 0x18F0;
            d.rot[0] = 0;
            d.rot[1] = 0x3E9;
            d.rot[2] = 0;
            d.hp = 0;
            d.Guard_r = 1;
            d.Character = 1;
            EmSetEvent(&d);

            d.id = 0x12;
            d.type = 0;
            d.set = 0x1C;
            d.flag = 0;
            d.pos[0] = -0x10EA;
            d.pos[1] = 0x63D;
            d.pos[2] = 0x18E3;
            d.rot[0] = 0;
            d.rot[1] = -0x60B;
            d.rot[2] = 0;
            d.hp = 0;
            d.Guard_r = 1;
            d.Character = 1;
            EmSetEvent(&d);

            d.id = 0x12;
            d.type = 3;
            d.set = 0x1C;
            d.flag = 0;
            d.pos[0] = -0x11CE;
            d.pos[1] = 0x63D;
            d.pos[2] = 0x1909;
            d.rot[0] = 0;
            d.rot[1] = 0x5B0;
            d.rot[2] = 0;
            d.hp = 0;
            d.Guard_r = 1;
            d.Character = 1;
            EmSetEvent(&d);
        }
    }
    EmSetNormal();
    if (RsfCheck(G_ROOM_ID, 0)) {
        EmSetZouenA();
    }
    if (RsfCheck(G_ROOM_ID, 1)) {
        EmSetZouenB();
    }
}

// Per frame: water OT type; once the boulder's flag bit 0 (landed / finished) is set, record Room_flg bit 2
// so it is not re-created.
void R10aMain()
{
    setPlWaterOtType();
    if (RsfCheck(G_ROOM_ID, 2) == 0 && r10a_work->rock != 0 && (r10a_work->rock->flag & 1)) {
        RsfSet(G_ROOM_ID, 2);
    }
}

// Battle stream while the Ganados see the player.
static void r10a_StrStart()
{
    int on;

    if (pG->Room_flg[0] & 0x80000000) {
        return;
    }
    pG->Room_flg[0] |= 0x80000000;
    on = 0;
    for (;;) {
        if (SceCkFindPL(0) == 1) {
            if (on == 0) {
                SndRoomStrStart(1, 0xF, 1);
                on = 1;
            }
        } else {
            if (on == 1) {
                SndRoomStrStop(3);
                on = 0;
            }
        }
        SceSleep(1);
    }
}

// Reinforcements: wave A when 5 or fewer Ganados are left, wave B after that.
static void r10a_zouen_ck()
{
    u32 cnt = 0;

    if (r10a_work->em[0].isActive() == 1) cnt++;
    if (r10a_work->em[1].isActive() == 1) cnt++;
    if (r10a_work->em[2].isActive() == 1) cnt++;
    if (r10a_work->em[3].isActive() == 1) cnt++;
    if (r10a_work->em[4].isActive() == 1) cnt++;
    if (r10a_work->em[5].isActive() == 1) cnt++;
    if (r10a_work->em[6].isActive() == 1) cnt++;
    if (r10a_work->em[7].isActive() == 1) cnt++;
    if (r10a_work->em[8].isActive() == 1) cnt++;
    if (r10a_work->em[9].isActive() == 1) cnt++;
    if (RsfCheck(G_ROOM_ID, 0)) {
        if (r10a_work->em[10].isActive() == 1) cnt++;
        if (r10a_work->em[11].isActive() == 1) cnt++;
        if (r10a_work->em[12].isActive() == 1) cnt++;
        if (r10a_work->em[13].isActive() == 1) cnt++;
        if (r10a_work->em[14].isActive() == 1) cnt++;
    }
    if (RsfCheck(G_ROOM_ID, 0) && RsfCheck(G_ROOM_ID, 1) == 0 && cnt <= 5) {
        RsfSet(G_ROOM_ID, 1);
        EmSetZouenB();
    }
    if (RsfCheck(G_ROOM_ID, 0) == 0 && cnt <= 5) {
        RsfSet(G_ROOM_ID, 0);
        EmSetZouenA();
    }
}
