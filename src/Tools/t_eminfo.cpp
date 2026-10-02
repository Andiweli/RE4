#include "types.h"
#include "atari.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "camera.h"
#include "db_cam.h"
#include "main_mem.h"
#include "main_sub.h"
#include "file.h"
#include "dbmodule.h"
#include "t_prim.h"
#include "t_util.h"
#include <string.h>
#include <stdio.h>
#include "math_sub.h"
#include "model.h"

// Enemy placement info editor (Tools/t_eminfo.cpp): edits the per-room .emi point list (position,
// direction and three work bytes per typed point) with a screen cursor, draws the points and the
// routes/areas some enemies build from them.

struct EMINFO_WK {
    u8 type;   // 0x00  0 = free slot, else workTypeName index
    u8 Work0;  // 0x01
    u8 Work1;  // 0x02
    u8 Work2;  // 0x03
    Vec Pos;   // 0x04
    f32 Dir;   // 0x10
    u8 pad_14[0x40 - 0x14];
};

struct EmInfoTool {
    int mode;             // 0x00  routine (eminfoFunc)
    int sub;              // 0x04  detail sub routine (Detail01Update table)
    f32 cx;               // 0x08  screen cursor
    f32 cy;               // 0x0C
    int near;             // 0x10  point nearest to the cursor (-1 none)
    int cur;              // 0x14  selected point (-1 none)
    u8 pad_18[0x10];
    int num;              // 0x28  used points
    int pad_2C;
    EMINFO_WK work[256]; // 0x30
    EMINFO_WK copy;      // 0x4030  copy buffer / template for new points
    int camMode;          // 0x4070  1: the pad drives the debug camera
    JOY joy;              // 0x4074
    s8 cursor;            // 0x42DC  menu cursor
    u8 yesNo;             // 0x42DD
    u8 workType;          // 0x42DE  type being chosen (Detail01UpdateWorkType)
    u8 pad_42DF;
};


static EmInfoTool* emInfo;
static void* emInfoFileBuf;
#define W emInfo

static char menuName[5][0x40] = {
    "Cancel", "Save", "Load", "Clear data", "Exit",
};

static char workTypeName[20][0x80] = {
    "------------------",
    "EM10 Hide pos     ",
    "EM2F Route pos    ",
    "EM2B House pos    ",
    "EM2B Rock pos     ",
    "Ashley take away  ",
    "Rock Roll route   ",
    "Rock Escape pos   ",
    "Rock Start pos    ",
    "Ashley Jump Point ",
    "Dog Bark pos      ",
    "Ashley Route Point",
    "Return Start Pos  ",
    "EM2B Catch Dir    ",
    "No.3 Snipe Pos    ",
    "EM10 Goto pos     ",
    "EM2D No Wall      ",
    "EM32 Jump Pos     ",
    "EM32 Ceiilng Pos  ",
    "EM3A Route        ",
};

void ToolEmInfo();
void eminfoInit();
void eminfoExit();
static void eminfo_r0_menu();
static void eminfo_r0_clear();
static void eminfo_r0_save();
static void eminfo_r0_load();
static void eminfo_r0_move();
static void eminfo_r0_catch();
static void Detail00Update();
static void Detail01UpdateSel();
void Detail01UpdateWorkType();
void Detail01UpdatePosX();
void Detail01UpdatePosY();
void Detail01UpdatePosZ();
void Detail01UpdateDir();
void Detail01UpdateWork0();
void Detail01UpdateWork1();
void Detail01UpdateWork2();
static void Detail01Update();
static void eminfo_r0_detail();
void eminfo_menu_disp();
void eminfo_detail_disp(int no);
void eminfoAddWork();
void eminfoDeleteWork(int no);
void eminfoGetNearPoint(Vec* cursor);
void eminfoCursorToWork(int no);
void eminfoDisp();
void eminfoDrawDir(Vec* pos, f32 dir, u32 col, u32 col2);
void eminfoDispNo(Vec* pos, int no);
void eminfoTargetDisp(int no);
void eminfoCopyDisp();
void eminfoEm2fRouteDisp();
void eminfoEm39AreaDisp();
void eminfoEm3aRouteDisp();
void eminfoFileSave();
int eminfoFileLoad();
void eminfoSetFileName(char* path);
void eminfoCameraMove();

void (*detailFunc[20])() = {
    Detail00Update,  Detail01Update, Detail01Update, Detail01Update, Detail01Update,
    Detail01Update,  Detail01Update, Detail01Update, Detail01Update, Detail01Update,
    Detail01Update,  Detail01Update, Detail01Update, Detail01Update, Detail01Update,
    Detail01Update,  Detail01Update, Detail01Update, Detail01Update, Detail01Update,
};

static int detailMenuNum[20] = {
    1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
};

static char detailName[9][0x80] = {
    "WorkType ", "Pos X    ", "Pos Y    ", "Pos Z    ", "Dir      ",
    "Work0    ", "Work1    ", "Work2    ", "EXIT     ",
};

static GXColor cursorCol[2] = {
    {0x80, 0x00, 0x00, 0xFF},
    {0xFF, 0x40, 0x40, 0xFF},
};

// Puts the screen cursor at the screen centre.
static inline void eminfoCursorCenter()
{
    W->cx = (Screen.x + Screen.width) * 0.5f;
    W->cy = (Screen.y + Screen.height) * 0.5f;
}

