#include "types.h"
#include "vec.h"
#include "joy.h"
#include "eprintf.h"
#include "main.h"
#include "main_mem.h"
#include "math_sub.h"
#include "path.h"
#include "dbmodule.h"
#include "db_path.h"

// B-spline path editor of the interface-design tool (D:/Bio4/Prog/db_path.cpp; the same object in
// t_id and t_event). Edits a FUNC_PATH_PTR (type POINT / LINEAR / B-SPL2..4, up to 64 control points)
// with a 3D cursor moved by the caller: pathEdit grabs, appends, inserts and deletes points, pathMenu
// is the Z menu (type, clear), pathDraw draws the parametrised curve. Entry: DbPath every frame.

#define PATH_MAX_POINT 64
#define PATH_GRAB_DIST 20.0f
#define PATH_DRAW_DIV 200.0f
#define PATH_T_START 0.0f

// FuncPathParametrize output with room for PATH_MAX_POINT control points (accessed through a
// FuncPathWork pointer, which is what keeps its address in a register).
struct PathParam {
    int k;
    int n;
    Vec alpha[PATH_MAX_POINT];
};

static int pathEdit(DB_PATH* w);
static int pathMenu(DB_PATH* w);
static int pathQuit(DB_PATH* w);
static int pathGrabPoint(DB_PATH* w);
static int pathGrabLine(DB_PATH* w);
static void pathDeletePoint(DB_PATH* w);

static int (*path_routine_tbl[3])(DB_PATH*) = {pathEdit, pathMenu, pathQuit};
static const char* path_menu_name[2] = {"Type :", "Clear:"};
static const char* path_type_name[5] = {"POINT", "LINEAR", "B-SPL2", "B-SPL3", "B-SPL4"};
static s8 path_edit_mode = 0;  // step 2: 0 = moving a grabbed point, 1 = insert / delete prompt

static Vec path_draw_prev;

// Runs the path editor one frame: menu at (x, y), draws the path (with w->ofs) and the 3D cross
// cursor, then the routine (0 pathEdit, 1 pathMenu, 2 pathQuit); 0 once the editor quit.
int DbPath(DB_PATH* w, int x, int y)
{
    w->blink++;
    w->x = x;
    w->y = y;
    pathDraw(w, &w->ofs);
    pathCursor(w);
    return path_routine_tbl[w->routine](w);
}

// Routine 2: resets the editor state, returns 0 (the caller closes it).
static int pathQuit(DB_PATH* w)
{
    w->routine = w->step = w->x2 = w->x3 = 0;
    return 0;
}

