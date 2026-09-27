// game/area.cpp: trigger volumes (AREA_HIT_DATA). An area is an XZ quadrilateral with floor / height
// (AREA_TYPE_XZ4), a vertical cylinder (AREA_TYPE_CYLINDER) or a view-cone trigger
// (AREA_TYPE_EYE). AreaHitCheck / AreaViewCheck are the game-side tests (effect areas, light
// areas, floor attribute areas, scenario triggers); the rest is the debug tool editor and display
// used by the Tools/t_* screens (pad-driven point / radius / height editing, wireframe drawing,
// value and help text).
// Original source: D:/Bio4/Prog/area.cpp.
#include "types.h"
#include "vec.h"
#include "global.h"
#include "area.h"
#include "geometry.h"
#include "db_log.h"
#include "db_cam.h"
#include "eprintf.h"
#include "joy.h"
#include "rnd.h"
#include "gx.h"
#include <string.h>
#include <math.h>
#include "model.h"
#include "dbmodule.h"
#include "math_sub_decl.h"

#define AREA_TYPE_ERR "AREA_HIT_DATA : AREA_TYPE[%d] invalid."

// 1 when `pos` is inside area (quad or cylinder types; the eye type never hits). Unknown types
// warn and return 0.
BOOL AreaHitCheck(AREA_HIT_DATA* pAre, Vec* pPos)
{
    int ret = 0;

    switch (pAre->type) {
    case AREA_TYPE_XZ4:
        ret = areaHitCheck_xz4(&pAre->xz4, pPos);
        break;
    case AREA_TYPE_CYLINDER:
        ret = areaHitCheck_Cylinder(&pAre->cylinder, pPos);
        break;
    case AREA_TYPE_EYE:
        break;
    default:
        pLog->warn(0, 0, AREA_TYPE_ERR, pAre->type);
        ret = 0;
        break;
    }
    return ret;
}

// Point-in-quad test: pos.y must be within [floor - 100, floor + height) and the point on the
// inner side of all four edges (cross products against edges 0-3, 0-1, 2-3, 2-1).
BOOL areaHitCheck_xz4(AREA_XZ4* pXz4, Vec* pPos)
{
    f32 dz, dx;

    if (pPos->y + 100.0f < pXz4->floor || pPos->y >= pXz4->floor + pXz4->height) {
        return 0;
    }
    if ((pXz4->p[3].x - pXz4->p[0].x) * (pPos->z - pXz4->p[0].z) > (pXz4->p[3].z - pXz4->p[0].z) * (pPos->x - pXz4->p[0].x) ||
        (pXz4->p[1].x - pXz4->p[0].x) * (pPos->z - pXz4->p[0].z) < (pXz4->p[1].z - pXz4->p[0].z) * (pPos->x - pXz4->p[0].x)) {
        return 0;
    }
    if ((pXz4->p[3].x - pXz4->p[2].x) * (pPos->z - pXz4->p[2].z) < (pXz4->p[3].z - pXz4->p[2].z) * (pPos->x - pXz4->p[2].x) ||
        (pXz4->p[1].x - pXz4->p[2].x) * (pPos->z - pXz4->p[2].z) > (pXz4->p[1].z - pXz4->p[2].z) * (pPos->x - pXz4->p[2].x)) {
        return 0;
    }
    return 1;
}

// Point-in-cylinder test: same height band, XZ distance from the centre below radius.
BOOL areaHitCheck_Cylinder(AREA_CYLINDER* pCld, Vec* pPos)
{
    f32 dx, dz;

    if (pPos->y + 100.0f < pCld->floor || pPos->y >= pCld->floor + pCld->height) {
        return 0;
    }
    dz = pPos->z - pCld->z;
    dx = pPos->x - pCld->x;
    return SQRTF(dx * dx + dz * dz) < pCld->radius;
}

// Eye trigger test: 1 when the trigger point (floor + height / 2), looking along ang_x / ang_y,
// sees the view cone `cone` (collision_point_cone_rev_play_face with the trigger's radius and
// opening angle). Quad / cylinder areas return 0.
BOOL AreaViewCheck(AREA_HIT_DATA* pAre, GEOM_CONE_REV* pCrev)
{
    Vec pos;
    Mtx m;
    Vec dir;
    Vec rot;
    f32 ang;
    int ret = 0;

    switch (pAre->type) {
    case AREA_TYPE_XZ4:
    case AREA_TYPE_CYLINDER:
        break;
    case AREA_TYPE_EYE:
        if (pAre->eye_trigger.open_ang == 0.0f) {
            ang = PI;
        } else {
            ang = pAre->eye_trigger.open_ang * 0.5f;
        }
        pos.x = pAre->eye_trigger.xz;
        pos.y = pAre->eye_trigger.floor;
        pos.z = pAre->eye_trigger.z;
        dir.x = 0.0f;
        dir.y = 0.0f;
        dir.z = 1.0f;
        rot.x = pAre->eye_trigger.ang_x;
        rot.y = pAre->eye_trigger.ang_y;
        rot.z = 0.0f;
        RotMatrix(m, &rot);
        PSMTXMultVecSR(m, &dir, &dir);
        ret = collision_point_cone_rev_play_face(&pos, pCrev, pAre->eye_trigger.radius, &dir, ang);
        break;
    default:
        pLog->warn(0, 0, AREA_TYPE_ERR, pAre->type);
        ret = 0;
        break;
    }
    return ret;
}