// Blinking cursor mark: on for half of the frames, always while a button is held.
#define eminfoBlink() ((pG->Frame_cnt & 0x10) || W->joy.on)

// Enemy info editor entry (debug menu 35): loads the room's .emi, then every frame moves the debug
// camera / cursor (TutilMoveCursor), finds the point nearest the cursor and runs tbl[mode]: 0 menu,
// 1 move (pick), 2 catch (drag), 3 detail, 4 clear, 5 save, 6 load; draws everything.
void ToolEmInfo()
{
    void (*tbl[7])() = {
        eminfo_r0_menu, eminfo_r0_move,  eminfo_r0_catch, eminfo_r0_detail,
        eminfo_r0_clear, eminfo_r0_save, eminfo_r0_load,
    };

    eminfoInit();
    for (;;) {
        eminfoCameraMove();
        CameraMove();
        TutilMoveCursor((Vec*) &W->cx, 40.0f, 1.0f);
        eminfoGetNearPoint((Vec*) &W->cx);
        tbl[W->mode]();
        eminfoDisp();
        TaskSleep(1);
    }
}

// Tool start: allocates the work and the file buffer, default tool flags, mode 1, loads
// x:\soft\room\st<n>\r<room>\r<room>.emi and centres the cursor.
void eminfoInit()
{
    EmInfoTool* p;
    void*& fileBuf = emInfoFileBuf;

    TaskSuspend(0);
    TaskSleep(1);
    TutilInitDefault();
    DbgFlagOn(pG, DBG_TEST_MODE);
    DbgFlagOn(pG, DBG_BACK_CLIP);
    DbgFlagOn(pG, DBG_DBG_CAM);
    SpfFlagOn(pG, SPF_SCE);
    p = (EmInfoTool*) Debug_alloc(sizeof(EmInfoTool), 1);
    W = p;
    memset(p, 0, sizeof(EmInfoTool));
    fileBuf = Debug_alloc(0x80000, 1);
    W->mode = 1;
    W->sub = 0;
    W->cursor = 0;
    W->num = 0;
    memclr_asm(&W->copy, sizeof(EMINFO_WK));
    W->copy.type = 1;
    eminfoFileLoad();
    eminfoCursorCenter();
    W->camMode = 0;
}

// Frees the work, restores the flags and ends the task.
void eminfoExit()
{
    DbgFlagOff(pG, DBG_TEST_MODE);
    DbgFlagOff(pG, DBG_BACK_CLIP);
    DbgFlagOff(pG, DBG_DBG_CAM);
    SpfFlagOff(pG, SPF_SCE);
    TutilQuitDefault();
    TaskSignal(0);
    TaskExit();
}

// Mode 0, the B menu: Cancel / Save (mode 5) / Load (6) / Clear data (4) / Exit.
static void eminfo_r0_menu()
{
    if (W->joy.rep2 & (JOY_UP | JOY_SUP)) {
        if (W->cursor > 0) {
            W->cursor--;
        } else {
            W->cursor = 4;
        }
    }
    if (W->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        if (W->cursor <= 3) {
            W->cursor++;
        } else {
            W->cursor = 0;
        }
    }
    if (W->joy.trg & JOY_B) {
        W->cursor = 4;
    }
    if (W->joy.trg & JOY_A) {
        switch (W->cursor) {
        case 0:
        default:
            W->mode = 1;
            W->sub = 0;
            W->cursor = 0;
            break;
        case 1:
            W->mode = 5;
            W->sub = 0;
            W->cursor = 0;
            W->yesNo = 1;
            break;
        case 2:
            W->mode = 6;
            W->sub = 0;
            W->cursor = 0;
            W->yesNo = 1;
            break;
        case 3:
            W->mode = 4;
            W->sub = 0;
            W->cursor = 0;
            W->yesNo = 1;
            break;
        case 4:
            eminfoExit();
            break;
        }
    } else {
        eminfoCursorCenter();
        eminfo_menu_disp();
    }
}

// "Clear data ?" YES/NO: YES empties the point list.
static void eminfo_r0_clear()
{
    if (W->joy.rep2 & (JOY_LEFT | 0x10000)) {
        W->yesNo = 0;
    }
    if (W->joy.rep2 & (JOY_RIGHT | 0x20000)) {
        W->yesNo = 1;
    }
    if (W->joy.trg & JOY_B) {
        W->mode = 0;
        W->sub = 0;
        W->cursor = 0;
    } else if (W->joy.trg & JOY_A) {
        if (W->yesNo == 0) {
            memclr_asm(W->work, sizeof(W->work));
            W->num = 0;
        }
        W->mode = 0;
        W->sub = 0;
        W->cursor = 0;
    } else {
        eminfoCursorCenter();
        eminfo_menu_disp();
        eprintf(40, 300, 0, 0, "Clear data ?");
        if (W->yesNo) {
            eprintf(89, 316, 4, 0, ">NO");
            eprintf(40, 316, 0, 0, " yes /");
        } else {
            eprintf(82, 316, 0, 0, "/ no");
            eprintf(40, 316, 4, 0, ">YES");
        }
    }
}

