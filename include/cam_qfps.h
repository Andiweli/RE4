#ifndef CAM_QFPS_H
#define CAM_QFPS_H

#include "types.h"
#include "vec.h"
#include "camera.h"

struct _CUT_INFO;
typedef _CUT_INFO CUT_INFO;
struct _CAMERA_DATA;
typedef _CAMERA_DATA CAMERA_DATA;

// Shoulder camera offsets in player space (0x2C bytes), one per [left/right][up/mid/down] site.
// g_readyOfs[16]/g_transOfs[7] are the per-area tables (game/cam_qfps.cpp); db_cam edits a copy.
struct CAMERA_POINT {
    Vec Campos;    // 0x00
    Vec campos2;   // 0x0C  close point
    Vec Target;    // 0x18
    f32 Roll;       // 0x24  roll
    f32 Fovy;      // 0x28
};

// Transition camera type (PS2 TRANS_CAM): CameraQuasiFPS::m_trans_type, the row of trans_tbl (checkCameraType
// picks it from the player type).
enum TRANS_CAM {
    TRANS_CAM_LEON = 0,
    TRANS_CAM_LEON_ASHLEY = 1,
    TRANS_CAM_ASHLEY = 2,
    TRANS_CAM_ADA = 3,
    TRANS_CAM_KLAUSER = 4,
    TRANS_CAM_WESKER = 5,
    TRANS_CAM_NUM = 6
};

// Transition offset table (PS2 TRANS_DATA): the row of g_transOfs; AREA is the room override, BLEND the copy
// blended from.
enum TRANS_DATA {
    TRANS_DATA_LEON = 0,
    TRANS_DATA_LEON_ASHLEY = 1,
    TRANS_DATA_ADA = 2,
    TRANS_DATA_KLAUSER = 3,
    TRANS_DATA_WESKER = 4,
    TRANS_DATA_AREA = 5,
    TRANS_DATA_BLEND = 6,
    TRANS_DATA_NUM = 7
};

extern CAMERA_POINT g_readyOfs[16][2][3];
extern CAMERA_POINT g_transOfs[TRANS_DATA_NUM][2][3];

// Over-the-shoulder ("quasi FPS") camera, game/cam_qfps.cpp. 0x214 bytes.
class CameraQuasiFPS {
public:
    CAMERA cam;                   // 0x000 (cam.param at 0xA4 is what CameraControl::Move copies)
    CAMERA_POINT (*ready_tbl[14])[3];  // 0x0F8  ready table per camera type (checkCameraType 0..0xC), [13] = area copy
    CAMERA_POINT (*trans_tbl[TRANS_CAM_NUM])[3];   // 0x130  transition table per TRANS_CAM type
private:
    CAMERA_POINT (*m_p_ready_array)[3]; // 0x148  current ready table
    CAMERA_POINT (*m_p_trans_array)[3]; // 0x14C  current transition table
public:
    CAMERA_POINT* cur;                 // 0x150  offsets of the current site
    CAMERA_POINT* old;                 // 0x154  offsets blended from (g_readyOfs[15] / g_transOfs[6] copies)
private:
    void* m_LR_info;                // 0x158
    Vec m_pl_ofs;                  // 0x15C  one-shot translation applied to the base matrix
    Vec m_pl_dir;                  // 0x168  one-shot look direction applied to the base matrix
    Mtx m_pl_mat;                   // 0x174  player matrix saved by setPlayerLocation
    Vec* m_p_floor_norm;                  // 0x1A4  player floor normal (cModel::pFloorNrm)
    f32 m_zoom_ratio;                     // 0x1A8
    f32 m_walk_ratio;             // 0x1AC  CamSmth.ratio while the player moves
    u8 m_trans_type;                // 0x1B0  TRANS_CAM
    u8 m_ready_type;                // 0x1B1
    u8 m_init_flag;                     // 0x1B2  1 = first frame after init
public:
    u8 pad_1B3[0x1E4 - 0x1B3];
private:
    f32 m_blend_ratio;              // 0x1E4
    s32 m_blend_frame;              // 0x1E8
    s32 m_blend_count;              // 0x1EC
public:
    Vec m_Aim;             // 0x1F0
    u8 m_site;                      // 0x1FC  0 right/up-mid-down, 1 left, 2 right far, 3 left far (db_cam)
    u8 pad_1FD[3];
    f32 m_depression_ratio;                  // 0x200
    f32 m_direction_ratio;                  // 0x204
    f32 m_floor_ratio;              // 0x208
    s16 m_search_frame;             // 0x20C
    s16 m_search_cnt;             // 0x20E
    u32 m_state;                    // 0x210  bit0 use pl_mat, bit2 blending, bit3 blend frozen

    void LRinfo(void* pInfo);
private:
    int LRcheck();
    void calcDepressionRatio();
public:
    void setPlayerLocation(Mtx mat, Vec* p_norm);
private:
    void calcBaseMatrix(Mtx mat);
    int checkFBLR();
public:
    void setBlendRatio(f32 ratio);
    void setBlendCount(int counter);
    f32 getFloorRatio();
    void setFloorRatio(f32 ratio);
private:
    void checkCameraType();
public:
    void calcOffset(CAMERA_POINT* p_offset);
    void hitCheck(Mtx m, CAMERA_POINT* ofs, CameraParam* out);
    CAMERA_POINT (*readyArrayPtr())[3] { return m_p_ready_array; }
    CAMERA_POINT (*transArrayPtr())[3] { return m_p_trans_array; }
    void setBlendData(void* src, void* dst);
    void getAreaData(CAMERA_POINT (*ready)[3], CAMERA_POINT (*trans)[3]);
    void setAreaData(CAMERA_POINT (*ready)[3], CAMERA_POINT (*trans)[3]);
    void setAreaData(CAMERA_DATA* pCdat);
    void offsetCorrection();
    void bindDefaultCamera();
    void bindAreaCamera(CUT_INFO* pCut);
    void init();
    void move();
    void resetDepressionRatio();
    void resetDirectionRatio();
};

#endif