// Routine 1, the Z menu: Type (POINT / LINEAR / B-SPL2..4 = FUNC_PATH_PTR::k), Clear (YES/NO
// empties the path); B back to editing. Returns 1.
static int pathMenu(DB_PATH* w)
{
    JOY* joy = &Joy[0];
    FUNC_PATH_PTR* path = w->path;
    int x;
    int y;
    int i;

    switch (w->step) {
    case 0:
        if (joy->trg & 0x200) {
            w->routine = 0;
            break;
        }
        if (joy->rep & 0x00080008) {
            w->cursor--;
        }
        if (joy->rep & 0x00040004) {
            w->cursor++;
        }
        if (joy->rep & 0x000C000C) {
            w->blink = 0x18;
        }
        w->cursor = w->cursor < 0 ? 0 : (w->cursor > 1 ? 1 : w->cursor);
        {
            switch (w->cursor) {
            case 0:
                w->pEnd = &w->path->Point[path->nPoint];
                if (joy->rep & 0x00010001) {
                    path->Order--;
                }
                if (joy->rep & 0x00020002) {
                    path->Order++;
                }
                path->Order = path->Order < 0 ? 0 : (path->Order > 3 ? 3 : path->Order);
                break;
            case 1:
                if (joy->trg & 0x100) {
                    w->yes = 0;
                    w->step++;
                }
                break;
            }
        }
        break;
    case 1:
        if (joy->trg & 0x200) {
            w->step = 0;
            break;
        }
        if (w->cursor == 1) {
            if (joy->trg & 0x00010001) {
                w->yes = 1;
            }
            if (joy->trg & 0x00020002) {
                w->yes = 0;
            }
            if (joy->trg & 0x100) {
                if (w->yes) {
                    FuncPathClear(w->path);
                }
                w->step = 0;
            }
        }
        break;
    }

    x = w->x;
    y = w->y;
    eprintf(x, y, 5, 0, "PATH");
    y += 14;
    for (i = 0; i <= 1; i++) {
        eprintf(x, y + i * 14, (i == w->cursor) ? 4 : 0, 0, "%s", path_menu_name[i]);
        if (i == w->cursor) {
            if (w->step == 0) {
                if (w->blink & 0x18) {
                    eprintf(x - 8, y + i * 14, 0x16, 0, ">");
                }
            } else {
                eprintf(x - 8, y + i * 14, 0x16, 0, ">");
            }
        }
        switch (i) {
        case 0: {
            int j;
            for (j = 0; j <= 3; j++) {
                int col = 7;
                if (j == path->Order) {
                    col = 0;
                }
                eprintf(x + 0x40 + j * 0x38, y + i * 14, col, 0, "%s", path_type_name[j]);
            }
            break;
        }
        case 1:
            if (w->step == 1) {
                if (w->yes) {
                    eprintf(x + 0x40, y + i * 14, 0, 0, "YES/---");
                } else {
                    eprintf(x + 0x40, y + i * 14, 0, 0, "---/NO-");
                }
            }
            break;
        }
    }
    return 1;
}

// Routine 0, control point editing (pad 1; the cursor is moved by the caller): step 0 idle (A grabs
// a point -> step 2 drag, Y on the curve grabs a point or an insertion spot -> step 2 with the
// delete / insert YES/NO, Z the menu, B quits); step 1 appends points (an empty path starts here:
// A places, grid-locked, B ends). Returns 0 when the path was emptied.
static int pathEdit(DB_PATH* w)
{
    Vec* cur = &w->pos;
    FUNC_PATH_PTR* path = w->path;
    JOY* joy = &Joy[0];
    int ret = 1;

    if (w->step != 2 || path_edit_mode == 0) {
        f32 spd = (joy->on & 0x000F0000) ? 5.0f : 1.0f;

        if (joy->on & 0x00010001) {
            cur->x -= spd;
        }
        if (joy->on & 0x00020002) {
            cur->x += spd;
        }
        if (joy->on & 0x00080008) {
            cur->y += spd;
        }
        if (joy->on & 0x00040004) {
            cur->y -= spd;
        }
        path_edit_mode = 0;
    }
    switch (w->step) {
    case 0:
        if (joy->trg & 0x200) {
            w->routine = 2;
            break;
        }
        if (path->nPoint == 0) {
            w->step = 1;
            path->Order = 1;
            path->nPoint = 1;
            break;
        }
        if (joy->trg & 0x10) {
            w->step = 0;
            w->routine = 1;
            break;
        }
        if (joy->trg & 0x100) {
            if (pathGrabPoint(w)) {
                *cur = path->Point[w->grab];
                w->step = 2;
                path_edit_mode = 0;
            }
        }
        if (joy->trg & 0x800) {
            if (pathGrabLine(w)) {
                if (w->grab != -1) {
                    *cur = path->Point[w->grab];
                } else if (w->insertIdx != -1) {
                    *cur = w->insertPos;
                }
                w->yes = 0;
                w->step = 2;
                path_edit_mode = 1;
            }
        }
        break;
    case 1:
        if (joy->trg & 0x200) {
            if (path->nPoint > 1) {
                w->step = 0;
                path->nPoint--;
            } else {
                path->nPoint = 0;
                ret = 0;
            }
            break;
        }
        path->Point[path->nPoint - 1] = *cur;
        if ((joy->trg & 0x100) && path->nPoint <= PATH_MAX_POINT - 1) {
            if (w->gridLock) {
                pathGridLock(&w->grid, cur, cur);
            }
            path->Point[path->nPoint - 1] = *cur;
            path->Point[path->nPoint] = path->Point[path->nPoint - 1];
            path->nPoint++;
        }
        if (joy->trg & 0x10) {
            w->routine = 1;
            w->step = 0;
        }
        break;
    case 2:
        switch (path_edit_mode) {
        case 0:
            if (joy->on & 0x100) {
                path->Point[w->grab] = *cur;
            } else {
                if (w->gridLock) {
                    pathGridLock(&w->grid, cur, cur);
                }
                path->Point[w->grab] = *cur;
                w->step = 0;
                path_edit_mode = 0;
            }
            break;
        case 1:
            if (joy->on & 0x800) {
                if (joy->rep & 0x00010001) {
                    w->yes = 1;
                }
                if (joy->rep & 0x00020002) {
                    w->yes = 0;
                }
                if (joy->trg & 0x100) {
                    if (w->yes) {
                        if (w->grab != -1) {
                            pathDeletePoint(w);
                        } else if (w->insertIdx != -1) {
                            pathInsertPoint(w);
                        }
                    }
                    w->step = 0;
                    path_edit_mode = 0;
                }
                if (w->grab != -1) {
                    eprintf(0xF0, 0x118, 2, 0, "DELETE");
                } else {
                    eprintf(0xF0, 0x118, 5, 0, "INSERT");
                }
                if (w->yes) {
                    eprintf(0x100, 0x126, 0, 0, "YES/---");
                } else {
                    eprintf(0x100, 0x126, 0, 0, "---/NO-");
                }
            } else {
                w->step = 0;
                path_edit_mode = 0;
            }
            break;
        }
        break;
    }
    return ret;
}

