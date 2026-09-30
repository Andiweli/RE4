#ifndef VIEW_H
#define VIEW_H

#include "types.h"
#include "vec.h"
#include "camera.h"
#include "geometry.h"

// Camera view volume (game/view.cpp `View`, 0x3A4 bytes).
class VIEW {
private:
    u32 _flag;
    CAMERA* _p_camera;             // 0x04
    f32 _aspect;               // 0x08
    f32 _fovy;                 // 0x0C
    f32 _zfar;                 // 0x10
    f32 _znear;                // 0x14
    f32 _old_fovy;              // 0x18
    f32 _old_zfar;              // 0x1C
    f32 _old_znear;            // 0x20
public:
    Mtx _mat;                  // 0x24  PS2 has it at 0x30, where Mtx is 4x4
    GEOM_HEXAHEDRON _l_effect_box;  // 0x054  half-width frustum, camera space
    GEOM_HEXAHEDRON _effect_box;    // 0x114  half-width frustum, world space
    GEOM_HEXAHEDRON _l_box;         // 0x1D4  full frustum, camera space
    GEOM_HEXAHEDRON _box;           // 0x294  full frustum, world space
    GEOM_SPHERE _l_sphere_inner;    // 0x354  camera space
    GEOM_SPHERE _sphere_inner;      // 0x368  world space
    GEOM_SPHERE _l_sphere_outer;        // 0x37C  camera space
    GEOM_SPHERE _sphere_outer;   // 0x390  world space

    void gameInit(CAMERA* p_camera);
    void roomInit();
    void init();
    void move();
    void setFarPlane(f32 zfar);
    void initPerspective(f32 fovy, f32 aspect, f32 znear, f32 zfar);
    void orientation();
};

extern VIEW View;
extern f32 ZNEAR;
extern f32 ZFAR;
// orthographic projection bounds (game/view.cpp .sbss, after ZFAR)
extern f32 ORTHO_T;
extern f32 ORTHO_B;
extern f32 ORTHO_L;
extern f32 ORTHO_R;

#endif