// "Save data ?" YES/NO: YES writes the .emi (eminfoFileSave).
static void eminfo_r0_save()
{
    if (W->joy.rep2 & (JOY_LEFT | 0x10000)) {
        W->yesNo = 0;
    }
    if (W->joy.rep2 & (JOY_RIGHT | 0x20000)) {
        W->yesNo = 1;
    }
    if (W->joy.trg & JOY_B) {
        W->mode = 0;
        W->sub = 0;
        W->cursor = 0;
    } else if (W->joy.trg & JOY_A) {
        if (W->yesNo == 0) {
            eminfoFileSave();
        }
        W->mode = 0;
        W->sub = 0;
        W->cursor = 0;
    } else {
        eminfoCursorCenter();
        eminfo_menu_disp();
        eprintf(40, 300, 0, 0, "Save data ?");
        if (W->yesNo) {
            eprintf(89, 316, 4, 0, ">NO");
            eprintf(40, 316, 0, 0, " yes /");
        } else {
            eprintf(82, 316, 0, 0, "/ no");
            eprintf(40, 316, 4, 0, ">YES");
        }
    }
}

// "Load data ?" YES/NO: YES re-reads the .emi.
static void eminfo_r0_load()
{
    if (W->joy.rep2 & (JOY_LEFT | 0x10000)) {
        W->yesNo = 0;
    }
    if (W->joy.rep2 & (JOY_RIGHT | 0x20000)) {
        W->yesNo = 1;
    }
    if (W->joy.trg & JOY_B) {
        W->mode = 0;
        W->sub = 0;
        W->cursor = 0;
    } else if (W->joy.trg & JOY_A) {
        if (W->yesNo == 0) {
            eminfoFileLoad();
        }
        W->mode = 0;
        W->sub = 0;
        W->cursor = 0;
    } else {
        eminfoCursorCenter();
        eminfo_menu_disp();
        eprintf(40, 300, 0, 0, "Load data ?");
        if (W->yesNo) {
            eprintf(89, 316, 4, 0, ">NO");
            eprintf(40, 316, 0, 0, " yes /");
        } else {
            eprintf(82, 316, 0, 0, "/ no");
            eprintf(40, 316, 4, 0, ">YES");
        }
    }
}

// Mode 1, free cursor: A on the nearest point selects it and drags it (mode 2), Y opens the
// selected point's detail (mode 3), sub stick up/down moves its height by 500, L cycles the
// selection through the points, X adds a new point (a copy of the template) under the cursor,
// B opens the menu.
static void eminfo_r0_move()
{
    if (W->joy.trg & JOY_B) {
        W->mode = 0;
        W->sub = 0;
        W->cursor = 0;
        return;
    }
    if ((W->joy.trg & JOY_A) && W->near != -1) {
        W->cur = W->near;
        eminfoCursorToWork(W->cur);
        W->mode = 2;
        W->sub = 0;
        W->cursor = 0;
        return;
    }
    if (W->joy.trg & JOY_Y) {
        if (W->cur != -1) {
            eminfoCursorToWork(W->cur);
            W->mode = 3;
            W->sub = 0;
            W->cursor = 0;
            return;
        }
    } else if (W->cur != -1) {
        EMINFO_WK* p = &W->work[W->cur];

        if (W->joy.rep & JOY_SSUP) {
            p->Pos.y += 500.0f;
        }
        if (W->joy.rep & JOY_SSDOWN) {
            p->Pos.y -= 500.0f;
        }
        if (W->cur != -1 && (W->joy.trg & JOY_L)) {
            W->cur--;
            if (W->cur < 0) {
                W->cur = 0;
            }
            if (W->joy.trg & JOY_L) {
                W->cur++;
                if (W->cur >= W->num) {
                    W->cur = W->num - 1;
                }
            }
        }
    }
    if (W->joy.trg & JOY_X) {
        eminfoAddWork();
    }
}

// Mode 2, dragging while A is held: the point follows the cursor on its height (Get3DPosFrom2D),
// L/R rotate its direction, sub stick up/down change the height, Z deletes it; releasing A returns
// to mode 1.
static void eminfo_r0_catch()
{
    EMINFO_WK* p = &W->work[W->cur];

    if (!(W->joy.on & JOY_A) || W->cur == -1) {
        W->mode = 1;
        W->sub = 0;
        W->cursor = 0;
        return;
    }
    W->copy = *p;
    if (p->type != 0) {
        Get3DPosFrom2D(&p->Pos, W->cx, W->cy, p->Pos.y);
        if (W->joy.on & JOY_L) {
            p->Dir += 0.09817477f;
            p->Dir = LIMIT_ANGLE(p->Dir);
        }
        if (W->joy.on & JOY_R) {
            p->Dir -= 0.09817477f;
            p->Dir = LIMIT_ANGLE(p->Dir);
        }
        if (W->joy.rep2 & JOY_SSUP) {
            p->Pos.y += 500.0f;
        }
        if (W->joy.rep2 & JOY_SSDOWN) {
            p->Pos.y -= 500.0f;
        }
        if (W->joy.trg & JOY_Z) {
            eminfoDeleteWork(W->cur);
            W->cur = -1;
            W->mode = 1;
            W->sub = 0;
            W->cursor = 0;
        }
    }
}

// Detail of a free slot: A back to mode 1.
static void Detail00Update()
{
    if (W->joy.trg & JOY_A) {
        W->mode = 1;
        W->sub = 0;
        W->cursor = 0;
    }
}

