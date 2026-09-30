#include "types.h"
#include "vec.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "main_mem.h"
#include "math_sub.h"
#include "hermite.h"
#include "dbmodule.h"
#include "db_log.h"
#include "db_sctrl.h"
#include <string.h>

// Hermite S-curve editor of the debug tools (D:/Bio4/Prog/db_sctrl.cpp; the same object in t_id and
// t_event). Screen space is 640x480 centred on the graph centre; the graph is drawn in world space
// through w->mtx (the camera matrix pushed 240 / tan(fovy / 2) in front of the camera).

#define SCTRL_MAX_KEY 64
#define SCTRL_GRAB_DIST 10.0f
#define SCTRL_HANDLE_LEN 10.0f
#define SCTRL_HANDLE_DRAW 40.0f
#define SCTRL_SCR_W 640.0f
#define SCTRL_SCR_H 480.0f
#define SCTRL_LINE_COL 0xFEFFFFFF
#define SCTRL_GRID_COL 0xFE404040

static int sctrlEdit(DB_SCTRL* w);
static int sctrlMenu(DB_SCTRL* w);
static int sctrlQuit(DB_SCTRL* w);

// Never called: the object starts with the pool of a function the original link dead-stripped (an
// int -> float conversion and 1.0f; STRIP_UNUSED in config/G4BE08/modules.py).
static f32 sctrlIndexF(int i)
{
    return (f32) i + 1.0f;
}


static int (*sctrl_routine_tbl[3])(DB_SCTRL*) = {sctrlEdit, sctrlMenu, sctrlQuit};

// Sets the graph's visible range (x = key time, y = key value) by hand.
void SctrlInitAxisRange(DB_SCTRL* w, f32 xMax, f32 xMin, f32 yMax, f32 yMin)
{
    w->Rlimit = xMax;
    w->Llimit = xMin;
    w->Tlimit = yMax;
    w->Blimit = yMin;
}

// Automatic range: x up to 1.2 x the last key time (down to -0.2 x), y around the value extremes.
void SctrlAdjustAxisRange(DB_SCTRL* w)
{
    HERMITE_1_PTR* c;
    f32 xmax;
    f32 xmin;
    f32 ymax;
    f32 ymin;
    f32 d;
    int i;

    if (!(w->Graph_flag & 2)) {
        return;
    }
    c = w->pScurve;
    if (c->nPoint <= 1) {
        return;
    }
    xmax = c->Point[0].T;
    ymin = c->Point[0].Q;
    xmin = xmax;
    ymax = ymin;
    // the .v reads go through `w->curve` (hoisted by loop.c into its own pseudo): the .t and .v address
    // givs then have different base registers and loop.c cannot combine them into one pointer with a
    // -4 displacement -- the target steps two pointers (&key[i].t at +0x14 and &key[i].v at +0x18)
    for (i = 1; i < c->nPoint; i++) {
        if (c->Point[i].T <= xmin) {
            xmin = c->Point[i].T;
        }
        if (c->Point[i].T >= xmax) {
            xmax = c->Point[i].T;
        }
        if (w->pScurve->Point[i].Q <= ymin) {
            ymin = w->pScurve->Point[i].Q;
        }
        if (w->pScurve->Point[i].Q >= ymax) {
            ymax = w->pScurve->Point[i].Q;
        }
    }
    w->Rlimit = xmax * 1.2f;
    w->Llimit = xmax * -0.2f;
    // the select as a ternary temp (a named `m` is a global pseudo that local-alloc cannot see, so the 0.1
    // pool load takes f0 and m falls to f13; the target has m in f0, the constant in f13)
    d = (fabsf(ymax) > fabsf(ymin) ? fabsf(ymax) : fabsf(ymin)) * 0.1f;
    w->Tlimit = ymax + d;
    w->Blimit = ymin - d;
}

// Axis labels (up to 7 chars each) printed by drawAxis.
void SctrlSetAxisLabel(DB_SCTRL* w, const char* x, const char* y)
{
    strcpy(w->labelX, x);
    strcpy(w->labelY, y);
}

// Puts the screen cursor at graph coordinates (x, y).
void SctrlInitCursor(DB_SCTRL* w, f32 x, f32 y)
{
    Vec g = {0.0f, 0.0f, 0.0f};

    g.x = x;
    g.y = y;
    posGraph2Screen(w, &g, &w->Cursor);
}

// Places the graph plane 240 / tan(fovy / 2) in front of the camera, facing it.
void dbSctrlScreenOrientation(DB_SCTRL* w, CAMERA* cam, f32 fovy)
{
    Vec dir;
    Vec pos;
    f32 dist;

    dist = 240.0 / tan(fovy * 0.5f * (PI / 180.0f));
    getColumn(cam->mat, 2, &dir);
    PSVECScale(&dir, &dir, -dist);
    getColumn(cam->mat, 3, &pos);
    PSVECAdd(&pos, &dir, &pos);
    {
        MtxPtr d_ = w->Scrn_mat;
        MtxPtr s_ = cam->mat;
        int i_ = 3;
        int j_;
        f32* sp_;
        f32* dp_;
        while (i_--) {
            dp_ = *d_;
            sp_ = *s_;
            for (j_ = 0; j_ < 4; j_++) {
                *dp_++ = *sp_++;
            }
            d_++;
            s_++;
        }
    }
    w->Scrn_mat[0][3] = pos.x;
    w->Scrn_mat[1][3] = pos.y;
    w->Scrn_mat[2][3] = pos.z;
}

