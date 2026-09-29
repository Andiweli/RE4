#ifndef SCROLL_H
#define SCROLL_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Scroll (room model) data file `SMD` (game/scroll.cpp). One SmdWork per placed model.
struct SmdWork {
    Vec pos;       // 0x00
    Vec rot;       // 0x0C
    Vec scale;     // 0x18
    u8 binNo;      // 0x24  bin table index (0xFF: none)
    u8 tplNo;      // 0x25  tpl table index (0xFF: none)
    u8 motNo;      // 0x26  motion table index (0xFF: none)
    u8 id;         // 0x27  scroll object id (0xFF: unused, 0xFE: not registered)
    u8 pad_28[0x44 - 0x28];
    union {
        u32 flags;   // 0x44  bit4: bin/tpl come from the common SMD, bit6: motion too
        struct {
            u8 pad_44[3];
            u8 attr;  // 0x47  low byte of flags -> cObj::attr
        } b;
    };
};

class cSmd {
public:
    u8 Version;    // 0x00
    u8 Flag;      // 0x01  bit0: group count table in front of the works
    u16 nModel;     // 0x02
    u32 BinTblOfs;    // 0x04  offset table of the bins
    u32 TplTblOfs;    // 0x08  offset table of the tpls
    u32 MotTblOfs;    // 0x0C  offset table of the motions
    union {
        SmdWork work[1];   // 0x10
        struct {
            u32 nGroup;    // 0x10
            u32 num[1];    // 0x14  works per group
        } grp;
    };

    void slide(int offset);
    SmdWork* getWorkPtr(int id);
    void* getBinPtr(int id);
    void* getTplPtr(int id);
    void* getMotPtr(int id);
    int getWorkNum();
};

// Scroll extra data `SMX`: per-id object parameters.
struct cSmxWork {
    u8 ModelNo;         // 0x00
    u8 Id;              // 0x01  -> cModel::type
    u8 OtType;          // 0x02  -> cModel::x12F
    u8 CullMode;        // 0x03  -> cModel::CullMode
    u32 LitSelectMask;  // 0x04  -> cLightInfo::SelectMask
    u32 Flag;           // 0x08  SmxSetFlag bits
    u32 MaterialColor;  // 0x0C  -> cModelInfo::color
    u8 Free[116];       // 0x10  copied to cObj::work (0x78 bytes including SpecularColor)
    u32 SpecularColor;  // 0x84
    f32 TexU;           // 0x88
    f32 TexV;           // 0x8C
};

// 0x10 bytes: the entries (cSmxWork) directly follow, reached only through at().
class cSmxData {
public:
    u8 Version;    // 0x00
    u8 nData;      // 0x01
    u8 Dummy02;    // 0x02
    u8 Dummy03;    // 0x03
    u32 Dummy10;   // 0x04
    u32 Dummy20;   // 0x08
    u32 Dummy30;   // 0x0C

    cSmxWork* at(u32 i) { return (cSmxWork*) ((u8*) this + sizeof(*this) + sizeof(cSmxWork) * i); }
};

// Pointer wrapper (0x4 bytes) around a loaded SMX file's cSmxData; PS2 evidence only (its own
// init()/m_pSmx use isn't in any file this tree captured), kept for reference, not used here.
class cSmx {
public:
    cSmxData* m_pSmx;

    void init(cSmxData* p) { m_pSmx = p; }
};

// nScrWork is not declared here on purpose: the .sbss order of scroll.cpp follows the first
// declarations (pSmd, pSmdComn, pSmx, scrObjTbl, scrTbl, nScrWork).
extern cSmd* pSmd;
extern cSmd* pSmdComn;

int SmdInit(cSmd* pSh, cSmxData* pSmxh, cSmd* pShCmn);
void SmdClear(int mode);
void workInit(cObj* pObj);
void SmdSetup(int blockNo);
int setObj(int blkNo);
int SmdSetParam(cObj* pObj, SmdWork* pSw);
void SmxSetFlag(cObj* pObj, u32 flag);
int SmxGetFlag(cObj* pObj);
void smxInit(cObj* obj, u8 id);
void smxInit(cObj* obj, cSmxWork* w);
void* SmdGetTplPtr(int idx);
cObj* SmdGetObjPtr(u32 idx);
int SmdGetObjNum();
int SmdGetWorkId(cObj* pObj);
void BlockCreate(int blkNo, cSmd* pBlock);
void BlockDestroy(int blkNo);
SmdWork* SmdGetWorkPtr(int idx);
cObj* SmdGetGroupObjPtr(u32 idx);
cObj* SmdGetGroupObjPtr2(u32 idx);
cObj* SmdGetGroupNext(cObj* pObj00);
void SmdSetTrans(u32 idx, int onoff);
cObj* SetObjSmd(void* bin, void* tpl, Vec* pos, Vec* rot, int lightFlag, int front);

#endif