// Nearest control point within PATH_GRAB_DIST of the cursor -> w->grab; 1 when one was found.
static int pathGrabPoint(DB_PATH* w)
{
    FUNC_PATH_PTR* path = w->path;
    Vec* cur = &w->pos;
    f32 min = PATH_GRAB_DIST;
    int i;

    for (i = 0; i < path->nPoint; i++) {
        f32 d = PSVECDistance(&path->Point[i], cur);
        if (d <= min) {
            w->grab = i;
            min = d;
        }
    }
    return min != PATH_GRAB_DIST;
}

// Nearest point of the curve (sampled PATH_DRAW_DIV times) -> insertPos / insertIdx, then the nearest
// control point -> grab; 1 when either is within PATH_GRAB_DIST.
static int pathGrabLine(DB_PATH* w)
{
    PathParam buf;
    Vec pt;
    f32 min = PATH_GRAB_DIST;
    FUNC_PATH_PTR* path = w->path;
    FuncPathWork* param = (FuncPathWork*) &buf;
    Vec* cur = &w->pos;
    int i;

    if (!FuncPathParametrize(path, param)) {
        return 0;
    }
    w->insertIdx = -1;
    if (param->k > 0 && param->n > 1) {
        for (i = 0; i <= PATH_DRAW_DIV; i++) {
            f32* basis;
            f32 t;
            int j;

#line 447 "D:/Bio4/Prog/db_path.cpp"
            basis = (f32*) MEM_ALLOC(param->n * 4, 1, 13);
            t = (f32) ((path->nPoint - 1) * i) / PATH_DRAW_DIV + PATH_T_START;
            de_Boor_Cox(path->nPoint, 0, t, param->k, basis);
            pt.z = 0.0f;
            pt.y = 0.0f;
            pt.x = 0.0f;
            for (j = 0; j < path->nPoint; j++) {
                pt.x = basis[j] * param->alpha[j].x + pt.x;
                pt.y = basis[j] * param->alpha[j].y + pt.y;
                pt.z = basis[j] * param->alpha[j].z + pt.z;
            }
            Mem_free(basis);
            {
                f32 d = PSVECDistance(&pt, cur);
                if (d <= min) {
                    w->insertIdx = (s8) t + 1;
                    w->insertPos = pt;
                    min = d;
                }
            }
        }
    }
    w->grab = -1;
    for (i = 0; i < path->nPoint; i++) {
        f32 d = PSVECDistance(&path->Point[i], cur);
        if (d <= min) {
            w->grab = i;
            min = d;
        }
    }
    return min != PATH_GRAB_DIST;
}