// Runs the S-curve editor one frame: menu at text position (x, y), graph plane in front of the
// camera, axis / curve / cursor drawn, then the routine (0 sctrlEdit, 1 sctrlMenu, 2 sctrlQuit);
// returns the routine's result (0 once the editor quit).
int DbSctrl(DB_SCTRL* w, int x, int y)
{
    w->sX = x;
    w->blink++;
    w->sY = y;
    // struct-view read: the pG load then depends on the three member stores above (sched1 true
    // dependence), so `stw r4,x` loses the anti-dependence bonus of the later `lwz r4,pG` and the
    // stores come out in RTL order (y, x)
    dbSctrlScreenOrientation(w, &pG->Camera, pG->Camera.param.Fovy);
    drawAxis(w);
    drawScurve(w);
    if (w->Rno0 == 0) {
        drawCursor(w);
    }
    return sctrl_routine_tbl[w->Rno0](w);
}

// defined here: the menu strings follow SctrlAdjustAxisRange's constant pool in .rodata
static const char* sctrl_menu_name[6] = {"Scale  :", "Grid   :", "Reverse:", "Offset :", "Range  :", "Clear  :"};
// unreferenced (db_path.cpp's edit-mode static, kept by the compiler)
static s8 sctrl_edit_mode = 0;

// Routine 2: resets the editor state and returns 0 (the caller closes the editor).
static int sctrlQuit(DB_SCTRL* w)
{
    w->Rno0 = w->Rno1 = w->Rno2 = w->Rno3 = 0;
    return 0;
}

