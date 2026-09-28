// game/cloth.cpp: grid cloth simulation (esp4e sheets): a nx*ny grid of points joined by springs,
// pulled down by gravity, drawn as triangle strips with the lights of the room.

#include "light.h"
#include "global.h"
#include "model.h"
#include "camera.h"
#include "gx.h"
#include "tpl.h"
#include "cloth.h"
#include "math_sub.h"
#include "main_mem.h"
#include "trans_ot.h"
#include <dolphin/os.h>

// game/trans_lit.cpp defines it with Vec* pos; this unit was built with the by-value prototype (three words
// in r5..r7), so trans_lit.h is not included.
void commonClothLightSet(cLight** list, int n, Vec pos, f32 radius);

Cloth ClothWk[8];

// clothTrans builds a temporary cModel for the light setup. The real cModel is 0x320 bytes
// (see the KNOWN DEBT note in docs/matching.md: motion/atari fields still live in cEm/cObj), so an
// unused pad local follows it to keep the original frame layout until cModel carries them itself.

f32 K_PARAM = 7.5f;   // spring constant
f32 G_PARAM = 0.5f;   // gravity
f32 T_PARAM = 0.06f;  // time step
static f32 D_PARAM = 0.006f;  // unused (the 0.006 word after T_PARAM in .sdata; name unknown)

// Boot: marks the 8 cloth works free.
void ClothInit()
{
    int i;

    for (i = 0; i < 8; i++) {
        ClothWk[i].be_flag = 0;
    }
}

// Room start: frees every live cloth's grid memory and marks the works free.
void ClothRoomInit()
{
    ClothInit();
}

// Dead-stripped in the DOL (only its strings and its 0.0f pool survive, ahead of Cloth::Set's
// .rodata): a helper that allocates a Vec and normalises into it. Body unknown; this one
// reproduces the string order (__FILE__ before "VECNormalize:[%s/%d]") and the pool.
static Vec* clothAllocNormal(Vec* dir)
{
#line 120 "D:/Bio4/Prog/cloth.cpp"
    Vec* v = (Vec*) MEM_ALLOC(sizeof(Vec), 1, 13);
    VECNormalize(dir, v);
    return v;
}

// Builds an nx x ny cloth: allocates the vertex / normal / speed grids, lays the points out w
// apart horizontally and h vertically hanging from the top row at pos_ / ang, binds the texture
// (and TLUT) and the owner pointer p_; be_flag 0x31 = live, drawn. The top row (or the top four
// rows in the alternate pin mode) stays fixed.
void Cloth::Set(Vec ang, Vec pos_, u8 nx_, u8 ny_, f32 w, f32 h, f32 d, GXTexObj* tex_, GXTexObj* p_, GXTlutObj* tlut_,
                int flag_)
{
    Vec zero = {0.0f, 0.0f, 0.0f};
    Vec n = {0.0f, 0.0f, 1.0f};
    Vec* pp;
    Vec* pn;
    Vec* ps;
    u32 rowSize;
    int i;
    int j;

    Wgap = w;
    Hgap = h;
    Scale = d;
    divH = nx_;
    divV = ny_;
    isWave = flag_;
    RotMatrix(mat, &ang);
    TransMatrix(mat, &pos_);
    rowSize = ny_ * sizeof(Vec);
#line 212 "D:/Bio4/Prog/cloth.cpp"
    m_pMem = MEM_ALLOC(nx_ * rowSize * 3, 1, 13);
    if (m_pMem == 0) {
        OSReport("ClothSet(): Can't allocate memory.\n");
        be_flag = 0;
    }
    pp = (Vec*) m_pMem;
    pVer = pp;
    pn = pp + nx_ * ny_;
    pNor = pn;
    ps = pp + nx_ * 2 * ny_;
    pSpd = ps;
    for (j = 0; j < ny_; j++) {
        for (i = 0; i < nx_; i++) {
            pp->x = i * Wgap;
            pp->y = -j * Hgap;
            if (flag_ != 0 && (j >= 0 && j <= 3)) {
                int m = i % 6;
                pp->z = sinf(m * 6.2831855f / 6.0f) * -2.0f;
            } else {
                pp->z = 0.0f;
            }
            *ps = zero;
            *pn = n;
            pp++;
            ps++;
            pn++;
        }
    }
    for (i = 0; i < 120; i++) {
        calcSpeed(0.9f);
        move();
    }
    center.x = Wgap * divH * 0.5f * Scale;
    center.y = 0.0f;
    center.z = 0.0f;
    PSMTXMultVec(mat, &center, &center);
    radius = Wgap * divH * 0.5f * Scale;
    tex = tex_;
    pTlobjC = tlut_;
    pTobjA = p_;
    be_flag = 0x31;
    colR = 0xFF;
    colG = 0xFF;
    colB = 0xFF;
    colA = 0xFF;
    blendMode = 0;
}

