#ifndef DB_SCTRL_H
#define DB_SCTRL_H

#include "types.h"
#include "vec.h"
#include "camera.h"
#include "hermite.h"

// Hermite S-curve editor of the debug tools (tools/db_sctrl.cpp, D:/Bio4/Prog/db_sctrl.cpp; the same
// object in t_id and t_event): a 2D graph drawn in world space in front of the camera, a screen-space
// cursor, key points with in/out tangent handles and a settings menu.
typedef struct _DB_SCTRL {
    s8 Rno0;      // 0x00  0 edit, 1 menu, 2 quit
    s8 Rno1;         // 0x01
    s8 Rno2;
    s8 Rno3;
    u8 pad_4[4];
    int sX;           // 0x08  menu position
    int sY;           // 0x0C
    s8 Menu_no;       // 0x10  menu row
    s8 Sub_menu_no;          // 0x11  menu column
    u8 blink;        // 0x12  frame counter (bits 3/4 blink the cursor)
    u8 pad_13;
    Vec Cursor;         // 0x14  screen-space cursor
    u8 pad_20[4];
    f32 Tlimit;        // 0x24  graph range
    f32 Blimit;        // 0x28
    f32 Rlimit;        // 0x2C
    f32 Llimit;        // 0x30
    f32 grid_disp_X;       // 0x34  drawn grid spacing (0 = none)
    f32 grid_disp_Y;       // 0x38
    Vec Grid;        // 0x3C  grid-lock step (x, y)
    f32 Hscale;      // 0x48  Scale menu factors
    f32 Vscale;      // 0x4C
    Mtx Scrn_mat;         // 0x50  screen -> world (the camera matrix moved in front of the camera)
    HERMITE_1_PTR* pScurve; // 0x80
    s8 p_no;         // 0x84  grabbed key, -1 = none
    s8 s_no;    // 0x85  insertion index found on the curve, -1 = none
    u8 pad_86[2];
    Vec New_point;   // 0x88
    u8 YesNo;          // 0x94  YES/NO of the delete / insert / clear prompts
    u8 pad_95[3];
    u32 Graph_flag;       // 0x98  bit0 grid lock, bit1 automatic range
    char labelX[8];  // 0x9C
    char labelY[8];  // 0xA4
} DB_SCTRL;

void SctrlInitAxisRange(DB_SCTRL* w, f32 xMax, f32 xMin, f32 yMax, f32 yMin);
void SctrlAdjustAxisRange(DB_SCTRL* w);
void SctrlSetAxisLabel(DB_SCTRL* w, const char* x, const char* y);
void SctrlInitCursor(DB_SCTRL* w, f32 x, f32 y);
void dbSctrlScreenOrientation(DB_SCTRL* w, CAMERA* cam, f32 fovy);
// returns the routine's result (0 once the editor quits; t_event's fog / focus tools test it)
int DbSctrl(DB_SCTRL* w, int x, int y);
int grabPoint(DB_SCTRL* w);
int grabLine(DB_SCTRL* w);
void deletePoint(DB_SCTRL* w);
void insertPoint(DB_SCTRL* w);
void drawCursor(DB_SCTRL* w);
void drawAxis(DB_SCTRL* w);
void drawScurve(DB_SCTRL* w);
void posScreen2Graph(DB_SCTRL* w, Vec* scr, Vec* gph);
void posGraph2Screen(DB_SCTRL* w, Vec* gph, Vec* scr);
void posScreen2World(DB_SCTRL* w, Vec* scr, Vec* out);
void posGraph2World(DB_SCTRL* w, Vec* gph, Vec* out);
void posGridLock(Vec* grid, Vec* in, Vec* out);
void posScreen2GridLock(DB_SCTRL* w, Vec* scr, Vec* gph);

#endif