// Detail row select: up/down over WorkType / Pos X/Y/Z / Dir / Work0..2 / EXIT, A edits the row
// (sub 1..8) or exits, B back; the point also becomes the copy template.
static void Detail01UpdateSel()
{
    EMINFO_WK* p = &W->work[W->cur];
    int n = detailMenuNum[p->type];

    if (W->joy.trg & JOY_B) {
        W->mode = 1;
        W->sub = 0;
        W->cursor = 0;
        return;
    }
    if (W->joy.rep2 & (JOY_UP | JOY_SUP)) {
        if (W->cursor > 0) {
            W->cursor--;
        } else {
            W->cursor = n - 1;
        }
    }
    if (W->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        if (W->cursor < n - 1) {
            W->cursor++;
        } else {
            W->cursor = 0;
        }
    }
    if (W->joy.trg & JOY_A) {
        switch (W->cursor) {
        case 0:
            W->sub = 1;
            W->workType = p->type;
            break;
        case 1:
            W->sub = 2;
            W->workType = 0;
            break;
        case 2:
            W->sub = 3;
            W->workType = 0;
            break;
        case 3:
            W->sub = 4;
            W->workType = 0;
            break;
        case 4:
            W->sub = 5;
            W->workType = 0;
            break;
        case 5:
            W->sub = 6;
            W->workType = 0;
            break;
        case 6:
            W->sub = 7;
            W->workType = 0;
            break;
        case 7:
            W->sub = 8;
            W->workType = 0;
            break;
        case 8:
        default:
            W->mode = 1;
            W->sub = 0;
            W->cursor = 0;
            break;
        }
    }
    eminfo_detail_disp(W->cur);
    W->copy = *p;
}

// WorkType row: up/down pick the type name (workTypeName), A applies, B cancels.
void Detail01UpdateWorkType()
{
    EMINFO_WK* p = &W->work[W->cur];
    int i;

    if (W->joy.trg & JOY_B) {
        W->sub = 0;
        return;
    }
    if (W->joy.rep2 & (JOY_UP | JOY_SUP)) {
        if (W->workType > 1) {
            W->workType--;
        }
    }
    if (W->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        if (W->workType < 19) {
            W->workType++;
        }
    }
    if (W->joy.trg & JOY_A) {
        if (W->workType == p->type) {
            W->sub = 0;
            return;
        }
        W->copy = *p;
        W->copy.type = W->workType;
        memclr_asm(p, sizeof(EMINFO_WK));
        p->type = W->copy.type;
        p->Pos = W->copy.Pos;
        p->Dir = W->copy.Dir;
        p->Work0 = 0;
        p->Work1 = 0;
        p->Work2 = 0;
        return;
    }
    eprintf(40, 60, 0, 0, "-- Change WorkType --------");
    for (i = 1; i < 20; i++) {
        int col = (p->type == i) ? 4 : 0;

        if (i > 18) {
            eprintf(280, 80 + (i - 19) * 16, col, 0, "[ %s ]", workTypeName[i]);
            if (W->workType == i && eminfoBlink()) {
                eprintf(272, 80 + (i - 19) * 16, 0, 0, ">");
            }
        } else {
            eprintf(40, 80 + (i - 1) * 16, col, 0, "[ %s ]", workTypeName[i]);
            if (W->workType == i && eminfoBlink()) {
                eprintf(32, 80 + (i - 1) * 16, 0, 0, ">");
            }
        }
    }
}

// Pos X row: up/down (repeat) +-1 unit, R x10, L x100; B back.
void Detail01UpdatePosX()
{
    EMINFO_WK* p = &W->work[W->cur];
    f32 step;

    if (W->joy.trg & JOY_B) {
        W->sub = 0;
        return;
    }
    step = 0.0f;
    if (W->joy.rep2 & (JOY_UP | JOY_SUP)) {
        step = 1.0f;
    }
    if (W->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        step = -1.0f;
    }
    if (W->joy.on & JOY_R) {
        step *= 10.0f;
    }
    if (W->joy.on & JOY_L) {
        step *= 100.0f;
    }
    p->Pos.x += step;
    eprintf(40, 60, 0, 0, "-- Position X Move --------");
    eprintf(40, 80, 0, 0, "Pos.x = ");
    if (eminfoBlink()) {
        eprintf(32, 80, 0, 0, ">");
    }
    eprintf(104, 80, 4, 0, "%.2f", p->Pos.x);
}

// Pos Y row: same as Pos X.
void Detail01UpdatePosY()
{
    EMINFO_WK* p = &W->work[W->cur];
    f32 step;

    if (W->joy.trg & JOY_B) {
        W->sub = 0;
        return;
    }
    step = 0.0f;
    if (W->joy.rep2 & (JOY_UP | JOY_SUP)) {
        step = 1.0f;
    }
    if (W->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        step = -1.0f;
    }
    if (W->joy.on & JOY_R) {
        step *= 10.0f;
    }
    if (W->joy.on & JOY_L) {
        step *= 100.0f;
    }
    p->Pos.y += step;
    eprintf(40, 60, 0, 0, "-- Position Y Move --------");
    eprintf(40, 80, 0, 0, "Pos.y = ");
    if (eminfoBlink()) {
        eprintf(32, 80, 0, 0, ">");
    }
    eprintf(104, 80, 4, 0, "%.2f", p->Pos.y);
}