// Moves the cloth's hanging matrix.
void Cloth::SetPosAng(Vec ang, Vec pos_)
{
    RotMatrix(mat, &ang);
    TransMatrix(mat, &pos_);
}

// Frees the grid memory and the work.
void Cloth::Destroy()
{
    Mem_free(m_pMem);
    be_flag = 0;
}

// Relocates an in-file TPL in place (descriptor / image / CLUT offsets -> pointers), once.
void ClothCalcTplAddr(void* tpl)
{
    TEXPalette* pal = (TEXPalette*) tpl;
    TEXDescriptor* d;
    u32 i;

    if (pal == 0) {
        return;
    }
    if ((s32) pal->descriptorArray < 0) {
        return;
    }
    pal->descriptorArray = (TEXDescriptor*) ((u32) pal->descriptorArray + (u32) pal);
    for (i = 0; i < pal->numDescriptors; i++) {
        d = &pal->descriptorArray[i];
        d->textureHeader = (TEXHeader*) ((u32) pal + (u32) d->textureHeader);
        d->textureHeader->data = (void*) ((u32) pal + (u32) d->textureHeader->data);
        if (d->CLUTHeader != 0) {
            d->CLUTHeader = (CLUTHeader*) ((u32) pal + (u32) d->CLUTHeader);
            d->CLUTHeader->data = (void*) ((u32) pal + (u32) d->CLUTHeader->data);
        }
    }
}

// Initialises a GXTexObj (and TLUT for CI formats); 1 on success.
int ClothTexSetUp(void* tpl, GXTexObj* tex, GXTexObj* unusedTex, GXTlutObj* tlut)
{
    TEXDescriptor* d;
    TEXHeader* t;
    CLUTHeader* c;
    int ret = 0;

    ClothCalcTplAddr(tpl);
    d = TEXGet((TEXPalette*) tpl, 0);
    t = d->textureHeader;
    if (t->format >= 8 && t->format <= 9) {
        GXInitTexObjCI(tex, t->data, t->width, t->height, t->format, 0, 0, 0, 0);
        ret = 1;
        c = d->CLUTHeader;
        GXInitTlutObj(tlut, c->data, c->format, c->numEntries);
        GXLoadTlut(tlut, 0);
    } else {
        GXInitTexObj(tex, t->data, t->width, t->height, t->format, 0, 0, 0);
    }
    return ret;
}

