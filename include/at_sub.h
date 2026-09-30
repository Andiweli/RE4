#ifndef AT_SUB_H
#define AT_SUB_H

#include "types.h"
#include "vec.h"

class cSat;

// Check flags of the At_poly_*_ck / hitCheck `flag` word (PS2 SAT_TYPE): which atari sets to test and, in
// SAT_TYPE_MIDDLE..SAT_TYPE_SEE, which *_NOHIT attribute bits exclude a polygon.
enum SAT_TYPE {
    SAT_TOP_WK = 0x00000001,
    SAT_SHAPE_RECTANGLE = 0x00000002,
    SAT_OBJ_ADJ_CANCEL0 = 0,
    SAT_OBJ_ADJ_CANCEL = 0x00000004,
    SAT_OBJ_ADJ_CANCEL1 = 0x00000004,
    SAT_OBJ_ADJ_CANCEL2 = 0x00000008,
    SAT_OBJ_ADJ_CANCEL3 = 0x00000010,
    SAT_EDGE_CANCEL = 0x00000020,
    SAT_CK_ALL = 0,
    SAT_CK_FLOOR = 0x00000040,
    SAT_CK_WALL = 0x00000080,
    SAT_SCA_ENABLE = 0x00000100,
    SAT_OBA_ENABLE = 0x00000200,
    SAT_TYPE_NONE = 0,
    SAT_TYPE_MIDDLE = 0x00000400,
    SAT_TYPE_SMALL = 0x00000800,
    SAT_TYPE_PL = 0x00001000,
    SAT_TYPE_EM = 0x00002000,
    SAT_TYPE_ROUTE = 0x00004000,
    SAT_TYPE_SEE = 0x00008000
};

// Attribute word of a scenario (SAT) polygon (PS2 SAT_ATTR): the At_poly_*_ck filters, hitCheck results.
// SAT_ATTR_HIT is or-ed into every returned attribute; bits 24..31 are the hit / A / V-cancel flags.
enum SAT_ATTR {
    SAT_ATTR_NOHIT = 0,
    SAT_ATTR_HIT = 0x01000000,
    SAT_ATTR_A02 = 0x02000000,
    SAT_ATTR_A04 = 0x04000000,
    SAT_ATTR_A08 = 0x08000000,
    SAT_ATTR_A10 = 0x10000000,
    SAT_ATTR_V0_CANCEL = 0x20000000,
    SAT_ATTR_V1_CANCEL = 0x40000000,
    SAT_ATTR_V2_CANCEL = 0x80000000,
    SAT_ATTR_SEE_NOHIT = 0x00800000,
    SAT_ATTR_SMALL_NOHIT = 0x00008000,
    SAT_ATTR_STEPS = 0x00000080,
    SAT_ATTR_PL_NOHIT = 0x00400000,
    SAT_ATTR_EM_NOHIT = 0x00004000,
    SAT_ATTR_ROUTE_NOHIT = 0x00000040,
    SAT_ATTR_UP = 0x00200000,
    SAT_ATTR_DOWN = 0x00002000,
    SAT_ATTR_FANCE = 0x00000020,
    SAT_ATTR_FALL = 0x00100000,
    SAT_ATTR_UP2 = 0x00001000,
    SAT_ATTR_DOWN2 = 0x00000010,
    SAT_ATTR_JUMPOVER = 0x00080000,
    SAT_ATTR_CLIFF = 0x00000800,
    SAT_ATTR_HIDE = 0x00000008,
    SAT_ATTR_FALL_FANCE = 0x00040000,
    SAT_ATTR_ONLY_SEE_HIT = 0x00000400,
    SAT_ATTR_NO_EFF_SET = 0x00000004,
    SAT_ATTR_R02 = 0x00020000,
    SAT_ATTR_G02 = 0x00000200,
    SAT_ATTR_B02 = 0x00000002,
    SAT_ATTR_SPECIAL3 = 0x00010000,
    SAT_ATTR_SPECIAL2 = 0x00000100,
    SAT_ATTR_SPECIAL = 0x00000001
};