// Routine 1, the Z menu (pad 1): rows Scale (x/y factors applied to every key), Grid (grid-lock
// step and drawn grid), Reverse (mirror the keys in time / value), Offset (shift all keys), Range
// (auto or manual axis range), Clear (delete all keys, YES/NO). step 0 picks a row (B leaves),
// 1 edits its columns with left/right / the stick, A applies. Returns 1.
static int sctrlMenu(DB_SCTRL* w)
{
    JOY* joy = &Joy[0];
    HERMITE_1_PTR* c = w->pScurve;
    f32 tx = c->Point[0].T;
    f32 ty = c->Point[0].Q;
    int x;
    int y;
    int i;
    // 18 dead pool labels: the "%s" string must hash below `sctrl_menu_name` in gcse's expression
    // table (.LC40+, not .LC22) so its PRE pseudo is numbered first and wins the equal-priority
    // global-alloc tie for r20 (the original's TU numbered its labels differently)
    // COMPILER-DIFF: candidate (gcse PRE pseudo numbering)
    f32 lc0 = 3.5f;
    f32 lc1 = 10.5f;
    f32 lc2 = 17.5f;
    f32 lc3 = 24.5f;
    f32 lc4 = 31.5f;
    f32 lc5 = 38.5f;
    f32 lc6 = 45.5f;
    f32 lc7 = 52.5f;
    f32 lc8 = 59.5f;
    f32 lc9 = 66.5f;
    f32 lc10 = 73.5f;
    f32 lc11 = 80.5f;
    f32 lc12 = 87.5f;
    f32 lc13 = 94.5f;
    f32 lc14 = 101.5f;
    f32 lc15 = 108.5f;
    f32 lc16 = 115.5f;
    f32 lc17 = 122.5f;

    switch (w->Rno1) {
    case 0:
        if (joy->trg & 0x200) {
            w->Rno0 = 0;
            break;
        }
        if (joy->rep & 0x00080008) {
            w->Menu_no--;
        }
        if (joy->rep & 0x00040004) {
            w->Menu_no++;
        }
        if (joy->rep & 0x000C000C) {
            w->blink = 0x18;
        }
        w->Menu_no = w->Menu_no < 0 ? 0 : (w->Menu_no > 5 ? 5 : w->Menu_no);
        switch (w->Menu_no) {
        case 2:
            if (joy->trg & 0x100) {
                Hermite_1Reverse(w->pScurve);
            }
            break;
        case 5:
            if (joy->trg & 0x100) {
                w->YesNo = 0;
                w->Rno1++;
            }
            break;
        default:
            if (joy->trg & 0x100) {
                w->Rno1++;
            }
            break;
        }
        break;
    case 1:
        if (joy->trg & 0x200) {
            w->Rno1 = 0;
            break;
        }
        switch (w->Menu_no) {
        case 0:
            if (joy->rep & 0x00080008) {
                w->Sub_menu_no--;
            }
            if (joy->rep & 0x00040004) {
                w->Sub_menu_no++;
            }
            if (joy->rep & 0x000C000C) {
                w->blink = 0x18;
            }
            w->Sub_menu_no = w->Sub_menu_no < 0 ? 0 : (w->Sub_menu_no > 1 ? 1 : w->Sub_menu_no);
            switch (w->Sub_menu_no) {
            case 0:
                if ((joy->rep & 1) || (joy->on & 0x10000)) {
                    w->Hscale -= 0.1f;
                }
                if ((joy->rep & 2) || (joy->on & 0x20000)) {
                    w->Hscale += 0.1f;
                }
                w->Hscale = w->Hscale < 0.1f ? 0.1f : (w->Hscale > 10.0f ? 10.0f : w->Hscale);
                if (joy->trg & 0x100) {
                    Hermite_1Scale(w->pScurve, w->Hscale, w->Vscale);
                    w->Hscale = 1.0f;
                }
                break;
            case 1:
                if ((joy->rep & 1) || (joy->on & 0x10000)) {
                    w->Vscale -= 0.1f;
                }
                if ((joy->rep & 2) || (joy->on & 0x20000)) {
                    w->Vscale += 0.1f;
                }
                w->Vscale = w->Vscale < 0.1f ? 0.1f : (w->Vscale > 10.0f ? 10.0f : w->Vscale);
                if (joy->trg & 0x100) {
                    Hermite_1Scale(w->pScurve, w->Hscale, w->Vscale);
                    w->Vscale = 1.0f;
                }
                break;
            }
            break;
        case 1:
            if (joy->rep & 0x00080008) {
                w->Sub_menu_no--;
            }
            if (joy->rep & 0x00040004) {
                w->Sub_menu_no++;
            }
            if (joy->rep & 0x000C000C) {
                w->blink = 0x18;
            }
            w->Sub_menu_no = w->Sub_menu_no < 0 ? 0 : (w->Sub_menu_no > 2 ? 2 : w->Sub_menu_no);
            switch (w->Sub_menu_no) {
            case 0:
                if (joy->trg & 0x00010001) {
                    w->Graph_flag |= 1;
                }
                if (joy->trg & 0x00020002) {
                    w->Graph_flag &= ~1;
                }
                break;
            case 1:
                if ((joy->rep & 1) || (joy->on & 0x10000)) {
                    w->Grid.x -= 1.0f;
                }
                if ((joy->rep & 2) || (joy->on & 0x20000)) {
                    w->Grid.x += 1.0f;
                }
                w->Grid.x = w->Grid.x < 1.0f ? 1.0f : (w->Grid.x > 60.0f ? 60.0f : w->Grid.x);
                break;
            case 2:
                if ((joy->rep & 1) || (joy->on & 0x10000)) {
                    w->Grid.y -= 0.01f;
                }
                if ((joy->rep & 2) || (joy->on & 0x20000)) {
                    w->Grid.y += 0.01f;
                }
                w->Grid.y = w->Grid.y < 0.01f ? 0.01f : (w->Grid.y > 10.0f ? 10.0f : w->Grid.y);
                break;
            }
            break;
        case 3:
            if ((joy->rep & 1) || (joy->on & 0x10000)) {
                tx -= 1.0f;
            }
            if ((joy->rep & 2) || (joy->on & 0x20000)) {
                tx += 1.0f;
            }
            if ((joy->rep & 8) || (joy->on & 0x80000)) {
                ty += 0.1f;
            }
            if ((joy->rep & 4) || (joy->on & 0x40000)) {
                ty -= 0.1f;
            }
            Hermite_1Trans(w->pScurve, tx, ty);
            break;
        case 4:
            if (joy->rep & 0x00080008) {
                w->Sub_menu_no--;
            }
            if (joy->rep & 0x00040004) {
                w->Sub_menu_no++;
            }
            if (joy->rep & 0x000C000C) {
                w->blink = 0x18;
            }
            w->Sub_menu_no = w->Sub_menu_no < 0 ? 0 : (w->Sub_menu_no > 2 ? 2 : w->Sub_menu_no);
            switch (w->Sub_menu_no) {
            case 0:
                if (joy->trg & 0x00010001) {
                    w->Graph_flag |= 2;
                }
                if (joy->trg & 0x00020002) {
                    w->Graph_flag &= ~2;
                }
                break;
            case 1: {
                f32 a = w->Tlimit;
                f32 b = w->Blimit;
                if (!(a - b < 1.0f)) {
                    f32 e;

                    // the original computes both fabs arms straight into f1 (the log10 argument): the
                    // SF->DF extend of the select gives its pseudo no f1 preference in ours (f0 + fmr f1)
                    register f32 m asm("fr1"); // COMPILER-DIFF: candidate #17 (FLOAT_EXTEND argument preference)
                    if (fabsf(a) > fabsf(b)) {
                        m = fabsf(a);
                    } else {
                        m = fabsf(b);
                    }
                    e = log10(m) - 2.0;
                    // the original issues the `lwz joy->rep` only after `frsp e`: a sched region
                    // split (LOOP_END anti-dependences) delays the load behind the FP chain
                    do { } while (0); // COMPILER-DIFF: #13 (region split)
                    if ((joy->rep & 1) || (joy->on & 0x10000)) {
                        w->Tlimit -= IPOW(10.0f, (int) e);
                        w->Blimit += IPOW(10.0f, (int) e);
                    }
                    if ((joy->rep & 2) || (joy->on & 0x20000)) {
                        w->Tlimit += IPOW(10.0f, (int) e);
                        w->Blimit -= IPOW(10.0f, (int) e);
                    }
                }
                break;
            }
            case 2: {
                f32 e = log10(w->Rlimit) - 2.0;
                if ((joy->rep & 1) || (joy->on & 0x10000)) {
                    w->Rlimit -= IPOW(10.0f, (int) e);
                }
                if ((joy->rep & 2) || (joy->on & 0x20000)) {
                    w->Rlimit += IPOW(10.0f, (int) e);
                }
                if (w->Rlimit <= 1.0f) {
                    w->Rlimit = 1.0f;
                }
                w->Llimit = w->Rlimit * -0.16666666f;
                break;
            }
            }
            break;
        case 5:
            if (joy->trg & 0x00010001) {
                w->YesNo = 1;
            }
            if (joy->trg & 0x00020002) {
                w->YesNo = 0;
            }
            if (joy->trg & 0x100) {
                if (w->YesNo) {
                    Hermite_1Clear(w->pScurve);
                }
                w->Rno1 = 0;
            }
            break;
        }
        break;
    }

    x = w->sX;
    y = w->sY;
    eprintf(x, y, 5, 0, "S-CURVE");
    y += 14;
    {
    int col = 0;
    for (i = 0; i <= 5; i++) {
        eprintf(x, y + i * 14, (i == w->Menu_no) ? 4 : 0, 0, "%s", sctrl_menu_name[i]);
        if (i == w->Menu_no) {
            if (w->Rno1 == 0) {
                if (w->blink & 0x18) {
                    eprintf(x - 8, y + i * 14, 0x16, 0, ">");
                }
            } else {
                eprintf(x - 8, y + i * 14, 0x16, 0, ">");
            }
        }
        col = 0;
        if (i == w->Menu_no && w->Rno1 == 1) {
            switch (i) {
            case 0:
                eprintf(x + 0x48, y + i * 14, 0, 0, "H: x%f", w->Hscale);
                eprintf(x + 0x48, y + (i + 1) * 14, 0, 0, "V: x%f", w->Vscale);
                if (w->blink & 0x18) {
                    eprintf(x + 0x40, y + (i + w->Sub_menu_no) * 14, 0, 0, ">");
                }
                break;
            case 1:
                eprintf(x + 0x48, y + i * 14, 0, 0, "LOCK:", w->Grid.y);
                if (w->Graph_flag & 1) {
                    eprintf(x + 0x70, y + i * 14, 0, 0, "ON-/---");
                } else {
                    eprintf(x + 0x70, y + i * 14, 0, 0, "---/OFF");
                }
                eprintf(x + 0x48, y + (i + 1) * 14, (u8) col, 0, "X   : %f", w->Grid.x);
                eprintf(x + 0x48, y + (i + 2) * 14, (u8) col, 0, "Y   : %f", w->Grid.y);
                if (w->blink & 0x18) {
                    eprintf(x + 0x40, y + (i + w->Sub_menu_no) * 14, (u8) col, 0, ">");
                }
                break;
            case 3:
                eprintf(x + 0x48, y + i * 14, 0, 0, "X: %f", tx);
                eprintf(x + 0x48, y + (i + 1) * 14, 0, 0, "Y: %f", ty);
                break;
            case 4:
                eprintf(x + 0x48, y + i * 14, 0, 0, "AUTO:", w->Rlimit);
                eprintf(x + 0x48, y + (i + 1) * 14, 0, 0, "H   :", w->Tlimit);
                eprintf(x + 0x48, y + (i + 2) * 14, 0, 0, "V   :", w->Rlimit);
                if (w->Graph_flag & 2) {
                    eprintf(x + 0x70, y + i * 14, 0, 0, "ON-/---");
                } else {
                    eprintf(x + 0x70, y + i * 14, 0, 0, "---/OFF");
                }
                if (w->Sub_menu_no != 0) {
                    if (joy->on & 0x00010001) {
                        eprintf(x + 0x70, y + (i + w->Sub_menu_no) * 14, 0, 0, "-");
                    }
                    if (joy->on & 0x00020002) {
                        eprintf(x + 0x70, y + (i + w->Sub_menu_no) * 14, 0, 0, "+");
                    }
                }
                if (w->blink & 0x18) {
                    // the original issued only `li r5` in this block's second cycle: a free
                    // weight-0 insn took the other slot (lbz, addi r3 | li r5, X | extsb, li r6 |
                    // add, addi r7); the codeless non-volatile asm is that insn in both passes
                    asm("" : "=m"(w->blink)); // COMPILER-DIFF: #13 (free sched slot filler)
                    eprintf(x + 0x40, y + (i + w->Sub_menu_no) * 14, 0, 0, ">");
                }
                break;
            case 5:
                if (w->YesNo) {
                    eprintf(x + 0x48, y + i * 14, 0, 0, "YES/---");
                } else {
                    eprintf(x + 0x48, y + i * 14, 0, 0, "---/NO-");
                }
                break;
            }
        }
    }
    // keeps `col` live to the loop exit: its 4-ref/98-insn allocno otherwise outranks the ">"
    // string high, whose REG_EQUIV (high) doubled live length halves its priority in ours but
    // not in the original (">" r22, col r21)
    asm("" : : "r"(col)); // COMPILER-DIFF: #13 (REG_EQUIV live-length doubling of a hoisted high)
    }
    return 1;
}