// Centre of the area at floor height (quad: mean of the 4 points).
void AreaGetCenterPos(Vec* pos, AREA_HIT_DATA* area)
{
    switch (area->type) {
    case AREA_TYPE_XZ4:
        pos->x = (area->xz4.p[0].x + area->xz4.p[1].x + area->xz4.p[2].x + area->xz4.p[3].x) * 0.25f;
        pos->y = area->xz4.floor;
        pos->z = (area->xz4.p[0].z + area->xz4.p[1].z + area->xz4.p[2].z + area->xz4.p[3].z) * 0.25f;
        break;
    case AREA_TYPE_CYLINDER:
        pos->x = area->cylinder.x;
        pos->y = area->cylinder.floor;
        pos->z = area->cylinder.z;
        break;
    case AREA_TYPE_EYE:
        pos->x = area->eye_trigger.xz;
        pos->y = area->eye_trigger.floor;
        pos->z = area->eye_trigger.z;
        break;
    default:
        pLog->warn(0, 0, AREA_TYPE_ERR, area->type);
        break;
    }
}

// A random point inside the area at floor height (bilinear on the quad; the centre for
// cylinder / eye) - enemy spawn points inside an area.
void AreaGetInsidePos(Vec* pos, AREA_HIT_DATA* area)
{
    switch (area->type) {
    case AREA_TYPE_XZ4: {
        Vec p0 = {area->xz4.p[0].x, area->xz4.floor, area->xz4.p[0].z};
        Vec p1 = {area->xz4.p[1].x, area->xz4.floor, area->xz4.p[1].z};
        Vec p2 = {area->xz4.p[2].x, area->xz4.floor, area->xz4.p[2].z};
        Vec p3 = {area->xz4.p[3].x, area->xz4.floor, area->xz4.p[3].z};
        Vec d01;
        Vec d32;
        Vec v0;
        Vec v3;
        Vec v;
        f32 s, t;

        PSVECSubtract(&p1, &p0, &d01);
        PSVECSubtract(&p2, &p3, &d32);
        s = fRand0_1();
        t = fRand0_1();
        PSVECScale(&d01, &v0, s);
        PSVECScale(&d32, &v3, s);
        PSVECAdd(&v3, &p3, &v3);
        PSVECSubtract(&v3, &p0, &v3);
        PSVECSubtract(&v3, &v0, &v);
        PSVECScale(&v, &v, t);
        PSVECAdd(&v, &v0, pos);
        PSVECAdd(pos, &p0, pos);
        break;
    }
    case AREA_TYPE_CYLINDER:
        pos->x = area->cylinder.x;
        pos->y = area->cylinder.floor;
        pos->z = area->cylinder.z;
        break;
    case AREA_TYPE_EYE:
        pos->x = area->eye_trigger.xz;
        pos->y = area->eye_trigger.floor;
        pos->z = area->eye_trigger.z;
        break;
    default:
        pos->x = 0.0f;
        pos->y = 0.0f;
        pos->z = 0.0f;
        break;
    }
}

// Builds a default area of `type` around `pos`: a size x size square, a cylinder of radius
// size / 2, or an eye trigger with cone length size / 2 and a 60 degree opening.
void AreaDataInit(AREA_HIT_DATA* area, Vec* pos, u8 type, f32 size, f32 height)
{
    area->be_flag = 1;
    area->pad = 0;
    area->type = type;

    switch (area->type) {
    case AREA_TYPE_XZ4: {
        AREA_XZ4* a = &area->xz4;
        f32 hs = size * 0.5f;
        a->floor = pos->y;
        a->height = height;
        a->radius = hs;
        a->p[0].x = pos->x - hs;
        a->p[0].z = pos->z - hs;
        a->p[1].x = pos->x + hs;
        a->p[1].z = pos->z - hs;
        a->p[2].x = pos->x + hs;
        a->p[2].z = pos->z + hs;
        a->p[3].x = pos->x - hs;
        a->p[3].z = pos->z + hs;
        break;
    }
    case AREA_TYPE_CYLINDER: {
        AREA_CYLINDER* a = &area->cylinder;
        a->x = pos->x;
        a->z = pos->z;
        a->floor = pos->y;
        a->height = height;
        a->radius = size * 0.5f;
        a->pad[0] = 0.0f;
        a->pad[1] = 0.0f;
        a->pad[2] = 0.0f;
        a->pad[3] = 0.0f;
        a->pad[4] = 0.0f;
        a->pad[5] = 0.0f;
        break;
    }
    case AREA_TYPE_EYE: {
        AREA_EYE_TRIGGER* a = &area->eye_trigger;
        a->xz = pos->x;
        a->z = pos->z;
        a->floor = pos->y;
        a->height = height;
        a->radius = size * 0.5f;
        a->open_ang = 0.0f;
        a->ang_x = 0.0f;
        a->ang_y = 0.0f;
        a->pad00 = 0.0f;
        a->pad[0] = 0.0f;
        a->pad[1] = 0.0f;
        break;
    }
    default:
        pLog->warn(0, 0, AREA_TYPE_ERR, area->type);
        break;
    }
}