// Pos Z row: same as Pos X.
void Detail01UpdatePosZ()
{
    EMINFO_WK* p = &W->work[W->cur];
    f32 step;

    if (W->joy.trg & JOY_B) {
        W->sub = 0;
        return;
    }
    step = 0.0f;
    if (W->joy.rep2 & (JOY_UP | JOY_SUP)) {
        step = 1.0f;
    }
    if (W->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        step = -1.0f;
    }
    if (W->joy.on & JOY_R) {
        step *= 10.0f;
    }
    if (W->joy.on & JOY_L) {
        step *= 100.0f;
    }
    p->Pos.z += step;
    eprintf(40, 60, 0, 0, "-- Position Z Move --------");
    eprintf(40, 80, 0, 0, "Pos.z = ");
    if (eminfoBlink()) {
        eprintf(32, 80, 0, 0, ">");
    }
    eprintf(104, 80, 4, 0, "%.2f", p->Pos.z);
}

// Dir row: up/down change the facing angle (radians, wrapped); B back.
void Detail01UpdateDir()
{
    EMINFO_WK* p = &W->work[W->cur];
    f32 step;

    if (W->joy.trg & JOY_B) {
        W->sub = 0;
        return;
    }
    step = 0.0f;
    if (W->joy.rep2 & (JOY_UP | JOY_SUP)) {
        step = 0.012271847f;
    }
    if (W->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        step = -0.012271847f;
    }
    if (W->joy.on & JOY_R) {
        step *= 4.0f;
    }
    if (W->joy.on & JOY_L) {
        step *= 8.0f;
    }
    p->Dir += step;
    p->Dir = LIMIT_ANGLE(p->Dir);
    eprintf(40, 60, 0, 0, "-- Direction Move --------");
    eprintf(40, 80, 0, 0, "Dir = ");
    eprintf(96, 80, 4, 0, "%.2f", p->Dir);
    if (eminfoBlink()) {
        eprintf(32, 80, 0, 0, ">");
    }
}

// Work0 row: up/down +-1 (R x8, L x16) on the type-specific byte; B back.
void Detail01UpdateWork0()
{
    EMINFO_WK* p = &W->work[W->cur];
    int step;

    if (W->joy.trg & JOY_B) {
        W->sub = 0;
        return;
    }
    step = 0;
    if (W->joy.rep2 & (JOY_UP | JOY_SUP)) {
        step = 1;
    }
    if (W->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        step = -1;
    }
    if (W->joy.on & JOY_R) {
        step *= 8;
    }
    if (W->joy.on & JOY_L) {
        step *= 16;
    }
    p->Work0 += step;
    eprintf(40, 60, 0, 0, "-- Direction Move --------");
    eprintf(40, 80, 0, 0, "Work0 = ");
    eprintf(104, 80, 4, 0, "%02x", p->Work0);
    if (eminfoBlink()) {
        eprintf(32, 80, 0, 0, ">");
    }
}

// Work1 row: same as Work0.
void Detail01UpdateWork1()
{
    EMINFO_WK* p = &W->work[W->cur];
    int step;

    if (W->joy.trg & JOY_B) {
        W->sub = 0;
        return;
    }
    step = 0;
    if (W->joy.rep2 & (JOY_UP | JOY_SUP)) {
        step = 1;
    }
    if (W->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        step = -1;
    }
    if (W->joy.on & JOY_R) {
        step *= 8;
    }
    if (W->joy.on & JOY_L) {
        step *= 16;
    }
    p->Work1 += step;
    eprintf(40, 60, 0, 0, "-- Direction Move --------");
    eprintf(40, 80, 0, 0, "Work1 = ");
    eprintf(104, 80, 4, 0, "%02x", p->Work1);
    if (eminfoBlink()) {
        eprintf(32, 80, 0, 0, ">");
    }
}

// Work2 row: same as Work0.
void Detail01UpdateWork2()
{
    EMINFO_WK* p = &W->work[W->cur];
    int step;

    if (W->joy.trg & JOY_B) {
        W->sub = 0;
        return;
    }
    step = 0;
    if (W->joy.rep2 & (JOY_UP | JOY_SUP)) {
        step = 1;
    }
    if (W->joy.rep2 & (JOY_DOWN | JOY_SDOWN)) {
        step = -1;
    }
    if (W->joy.on & JOY_R) {
        step *= 8;
    }
    if (W->joy.on & JOY_L) {
        step *= 16;
    }
    p->Work2 += step;
    eprintf(40, 60, 0, 0, "-- Direction Move --------");
    eprintf(40, 80, 0, 0, "Work2 = ");
    eprintf(104, 80, 4, 0, "%02x", p->Work2);
    if (eminfoBlink()) {
        eprintf(32, 80, 0, 0, ">");
    }
}

// Detail of a typed point: runs the row editor for `sub` (0 select, 1..8 the rows).
static void Detail01Update()
{
    void (*tbl[9])() = {
        Detail01UpdateSel,  Detail01UpdateWorkType, Detail01UpdatePosX,  Detail01UpdatePosY,  Detail01UpdatePosZ,
        Detail01UpdateDir,  Detail01UpdateWork0,    Detail01UpdateWork1, Detail01UpdateWork2,
    };

    tbl[W->sub]();
}

// Mode 3: the detail editor of the selected point (detailFunc by type) and its panel.
static void eminfo_r0_detail()
{
    EMINFO_WK* p = &W->work[W->cur];

    eminfoCursorCenter();
    if (p->type < 20) {
        detailFunc[p->type]();
    }
}

