#ifndef CLOTH_H
#define CLOTH_H

#include "types.h"
#include "vec.h"
#include "gx.h"

// Cloth simulation (game/cloth.cpp). ClothWk[8], stride 0x78. Layout from Cloth::Set / esp4e.
class Cloth {
public:
    u8 be_flag;           // 0x00 bit0: in use
    u8 attr;             // 0x01
    u8 divH;             // 0x02 grid columns
    u8 divV;             // 0x03 grid rows
    f32 Wgap;            // 0x04 cell width
    f32 Hgap;            // 0x08 cell height
    f32 Scale;            // 0x0C
    Vec* pVer;          // 0x10 grid positions (nx*ny)
    Vec* pNor;          // 0x14 grid normals (calcNormal; {0,0,1} initially)
    Vec* pSpd;          // 0x18
    Mtx mat;           // 0x1C
    Vec center;           // 0x4C
    f32 radius;           // 0x58
    GXTexObj* tex;     // 0x5C
    GXTlutObj* pTlobjC;   // 0x60
    void* pTobjA;         // 0x64
    union {
        GXColor color; // 0x68 material colour (clothTrans passes it by value)
        struct {
            u8 colR;   // 0x68
            u8 colG;   // 0x69
            u8 colB;   // 0x6A
            u8 colA;   // 0x6B
        };
    };
    void* m_pMem;         // 0x6C
    int isWave;        // 0x70  Set's last argument: 1 pins the top four rows, 0 only the first (PS2 BOOL isWave)
    int blendMode;     // 0x74  1: additive (cloth draw GXSetBlendMode; esp4e sets it from Blend_type) (PS2 blendMode)

    void Set(Vec ang, Vec pos, u8 nx, u8 ny, f32 w, GXTexObj* tex, f32 h, void* p, f32 d, GXTlutObj* tlut,
             int flag);
    void SetPosAng(Vec ang, Vec pos);
    void Destroy();
    void calcSpeed(f32 mul);
    void move();
    void calcNormal();
    void disturbance(u32 w, u32 h, f32 pow);
};

void ClothInit();
void ClothRoomInit();
void ClothCalcTplAddr(void* tpl);
int ClothTexSetUp(void* tpl, GXTexObj* tex, int no, GXTlutObj* tlut);
int PullCloth(Cloth** ppCl);
void ClothDraw();
void clothTrans(Cloth* pCL);

#endif
