#include "types.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "main_mem.h"
#include "file.h"
#include "db_log.h"
#include "t_util.h"
#include <stdio.h>


// Room constant editor (Tools/t_cons.cpp): edits the per-room / core `.cns` limit tables (work counts) and
// writes them back to the host.

// one .cns table: `num` limits, an enable bitmap and the values (0x1A0 bytes)
struct ConsTable {
    u32 num;         // 0x00
    u32 enable[3];   // 0x04
    int val[100];    // 0x10
};

struct ConsParam {
    const char* name;
    int min;
    int max;
};

struct ConsWork {
    u8 step;          // 0x00  index into consFunc
    u8 init;          // 0x01  save / quit prompt initialised
    u8 x2;
    u8 x3;
    u8 pad_4[4];
    int x8;
    int xC;
    int x10;
    int x14;
    f32 f18;
    f32 f1C;
    f32 f20;
    f32 f24;
    u8 pad_28[4];
    JOY joy[2];       // 0x2C
    u8 counter;       // 0x4FC  frame counter (cursor blink)
    u8 alive;         // 0x4FD  0 = leave the tool
    u8 cursor;        // 0x4FE
    u8 debugBak;      // 0x4FF
    u8 x500;
    u8 x501;
    u8 x502;
    u8 pad_503[0x520 - 0x503];
    ConsTable room;   // 0x520
    ConsTable core;   // 0x6C0
};

static ConsWork consWork;
static ConsWork* consWorkPtr;
#define pCons consWorkPtr

static char* roomPath = "x:\\soft/room/st%x/r%x%02x/r%x%02x.cns";
static char* corePath = "x:\\soft/room/etc/core/core.cns";

static ConsParam cons_tbl[100] = {
    {"ENEMY NUM       ", 1, 200},
    {"OBJ NUM         ", 0, 800},
    {"ESP NUM         ", 0, 0x4000},
    {"ESPGEN NUM      ", 0, 1000},
    {"CTRL NUM        ", 0, 200},
    {"LIGHT NUM       ", 0, 200},
    {"PARTS NUM       ", 0, 5000},
    {"MODELINFO NUM   ", 0, 3000},
    {"PRIM NUM        ", 0, 0x800000},
    {"EVT             ", 0, 100},
    {"SAT             ", 1, 200},
    {"EAT             ", 1, 200},
};

void toolInit();
int ConsMove();
static void soft_menu();
static void soft_room();
static void soft_core();
void cons_edit(ConsTable* t);
static void soft_save();
void save_main(const char* path, ConsTable* t);
static void quit();
void printCursor(int x, int y);
void clearWork();
void read_main(const char* path, ConsTable* t);

static void (*consFunc[])() = {soft_menu, soft_room, soft_core, soft_save, quit};

// CONS TOOL entry (debug menu 14): suspends the game task, runs ConsMove every frame until it
// returns 0, restores the tool flags and ends.
void ToolCons()
{
    TaskSuspend(0);
    TaskSleep(1);
    TutilInitDefault();
    toolInit();
    while (ConsMove()) {
        TaskSleep(1);
    }
    TutilQuitDefault();
    TaskSignal(0);
    TaskExit();
}

// Tool start: log window with the build stamp, clears the work and reads the core table
// (etc/core/core.cns) and the room table (st<n>/r<room>/r<room>.cns) from the host.
void toolInit()
{
    char path[256];

    pLog->clear();
    pLog->modeSet(0x18, 0x8C, 0x5A, 0xA);
    pLog->mes(0, 0, "BIO HAZARD 4 CONSTANT EDITOR");
    pLog->mes(0, 0, "MEM:%08X", pCons);
    pLog->mes(0, 0, "%s", "Nov 25 2004");  // __DATE__ of the original build
    pLog->mes(0, 0, "%s", "10:05:27");     // __TIME__
    pCons = &consWork;
    memclr_asm(pCons, sizeof(ConsWork));
    pCons->alive = 1;
    pCons->x502 = 0;
    sprintf(path, corePath);
    read_main(path, &pCons->core);
    sprintf(path, roomPath, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no);
    read_main(path, &pCons->room);
}

// Frame: copies both pads into the work, counts the blink counter, runs consFunc[step] (menu,
// room, core, save, quit); returns `alive`.
int ConsMove()
{
    eprintf(0x18, 0xE, 0, 0, "CONS TOOL [Ver.%s %s]", "Nov 25 2004", "10:05:27");
    JOY_COPY(pCons, 0x2C, 0);
    JOY_COPY(pCons, 0x294, 1);
    pCons->counter++;
    if (Joy[0].rep != 0) {
        pCons->counter = 0;
    }
    consFunc[pCons->step]();
    return pCons->alive;
}

