#include "types.h"
#define DB_TOOLBASE_IMPLEMENTATION
#include "dbg_tool.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "dbmodule.h"

// Debug tool window / button base (D:/Bio4/Prog/db_toolbase.cpp): colour packing, box drawing and the
// cDbgWindow button container the tool editors derive from.

u32 MakeCol(f32 r, f32 g, f32 b, f32 a)
{
    u32 col;

    col = (u8) (a * 255.0f) << 24;
    col += (u8) (r * 255.0f) << 16;
    col += (u8) (g * 255.0f) << 8;
    col += (u8) (b * 255.0f);
    return col;
}

// Screen-space rectangle outline (four Draw_line) in a 0..1 float colour.
void DbgDrawBox(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a)
{
    Vec p0;
    Vec p1;
    f32 x2 = x + w;
    f32 y2;

    p0.x = x;
    p0.y = y;
    p1.x = x2;
    p1.y = y;
    Draw_line(&p0, &p1, MakeCol(r, g, b, a));
    y2 = y + h;
    p0.x = x2;
    p0.y = y;
    p1.x = x2;
    p1.y = y2;
    Draw_line(&p0, &p1, MakeCol(r, g, b, a));
    p0.x = x2;
    p0.y = y2;
    p1.x = x;
    p1.y = y2;
    Draw_line(&p0, &p1, MakeCol(r, g, b, a));
    p0.x = x;
    p0.y = y2;
    p1.x = x;
    p1.y = y;
    Draw_line(&p0, &p1, MakeCol(r, g, b, a));
}

// Filled screen-space rectangle (Draw_quad) in a 0..1 float colour.
void DbgDrawBoxFill(f32 x, f32 y, f32 w, f32 h, f32 r, f32 g, f32 b, f32 a)
{
    Vec p0;
    Vec p1;

    p0.x = x;
    p0.y = y;
    p1.x = w;
    p1.y = h;
    Draw_quad(&p0, &p1, MakeCol(r, g, b, a));
}

// Adds a button at text cell (bx, by) inside the window with cursor coordinates (bcx, bcy; 0xFFFF =
// not selectable) and its exec / update callbacks; grows the window size and cursor range, the
// first selectable button becomes current / top, the last one bottom.
void cDbgWindow::AddButton(int bx, int by, const char* name, int bcx, int bcy, void (*func)(cDbgButton*),
                           void (*update)(cDbgButton*))
{
    cDbgButton* b;

    m_pButList[m_nBut] = b = new cDbgButton(bx, by, name, bcx, bcy);
    b->m_pFuncUpdate = update;
    b->m_pFuncExec = func;
    if (m_pButList[m_nBut] == 0) {
        pLog->err(0, 0, "AddButton(): new failed.");
        return;
    }
    if (m_wx < bx + strlen(name)) {
        m_wx = bx + strlen(name);
    }
    if (m_wy < by) {
        m_wy = by;
    }
    if (bcx < 0xFFFF && bcy < 0xFFFF) {
        if (m_pCurrentBut == 0) {
            m_pStartBut = m_pCurrentBut = m_pButList[m_nBut];
        }
        m_pEndBut = m_pButList[m_nBut];
        if (m_max_cx < bcx) {
            m_max_cx = bcx;
        }
        if (m_max_cy < bcy) {
            m_max_cy = bcy;
        }
    }
    m_nBut++;
}

// Finds the button at cursor cell (bcx, bcy); 1 and *out when found.
int cDbgWindow::FindButton(int bcx, int bcy, cDbgButton** out)
{
    u32 i;

    *out = 0;
    for (i = 0; i < m_nBut; i++) {
        if (m_pButList[i]->GetCx() == bcx && m_pButList[i]->GetCy() == bcy) {
            *out = m_pButList[i];
            return 1;
        }
    }
    return 0;
}

// Window input (pad 1, repeat): d-pad / stick move the cursor over the button grid with wrap,
// every button's update callback runs; returns 0 on B (close the window).
int cDbgWindow::LocalUpdate()
{
    int ret = 1;

    CursorMove();
    ButtonAllUpdate();
    if (Joy[0].trg & 0x200) {
        ret = 0;
    }
    return ret;
}

// Draws the button labels (row 0 is the title line), the blinking ">" before the current button
// and its highlight box.
void cDbgWindow::LocalDisp()
{
    u32 i;
    cDbgButton* cur;

    for (i = 0; i < m_nBut; i++) {
        cDbgButton* b = m_pButList[i];
        int by = m_py + 1;

        eprintf2(8, 12, (m_px + b->m_px) * 8, (by + b->m_py) * 14, 0x10, 0, b->m_pStr);
    }
    cur = m_pCurrentBut;
    if (cur) {
        int bx = m_px;
        int by = m_py + 1;

        if (pG->Frame_cnt & 4) {
            eprintf2(8, 12, (bx + cur->m_px - 1) * 8, (by + cur->m_py) * 14, 0, 0, cDbgStr::cursor());
        }
        eprintf2(8, 12, (bx + cur->m_px) * 8, (by + cur->m_py) * 14, 0, 0, cur->m_pStr);
        {
            f32 fx = (f32) ((bx + cur->m_px) * 8);
            f32 fh = 14.0f;
            f32 mgn = 2.0f;
            f32 zero = 0.0f;

            DbgDrawBoxFill(fx - mgn, (f32) ((by + cur->m_py) * 14) - mgn, (f32) (cur->m_strlen * 8) + zero,
                           fh + mgn, 0.7f, 0.7f, zero, 0.3f);
        }
    }
}

