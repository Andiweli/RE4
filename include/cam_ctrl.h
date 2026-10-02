#ifndef CAM_CTRL_H
#define CAM_CTRL_H

#include "types.h"
#include "vec.h"
#include "camera.h"
#include "cam_qfps.h"

class cCamera;
class cModel;

// ---------------------------------------------------------------------------
// Room camera data ("B40x" file). Layout after the 0x10 header:
//   CUT_INFO[numArea]   0x10 each
//   AREA_DATA[numArea]  0x30 each
//   CAMERA_DATA[numCut]        0x34 each
//   LERP_DATA[numLerp]      0x10 each
// File offsets are relocated to pointers by CameraControl::calcAddr.
// ---------------------------------------------------------------------------

struct _AREA_DATA {  // hit area
    u8 Be_flag;    // 0x00
    s8 No;         // 0x01
    s8 Suffix;     // 0x02
    u8 Attr;       // 0x03  bit 4 = ?, bit 8 = ?, 0x20 set from 8 by calcAddr, 0x40 = check dir, 0x80 = no light update
    f32 Dir;       // 0x04  facing angle the player must have (Attr & 0x40)
    u8 Type_char;  // 0x08  matched against battle/state attribute
    u8 Type_addr;  // 0x09  t_camera TC_AREA_DATA::attr3; 0xFF = none
    u8 dummy[0x20 - 0x0A];
    f32 Height;    // 0x20
    f32 Y;         // 0x24
    s32 nVer;      // 0x28  polygon vertex count
    Vec* pVer;     // 0x2C
};
typedef _AREA_DATA AREA_DATA;

struct _CUT_INFO {  // area -> cut link
    u8 Attr;              // 0x00  camera type of the linked cut (t_camera tcTypeTbl)
    u8 dummy[7];
    AREA_DATA* pAdat;     // 0x08
    CAMERA_DATA* pCdat;   // 0x0C
};
typedef _CUT_INFO CUT_INFO;

struct _CAMERA_DATA {
    u8 Be_flag;      // 0x00
    s8 No;           // 0x01
    s8 Id;           // 0x02  CameraControl state selector
    u8 Attr;         // 0x03  bit 0: offset valid
    Vec offset;      // 0x04  added to the player position to get the aim point
    u16* pFrame;     // 0x10  key frame times
    f32 floor_ratio; // 0x14  shoulder camera floor ratio (cam_qfps setAreaData)
    u8 dummy[0x20 - 0x18];
    s32 nPoint;      // 0x20  key count
    Vec* pCampos;    // 0x24
    Vec* pTarget;    // 0x28
    f32* pRoll;      // 0x2C
    f32* pFovy;      // 0x30
};
typedef _CAMERA_DATA CAMERA_DATA;

struct _LERP_DATA {
    u8 Be_flag;    // 0x00
    s8 SrcNo;      // 0x01
    s8 SrcSuffix;  // 0x02
    s8 DstNo;      // 0x03
    s8 DstSuffix;  // 0x04
    u8 Attr;       // 0x05
    u8 dummy[2];
    s32 InterFrame; // 0x08
    u8 pad_C[4];
};
typedef _LERP_DATA LERP_DATA;

struct _CAM_FILE_HEADER {
    char Version[4]; // 0x00  "B400".."B404"
    u8 nCdat;        // 0x04
    u8 nAdat;        // 0x05
    u8 nLdat;        // 0x06
    u8 dummy[0x10 - 0x07];
};
typedef _CAM_FILE_HEADER CAM_FILE_HEADER;

// Per-attach-camera record registered by other units (only the frame count is used here).
struct ATTACH_CAMERA {
    u8 parts[5];    // 0x00  motion parts index feeding each channel (0xFF = none): 0/1 pos, 2/3 rot, 4 misc
    u8 type;        // 0x05  0 = off, 1 = follows the model matrix, 2 = own matrix copy (MotionSetCore)
    u8 frame;       // 0x06  (u8)(out[4].y / 100)
    u8 pad_7;
    Mtx* p_mat;      // 0x08  &model->mat or &mat
    Mtx mat;        // 0x0C
    Vec camera_data[5];     // 0x3C  interpolated channels (MotionMoveCore)
    u16 history[5][3]; // 0x78  key history per channel / axis
};