// Attribute word of an enemy / object atari (EAT) polygon (PS2 EAT_ATTR): same layout, the low bits are
// the effect type (EatGetEffectType: EFF_BIT0..2) and the RGB colour bits. The At_poly_*_ck filters
// use it when SEck is set.
enum EAT_ATTR {
    EAT_ATTR_NOHIT = 0,
    EAT_ATTR_HIT = 0x01000000,
    EAT_ATTR_A02 = 0x02000000,
    EAT_ATTR_A04 = 0x04000000,
    EAT_ATTR_A08 = 0x08000000,
    EAT_ATTR_A10 = 0x10000000,
    EAT_ATTR_V0_CANCEL = 0x20000000,
    EAT_ATTR_V1_CANCEL = 0x40000000,
    EAT_ATTR_V2_CANCEL = 0x80000000,
    EAT_ATTR_EFF_BIT0 = 0x00800000,
    EAT_ATTR_EFF_BIT1 = 0x00008000,
    EAT_ATTR_EFF_BIT2 = 0x00000080,
    EAT_ATTR_SMALL_NOHIT = 0x00400000,
    EAT_ATTR_MIDDLE_NOHIT = 0x00004000,
    EAT_ATTR_NO_EFF_SET = 0x00000040,
    EAT_ATTR_R20 = 0x00200000,
    EAT_ATTR_G20 = 0x00002000,
    EAT_ATTR_B20 = 0x00000020,
    EAT_ATTR_R10 = 0x00100000,
    EAT_ATTR_G10 = 0x00001000,
    EAT_ATTR_B10 = 0x00000010,
    EAT_ATTR_R08 = 0x00080000,
    EAT_ATTR_G08 = 0x00000800,
    EAT_ATTR_B08 = 0x00000008,
    EAT_ATTR_R04 = 0x00040000,
    EAT_ATTR_G04 = 0x00000400,
    EAT_ATTR_B04 = 0x00000004,
    EAT_ATTR_R02 = 0x00020000,
    EAT_ATTR_G02 = 0x00000200,
    EAT_ATTR_B02 = 0x00000002,
    EAT_ATTR_R01 = 0x00010000,
    EAT_ATTR_G01 = 0x00000100,
    EAT_ATTR_SPECIAL = 0x00000001
};

// One collision triangle (0x14 bytes): three vertex, one normal and three edge indices, attribute.
struct AtPoly {
    u16 v[3];        // 0x00
    u16 n;           // 0x06
    u16 e[3];        // 0x08
    u8 pad_E[2];
    union {
        struct {
            u16 attrHi;  // 0x10  attribute word high half
            u16 attrLo;  // 0x12
        };
        u32 attr;        // 0x10  the attribute word (SAT_ATTR / EAT_ATTR; atari createSat)
    };
};

extern int SEck;   // game/atari.cpp: scenario-effect check mode (skips the attribute filters)

// Signed distance of `p` from the plane through `a` with normal `n`.
f32 At_surface_point_rel(Vec* vert, Vec* norm, Vec* point);
// Segment p0-p1 against that plane; the crossing point goes to `out` (p1 when it does not cross).
int At_surface_line_ck(Vec* cross, Vec* a, Vec* norm, Vec* point1, Vec* point2);
// Point `p` (on the plane) inside the triangle `poly` with normal `nrm`.
int At_poly_point_rel(Vec* poly, Vec* nrm, Vec* p);
// Sphere against a box given as 8 vertices.
int At_box_sphere_ck(Vec* pBoxVec, Vec* pPos, f32 r);
// Normal of a triangle.
void Get_normal(Vec* pv, Vec* norm);
u32 AtBoxCapsuleCk3(Vec* pBox, Vec* p0, Vec* p1, f32 r);
u32 AtSphereCapsuleCk(Vec* c, f32 sph_r, Vec* p0, Vec* p1, f32 cap_r);
void AtCapsuleDisp(Vec* pPosTop, Vec* pPosBot, f32 r, u32 rgba);
void AtCubeDisp(Mtx m, Vec* pos, f32 sx, f32 sy, f32 sz, u32 color);
// Segment p0-p1 against one polygon; returns the attribute | SAT_ATTR_HIT or 0.
u32 At_poly_line_ck(cSat* atp, Vec* cross, u16* polygon, Vec* vert0, Vec* vert1, u32 flag, u32 mask);
// Sphere moving from `oldPos` to `pos` against one polygon; `pos` is pushed out. Returns the hit
// kind (1 crossed the plane, 2 touching) or 0.
u32 At_poly_sphere_ck(cSat* atp, u16* polygon, Vec* pos0, Vec* pos1, f32 r, u32 flag, u32 mask);
u32 At_poly_sphere_ck2(Vec* tri, Vec* n, u32 attr, Vec* oldPos, Vec* pos, f32 r, u32 flag, u32 mask);
u32 Get_poly_attr(AtPoly* poly);
// XZ rectangles of 4 corners.
int At_rect_point_ck(Vec* rect, Vec* pnt);
int At_rect_rect_ck(Vec* rect0, Vec* rect1);
// Quadrant of an angle: 0 front (|a| <= pi/4), 1 right, 2 back, 3 left.
int Get_ang_dir(f32 ay);
// out = a * t + b * (1 - t)
void InterVectorXYZ(Vec* cross, Vec* p0, Vec* p1, f32 rate);
int EatGetEffectType(u32 rgba);

#endif