// One simulation step of the speeds: spring forces (K_PARAM) toward each neighbour's rest
// distance (Wgap / Hgap), gravity G_PARAM, then the speed scaled by `damping`.
void Cloth::calcSpeed(f32 mul)
{
    Vec v;
    Vec* p = pVer;
    Vec* s = pSpd;
    int i;
    int j;
    int k;
    int w;
    f32 len;

    for (i = 0; i < divV; i++) {
        for (j = 0; j < divH; j++) {
            if (i == 0 && j != 0 && j != divH - 1) {
                continue;
            }
            w = divH;
            k = j + i * w;
            if (j != 0) {
                PSVECSubtract(&p[k - 1], &p[k], &v);
                len = PSVECMag(&v);
                if (len > Wgap) {
                    PSVECScale(&v, &v, K_PARAM * (len - Wgap) / len);
                    PSVECAdd(&v, &s[k], &s[k]);
                }
            }
            if (j != divH - 1) {
                PSVECSubtract(&p[k + 1], &p[k], &v);
                len = PSVECMag(&v);
                if (len > Wgap) {
                    PSVECScale(&v, &v, K_PARAM * (len - Wgap) / len);
                    PSVECAdd(&v, &s[k], &s[k]);
                }
            }
            if (i != 0) {
                PSVECSubtract(&p[k - w], &p[k], &v);
                len = PSVECMag(&v);
                if (len > Hgap) {
                    PSVECScale(&v, &v, K_PARAM * (len - Hgap) / len);
                    PSVECAdd(&v, &s[k], &s[k]);
                }
            }
            if (i != divV - 1) {
                PSVECSubtract(&p[k + w], &p[k], &v);
                len = PSVECMag(&v);
                if (len > Hgap) {
                    PSVECScale(&v, &v, K_PARAM * (len - Hgap) / len);
                    PSVECAdd(&v, &s[k], &s[k]);
                }
            }
            s[k].y -= G_PARAM;
            PSVECScale(&s[k], &s[k], mul);
        }
    }
}

// A free cloth work in *out; 0 when all 8 are used.
int PullCloth(Cloth** ppCl)
{
    Cloth* c = ClothWk;
    int i;

    *ppCl = 0;
    for (i = 0; i < 8; i++) {
        if ((c->be_flag & 1) == 0) {
            *ppCl = c;
            return 1;
        }
        c++;
    }
    return 0;
}

// Integrates the speeds into the vertex positions (time step T_PARAM), skipping the pinned top
// row(s).
void Cloth::move()
{
    Vec v;
    int i;
    int j;

    for (i = 0; i < divV; i++) {
        if (isWave == 0) {
            if (i == 0) {
                continue;
            }
        } else if (i >= 0 && i <= 3) {
            continue;
        }
        for (j = 0; j < divH; j++) {
            int k = j + divH * i;
            PSVECScale(&pSpd[k], &v, T_PARAM);
            PSVECAdd(&pVer[k], &v, &pVer[k]);
        }
    }
}

// Recomputes the vertex normals from the neighbouring points for lighting.
void Cloth::calcNormal()
{
    Vec a;
    Vec b;
    Vec c;
    int i;
    int j;
    int k;

    for (i = 0; i < divV; i++) {
        for (j = 0; j < divH; j++) {
            u8 w = divH;
            k = j + i * w;
            if (i == divV - 1) {
                pNor[k] = pNor[k - w];
            } else if (j == w - 1) {
                pNor[k] = pNor[k - 1];
            } else {
                PSVECSubtract(&pVer[k + 1], &pVer[k], &a);
                PSVECSubtract(&pVer[k + w], &pVer[k], &b);
                PSVECCrossProduct(&b, &a, &c);
#line 622 "D:/Bio4/Prog/cloth.cpp"
                VECNormalize(&c, &pNor[k]);
            }
        }
    }
}

// Pushes grid point (x, y): adds `power` to its z speed and half to y (bullets / wind).
void Cloth::disturbance(u32 w, u32 h, f32 pow)
{
    u32 idx = w + divH * h;

    pSpd[idx].z += pow;
    pSpd[idx].y += pow * 0.5f;
}

// Draw registration: queues clothTrans in OT 13 for every cloth flagged drawn (be_flag 0x20).
void ClothDraw()
{
    Cloth* c = ClothWk;
    int i;

    for (i = 0; i < 8; i++, c++) {
        if (c->be_flag & 0x20) {
            AddOtDirect(0xD, c, (void (*)()) clothTrans, 0, 0x1000, 0, 0.0f);
        }
    }
}