// B-spline rail work used by the Track/RailPan/RailBehind cameras (static CamBSpline, 0x3B8).
// Parametrize() fits the cut's key positions with de_Boor_Cox basis functions (up to 26 keys),
// searchRail() picks the segment/parameter nearest the aim point, BSpline() evaluates the curve.
struct _CAM_B_SPLINE {
    s32 order;        // 0x000  spline degree (min(2, num - 1))
    f32 cand_t;       // 0x004  curve parameter
    s32 history_i;    // 0x008  key index the parameter was searched from
    s32 p_num;        // 0x00C  key count
    f32 c_alpha[26];  // 0x010  control point x
    f32 c_beta[26];   // 0x078  control point y
    f32 c_gamma[26];  // 0x0E0  control point z
    f32 t_alpha[26];  // 0x148  angle x
    f32 t_beta[26];   // 0x1B0  angle y
    f32 t_gamma[26];  // 0x218  angle z
    f32 r_alpha[26];  // 0x280  roll
    f32 f_alpha[26];  // 0x2E8  fovy
    f32 B[26];        // 0x350  de_Boor_Cox basis output

    _CAM_B_SPLINE() {}  // empty: makes CamBSpline emit at its definition (cam_ctrl .bss order)
};
typedef _CAM_B_SPLINE CAM_B_SPLINE;

// ---------------------------------------------------------------------------

class CameraInterpolation {
public:
    CAMERA_POINT param; // 0x00
    s32 frame;         // 0x20

    void set(int frame, CAMERA_POINT* p);
    void move(CAMERA_POINT* arg);
    CAMERA_POINT* getCamPtr() { return &param; }
};

class CameraSmooth : public CAMERA {
private:
    u32 m_flag;         // 0xF8  bit 0 = reinit on next move
    f32 m_ratio;         // 0xFC
    CAMERA_POINT m_effect; // 0x100
public:
    u8 pad_120[0x12C - 0x120];

    void init(CAMERA_POINT* p);
    void move(CAMERA_POINT* arg);
    void setRatio(f32 ratio) { m_ratio = ratio; }
    CAMERA_POINT* getCamPtr() { return &m_effect; }
    void setFlag() { m_flag |= 1; }
    void unsetFlag() { m_flag &= ~1; }
};

class CameraControl {
private:
    u8 m_attached_cam_flag_old;                        // 0x00
    u8 m_attach_cam_flag;                        // 0x01
    u8 m_attach_num;                // 0x02
public:
    u8 x3;                        // 0x03
private:
    ATTACH_CAMERA* m_p_attach[3];  // 0x04
    cModel* m_p_model[3];      // 0x10
    cModel* m_p_attach_model_old;           // 0x1C
    f32 m_scope_zoom;             // 0x20
    f32 m_scope_ang_x;             // 0x24
public:
    u8 be_flag;                  // 0x28  bit 0 = data valid, bit 2 = disabled
    u8 pad_29[3];
    u32 m_system_flag;                 // 0x2C
    u32 m_state_flag;                 // 0x30
    u8 r0;                     // 0x34
    u8 r1;                 // 0x35
    u8 r2;                       // 0x36
    u8 r0_old;                // 0x37
    CAMERA_POINT cur;              // 0x38
    u32 counter_58;               // 0x58
    CAM_FILE_HEADER* pCamData;       // 0x5C
    CAMERA camera;                // 0x60
    Mtx prev_mat;                 // 0x158  camera matrix CamStick2World keeps while the cut changes
    u8 pad_188[0x250 - 0x188];
    CAMERA* m_pExtraCamera;       // 0x250  boss/event camera (em2a/em2b/em2c/em2d); nonzero blocks the fall-check in Check()
    CameraInterpolation m_Inter;   // 0x254
    CameraQuasiFPS m_QuasiFPS;          // 0x278
    u8 m_Free[0x200];          // 0x48C  placement storage for cCamera subclasses
    cCamera* m_pProc;               // 0x68C
    s8 areaNo;                   // 0x690
    s8 areaSuffix;                      // 0x691
    s8 cameraNo;                 // 0x692
    u8 m_cut_attr;                 // 0x693
    CUT_INFO* area_rec;      // 0x694
    s32 Battle_delay;             // 0x698
    Vec Aim;                      // 0x69C
    Vec upcut_pos;                   // 0x6A8
    Vec upcut_ang;                    // 0x6B4
    Vec upcut_scale;                   // 0x6C0
    f32 m_behind_fovy;                     // 0x6CC
    f32 m_side_play;                     // 0x6D0
    f32 m_back_play;                     // 0x6D4
    f32 m_ang_h_limit;                     // 0x6D8
    f32 m_ang_v_limit;                     // 0x6DC
    s32 m_quick_cnt;                     // 0x6E0
    f32 m_key_speed;                     // 0x6E4
    f32 m_behind_A_ratio;                     // 0x6E8
    Vec campos_ofs;                  // 0x6EC
    Vec target_ofs;                   // 0x6F8