// Debug draw helper: sphere at `pos` (transformed by mtx when given).
void area_Draw_sphere(Vec pos, f32 r, u32 rgb, Mtx pMat)
{
    if (pMat) {
        PSMTXMultVec(pMat, &pos, &pos);
    }
    Draw_sphere(&pos, r, rgb, 0, 0);
}

// Debug draw helper: line between two points (transformed by mtx when given).
void area_Draw_line(Vec pos1, Vec pos2, u32 rgb, Mtx pMat)
{
    if (pMat) {
        PSMTXMultVec(pMat, &pos1, &pos1);
        PSMTXMultVec(pMat, &pos2, &pos2);
    }
    GXSetLineWidth(16, 0);
    Draw_line3d(&pos1, &pos2, rgb, 0);
    GXSetLineWidth(6, 0);
}

// Debug tool editor step: Y + X cycles the area type (rebuilding a 4000 unit default at the old
// centre), computes the camera-relative move axes and stick deltas (scaled by `rate`), then runs
// the type's editor and draws it in `color`.
void AreaDataEdit(AREA_HIT_DATA* area, u32 col, int flg, Mtx pMat, f32 move_scale)
{
    Vec vx;
    Vec vy;
    Vec v;
    Vec center;
    f32 dx, dy;
    int mode;

    if ((Joy[0].on & JOY_Y) && (Joy[0].trg & JOY_X)) {
        switch (area->type) {
        case AREA_TYPE_XZ4:
            area->type = AREA_TYPE_CYLINDER;
            AreaGetCenterPos(&center, area);
            AreaDataInit(area, &center, area->type, 4000.0f, 4000.0f);
            break;
        case AREA_TYPE_CYLINDER:
            area->type = AREA_TYPE_EYE;
            AreaGetCenterPos(&center, area);
            AreaDataInit(area, &center, area->type, 4000.0f, 4000.0f);
            break;
        case AREA_TYPE_EYE:
            area->type = AREA_TYPE_XZ4;
            AreaGetCenterPos(&center, area);
            AreaDataInit(area, &center, area->type, 4000.0f, 4000.0f);
            break;
        default:
            pLog->warn(0, 0, AREA_TYPE_ERR, area->type);
            return;
        }
    }

    v.x = 1.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    moveOnPlaneXZ(&v, &vx);
    v.x = 0.0f;
    v.y = 1.0f;
    v.z = 0.0f;
    moveOnPlaneXZ(&v, &vy);

    dx = (f32) Joy[0].stickX * 1.3f * move_scale;
    if (Joy[0].on & 0x10001) {
        dx -= 100.0f;
    }
    if (Joy[0].on & 0x20002) {
        dx += 100.0f;
    }
    dy = (f32) Joy[0].stickY * 1.3f * move_scale;
    if (Joy[0].on & 0x80008) {
        dy += 100.0f;
    }
    if (Joy[0].on & 0x40004) {
        dy -= 100.0f;
    }

    mode = 0;
    if (Joy[0].on & JOY_Y) {
        mode = 3;
    }
    if (Joy[0].on & JOY_A) {
        mode = 1;
    }
    if (Joy[0].on & JOY_X) {
        mode = 2;
    }
    if ((Joy[0].on & (JOY_Y | JOY_A)) == (JOY_Y | JOY_A)) {
        mode = 4;
    }

    {
        switch (area->type) {
        case AREA_TYPE_XZ4:
            area_xz4_Edit(&area->xz4, col, flg, pMat, mode, vx, vy, dx, dy, move_scale);
            break;
        case AREA_TYPE_CYLINDER:
            area_cylinder_Edit(&area->cylinder, col, flg, pMat, mode, vx, vy, dx, dy, move_scale);
            break;
        case AREA_TYPE_EYE:
            area_eye_trigger_Edit(&area->eye_trigger, col, flg, pMat, mode, vx, vy, dx, dy, move_scale);
            break;
        default: {
            pLog->warn(0, 0, AREA_TYPE_ERR, area->type);
            Vec zero = {0.0f, 0.0f, 0.0f};
            AreaDataInit(area, &zero, AREA_TYPE_XZ4, 2000.0f, 1000.0f);
            break;
        }
        }
    }
}