// Draws the B menu with the cursor.
void eminfo_menu_disp()
{
    int i;

    eprintf(40, 170, 0, 0, "-- MENU --------");
    eminfoCursorCenter();
    for (i = 0; i < 5; i++) {
        eprintf(40, 186 + i * 16, (W->cursor == i) ? 4 : 0, 0, "%s", menuName[i]);
        if (W->cursor == i && eminfoBlink()) {
            eprintf(32, 186 + i * 16, 0, 0, ">");
        }
    }
}

// Draws the detail panel of point `no`: type name, the rows with their values, the cursor row.
void eminfo_detail_disp(int no)
{
    EMINFO_WK* p = &W->work[no];
    int i;
    int y = 80;

    eprintf(40, 60, 0, 0, "-- DETAIL --------");
    if (p->type == 0) {
        return;
    }
    for (i = 0; i < 9; y += 16, i++) {
        eprintf(40, y, (i == W->cursor) ? 4 : 0, 0, "%s", detailName[i]);
        if (i == W->cursor && eminfoBlink()) {
            eprintf(32, y, 0, 0, ">");
        }
        switch (i) {
        case 0:
            eprintf(136, y, 0, 0, "[ %s ]", workTypeName[p->type]);
            break;
        case 1:
            eprintf(136, y, 0, 0, "[ %.2f ]", p->Pos.x);
            break;
        case 2:
            eprintf(136, y, 0, 0, "[ %.2f ]", p->Pos.y);
            break;
        case 3:
            eprintf(136, y, 0, 0, "[ %.2f ]", p->Pos.z);
            break;
        case 4:
            eprintf(136, y, 0, 0, "[ %.2f, %.2f ]", p->Dir, p->Dir * 0.017453292f);
            break;
        case 5:
            eprintf(136, y, 0, 0, "[ %d ]", p->Work0);
            break;
        case 6:
            eprintf(136, y, 0, 0, "[ %d ]", p->Work1);
            break;
        case 7:
            eprintf(136, y, 0, 0, "[ %d ]", p->Work2);
            break;
        case 8:
            break;
        }
    }
}

// Appends a point (copy of the template) at the cursor's ground position; up to 256.
void eminfoAddWork()
{
    int i;

    for (i = 0; i < 256; i++) {
        EMINFO_WK* p = &W->work[i];

        if (p->type == 0) {
            W->cur = i;
            *p = W->copy;
            Get3DPosFrom2D(&p->Pos, W->cx, W->cy, 100000000.0f);
            p->Pos.y += 50.0f;
            W->num++;
            break;
        }
    }
}

// Removes point `no` (the rest shift down, the tail cleared).
void eminfoDeleteWork(int no)
{
    int i;

    for (i = no; i < W->num - 1; i++) {
        W->work[i] = W->work[i + 1];
    }
    W->num--;
    for (i = W->num; i < 256; i++) {
        EMINFO_WK* q = &W->work[i];

        memclr_asm(q, sizeof(EMINFO_WK));
        q->type = 0;
    }
}

// Finds the typed point whose screen position is nearest the cursor within the pick radius
// (W->near, -1 none).
void eminfoGetNearPoint(Vec* cursor)
{
    Vec scr;
    f32 min = 1254400.0f;
    int near = -1;
    int i;

    for (i = 0; i < 256; i++) {
        EMINFO_WK* p = &W->work[i];

        if (p->type != 0) {
            Vec posCopy = p->Pos;
            if (GetScreenPos(&posCopy, &scr)) {
                f32 d = (scr.x - cursor->x) * (scr.x - cursor->x) + (scr.y - cursor->y) * (scr.y - cursor->y);

                if (!(d > min)) {
                    min = d;
                    near = i;
                }
            }
        }
    }
    W->near = near;
}

// Moves the screen cursor onto point `no`.
void eminfoCursorToWork(int no)
{
    Vec scr;
    EMINFO_WK* p = &W->work[no];

    if (p->type != 0) {
        Vec posCopy = p->Pos;
        if (GetScreenPos(&posCopy, &scr)) {
            W->cx = scr.x;
            W->cy = scr.y;
        }
    }
}

// Draws every point (direction triangle, number; the selected / nearest one highlighted), the 2D
// cursor, the EM2F / EM39 / EM3A route and area overlays, the selected point's values and the copy
// template's type.
void eminfoDisp()
{
    int i;
    int t;
    int on;

    eprintf(40, 20, 0, 0, "<<<<< EM INFO TOOL >>>>>");
    t = pG->Frame_cnt & 0xF;
    if (pG->Frame_cnt & 0x10) {
        t = 15 - t;
    }
    t *= 3;
    t = (t << 16) + (t << 8) + t;
    for (i = 0; i < 256; i++) {
        EMINFO_WK* p = &W->work[i];

        if (p->type != 0) {
            u32 c = 0x30508020;

            if (W->cur == i) {
                c = t + 0x60A0D060;
            }
            eminfoDrawDir(&p->Pos, p->Dir, c, 0xFF80FF80);
            eminfoDispNo(&p->Pos, i);
        }
    }
    TprimDraw2D(0);
    on = 1;
    if ((W->joy.on & (JOY_A | JOY_Y)) == 0) {
        on = 0;
    }
    TprimDrawCursor((Vec*) &W->cx, 0.0f, &cursorCol[on]);
    eminfoEm2fRouteDisp();
    eminfoEm39AreaDisp();
    eminfoEm3aRouteDisp();
    eminfoTargetDisp(W->cur);
    eminfoCopyDisp();
}