// Routine 0, curve editing on pad 1. The cursor drags keys and their tangent handles, Y deletes or
// inserts a key, and an empty curve starts in the append step. Returns 1.
static int sctrlEdit(DB_SCTRL* w)
{
    Vec* cur = &w->Cursor;
    JOY* joy = &Joy[0];
    Vec g;
    Vec g2;

    if (w->Rno1 != 5) {
        f32 spd = (joy->on & 0x000F0000) ? 5.0f : 1.0f;

        if ((joy->rep & 1) || (joy->on & 0x10000)) {
            cur->x -= spd;
        }
        if ((joy->rep & 2) || (joy->on & 0x20000)) {
            cur->x += spd;
        }
        if ((joy->rep & 8) || (joy->on & 0x80000)) {
            cur->y += spd;
        }
        if ((joy->rep & 4) || (joy->on & 0x40000)) {
            cur->y -= spd;
        }
    }
    switch (w->Rno1) {
    case 0:
        if (w->pScurve->nPoint == 0) {
            w->pScurve->nPoint = 1;
            w->Rno1 = 1;
            break;
        }
        if (joy->trg & 0x10) {
            w->Rno1 = 0;
            w->Hscale = 1.0f;
            w->Rno0 = 1;
            w->Vscale = 1.0f;
            break;
        }
        if (joy->trg & 0x200) {
            w->Rno0 = 2;
            break;
        }
        if (joy->trg & 0x100) {
            s8 hit = grabPoint(w);
            switch (hit) {
            case 0:
                w->Rno1 = 2;
                g2.x = w->pScurve->Point[w->p_no].T;
                g2.y = w->pScurve->Point[w->p_no].Q;
                posGraph2Screen(w, &g2, cur);
                break;
            case 1:
                w->Rno1 = 3;
                break;
            case 2:
                w->Rno1 = 4;
                break;
            }
        } else if (joy->trg & 0x800) {
            if (grabLine(w)) {
                if (w->p_no != -1) {
                    g.x = w->pScurve->Point[w->p_no].T;
                    g.y = w->pScurve->Point[w->p_no].Q;
                    g.z = 0.0f;
                    posGraph2Screen(w, &g, &w->Cursor);
                } else if (w->s_no != -1) {
                    posGraph2Screen(w, &w->New_point, &w->Cursor);
                }
                w->YesNo = 0;
                w->Rno1 = 5;
            }
        }
        break;
    case 1:
        if (joy->trg & 0x10) {
            w->Rno0 = 1;
            w->Rno1 = 0;
            break;
        }
        posScreen2Graph(w, cur, &g);
        w->pScurve->Point[w->pScurve->nPoint - 1].T = g.x;
        w->pScurve->Point[w->pScurve->nPoint - 1].Q = g.y;
        if ((joy->trg & 0x100) && w->pScurve->nPoint <= SCTRL_MAX_KEY - 1) {
            posScreen2GridLock(w, cur, &g);
            w->pScurve->Point[w->pScurve->nPoint - 1].T = g.x;
            w->pScurve->Point[w->pScurve->nPoint - 1].Q = g.y;
            if (w->pScurve->nPoint > 1) {
                SctrlAdjustAxisRange(w);
                posGraph2Screen(w, &g, cur);
            }
            w->pScurve->nPoint++;
        }
        if (joy->trg & 0x200) {
            if (w->pScurve->nPoint > 1) {
                w->Rno1 = 0;
            } else {
                w->Rno0 = 2;
            }
            w->pScurve->nPoint--;
        }
        break;
    case 2:
        if (joy->on & 0x100) {
            posScreen2Graph(w, cur, &g2);
            w->pScurve->Point[w->p_no].T = g2.x;
            w->pScurve->Point[w->p_no].Q = g2.y;
        } else {
            posScreen2GridLock(w, cur, &g2);
            w->pScurve->Point[w->p_no].T = g2.x;
            w->pScurve->Point[w->p_no].Q = g2.y;
            w->Rno1 = 0;
            if (w->pScurve->nPoint > 1) {
                SctrlAdjustAxisRange(w);
                posGraph2Screen(w, &g2, cur);
            }
        }
        break;
    case 3:
    case 4:
        if (joy->on & 0x100) {
            HERMITE_1_PTR* c;
            f32 dx;
            f32 dy;

            posScreen2Graph(w, cur, &g2);
            c = w->pScurve;
            dx = c->Point[w->p_no].T - g2.x;
            dy = c->Point[w->p_no].Q - g2.y;
            if (w->Rno1 == 3) {
                c->Point[w->p_no].dQ[1] = dy / dx;
            } else {
                c->Point[w->p_no].dQ[0] = dy / dx;
            }
        } else {
            w->Rno1 = 0;
        }
        break;
    case 5:
        if (joy->on & 0x800) {
            if (joy->rep & 0x00010001) {
                w->YesNo = 1;
            }
            if (joy->rep & 0x00020002) {
                w->YesNo = 0;
            }
            if (joy->trg & 0x100) {
                if (w->YesNo) {
                    if (w->p_no != -1) {
                        deletePoint(w);
                    } else if (w->s_no != -1) {
                        insertPoint(w);
                    }
                }
                w->Rno1 = 0;
            }
            if (w->p_no != -1) {
                eprintf(0xF0, 0x118, 2, 0, "DELETE");
            } else {
                eprintf(0xF0, 0x118, 5, 0, "INSERT");
            }
            if (w->YesNo) {
                eprintf(0x100, 0x126, 0, 0, "YES/---");
            } else {
                eprintf(0x100, 0x126, 0, 0, "---/NO-");
            }
        } else {
            w->Rno1 = 0;
        }
        break;
    }
    return 1;
}