// The split object's .sdata (sel/Rcnt below) is 8-byte aligned.
asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");

// Quad editor: L / R select a point, A moves the selected point, X moves all four, Y moves the
// floor, Y + B changes the height, Z rotates the quad about its centre; then draws it.
void area_xz4_Edit(AREA_XZ4* pXz4, u32 col, int flg, Mtx pMat, u32 state, Vec vec1, Vec vec2, f32 move_x, f32 move_y, f32 move_scale)
{
    static u32 sel = 0;
    static f32 Rcnt = 0.0f;
    Vec p;
    Vec q;
    Vec t;
    f32 r;
    u32 i;
    f32* px;
    f32* pz;
    f32* d;

    Rcnt += move_scale * 3.0f;
    if (Rcnt > move_scale * 90.0f) {
        Rcnt = move_scale * 45.0f;
    }

    switch (state) {
    case 0:
        if (Joy[0].rep & 0x10001) {
            sel--;
            Rcnt = move_scale * 45.0f;
        }
        if (sel > 3) {
            sel = 3;
        }
        if (Joy[0].rep & 0x20002) {
            sel++;
            Rcnt = move_scale * 45.0f;
        }
        if (sel > 3) {
            sel = 0;
        }
        break;
    case 1:
        // The point is written back through plain float pointers (`*d = v`, no member access): such
        // a store is assumed to alias the static `sel`, which is reloaded before the second store.
        px = &pXz4->p[0].x;
        pz = &pXz4->p[0].z;
        p.x = pXz4->p[sel].x;
        p.y = pXz4->floor;
        p.z = pXz4->p[sel].z;
        PSVECScale(&vec1, &t, move_x);
        PSVECAdd(&t, &p, &p);
        PSVECScale(&vec2, &t, move_y);
        PSVECAdd(&t, &p, &p);
        d = px + sel * 2;
        *d = p.x;
        d = pz + sel * 2;
        *d = p.z;
        break;
    case 2:
        for (i = 0; i < 4; i++) {
            p.x = pXz4->p[i].x;
            p.y = pXz4->floor;
            p.z = pXz4->p[i].z;
            PSVECScale(&vec1, &t, move_x);
            PSVECAdd(&t, &p, &p);
            PSVECScale(&vec2, &t, move_y);
            PSVECAdd(&t, &p, &p);
            pXz4->p[i].x = p.x;
            pXz4->p[i].z = p.z;
        }
        break;
    case 3:
        pXz4->height += move_y;
        break;
    case 4:
        pXz4->floor += move_y;
        break;
    }

    r = move_scale * 100.0f;
    switch (state) {
    case 0:
        p.x = pXz4->p[sel].x;
        p.y = pXz4->floor;
        p.z = pXz4->p[sel].z;
        area_Draw_sphere(p, Rcnt, 0x00FF00FE, pMat);
        q.x = pXz4->p[sel].x;
        q.y = pXz4->floor + pXz4->height;
        q.z = pXz4->p[sel].z;
        area_Draw_line(p, q, 0xFF00FF00, pMat);
        break;
    case 1:
        p.x = pXz4->p[sel].x;
        p.y = pXz4->floor;
        p.z = pXz4->p[sel].z;
        area_Draw_sphere(p, r, 0xFFFF00FE, pMat);
        break;
    case 2:
        for (i = 0; i < 4; i++) {
            p.x = pXz4->p[i].x;
            p.y = pXz4->floor;
            p.z = pXz4->p[i].z;
            area_Draw_sphere(p, r, 0xFFFF00FE, pMat);
        }
        break;
    case 3:
        for (i = 0; i < 4; i++) {
            p.x = pXz4->p[i].x;
            p.y = pXz4->floor + pXz4->height;
            p.z = pXz4->p[i].z;
            area_Draw_sphere(p, r, 0xFFFF00FE, pMat);
        }
        break;
    case 4:
        for (i = 0; i < 4; i++) {
            p.x = pXz4->p[i].x;
            p.y = pXz4->floor;
            p.z = pXz4->p[i].z;
            area_Draw_sphere(p, r, 0xFFFF00FE, pMat);
            p.y = pXz4->floor + pXz4->height;
            area_Draw_sphere(p, r, 0xFFFF00FE, pMat);
        }
        break;
    }

    area_xz4_Disp(pXz4, col, flg, pMat);
}