// Filled triangle at `pos` pointing along `dir` (fill `col`, outline `col2`).
void eminfoDrawDir(Vec* pos, f32 dir, u32 col, u32 col2)
{
    static const f32 unused[5] = {50.0f, 100.0f, 0.0f, -100.0f, 500.0f};
    Mtx m;
    Vec v[3];
    Mtx m2;
    Vec poly[3];

    PSMTXRotRad(m, 'y', dir);
    TransMatrix(m, pos);
    v[0].x = 0.0f;
    v[0].y = 0.0f;
    v[0].z = 600.0f;
    v[1].x = 300.0f;
    v[1].y = 0.0f;
    v[1].z = -300.0f;
    v[2].x = -300.0f;
    v[2].y = 0.0f;
    v[2].z = -300.0f;
    PSMTXMultVec(m, &v[0], &v[0]);
    PSMTXMultVec(m, &v[1], &v[1]);
    PSMTXMultVec(m, &v[2], &v[2]);
    poly[0] = v[0];
    poly[1] = v[1];
    poly[2] = v[2];
    Draw_poly(poly, col, 1);
    Draw_line3d(&v[0], &v[1], col2, 0);
    Draw_line3d(&v[1], &v[2], col2, 0);
    Draw_line3d(&v[2], &v[0], col2, 0);
}

// Prints the point number at its screen position.
void eminfoDispNo(Vec* pos, int no)
{
    Vec scr;

    Vec posCopy = *pos;

    if (GetScreenPos(&posCopy, &scr)) {
        if (no == W->cur) {
            eprintf2(10, 16, (u32) scr.x - 20, (u32) scr.y + 16, 4, 0, "[%02d]", no);
        } else {
            eprintf2(10, 16, (u32) scr.x - 20, (u32) scr.y + 16, 7, 0, "[%02d]", no);
        }
    }
}

// Bottom panel of point `no`: number / type, position, direction, the three work bytes.
void eminfoTargetDisp(int no)
{
    EMINFO_WK* p = &W->work[no];

    if (no > 255) {
        return;
    }
    if (p->type > 19) {
        return;
    }
    if (p->type == 0) {
        return;
    }
    eprintf(40, 388, 0, 0, "Pos[ %.2f, %.2f, %.2f]   Dir[ %.2f, %.2f ]", p->Pos.x, p->Pos.y, p->Pos.z, p->Dir,
            p->Dir * 57.295776f);
    eprintf(40, 404, 0, 0, "Work0[ 0x%02x ], Work1[ 0x%02x ], Work2[ 0x%02x ]", p->Work0, p->Work1, p->Work2);
    eprintf(40, 372, 0, 0, "Work No.[ %02d / %02d ] -->> WorkType [ %s ]", no, 255, workTypeName[p->type]);
}

// Shows the copy template's type ("Copy type").
void eminfoCopyDisp()
{
    if (W->copy.type < 20) {
        eprintf(350, 292, 0, 0, "Copy type");
        eprintf(350, 308, 0, 0, "[ %s ]", workTypeName[W->copy.type]);
    }
}

// Draws the EM2F (type 2) route: consecutive points joined, closed back to the first, coloured
// by Work0.
void eminfoEm2fRouteDisp()
{
    EMINFO_WK* first = NULL;
    EMINFO_WK* cur = NULL;
    EMINFO_WK* prev = NULL;
    u32 i;

    for (i = 0; i < W->num; i++) {
        EMINFO_WK* p = &W->work[i];

        if (p->type == 2) {
            if (cur == NULL) {
                first = p;
            }
            prev = cur;
            cur = p;
            if (cur && prev) {
                u32 c;

                switch (prev->Work0) {
                case 0:
                default:
                    c = 0xFFFFFFFF;
                    break;
                case 1:
                    c = 0xFF0000FF;
                    break;
                case 2:
                    c = 0xFFFF0000;
                    break;
                }
                Draw_line3d(&prev->Pos, &cur->Pos, c, 0);
            }
        }
    }
    if (first && prev && cur) {
        u32 c;

        switch (cur->Work0) {
        case 0:
        default:
            c = 0xFFFFFFFF;
            break;
        case 1:
            c = 0xFF0000FF;
            break;
        case 2:
            c = 0xFFFF0000;
            break;
        }
        Draw_line3d(&cur->Pos, &first->Pos, c, 0);
    }
}