// Nearest key (0), in-tangent handle (1) or out-tangent handle (2) within SCTRL_GRAB_DIST screen units
// of the cursor -> w->grab; -1 when none.
int grabPoint(DB_SCTRL* w)
{
    Vec scr;
    Vec hnd;
    Vec gph;
    Vec dir;
    HERMITE_1_PTR* c = w->pScurve;
    f32 min = SCTRL_GRAB_DIST;
    int ret = -1;
    int i;

    for (i = 0; i < c->nPoint; i++) {
        HERMITE_1_POINT* k = &c->Point[i];
        f32 ang;
        f32 d;

        gph.x = k->T;
        gph.y = k->Q;
        posGraph2Screen(w, &gph, &scr);
        d = PSVECDistance(&scr, &w->Cursor);
        if (d < min) {
            w->p_no = i;
            min = d;
            ret = 0;
        }

        ang = atanf(k->dQ[1]) + PI;
        gph.x = cosf(ang) * SCTRL_HANDLE_LEN + k->T;
        gph.y = sinf(ang) * SCTRL_HANDLE_LEN + k->Q;
        posGraph2Screen(w, &gph, &hnd);
        PSVECSubtract(&hnd, &scr, &dir);
#line 790 "D:/Bio4/Prog/db_sctrl.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, SCTRL_HANDLE_DRAW);
        PSVECAdd(&scr, &dir, &hnd);
        d = PSVECDistance(&hnd, &w->Cursor);
        if (d < min) {
            w->p_no = i;
            min = d;
            ret = 1;
        }

        ang = atanf(k->dQ[0]);
        gph.x = cosf(ang) * SCTRL_HANDLE_LEN + k->T;
        gph.y = sinf(ang) * SCTRL_HANDLE_LEN + k->Q;
        posGraph2Screen(w, &gph, &hnd);
        PSVECSubtract(&hnd, &scr, &dir);
#line 811 "D:/Bio4/Prog/db_sctrl.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, SCTRL_HANDLE_DRAW);
        PSVECAdd(&scr, &dir, &hnd);
        d = PSVECDistance(&hnd, &w->Cursor);
        if (d < min) {
            w->p_no = i;
            min = d;
            ret = 2;
        }
    }
    return ret;
}