// Cylinder editor: A / X move the centre, B changes the radius, Y the floor, Y + B the height;
// then draws it.
void area_cylinder_Edit(AREA_CYLINDER* pCld, u32 color, int flag, Mtx mtx, u32 mode, Vec vx, Vec vy, f32 dx, f32 dy, f32 rate)
{
    Vec p;
    Vec t;
    Vec q;
    Vec c;
    f32 r;
    u32 n = 0;
    u32 start = 0;
    u32 col = 0;
    u32 div = 12;
    u32 j, k;
    f32 ang;

    switch (mode) {
    case 0:
        break;
    case 1:
        pCld->radius += dx;
        if (pCld->radius < 10.0f) {
            pCld->radius = 10.0f;
        }
        break;
    case 2:
        p.x = pCld->x;
        p.y = pCld->floor;
        p.z = pCld->z;
        PSVECScale(&vx, &t, dx);
        PSVECAdd(&t, &p, &p);
        PSVECScale(&vy, &t, dy);
        PSVECAdd(&t, &p, &p);
        pCld->x = p.x;
        pCld->z = p.z;
        break;
    case 3:
        pCld->height += dy;
        break;
    case 4:
        pCld->floor += dy;
        break;
    }

    r = rate * 100.0f;
    switch (mode) {
    case 0:
        start = 0;
        n = 1;
        col = 0x00FF00FE;
        break;
    case 1:
        start = 0;
        n = 1;
        col = 0xFFFF00FE;
        div = 1;
        c.x = pCld->x;
        c.y = pCld->floor;
        c.z = pCld->z;
        area_Draw_sphere(c, r, 0xFFFF00FE, mtx);
        break;
    case 2:
        start = 0;
        n = 1;
        col = 0xFFFF00FE;
        break;
    case 3:
        start = 1;
        n = 2;
        col = 0xFFFF00FE;
        break;
    case 4:
        start = 0;
        n = 2;
        col = 0xFFFF00FE;
        break;
    }

    for (j = start; j < n; j++) {
        if (j == 0) {
            c.y = pCld->floor;
        } else {
            c.y = pCld->floor + pCld->height;
        }
        c.x = pCld->x;
        c.z = pCld->z;
        ang = 0.0f;
        for (k = 0; k < div; k++) {
            q.x = pCld->radius * sinf(ang);
            q.y = 0.0f;
            q.z = pCld->radius * cosf(ang);
            PSVECAdd(&c, &q, &p);
            area_Draw_sphere(p, r, col, mtx);
            ang += 6.28f / (f32) (div - 1);
        }
    }

    area_cylinder_Disp(pCld, color, flag, mtx);
}

// Eye trigger editor: A / X move the point, B changes the cone length, Y the floor / height, Z
// turns the view direction and R the opening angle; then draws it.
void area_eye_trigger_Edit(AREA_EYE_TRIGGER* pEtg, u32 color, int flag, Mtx mtx, u32 mode, Vec vx, Vec vy, f32 dx, f32 dy, f32 rate)
{
    Vec p;
    Vec t;
    Vec q;
    Vec c;
    f32 r;
    u32 n = 0;
    u32 col = 0;
    u32 div = 12;
    u32 j, k;
    f32 ang;

    switch (mode) {
    case 0:
        break;
    case 1:
        pEtg->radius += dy * 0.05f;
        if (pEtg->radius < 0.0f) {
            pEtg->radius = 0.0f;
        }
        pEtg->open_ang += dx * 0.0005f;
        if (pEtg->open_ang < 0.0f) {
            pEtg->open_ang = 0.0f;
        }
        if (pEtg->open_ang > 6.28f) {
            pEtg->open_ang = 6.28f;
        }
        break;
    case 2:
        p.x = pEtg->xz;
        p.y = pEtg->floor;
        p.z = pEtg->z;
        PSVECScale(&vx, &t, dx);
        PSVECAdd(&t, &p, &p);
        PSVECScale(&vy, &t, dy);
        PSVECAdd(&t, &p, &p);
        pEtg->xz = p.x;
        pEtg->z = p.z;
        break;
    case 3:
        pEtg->floor += dy;
        break;
    case 4:
        pEtg->ang_x += dy * 0.0005f;
        pEtg->ang_y += dx * 0.0005f;
        pEtg->ang_x = LIMIT_ANGLE(pEtg->ang_x);
        pEtg->ang_y = LIMIT_ANGLE(pEtg->ang_y);
        break;
    }

    r = rate * 80.0f;
    switch (mode) {
    case 0:
        n = 1;
        col = 0x00FF00FE;
        break;
    case 1:
        n = 1;
        col = 0xFFFF00FE;
        div = 1;
        c.x = pEtg->xz;
        c.y = pEtg->floor;
        c.z = pEtg->z;
        area_Draw_sphere(c, r, 0xFFFF00FE, mtx);
        break;
    case 2:
        n = 1;
        col = 0xFFFF00FE;
        break;
    case 3:
        n = 2;
        col = 0xFFFF00FE;
        break;
    case 4:
        n = 0;
        col = 0xFFFF00FE;
        break;
    }

    for (j = 0; j < n; j++) {
        if (j == 0) {
            c.y = pEtg->floor;
        } else {
            c.y = pEtg->floor + pEtg->height;
        }
        c.x = pEtg->xz;
        c.z = pEtg->z;
        ang = 0.0f;
        for (k = 0; k < div; k++) {
            q.x = pEtg->radius * sinf(ang);
            q.y = 0.0f;
            q.z = pEtg->radius * cosf(ang);
            PSVECAdd(&c, &q, &p);
            area_Draw_sphere(p, r, col, mtx);
            ang += 6.28f / (f32) (div - 1);
        }
    }

    area_eye_trigger_Disp(pEtg, color, flag, mtx);
}

