#ifndef SHADOW_H
#define SHADOW_H

#include "types.h"
#include "vec.h"
#include "gx.h"

class cLight;
class cModel;
class cObj;

// One projected shadow (game/shadow.cpp `ShadowMngWork`, 0xD4 bytes): the light that casts it,
// the models it covers and the texture the models are rendered into from the light.
struct ShadowMng {
    cLight* pLight;      // 0x00
    cModel* pModel[8];   // 0x04
    u8 self;             // 0x24  1: self shadow (the model's own shadow map, SelfShadowSetup)
    u8 num;              // 0x25  models in pModel
    u16 no;              // 0x26  index in ShadowMngWork
    Mtx lookAt;          // 0x28  light view matrix
    Mtx texMat;          // 0x58  texture projection matrix (light perspective * lookAt)
    Vec lightPos;        // 0x88
    Vec target;          // 0x94
    Vec dir;             // 0xA0  normalised target - lightPos
    f32 fov;             // 0xAC  light perspective angle (degrees)
    GXTexObj texObj;     // 0xB0
    void* pTex;          // 0xD0  g_Shd_tex_size^2 I8 shadow texture
};

// Shadow object placement file (room "SHD" data): header then `num` entries.
struct ShdEntry {
    Vec pos;      // 0x00
    Vec rot;      // 0x0C
    Vec scale;    // 0x18
    u8 model;     // 0x24  index into the model offset table
    u8 x25;
    u8 shdCol;    // 0x26  -> cModel::shdCol
    u8 pad_27[0x48 - 0x27];
};

struct ShdHeader {
    u8 version;   // 0x00  (> 0x41 rejected; <= 0x1F: shdCol 0 means 0xFF)
    u8 x1;
    u16 num;      // 0x02
    u32 tblOfs;   // 0x04  byte offset of the model offset table (u32[], relative to itself)
    u8 pad_8[0x10 - 0x08];
    ShdEntry entry[1];  // 0x10
};

void SetShadowCamMoveSize(f32 size);
void ResetShadowCamMoveSize();
void SetShadowParallelDirX(f32 x);
void ReetShadowParallelDirX();
ShadowMng* GetSelfShadowMng(int no);
int ShdInit(ShdHeader* data);
cObj* ShdGetObjPtr(int no);
void ShadowInit();
void ShadowRoomInit();
void ShadowMngReAlloc(int n);
void ShadowMemClear();
ShadowMng* getShadowMng();
void ShadowTrans();
int Fit_ParallelShadowModelSet(cModel* m, int self);
void Fit_ParallelShadowModelAddOt(cLight* l, cModel* m, int self);
void FixShadowLightSet(cLight* l);
void shadowModelRender(ShadowMng* mng);
void make_comn_fit_light(ShadowMng* mng, cModel* m);
void make_comn_parallel_light(ShadowMng* mng, cModel* m);
void make_fix_light(ShadowMng* mng);
void SoftShadowGetEFB(ShadowMng* mng, f32 sx, f32 sy, int clear);
void SoftShadowGXDraw(ShadowMng* mng, f32 x, f32 y, f32 z, u32 div, f32 u, f32 v, f32 alpha, f32 scale);
void MakeSoftShadow(ShadowMng* mng);
void make_shadow_texture(ShadowMng* mng);
int shadowChkInFrustum(ShadowMng* mng, cModel* m);
void ProcShadowScrModel(cModel* m, ShadowMng* mngs);
void shadowScrModelRender(ShadowMng* mngs);
void shadowShaderSetup2(cModel* m, struct ModelPart* part, ShadowMng** tbl, u32 num);
void shadowModelTrans(cModel* m, class cModelInfo* info, Mtx viewMat, ShadowMng** tbl, u32 num);
void shadowModelTrans2(cModel* m, class cModelInfo* info, Mtx viewMat);
void TransLightTexture(GXTexObj* tex, GXTlutObj* tlut, s16 x, s16 y, s16 z, s16 w, s16 h, ShadowMng* mng, int flag2, int flag1);
ShadowMng* GetCastShadowMngPtr(cModel* m);

// game/shadow.cpp: self shadow switches and the shadow texture matrix constants (trans.cpp SelfShadowSetup).
extern int isSelfUse;
extern int g_SelfShdNum;
extern f32 shd_ofs;
extern f32 shd_tex_scale_x;

#endif