// Nearest curve sample (1 -> insertPos / insertIdx) or key (2 -> grab) within SCTRL_GRAB_DIST of the
// cursor; 0 when neither.
int grabLine(DB_SCTRL* w)
{
    Vec gph;
    Vec scr;
    f32 v;
    HERMITE_1_PTR* c = w->pScurve;
    f32 min = SCTRL_GRAB_DIST;
    int ret = 0;
    int i;

    w->s_no = -1;
    for (i = 0; i < c->nPoint - 1; i++) {
        HERMITE_1_POINT* a = &c->Point[i];
        HERMITE_1_POINT* b = &c->Point[i + 1];
        f32 t0 = a->T;
        f32 span = b->T - a->T;
        int j;

        for (j = 0; j <= 100.0f; j++) {
            f32 t = t0 + (f32) j * span / 100.0f;
            f32 d;

            Hermite_1(a, b, t, &v);
            gph.x = t;
            gph.y = v;
            gph.z = 0.0f;
            posGraph2Screen(w, &gph, &scr);
            d = PSVECDistance(&scr, &w->Cursor);
            if (d <= min) {
                min = d;
                ret = 1;
                w->s_no = i + 1;
                w->New_point = gph;
            }
        }
    }
    w->p_no = -1;
    for (i = 0; i < c->nPoint; i++) {
        f32 d;

        gph.x = c->Point[i].T;
        gph.y = c->Point[i].Q;
        gph.z = 0.0f;
        posGraph2Screen(w, &gph, &scr);
        d = PSVECDistance(&scr, &w->Cursor);
        if (d <= min) {
            w->p_no = i;
            min = d;
            ret = 2;
        }
    }
    return ret;
}

// Removes key w->grab from the curve (the rest shift down, the freed slot is zeroed).
void deletePoint(DB_SCTRL* w)
{
    HERMITE_1_PTR* c = w->pScurve;
    int i;

    c->nPoint--;
    for (i = w->p_no; i < c->nPoint; i++) {
        c->Point[i] = c->Point[i + 1];
    }
    memclr_asm(&c->Point[c->nPoint], sizeof(HERMITE_1_POINT));
}

// Inserts a key at w->insertIdx with the grabbed curve position (insertPos); no-op at 64 keys.
void insertPoint(DB_SCTRL* w)
{
    HERMITE_1_PTR* c = w->pScurve;
    int i;

    if (c->nPoint > SCTRL_MAX_KEY - 1) {
        return;
    }
    for (i = c->nPoint; i > w->s_no; i--) {
        c->Point[i] = c->Point[i - 1];
    }
    memclr_asm(&c->Point[w->s_no], sizeof(HERMITE_1_POINT));
    c->Point[w->s_no].T = w->New_point.x;
    c->Point[w->s_no].Q = w->New_point.y;
    c->nPoint++;
}