// Debug wireframe of the area in `color` (flag: filled / outline variant) by type.
void AreaDataDisp(AREA_HIT_DATA* pAre, u32 col, int flg, Mtx pMat)
{
    switch (pAre->type) {
    case AREA_TYPE_XZ4:
        area_xz4_Disp(&pAre->xz4, col, flg, pMat);
        break;
    case AREA_TYPE_CYLINDER:
        area_cylinder_Disp(&pAre->cylinder, col, flg, pMat);
        break;
    case AREA_TYPE_EYE:
        area_eye_trigger_Disp(&pAre->eye_trigger, col, flg, pMat);
        break;
    default: {
        pLog->warn(0, 0, AREA_TYPE_ERR, pAre->type);
        Vec zero = {0.0f, 0.0f, 0.0f};
        AreaDataInit(pAre, &zero, AREA_TYPE_XZ4, 2000.0f, 1000.0f);
        break;
    }
    }
}

// Draws the quad prism: the four side polygons from floor to floor + height and the edges.
void area_xz4_Disp(AREA_XZ4* pXz4, u32 color, int flag, Mtx mtx)
{
    Vec v[5];
    Vec w[5];
    u32 i;

    for (i = 0; i < 4; i++) {
        v[i].x = pXz4->p[i].x;
        v[i].y = pXz4->floor;
        v[i].z = pXz4->p[i].z;
    }
    v[4].x = pXz4->p[0].x;
    v[4].y = pXz4->floor;
    v[4].z = pXz4->p[0].z;
    for (i = 0; i < 5; i++) {
        w[i] = v[i];
        w[i].y += pXz4->height;
    }
    if (mtx) {
        for (i = 0; i < 5; i++) {
            PSMTXMultVec(mtx, &v[i], &v[i]);
            PSMTXMultVec(mtx, &w[i], &w[i]);
        }
    }
    Draw_poly(v, color, 0);
    Draw_poly(&v[2], color, 0);
    if (flag & 1) {
        Vec tri[3];
        u32 col = (color & 0x00FFFFFF) + ((color & 0xFF000000) >> 2);
        tri[0] = v[0];
        tri[1] = v[1];
        tri[2] = w[1];
        Draw_poly(tri, col, 0);
        tri[0] = w[0];
        tri[1] = w[1];
        tri[2] = v[0];
        Draw_poly(tri, col, 0);
        tri[0] = v[1];
        tri[1] = v[2];
        tri[2] = w[2];
        Draw_poly(tri, col, 0);
        tri[0] = w[1];
        tri[1] = w[2];
        tri[2] = v[1];
        Draw_poly(tri, col, 0);
        tri[0] = v[2];
        tri[1] = v[3];
        tri[2] = w[3];
        Draw_poly(tri, col, 0);
        tri[0] = w[2];
        tri[1] = w[3];
        tri[2] = v[2];
        Draw_poly(tri, col, 0);
        tri[0] = v[3];
        tri[1] = v[0];
        tri[2] = w[0];
        Draw_poly(tri, col, 0);
        tri[0] = w[3];
        tri[1] = w[0];
        tri[2] = v[3];
        Draw_poly(tri, col, 0);
    }
    for (i = 0; i < 4; i++) {
        Draw_line3d(&v[i], &v[i + 1], color, 0);
        Draw_line3d(&w[i], &w[i + 1], color, 0);
        Draw_line3d(&v[i], &w[i], color, 0);
    }
}