    int HermiteExport(CAMERA_DATA* pCdat, u8* buf);
    int IsChangeCamera();
    void Comeback(int);
    void Disable();
    void SetExtraCamera(CAMERA* p_cam) { m_pExtraCamera = p_cam; }
    void AreaCheckOnOff(int sw);
    u8 AreaNum();
    int CurrentAreaNo();
    int CurrentCameraNo();
    CAMERA_DATA* DataSearch(int cameraNo);
private:
    LERP_DATA* LerpDataSearch(int srcNo, int srcSuf, int dstNo, int dstSuf);
    CAM_FILE_HEADER* calcAddr(u8* head);
public:
    void RoomDataRead(u8* pBuff);
    void CoreDataRead(u8* data);
    void AreaOnOff(int No, int Suffix, int OnOff);
    void SetAreaAttr(int No, int Suffix, u8 attr);
    void UnsetAreaAttr(int No, int Suffix, u8 attr);
    void CutCall(int cutNo);
private:
    void switchCamera(CUT_INFO* rec);
    void areaHitCheck();
public:
    void roomInit();
    void Check();
    void Move();
    void CalcAim(CAMERA_DATA* pCdat);
    f32 getCameraPitch();
private:
    void r0_Wait();
    void r0_Debug();
    void r0_Fix();
    void r0_Pan();
    void r0_Track();
    void r0_RailPan();
    void r0_UpCut();
    void r0_RailBehind();
    void r0_Free();
public:
    void resetCameraAngle();
    f32 getCameraDirection();
private:
    void debugDrawRail(CAMERA_DATA* pCdat);
public:
    void UpCutCall(int cutNo, Vec* pos, Vec* ang, Vec* scale, int data_sel);
    void startPushObject();
    void endPushObject();
    void StartLookDownEm(void* pEm);
    void EndLookDownEm();
    void startScope(Vec* campos, Vec* target);
    void endScope();
    void getTrajectory(Vec* p_pos0, Vec* p_pos1);
    void saveScopeParam();
    void loadScopeParam();
    void SetBinocularRange(f32 x_low, f32 x_up, f32 y_low, f32 y_up);
    void HoldBinocular(void* id_a, void* id_b, Vec* pos, Vec* at);
    void LowerBinocular();
    void GetBinocularIDAddr(void** eff_addr, void** uwf_addr);
    void MotionSet(void* motion, f32 speed, int frame);
    int IsMotionSet();
    int IsMotionEnd();
    void setMotionBaseMatPtr(Mtx* p_mat);
    struct MOTION_INFO* getMotionInfoPtr();
    void clearAttachCamera();
    void registAttachCamera(ATTACH_CAMERA* p_attach, cModel* p_model);
    void deleteAttachCamera(ATTACH_CAMERA* p_attach, cModel* p_model);
    int getAttachCameraNum() { return m_attach_num; }
    cModel* getAttachModel(cModel* p_model);
    ATTACH_CAMERA* getAttachCamera(cModel* p_model);
    void checkAttachCamera();

    // Empty ctor/dtor: cam_ctrl's `__static_initialization_and_destruction_0` and the
    // `global constructors/destructors keyed to g_pToolCamData` pair.
    CameraControl() {}
    ~CameraControl() {}
};

extern CameraControl CamCtrl;
extern CameraSmooth CamSmth;
extern void* g_pToolCamData;

int cameraDataVersion(char* verStr);
int cameraHitCheck(Vec* pos, Vec* nrm, Vec* from, Vec* to);
void CameraSetCutData(CAMERA* pCam, CAMERA_DATA* pData);
int areaAttr(AREA_DATA* p_area, u8 cut_attr, u8 char_type);
int areaHit(Vec* pPos, AREA_DATA* pArea, f32 dir_y);
int area_hit_p3(Vec* pPos, AREA_DATA* pArea);
int area_hit_pN(Vec* pPos, AREA_DATA* pArea);
void CamCtrlShoulderSetSearchFrame(s16 frame);
void CamCtrlShoulderSetAim(Vec* pos);
void Parametrize(CAMERA_DATA* pCdat, CAM_B_SPLINE* pB);
void BSpline(CAM_B_SPLINE* bs, CAMERA* cam, int mode);
void searchRail(CAM_B_SPLINE* bs, CAMERA_DATA* cut, Vec* aim, int mode);


#endif
