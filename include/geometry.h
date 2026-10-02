#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "types.h"
#include "vec.h"

// game/geometry.cpp: collision primitives.

// Cone (or fan/cylinder) volume: apex at pos, axis dir, half angle, height; radius is derived.
struct GEOM_CONE_REV {
    Vec pos;     // 0x00
    Vec direction;     // 0x0C
    f32 angle;   // 0x18
    f32 height;  // 0x1C
    f32 radius;  // 0x20  height * sin(angle), written by collision_point_cone_rev_play
};

// Sphere volume (0x14 bytes on GC, where Vec is 12 bytes).
struct GEOM_SPHERE {
    Vec pos;      // 0x00
    f32 radius;   // 0x0C
    f32 color;    // 0x10  PS2 has u32 color; viewSphereReset stores a float 0.0 here on GC
};

// Convex box for frustum culling: six outward plane normals, the eight corner points and the
// sphere circumscribing them.
struct GEOM_HEXAHEDRON {
    Vec normal[6];                  // 0x00
    Vec vertex[8];                  // 0x48
    GEOM_SPHERE circumscribe_sphere;  // 0xA8
    u32 color;                      // 0xBC  debug draw colour
};

int collision_point_cone_rev_play(Vec* pPoint, GEOM_CONE_REV* pConeRev, f32 play);
int collision_point_cone_rev_play_face(Vec* pPoint, GEOM_CONE_REV* pConeRev, f32 play, Vec* pDirection, f32 open_angle);
int collision_sphere_hexahedron(GEOM_SPHERE* pSphere, GEOM_HEXAHEDRON* pHexahedron);

#endif