// Draws the cylinder as a 16-segment ring at the floor and the top with vertical edges.
void area_cylinder_Disp(AREA_CYLINDER* pCld, u32 color, int flag, Mtx mtx)
{
    Vec tri2[3];
    Vec tri[3];
    Vec q;
    Vec c;
    Vec p;
    Vec prev;
    u32 j, k;
    u32 div = 16;
    f32 ang;
    u32 col;

    for (j = 0; j < 2; j++) {
        if (j == 0) {
            c.y = pCld->floor;
        } else {
            c.y = pCld->floor + pCld->height;
        }
        c.x = pCld->x;
        c.z = pCld->z;
        if (mtx) {
            PSMTXMultVec(mtx, &c, &c);
        }
        ang = 0.0f;
        for (k = 0; k < div; k++) {
            q.x = pCld->radius * sinf(ang);
            q.y = 0.0f;
            q.z = pCld->radius * cosf(ang);
            PSVECAdd(&c, &q, &p);
            if (k != 0) {
                Draw_line3d(&prev, &p, color, 0);
            }
            if (j == 0) {
                q.x = 0.0f;
                q.y = pCld->height;
                q.z = 0.0f;
                PSVECAdd(&p, &q, &q);
                Draw_line3d(&p, &q, color, 0);
                if (k != 0) {
                    tri[0] = p;
                    tri[1] = prev;
                    tri[2] = c;
                    Draw_poly(tri, color, 1);
                    if (flag & 1) {
                        col = color & 0x00FFFFFF;
                        col += (color & 0xFF000000) >> 2;
                        tri2[0] = p;
                        tri2[1] = prev;
                        tri2[2] = prev;
                        tri2[2].y += pCld->height;
                        Draw_poly(tri2, col, 0);
                        tri2[0] = p;
                        tri2[1] = prev;
                        tri2[2] = p;
                        tri2[0].y += pCld->height;
                        tri2[1].y += pCld->height;
                        Draw_poly(tri2, col, 0);
                    }
                }
            }
            prev = p;
            ang += 6.2831855f / (f32) (div - 1);
        }
    }
}

// Draws the eye trigger: its point and the view cone (Draw_corn2) of its length / opening.
void area_eye_trigger_Disp(AREA_EYE_TRIGGER* pEtg, u32 col, int flg, Mtx pMat)
{
    Vec c;

    c.x = pEtg->xz;
    c.y = pEtg->floor;
    c.z = pEtg->z;
    area_Draw_sphere(c, 10.0f, col, pMat);
    area_Draw_sphere(c, pEtg->radius, col, pMat);
    if (pEtg->open_ang != 0.0f) {
        Mtx m;
        Vec dir;
        Vec rot;
        dir.x = 0.0f;
        dir.y = 0.0f;
        dir.z = 1.0f;
        rot.x = pEtg->ang_x;
        rot.y = pEtg->ang_y;
        rot.z = 0.0f;
        RotMatrix(m, &rot);
        PSMTXMultVecSR(m, &dir, &dir);
        Draw_corn2(&c, &dir, 1000.0f, pEtg->open_ang * 360.0f / 6.28f, col);
    }
}

