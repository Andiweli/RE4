#ifndef CAMERA_H
#define CAMERA_H

#include "types.h"
#include "vec.h"

// Position/target/roll/fov set that the camera system interpolates and copies around
// (0x20 bytes here; the compiler copies it with a 0x18-stride loop + 8 bytes). PS2's own,
// unrelated CAMERA_POINT (game/cam_qfps.h's struct is really PS2's QFPS_OFFSET) uses the
// 16-byte SIMD Point3d for Campos/Target, so its copy of this struct is 0x30 bytes there.
struct CAMERA_POINT {
    Vec Campos; // 0x00
    Vec Target; // 0x0C
    f32 Roll;   // 0x18
    f32 Fovy;   // 0x1C
};

// Camera state block used by camera.cpp / cam_sys.cpp (0xF8 bytes). Only the fields the
// cam_ctrl unit touches are named; extend this, do not rewrite it.
struct CAMERA {
    Mtx mat;            // 0x00 camera orientation matrix (QuakeMain rotates the quake offset by it)
    Mtx v_mat;        // 0x30 look-at matrix (C_MTXLookAt)
    u8 pad_60[4];
    Mtx44 ProjMat;      // 0x64 projection matrix
    CAMERA_POINT param;  // 0xA4 (Campos 0xA4, Target 0xB0, Roll 0xBC, Fovy 0xC0)
    Vec Up;             // 0xC4 up vector (C_MTXLookAt)
    Vec Look;            // 0xD0 pos - at, normalised (matrix column 2)
    Vec Right;          // 0xDC up x dir (matrix column 0)
    u8 pad_E8[0xF4 - 0xE8];
    f32 Distance;           // 0xF4 |pos - at| (db_cam keeps it current for the debug camera)
};

// game/cam_sys.cpp
void CameraSetOrientationUp(CAMERA* pCam);
void CameraSetOrientationRoll(CAMERA* pCam);
void CameraSetOrientationZeroRoll(CAMERA* pCam);
f32 CameraGetRoll(CAMERA* pCam);
void CameraRotAxisPosRad(CAMERA* cam, Vec* axis, Vec* pos, f32 rad);
void CameraTargetRot(CAMERA* pCam, char axis, f32 rad);
void CameraCamposRot(CAMERA* pCam, char axis, f32 rad);
void CameraDolly(CAMERA* pCam, Vec* speed);
void CameraCamposDistance(CAMERA* pCam, f32 distance);
void CameraSetWithRoll(CAMERA* pCam, Vec* campos, Vec* target, f32 roll, f32 fovy);
// game/camera.cpp
void CameraSetProjection(int projType);
int CameraGetProjection();
void CameraGameInit();
void CameraRoomInit();
void CameraMove();
struct GEOM_HEXAHEDRON* CameraViewFrustumPtr(CAMERA* cam);
void CameraGetUpVec(CAMERA* pCam, Vec* up);
void CameraGetLookVec(CAMERA* pCam, Vec* look);
void CameraGetLookVecInverse(CAMERA* pCam, Vec* look_inv);
void CamPos2ScrnVec(f32 sX, f32 sY, Vec* vec);
// game/camera.cpp (C++ linkage): loads the current projection matrix into GX
void CameraCurrentProjection();
extern int ProjType;   // current projection type (db_cam.cpp toggles it)
// game/cam_sys.cpp, C++ linkage (`CameraTargetDistance__FP6CAMERAf` in Bio4.sym, marked local there;
// the t_camera REL imports it, so cam_sys.cpp defines it non-static)
void CameraTargetDistance(CAMERA* pCam, f32 distance);
struct JOY;
void CamStick2World(CAMERA* pCam, JOY* pJoy, Vec* pVec);

#endif