// Draws the cloth: a temporary cModel picks up the room lights (commonClothLightSet), then one
// two-sided textured triangle strip per row with the vertex normals.
void clothTrans(Cloth* pCL)
{
    Mtx texMtx;
    GXTexObj* tex = pCL->tex;
    GXTexObj* tex2 = pCL->pTobjA;
    GXTlutObj* tlut = pCL->pTlobjC;
    int nStages;
    int i;
    int j;
    int k;
    int t;
    f32 r;

    CameraSetProjection(1);
    PSMTXIdentity(texMtx);
    GXSetCullMode(0);
    GXSetZMode(1, 3, 1);
    GXSetNumChans(1);
    GXSetTevColorIn(0, 0xF, 8, 0xA, 0xF);
    GXSetTevColorOp(0, 0, 0, 2, 1, 0);
    GXSetTevAlphaIn(0, 7, 4, 5, 7);
    GXSetTevAlphaOp(0, 0, 0, 2, 1, 0);
    GXSetTevOrder(0, 0, 0, 4);
    cModel model;
    u8 modelPad[0x320 - sizeof(cModel)];  // see the cModel size note above
    PSMTXIdentity(model.mat);
    {
        static const Vec p0 = {0.0f, 0.0f, 0.0f};
        static const Vec p1 = {10000.0f, 10000.0f, 10000.0f};
        model.LightInfo.init2(1, 0, &p0, &p1, 0x10);
    }
    model.pos = pCL->center;
    LightMgr.setCloth(&model, 8);
    if (model.LightInfo.Size.x > model.LightInfo.Size.y) {
        r = model.LightInfo.Size.x;
    } else {
        r = model.LightInfo.Size.y;
    }
    commonClothLightSet(model.LightInfo.pLight, 8, model.pos, r);
    GXSetChanMatColor(4, pCL->color);
    GXSetNumTexGens(1);
    GXLoadTexObj(tex, 0);
    if (tlut != 0) {
        GXLoadTlut(tlut, 0);
    }
    GXLoadTexMtxImm(texMtx, 0x1E, 1);
    GXSetTexCoordGen2(0, 1, 4, 0x1E, 0, 0x7D);
    if (pCL->blendMode == 0) {
        GXSetBlendMode(1, 4, 5, 0);
    } else {
        GXSetBlendMode(1, 1, 1, 0);
        GXSetZMode(1, 3, 0);
    }
    GXSetAlphaCompare(4, 4, 1, 4, 0xFF);
    nStages = 1;
    if (tex2 != 0) {
        nStages = 2;
        GXSetZCompLoc(0);
        GXSetBlendMode(1, 4, 5, 0);
        GXLoadTexObj(tex2, 1);
        GXSetTevOrder(1, 0, 1, 4);
        GXSetTevOp(1, 0);
    }
    GXSetNumTevStages(nStages);
    {
        Mtx scale;
        Mtx tmp;
        PSMTXScale(scale, pCL->Scale, pCL->Scale, pCL->Scale);
        PSMTXConcat(pCL->mat, scale, tmp);
        PSMTXConcat(pG->Camera.v_mat, tmp, scale);
        GXLoadPosMtxImm(scale, 0);
        PSMTXInverse(scale, tmp);
        PSMTXTranspose(tmp, scale);
        GXLoadNrmMtxImm(scale, 0);
    }
    GXSetCurrentMtx(0);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(10, 1);
    GXSetVtxDesc(13, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 10, 0, 4, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    for (i = 0; i < pCL->divV - 1; i++) {
        GXBegin(0x98, 0, pCL->divH * 2);
        for (j = 0; j < pCL->divH * 2; j++) {
            Vec* pp;
            Vec* pn;
            if (j & 1) {
                k = pCL->divH * (i + 1) + j / 2;
                t = 1;
            } else {
                k = i * pCL->divH + j / 2;
                t = 0;
            }
            pp = &pCL->pVer[k];
            pn = &pCL->pNor[k];
            GXPosition3f32(pp->x, pp->y, pp->z);
            GXNormal3f32(pn->x, pn->y, pn->z);
            GXTexCoord2f32((f32) (j / 2) / (f32) (pCL->divH - 1), (f32) (i + t) / (f32) (pCL->divV - 1));
        }
    }
}