// Debug text: the area's numeric parameters (points / centre, radius, floor, height, angles)
// printed from screen position x / y.
void AreaDataInfoDisp(AREA_HIT_DATA* pArea, int x, s16 y)
{
    Vec pos;
    Vec scr;
    Vec posCopy;

    switch (pArea->type) {
    case AREA_TYPE_XZ4: {
        AREA_XZ4* a = &pArea->xz4;
        eprintf(x, y, 0, 0, "P0[%6.0f,%6.0f]", a->p[0].x, a->p[0].z);
        y += 16;
        eprintf(x, y, 0, 0, "P1[%6.0f,%6.0f]", a->p[1].x, a->p[1].z);
        y += 16;
        eprintf(x, y, 0, 0, "P2[%6.0f,%6.0f]", a->p[2].x, a->p[2].z);
        y += 16;
        eprintf(x, y, 0, 0, "P3[%6.0f,%6.0f]", a->p[3].x, a->p[3].z);
        y += 16;
        eprintf(x, y, 0, 0, "PY[%6.0f]", a->floor);
        y += 16;
        eprintf(x, y, 0, 0, "HEIGHT[%6.0f]", a->height);

        pos.x = a->p[0].x;
        pos.y = a->floor;
        pos.z = a->p[0].z;
        posCopy = pos;
        GetScreenPos(&posCopy, &scr);
        eprintf2(6, 12, (int) scr.x + 8, (int) scr.y + 16, 0, 0, "P0");
        pos.x = a->p[1].x;
        pos.y = a->floor;
        pos.z = a->p[1].z;
        posCopy = pos;
        GetScreenPos(&posCopy, &scr);
        eprintf2(6, 12, (int) scr.x + 8, (int) scr.y + 16, 0, 0, "P1");
        pos.x = a->p[2].x;
        pos.y = a->floor;
        pos.z = a->p[2].z;
        posCopy = pos;
        GetScreenPos(&posCopy, &scr);
        eprintf2(6, 12, (int) scr.x + 8, (int) scr.y + 16, 0, 0, "P2");
        pos.x = a->p[3].x;
        pos.y = a->floor;
        pos.z = a->p[3].z;
        posCopy = pos;
        GetScreenPos(&posCopy, &scr);
        eprintf2(6, 12, (int) scr.x + 8, (int) scr.y + 16, 0, 0, "P3");
        break;
    }
    case AREA_TYPE_CYLINDER: {
        AREA_CYLINDER* a = &pArea->cylinder;
        eprintf(x, y, 0, 0, "P0[%6.0f,%6.0f,%6.0f]", a->x, a->floor, a->z);
        y += 16;
        eprintf(x, y, 0, 0, "HEIGHT[%6.0f]", a->height);
        y += 16;
        eprintf(x, y, 0, 0, "RADIUS[%6.0f]", a->radius);

        pos.x = a->x;
        pos.y = a->floor;
        pos.z = a->z;
        posCopy = pos;
        GetScreenPos(&posCopy, &scr);
        eprintf2(6, 12, (int) scr.x + 8, (int) scr.y + 16, 0, 0, "P0");
        break;
    }
    case AREA_TYPE_EYE: {
        AREA_EYE_TRIGGER* a = &pArea->eye_trigger;
        eprintf(x, y, 0, 0, "P0[%6.0f,%6.0f,%6.0f]", a->xz, a->floor, a->z);
        y += 16;
        eprintf(x, y, 0, 0, "RADIUS[%6.0f]", a->radius);
        y += 16;
        if (a->open_ang == 0.0f) {
            eprintf(x, y, 0, 0, "OPEN ANGLE[360]");
        } else {
            eprintf(x, y, 0, 0, "OPEN ANGLE[%3.0f]", a->open_ang * 57.295776f);
        }
        y += 16;
        eprintf(x, y, 0, 0, "ANGLE_X[%3.0f]", a->ang_x * 57.295776f);
        y += 16;
        eprintf(x, y, 0, 0, "ANGLE_Y[%3.0f]", a->ang_y * 57.295776f);

        pos.x = a->xz;
        pos.y = a->floor;
        pos.z = a->z;
        posCopy = pos;
        GetScreenPos(&posCopy, &scr);
        eprintf2(6, 12, (int) scr.x + 8, (int) scr.y + 16, 0, 0, "P0");
        break;
    }
    }
}

#define HELP_LINE(cond, str)          \
    if (cond) {                       \
        eprintf(x, y, 6, 0, str);     \
    } else {                          \
        eprintf(x, y, 5, 0, str);     \
    }

// Debug text: the pad help of the current editor, highlighting the buttons being held.
void AreaDataHelpDisp(AREA_HIT_DATA* pArea, int x, s16 y)
{
    switch (pArea->type) {
    case AREA_TYPE_XZ4:
        HELP_LINE(Joy[0].on & 0x30003, " LR: point select");
        y += 16;
        HELP_LINE((Joy[0].on & (JOY_Y | JOY_A)) == JOY_A, "  A: 1 point move");
        y += 16;
        HELP_LINE(Joy[0].on & JOY_X, "  X: all point move");
        y += 16;
        HELP_LINE((Joy[0].on & (JOY_Y | JOY_A)) == JOY_Y, "  Y: height move");
        y += 16;
        HELP_LINE((Joy[0].on & (JOY_Y | JOY_A)) == (JOY_Y | JOY_A), "Y+A: Y pos move");
        y += 16;
        break;
    case AREA_TYPE_CYLINDER:
        HELP_LINE(Joy[0].on & JOY_X, "  X: pos move");
        y += 16;
        HELP_LINE((Joy[0].on & (JOY_Y | JOY_A)) == JOY_A, "  A: radius move");
        y += 16;
        HELP_LINE((Joy[0].on & (JOY_Y | JOY_A)) == JOY_Y, "  Y: height move");
        y += 16;
        HELP_LINE((Joy[0].on & (JOY_Y | JOY_A)) == (JOY_Y | JOY_A), "Y+A: Y pos move");
        y += 16;
        break;
    case AREA_TYPE_EYE:
        HELP_LINE(Joy[0].on & JOY_X, "  X: pos move");
        y += 16;
        HELP_LINE(Joy[0].on & JOY_Y, "  Y: Y pos move");
        y += 16;
        if ((Joy[0].on & (JOY_Y | JOY_A)) == JOY_A) {
            eprintf(x, y, 6, 0, "  A: open angle");
            y += 16;
            eprintf(x, y, 6, 0, "   : radius move");
        } else {
            eprintf(x, y, 5, 0, "  A: open angle");
            y += 16;
            eprintf(x, y, 5, 0, "   : radius move");
        }
        y += 16;
        HELP_LINE((Joy[0].on & (JOY_Y | JOY_A)) == (JOY_Y | JOY_A), "Y+A: front angle");
        y += 16;
        break;
    }
}