// Removes control point w->grab.
static void pathDeletePoint(DB_PATH* w)
{
    FUNC_PATH_PTR* path = w->path;
    int i;

    path->nPoint--;
    for (i = w->grab; i < path->nPoint; i++) {
        path->Point[i] = path->Point[i + 1];
    }
    path->Point[path->nPoint] = vecZero;
}

// Inserts a control point at w->insertIdx with the grabbed curve position; no-op at 64 points.
void pathInsertPoint(DB_PATH* w)
{
    FUNC_PATH_PTR* path = w->path;
    int i;

    if (path->nPoint > PATH_MAX_POINT - 1) {
        return;
    }
    for (i = path->nPoint; i > w->insertIdx; i--) {
        path->Point[i] = path->Point[i - 1];
    }
    path->Point[w->insertIdx] = w->insertPos;
    path->nPoint++;
}

// Draws the full-screen cross-hair through the cursor (plus ofs) and prints its coordinates.
void pathCursor(DB_PATH* w)
{
    Vec a;
    Vec b;

    PSVECAdd(&w->pos, &w->ofs, &a);
    PSVECAdd(&w->pos, &w->ofs, &b);
    a.y = 240.0f;
    b.y = -240.0f;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
    PSVECAdd(&w->pos, &w->ofs, &a);
    PSVECAdd(&w->pos, &w->ofs, &b);
    a.x = 320.0f;
    b.x = -320.0f;
    Draw_line3d(&a, &b, 0xFFFFFFFF, 0);
    eprintf(200, 200, 0, 0, "(%f, %f)", w->pos.x, w->pos.y);
}

// Draws the control points (the grabbed one marked) and the parametrised curve (200 segments,
// FuncPathParametrize) offset by `ofs`.
void pathDraw(DB_PATH* w, Vec* ofs)
{
    PathParam buf;
    Vec pt;
    Vec a;
    Vec b;
    FUNC_PATH_PTR* path = w->path;
    FuncPathWork* param = (FuncPathWork*) &buf;
    int i;

    if (!FuncPathParametrize(path, param)) {
        return;
    }
    if (param->k > 0 && param->n > 1) {
        for (i = 0; i <= PATH_DRAW_DIV; i++) {
            f32* basis;
            f32 t;
            int j;

#line 586 "D:/Bio4/Prog/db_path.cpp"
            basis = (f32*) MEM_ALLOC(param->n * 4, 1, 13);
            t = (f32) ((path->nPoint - 1) * i) / PATH_DRAW_DIV + PATH_T_START;
            de_Boor_Cox(path->nPoint, 0, t, param->k, basis);
            pt.z = 0.0f;
            pt.y = 0.0f;
            pt.x = 0.0f;
            for (j = 0; j < path->nPoint; j++) {
                pt.x = basis[j] * param->alpha[j].x + pt.x;
                pt.y = basis[j] * param->alpha[j].y + pt.y;
                pt.z = basis[j] * param->alpha[j].z + pt.z;
            }
            Mem_free(basis);
            if (i > 0) {
                PSVECAdd(&pt, ofs, &a);
                PSVECAdd(&path_draw_prev, ofs, &b);
                Draw_line3d(&b, &a, 0xFFFFFFFF, 0);
            }
            path_draw_prev = pt;
        }
    }
    for (i = 0; i < w->path->nPoint; i++) {
        PSVECAdd(&w->path->Point[i], ofs, &pt);
        Draw_sphere(&pt, 2.0f, 0xFFFFFFFF, 1, 1);
    }
}

// Snaps in->x/y to the grid (a zero grid step counts as 1).
void pathGridLock(Vec* grid, Vec* in, Vec* out)
{
    if (grid->x == 0.0f) {
        grid->x = 1.0f;
    }
    if (grid->y == 0.0f) {
        grid->y = 1.0f;
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

// the next object's .data is 8-aligned
asm(".section .data; .balign 8");