static char* consMenu[5] = {"ROOM", "CORE", "SAVE", "QUIT"};

// MENU: ROOM / CORE / SAVE / QUIT (step = row + 1); B jumps to QUIT.
static void soft_menu()
{
    eprintf(0x20, 0x2A, 4, 0, "MENU");
    dispList(0x20, 0x38, consMenu, 4);
    printCursor(3, pCons->cursor + 4);
    if (pCons->joy[0].rep & 8) {
        pCons->cursor = (pCons->cursor + 4 - 1) % 4;
    } else if (pCons->joy[0].rep & 4) {
        pCons->cursor = (pCons->cursor + 4 + 1) % 4;
    }
    if (pCons->joy[0].rep & 0x100) {
        pCons->step = pCons->cursor + 1;
        pCons->init = pCons->x2 = pCons->x3 = 0;
        clearWork();
    }
    if (pCons->joy[0].rep & 0x200) {
        pCons->cursor = 3;
    }
}

// R<room> CONSTANT TABLE: edits the room's table (cons_edit); B back.
static void soft_room()
{
    eprintf(0x20, 0x2A, 4, 0, "R%03X CONSTANT TABLE", pG->room_id);
    cons_edit(&pCons->room);
    if (pCons->joy[0].rep & 0x200) {
        clearWork();
        pCons->step = 0;
    }
}

// CORE CONSTANT TABLE: edits the core table; B back.
static void soft_core()
{
    eprintf(0x20, 0x2A, 4, 0, "CORE CONSTANT TABLE");
    cons_edit(&pCons->core);
    if (pCons->joy[0].rep & 0x200) {
        clearWork();
        pCons->step = 0;
    }
}

// Table editor: 12 rows (ENEMY / OBJ / ESP / ESPGEN / CTRL / LIGHT / PARTS / MODELINFO / PRIM NUM,
// EVT, SAT, EAT) with their values; up/down pick, left/right change within cons_tbl's min..max
// (A x10, X x100; the PRIM row steps by 0x100), Y toggles the row's enable bit, B back to the menu.
void cons_edit(ConsTable* t)
{
    u32 i;
    int step;

    for (i = 0; i < t->num; i++) {
        int col = 0;

        if (!(t->enable[i >> 5] & (1 << (i & 31)))) {
            col = 0x14;
        }
        if (i == 8) {
            eprintf(0x20, 0x38 + i * 14, col, 0, "%s %8X", cons_tbl[i].name, t->val[i]);
        } else {
            eprintf(0x20, 0x38 + i * 14, col, 0, "%s %4d", cons_tbl[i].name, t->val[i]);
        }
    }
    eprintf(0x150, 0x46, 4, 0, "HELP");
    eprintf(0x150, 0x54, 0, 0, "UD:SELECT PROPATY");
    eprintf(0x150, 0x62, 0, 0, "RL:INCREASE/DECREASE");
    eprintf(0x150, 0x70, 0, 0, " Y:ENABLE/DISABLE");
    eprintf(0x150, 0x7E, 0, 0, " A:SPEED UP");
    if (pCons->cursor == 8) {
        step = 0x100;
        if (Joy[0].on & 0x100) {
            step = 0x1000;
        }
        if (Joy[0].on & 0x400) {
            step = 0x10000;
        }
    } else {
        step = 1;
        if (Joy[0].on & 0x100) {
            step = 10;
        }
        if (Joy[0].on & 0x400) {
            step = 100;
        }
    }
    printCursor(3, pCons->cursor + 4);
    if (pCons->joy[0].rep & 2) {
        if (t->val[pCons->cursor] < cons_tbl[pCons->cursor].max) {
            t->val[pCons->cursor] += step;
            if (t->val[pCons->cursor] > cons_tbl[pCons->cursor].max) {
                t->val[pCons->cursor] = cons_tbl[pCons->cursor].max;
            }
        }
    }
    if (pCons->joy[0].rep & 1) {
        if (t->val[pCons->cursor] > cons_tbl[pCons->cursor].min) {
            t->val[pCons->cursor] -= step;
            if (t->val[pCons->cursor] < cons_tbl[pCons->cursor].min) {
                t->val[pCons->cursor] = cons_tbl[pCons->cursor].min;
            }
        }
    }
    if (pCons->joy[0].rep & 8) {
        pCons->cursor = (t->num + pCons->cursor - 1) % t->num;
    } else if (pCons->joy[0].rep & 4) {
        pCons->cursor = (t->num + pCons->cursor + 1) % t->num;
    }
    if (pCons->joy[0].rep & 0x800) {
        t->enable[pCons->cursor >> 5] ^= 1 << pCons->cursor;
    }
}