// Draws the cross-hair cursor at the screen position (world space through w->mtx) and prints its
// graph coordinates.
void drawCursor(DB_SCTRL* w)
{
    Vec a;
    Vec b;
    Vec wa;
    Vec wb;
    Vec gph;
    int sx;
    int sy;
    int dx = 8;
    int dy;

    a = w->Cursor;
    b = w->Cursor;
    a.y += 50.0f;
    b.y -= 50.0f;
    posScreen2World(w, &a, &wa);
    posScreen2World(w, &b, &wb);
    Draw_line3d(&wa, &wb, SCTRL_LINE_COL, 0);
    a = w->Cursor;
    b = w->Cursor;
    a.x += 50.0f;
    b.x -= 50.0f;
    posScreen2World(w, &a, &wa);
    posScreen2World(w, &b, &wb);
    Draw_line3d(&wa, &wb, SCTRL_LINE_COL, 0);
    sx = (int) (w->Cursor.x * 256.0f / 320.0f + 256.0f);
    sy = (int) (224.0f - w->Cursor.y * 224.0f / 240.0f);
    if (sx > 0x198) {
        dx = -0x68;
    }
    if (sy > 0x1A4) {
        dy = -0x1C;
    } else {
        dy = 14;
    }
    posScreen2Graph(w, &w->Cursor, &gph);
    eprintf(sx + dx, sy + dy, 0, 0, "(%3.3f, %3.3f)", gph.x, gph.y);
}

// Draws the graph frame, the grid lines at gridX/gridY spacing and the axis labels with the range
// values.
void drawAxis(DB_SCTRL* w)
{
    Vec g0;
    Vec g1;
    Vec w0;
    Vec w1;
    f32 zero = 0.0f;
    int i;
    // function-scope sx/sy (set in both label blocks): a block-local sx is a local-alloc qty and the
    // fix_trunc load `(set sx (unspec [fpmem P]))` ties the dying stack-slot address pseudo P to it
    // (r3, the argument register); a multi-block sx has no qty, P is allocated alone (r9, r11)
    int sx;
    int sy;

    g0.x = w->Llimit;
    g1.x = w->Rlimit;
    g0.y = zero;
    g1.y = zero;
    posGraph2World(w, &g0, &w0);
    posGraph2World(w, &g1, &w1);
    Draw_line3d(&w0, &w1, SCTRL_LINE_COL, 0);
    if (strlen(w->labelX) != 0) {
        sx = (int) (w1.x * 256.0f / 320.0f + 256.0f);
        sy = (int) (224.0f - w1.y);
        eprintf(sx - 0x48, sy, 0, 0, "%s", w->labelX);
    }
    g0.y = w->Blimit;
    g1.y = w->Tlimit;
    g0.x = zero;
    g1.x = zero;
    posGraph2World(w, &g0, &w0);
    posGraph2World(w, &g1, &w1);
    Draw_line3d(&w0, &w1, SCTRL_LINE_COL, 0);
    if (strlen(w->labelY) != 0) {
        sx = (int) (w1.x * 256.0f / 320.0f + 256.0f);
        sy = (int) (224.0f - w1.y);
        eprintf(sx - 0x40, sy + 0x2A, 0, 0, "%s", w->labelY);
    }
    if (w->grid_disp_X > zero) {
        memclr_asm(&g0, sizeof(Vec));
        memclr_asm(&g1, sizeof(Vec));
        g0.y = w->Blimit;
        g1.y = w->Tlimit;
        for (i = 1; (f32) i * w->grid_disp_X < w->Rlimit; i++) {
            g1.x = g0.x = (f32) i * w->grid_disp_X;
            posGraph2World(w, &g0, &w0);
            posGraph2World(w, &g1, &w1);
            Draw_line3d(&w0, &w1, SCTRL_GRID_COL, 0);
        }
        for (i = -1; (f32) i * w->grid_disp_X > w->Llimit; i--) {
            g1.x = g0.x = (f32) i * w->grid_disp_X;
            posGraph2World(w, &g0, &w0);
            posGraph2World(w, &g1, &w1);
            Draw_line3d(&w0, &w1, SCTRL_GRID_COL, 0);
        }
    }
    if (w->grid_disp_Y > 0.0f) {
        memclr_asm(&g0, sizeof(Vec));
        memclr_asm(&g1, sizeof(Vec));
        g0.x = w->Llimit;
        g1.x = w->Rlimit;
        for (i = 1; (f32) i * w->grid_disp_Y < w->Tlimit; i++) {
            g1.y = g0.y = (f32) i * w->grid_disp_Y;
            posGraph2World(w, &g0, &w0);
            posGraph2World(w, &g1, &w1);
            Draw_line3d(&w0, &w1, SCTRL_GRID_COL, 0);
        }
        for (i = -1; (f32) i * w->grid_disp_Y > w->Blimit; i--) {
            g1.y = g0.y = (f32) i * w->grid_disp_Y;
            posGraph2World(w, &g0, &w0);
            posGraph2World(w, &g1, &w1);
            Draw_line3d(&w0, &w1, SCTRL_GRID_COL, 0);
        }
    }
}