// Draws the No.3 snipe (type 14) areas: each Work1 == 0 point paired with its Work1 == 1 partner of
// the same Work0 / Work2 as a box, coloured by Work0.
void eminfoEm39AreaDisp()
{
    u32 i;
    u32 j;
    EMINFO_WK* q;
    Vec a;
    Vec b;
    u32 c1;
    u32 c2;

    for (i = 0; i < W->num; i++) {
        EMINFO_WK* p = &W->work[i];

        if (p->type == 14) {
            if (p->Work1 == 0) {
                for (j = 0; j < W->num; j++) {
                    q = &W->work[j];
                    if (q->type == 14 && p->Work0 == q->Work0 && q->Work1 == 1 && p->Work2 == q->Work2) {
                        a = p->Pos;
                        b = q->Pos;
                        a.y += 500.0f;
                        b.y += 500.0f;
                        switch (p->Work0) {
                        case 0:
                        default:
                            c1 = 0xFFFFFFFF;
                            c2 = 0xFFFFFFFF;
                            break;
                        case 1:
                            c1 = 0xFFFF0000;
                            c2 = 0xFF0000FF;
                            break;
                        case 2:
                            c1 = 0xFF00FF00;
                            c2 = 0x00FF00FF;
                            break;
                        }
                        Draw_line3d(&a, &b, c1, 0);
                        Draw_sphere(&q->Pos, 2000.0f, c2, 1, 1);
                        switch (p->Work0) {
                        case 0:
                        default:
                            c2 = 0xFFFFFFFF;
                            break;
                        case 1:
                            c2 = 0x0000FFFF;
                            break;
                        case 2:
                            c2 = 0x00FF00FF;
                            break;
                        }
                        Draw_sphere(&p->Pos, 500.0f, c2, 1, 1);
                    }
                }
                for (j = 0; j < W->num; j++) {
                    q = &W->work[j];
                    if (q->type == 14 && p->Work0 == q->Work0 && q->Work1 == 2 && p->Work2 == q->Work2) {
                        a = p->Pos;
                        b = q->Pos;
                        a.y += 500.0f;
                        b.y += 500.0f;
                        switch (p->Work0) {
                        case 0:
                        default:
                            c1 = 0x80808080;
                            c2 = 0x80808080;
                            break;
                        case 1:
                            c1 = 0x80800000;
                            c2 = 0x80000080;
                            break;
                        case 2:
                            c1 = 0x80008000;
                            c2 = 0x00800080;
                            break;
                        }
                        Draw_line3d(&a, &b, c1, 0);
                        Draw_sphere(&q->Pos, 2000.0f, c2, 1, 1);
                    }
                }
                for (j = 0; j < W->num; j++) {
                    q = &W->work[j];
                    if (q->type == 14 && p->Work0 == q->Work0 && q->Work1 == 3 && p->Work2 == q->Work2) {
                        a = p->Pos;
                        b = q->Pos;
                        a.y += 500.0f;
                        b.y += 500.0f;
                        switch (p->Work0) {
                        case 0:
                        default:
                            c1 = 0x40404040;
                            c2 = 0x40404040;
                            break;
                        case 1:
                            c1 = 0x40400000;
                            c2 = 0x40000040;
                            break;
                        case 2:
                            c1 = 0x40004000;
                            c2 = 0x00400040;
                            break;
                        }
                        Draw_line3d(&a, &b, c1, 0);
                        Draw_sphere(&q->Pos, 500.0f, c2, 1, 1);
                    }
                }
            }
        }
    }
}

// Draws the EM3A (type 19) routes: spheres on the points, lines to the next point of the same
// route (Work1) in order (Work2).
void eminfoEm3aRouteDisp()
{
    u32 i;
    u32 j;

    for (i = 0; i < W->num; i++) {
        EMINFO_WK* p = &W->work[i];

        if (p->type == 19 && p->Work1 != 0) {
            if (p->Work2 == 0) {
                Draw_sphere(&p->Pos, 500.0f, 0xFFFFFFFF, 1, 1);
            } else {
                Draw_sphere(&p->Pos, 500.0f, 0x60606060, 1, 1);
            }
            for (j = 0; j < W->num; j++) {
                EMINFO_WK* q = &W->work[j];

                if (q->type == 19 && q->Work1 == p->Work1 && q->Work2 == p->Work2 + 1) {
                    Vec a;
                    Vec b;

                    a = p->Pos;
                    b = q->Pos;
                    a.y += 250.0f;
                    b.y += 250.0f;
                    Draw_line3d(&a, &b, 0xFFFF0000, 0);
                }
            }
        }
    }
}

// Writes the point count + the used points to the room's .emi on x:.
void eminfoFileSave()
{
    char path[0x100];
    int size = W->num * sizeof(EMINFO_WK) + 8;

    eminfoSetFileName(path);
    HDWrite(path, &W->num, size);
}

// Reads the room's .emi (count + points) into the work; 0 when missing.
int eminfoFileLoad()
{
    char path[0x100];
    int size;

    memclr_asm(W->work, sizeof(W->work));
    W->num = 0;
    eminfoSetFileName(path);
    size = HDRead(path, &W->num);
    if (size == 0) {
        return 0;
    }
    return size;
}

// x:\soft\room\st<n>\r<room>\r<room>.emi
void eminfoSetFileName(char* path)
{
    sprintf(path, "x:\\soft\\room\\st%1x\\r%1x%02x\\r%1x%02x.emi", pG->stage_no, pG->stage_no, pG->room_no,
            pG->stage_no, pG->room_no);
}

// START toggles camera mode: the debug camera takes pad 1 and the tool's pad copy is cleared.
void eminfoCameraMove()
{
    W->joy = Joy[0];
    if (Joy[0].trg & JOY_START) {
        W->camMode ^= 1;
    }
    if (W->camMode) {
        CamDbg.move(&pG->Camera, Joy, 0);
        W->joy.trg = 0;
        W->joy.on = 0;
        W->joy.rep = 0;
        W->joy.rep2 = 0;
        DbgFlagOn(pG, DBG_DBG_CAM);
        if (pG->Frame_cnt & 0x10) {
            eprintf(320, 24, 4, 0, "1P CAMERA MODE");
        }
        eminfoCursorCenter();
    }
}