// SAVE YES/NO: writes both tables to the host (save_main) and logs it.
static void soft_save()
{
    char path[256];

    eprintf(0x20, 0x2A, 4, 0, "SAVE");
    if (pCons->init == 0) {
        pCons->cursor = 1;
        pCons->init = 1;
    }
    eprintf(0x30, 0x46, pCons->cursor != 0 ? 0x14 : 0, 0, "YES");
    eprintf(0x30, 0x54, pCons->cursor != 1 ? 0x14 : 0, 0, "NO");
    if (pCons->joy[0].rep & 8) {
        pCons->cursor = 0;
    } else if (pCons->joy[0].rep & 4) {
        pCons->cursor = 1;
    }
    if (pCons->joy[0].rep & 0x100) {
        if (pCons->cursor == 0) {
            sprintf(path, roomPath, pG->stage_no, pG->stage_no, pG->room_no, pG->stage_no, pG->room_no);
            save_main(path, &pCons->room);
            pLog->mes(0, 0, "DATA SAVE COMPLEATED");
        } else {
            clearWork();
            pCons->step = 0;
        }
    }
    if (pCons->joy[0].rep & 0x200) {
        clearWork();
        pCons->step = 0;
    }
}

// Writes a .cns image: count (highest enabled row + 1), enable words, values; `num` back to 12.
void save_main(const char* path, ConsTable* t)
{
    u32 s = t->num >> 5;
    u32 n = t->num + 2;
    u32 size = (s + n) * 4;
    u32* buf;
    int i;

    buf = (u32*) Debug_alloc(size, 1);
    t->num = i = 0;
    for (; i < 100; i++) {
        if (t->enable[i / 32] & (1 << i)) {
            t->num = i + 1;
        }
    }
    buf[0] = t->num;
    memcpy(buf + 1, t->enable, ((t->num >> 5) << 2) + 4);
    memcpy((u32*) (((t->num >> 5) << 2) + (u32) buf + 8), t->val, t->num * 4);
    HDWrite(path, buf, size);
    Debug_free(buf);
    t->num = 0xC;
}

// QUIT ? YES/NO: YES clears `alive`.
static void quit()
{
    eprintf(0x20, 0x2A, 4, 0, "QUIT ?");
    if (pCons->init == 0) {
        pCons->cursor = 1;
        pCons->init = 1;
    }
    eprintf(0x30, 0x46, pCons->cursor != 0 ? 0x14 : 0, 0, "YES");
    eprintf(0x30, 0x54, pCons->cursor != 1 ? 0x14 : 0, 0, "NO");
    if (pCons->joy[0].rep & 8) {
        pCons->cursor = 0;
    } else if (pCons->joy[0].rep & 4) {
        pCons->cursor = 1;
    }
    if (pCons->joy[0].rep & 0x100) {
        if (pCons->cursor == 0) {
            pCons->alive = 0;
            pG->debug_mode = pCons->debugBak;
            pLog->clear();
            pLog->modeReset();
            DpfFlagOff(pG, DPF_SCR);
        } else {
            clearWork();
            pCons->step = 0;
        }
    }
    if (pCons->joy[0].rep & 0x200) {
        clearWork();
        pCons->step = 0;
    }
}

// Blinking ">" at text cell (x, y).
void printCursor(int x, int y)
{
    if (!(pCons->counter & 8)) {
        eprintf(x * 8, y * 14, 0, 0, ">");
    }
}

// Resets the menu cursor and the prompt init flag.
void clearWork()
{
    pCons->cursor = 0;
    pCons->x500 = pCons->x501 = 0;
    pCons->x8 = pCons->xC = pCons->x10 = pCons->x14 = 0;
    pCons->f18 = pCons->f1C = pCons->f20 = pCons->f24 = 0.0f;
}

// Reads a .cns image (count, enable words, values) into `t`; an empty table with a warning when
// the file is missing. `num` is forced to the 12 editable rows.
void read_main(const char* path, ConsTable* t)
{
    u32* buf;
    u32 size;

    t->num = 0;
    memclr_asm(t->enable, sizeof(t->enable));
    memclr_asm(t->val, sizeof(t->val));
    size = HDReadMemAlloc(path, (void**) &buf);
    if (size == 0) {
        pLog->warn(0, 0, "NO CONS FILE %s", path);
        t->num = size;
        return;
    }
    t->num = buf[0];
    memcpy(t->enable, buf + 1, ((t->num >> 5) << 2) + 4);
    size = (t->num >> 5) << 2;
    {
        u32 ofs = size + 8;

        memcpy(t->val, (u32*) (ofs + (u32) buf), t->num * 4);
    }
    Mem_free(buf);
    t->num = 0xC;
}