// Draws the Hermite curve sampled between the keys, every key point (the grabbed one highlighted)
// and the in/out tangent handles of the grabbed key.
void drawScurve(DB_SCTRL* w)
{
    Vec gph;
    Vec wp;
    Vec prev;
    Vec hnd;
    Vec wh;
    Vec dir;
    f32 v;
    HERMITE_1_PTR* c = w->pScurve;
    HERMITE_1_POINT* k;
    int i;

    // k as `&c->key[i]` inside the body: loop.c reduces it to a pointer giv whose `addi rK,c,4` init is
    // emitted in the loop preheader (a `k = c->key; ... k++` form puts the init in the entry block
    // before the exit test, which shifts the callee-saved allocation of i/k/&wp)
    for (i = 0; i < c->nPoint; i++) {
        f32 ang;

        k = &c->Point[i];
        gph.x = k->T;
        gph.y = k->Q;
        posGraph2World(w, &gph, &wp);
        Draw_sphere(&wp, 2.0f, 0xFFFFFFFF, 0, 0);

        ang = atanf(k->dQ[1]) + PI;
        hnd.x = cosf(ang) * SCTRL_HANDLE_LEN + k->T;
        hnd.y = sinf(ang) * SCTRL_HANDLE_LEN + k->Q;
        posGraph2World(w, &hnd, &wh);
        PSVECSubtract(&wh, &wp, &dir);
#line 1116 "D:/Bio4/Prog/db_sctrl.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, SCTRL_HANDLE_DRAW);
        PSVECAdd(&wp, &dir, &wh);
        Draw_sphere(&wh, 2.0f, 0xFFFFFFFF, 0, 0);
        Draw_line3d(&wp, &wh, SCTRL_LINE_COL, 0);

        ang = atanf(k->dQ[0]);
        hnd.x = cosf(ang) * SCTRL_HANDLE_LEN + k->T;
        hnd.y = sinf(ang) * SCTRL_HANDLE_LEN + k->Q;
        posGraph2World(w, &hnd, &wh);
        PSVECSubtract(&wh, &wp, &dir);
#line 1133 "D:/Bio4/Prog/db_sctrl.cpp"
        VECNormalize(&dir, &dir);
        PSVECScale(&dir, &dir, SCTRL_HANDLE_DRAW);
        PSVECAdd(&wp, &dir, &wh);
        Draw_sphere(&wh, 2.0f, 0xFFFFFFFF, 0, 0);
        Draw_line3d(&wp, &wh, SCTRL_LINE_COL, 0);
    }
    for (i = 0; i < c->nPoint - 1; i++) {
        HERMITE_1_POINT* a = &c->Point[i];
        HERMITE_1_POINT* b = &c->Point[i + 1];
        f32 t0 = a->T;
        f32 span = b->T - a->T;
        int j;

        for (j = 0; j <= 99; j++) {
            f32 t = t0 + (f32) j * span / 99.0f;

            Hermite_1(a, b, t, &v);
            gph.x = t;
            gph.y = v;
            gph.z = 0.0f;
            posGraph2World(w, &gph, &wp);
            if (j != 0) {
                Draw_line3d(&prev, &wp, SCTRL_LINE_COL, 0);
            }
            prev = wp;
        }
    }
}

// Screen (640 x 480 centred) -> graph coordinates by the current range.
void posScreen2Graph(DB_SCTRL* w, Vec* scr, Vec* gph)
{
    f32 dx = w->Rlimit - w->Llimit;
    f32 dy = w->Tlimit - w->Blimit;

    gph->x = scr->x * (dx / SCTRL_SCR_W) + dx * 0.5f + w->Llimit;
    gph->y = scr->y * (dy / SCTRL_SCR_H) + dy * 0.5f + w->Blimit;
    gph->z = 0.0f;
}

// Graph -> screen coordinates.
void posGraph2Screen(DB_SCTRL* w, Vec* gph, Vec* scr)
{
    f32 dx = w->Rlimit - w->Llimit;
    f32 dy = w->Tlimit - w->Blimit;

    scr->x = (gph->x - w->Llimit - dx * 0.5f) * (SCTRL_SCR_W / dx);
    scr->y = (gph->y - w->Blimit - dy * 0.5f) * (SCTRL_SCR_H / dy);
    scr->z = 0.0f;
}

// Screen -> world through the graph plane matrix (for the GX primitives).
void posScreen2World(DB_SCTRL* w, Vec* scr, Vec* out)
{
    PSMTXMultVec(w->Scrn_mat, scr, out);
}

// Graph -> world (posGraph2Screen then posScreen2World).
void posGraph2World(DB_SCTRL* w, Vec* gph, Vec* out)
{
    Vec scr;

    posGraph2Screen(w, gph, &scr);
    posScreen2World(w, &scr, out);
}

// Snaps in->x/y to the grid (a zero grid step counts as 1 / 0.01).
void posGridLock(Vec* grid, Vec* in, Vec* out)
{
    if (grid->x == 0.0f) {
        grid->x = 1.0f;
    }
    if (grid->y == 0.0f) {
        grid->y = 0.01f;
    }
    if (in->x >= 0.0f) {
        out->x = (f32) (int) (in->x / grid->x + 0.5f) * grid->x;
    } else {
        out->x = (f32) (int) (in->x / grid->x - 0.5f) * grid->x;
    }
    if (in->y >= 0.0f) {
        out->y = (f32) (int) (in->y / grid->y + 0.5f) * grid->y;
    } else {
        out->y = (f32) (int) (in->y / grid->y - 0.5f) * grid->y;
    }
}

// Screen -> graph, snapped to the grid-lock step when flags bit 0 is set.
void posScreen2GridLock(DB_SCTRL* w, Vec* scr, Vec* gph)
{
    posScreen2Graph(w, scr, gph);
    if (w->Graph_flag & 1) {
        posGridLock(&w->Grid, gph, gph);
        posGraph2Screen(w, gph, scr);
    }
}
