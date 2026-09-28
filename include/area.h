#ifndef AREA_H
#define AREA_H

#include "types.h"
#include "vec.h"

// Trigger volumes (game/area.cpp): AREA_HIT_DATA is a 4-byte header followed by a 0x2C-byte body whose
// layout depends on the type (0x30 bytes total, e.g. flr_at.h `area[0x30]`).
#define AREA_TYPE_XZ4      1  // vertical prism over a quadrilateral in the XZ plane
#define AREA_TYPE_CYLINDER 2
#define AREA_TYPE_EYE      3  // view cone trigger

struct AreaXZ {
    f32 x;
    f32 z;
};

struct AREA_XZ4 {
    f32 floor;         // 0x00  floor height
    f32 height;         // 0x04  height
    f32 radius;         // 0x08  (editor) point marker radius
    AreaXZ p[4];   // 0x0C
};

struct AREA_CYLINDER {
    f32 floor;         // 0x00
    f32 height;         // 0x04
    f32 radius;         // 0x08  radius
    f32 x;         // 0x0C
    f32 z;         // 0x10
    f32 pad[6];    // 0x14  zeroed by AreaDataInit
};

struct AREA_EYE_TRIGGER {
    f32 floor;         // 0x00
    f32 height;         // 0x04
    f32 radius;         // 0x08  cone length (margin)
    f32 xz;         // 0x0C
    f32 z;         // 0x10
    f32 ang_x;     // 0x14  view direction (rotation about X)
    f32 ang_y;     // 0x18  view direction (rotation about Y)
    f32 pad00;     // 0x1C  zeroed by AreaDataInit
    f32 open_ang;      // 0x20  opening angle in radians (0 = all round)
    f32 pad[2];    // 0x24  zeroed by AreaDataInit
};

struct AREA_HIT_DATA {
    u8 be_flag;       // 0x00  1 = in use
    u8 type;       // 0x01  AREA_TYPE_*
    u16 pad;       // 0x02
    union {
        AREA_XZ4 xz4;             // 0x04
        AREA_CYLINDER cylinder;   // 0x04
        AREA_EYE_TRIGGER eye_trigger; // 0x04
    };
};

struct GEOM_CONE_REV;

BOOL AreaHitCheck(AREA_HIT_DATA* pAre, Vec* pPos);

BOOL areaHitCheck_xz4(AREA_XZ4* pXz4, Vec* pPos);
BOOL areaHitCheck_Cylinder(AREA_CYLINDER* pCld, Vec* pPos);
BOOL AreaViewCheck(AREA_HIT_DATA* pAre, GEOM_CONE_REV* pCrev);
void AreaGetCenterPos(Vec* pos, AREA_HIT_DATA* area);
void AreaGetInsidePos(Vec* pos, AREA_HIT_DATA* area);
void AreaDataInit(AREA_HIT_DATA* area, Vec* pos, u8 type, f32 size, f32 height);
void area_Draw_sphere(Vec pos, f32 r, u32 rgb, Mtx pMat);
void area_Draw_line(Vec pos1, Vec pos2, u32 rgb, Mtx pMat);
void AreaDataEdit(AREA_HIT_DATA* area, u32 col, int flg, Mtx pMat, f32 move_scale);
void area_xz4_Edit(AREA_XZ4* pXz4, u32 col, int flg, Mtx pMat, u32 state, Vec vec1, Vec vec2, f32 move_x, f32 move_y, f32 move_scale);
void area_cylinder_Edit(AREA_CYLINDER* pCld, u32 color, int flag, Mtx mtx, u32 mode, Vec vx, Vec vy, f32 dx, f32 dy, f32 rate);
void area_eye_trigger_Edit(AREA_EYE_TRIGGER* pEtg, u32 color, int flag, Mtx mtx, u32 mode, Vec vx, Vec vy, f32 dx, f32 dy, f32 rate);
void AreaDataDisp(AREA_HIT_DATA* pAre, u32 col, int flg, Mtx pMat);
void area_xz4_Disp(AREA_XZ4* pXz4, u32 color, int flag, Mtx mtx);
void area_cylinder_Disp(AREA_CYLINDER* pCld, u32 color, int flag, Mtx mtx);
void area_eye_trigger_Disp(AREA_EYE_TRIGGER* pEtg, u32 col, int flg, Mtx pMat);
void AreaDataInfoDisp(AREA_HIT_DATA* pArea, int x, s16 y);
void AreaDataHelpDisp(AREA_HIT_DATA* pArea, int x, s16 y);

#endif
