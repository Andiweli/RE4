// game/id_tex: textures of the ID sprite system (D:/Bio4/Prog/id_tex.cpp). A cTexSys ("IdTex", 0x200
// slots) registers TPL + TexAnm pairs from id texture data blocks per owner id (IdTexDataLoad);
// IdTexSet loads a texture frame for a quad, IdChannelSet its colour.
#include "light.h"
#include "atari.h"
#include "id_sys.h"
#include "texture.h"
#include "main_mem.h"
#include "db_log.h"
#include "gx.h"

cTexSys* g_pIdTexSys;

// Allocates and inits the id texture system.
void IdTexGameInit()
{
    cTexSys* sys;

#line 46 "D:/Bio4/Prog/id_tex.cpp"
    sys = (cTexSys*) MEM_ALLOC(sizeof(cTexSys), 1, 13);
    g_pIdTexSys = sys;
    sys->Init("IdTex", 0x200);
    IdTexRoomInit();
}

// Room init: drops every registered texture.
void IdTexRoomInit()
{
    g_pIdTexSys->Clear();
}

// Releases the textures registered by owner `id`.
void IdTexRelease(int owner)
{
    g_pIdTexSys->TexRelease(owner);
}

// Effect texture pack: version 0xB, id table at 0x04, TPL table at 0x18, animation table at 0x1C.
struct IdTexData {
    u32 version;  // 0x00  == 0xB
    u32 ofsId;    // 0x04  -> TexIdTbl
    u8 pad_8[0x18 - 0x8];
    u32 ofsTpl;   // 0x18  -> TexOfsTbl of TPLs
    u32 ofsAnm;   // 0x1C  -> TexOfsTbl of TexAnms
};

// Registers every texture of an id texture data block (version 0xB: id table, TPL table, TexAnm
// table) under owner `id`; texture id 0x80 skips the duplicate check. Returns 0 on a bad version.
int IdTexDataLoad(void* data, int id)
{
    u32 addr = (u32) data;
    IdTexData* d;
    TexIdTbl* idTbl;
    TexOfsTbl* tplTbl;
    TexOfsTbl* anmTbl;
    u32 i;

    // The table offsets live in r9/r11 (BASE_REGS): the base is an integer, not the pointer parameter.
    // The empty asm keeps cse from folding `addr` back into `data`'s pointer-flagged pseudo.
    asm("" : "+r"(addr));
    d = (IdTexData*) addr;
    if (d->version != 0xB) {
        pLog->err(0, 0, "IdDataLoad():EffData [0x%x] Invalid.", addr);
        return 0;
    }
    idTbl = (TexIdTbl*) (addr + d->ofsId);
    tplTbl = (TexOfsTbl*) (addr + d->ofsTpl);
    anmTbl = (TexOfsTbl*) (addr + d->ofsAnm);
    for (i = 0; i < idTbl->num; i++) {
        TEXPalette* tpl = (TEXPalette*) ((u8*) tplTbl + tplTbl->ofs[i]);
        TexAnm* anm = (TexAnm*) ((u8*) anmTbl + anmTbl->ofs[i]);
        u8 texId = idTbl->ent[i].id;
        int check = 1;

        if (texId == 0x80) {
            check = 0;
        }
        g_pIdTexSys->TexRegist(tpl, anm, texId, id, 0, check);
    }
    return 1;
}


// Loads frame `no` of texture `id` (and its TLUT when CI) into texture map 0 with an identity texture matrix.
void IdTexSet(u8 id, u8 no)
{
    Mtx m;
    GXTexObj* tex;
    GXTlutObj* tlut;
    // The original re-extends `no` for the u8 parameter (`clrlwi r5, r4, 24`, narrow-argument compiler
    // difference); the empty asm hides the incoming promotion from combine (emobj setYarare).
    int n = no;

    asm("" : "+r"(n));
    if (g_pIdTexSys->GetTexObj(id, (u8) n, &tex) == 0) {
        pLog->err(0, 0, "IdTexSet: TexId[%x] no data", id);
        return;
    }
    GXLoadTexObj(tex, 0);
    if (g_pIdTexSys->GetTlutObj(id, &tlut) != 0) {
        GXLoadTlut(tlut, 0);
    }
    GXSetZMode(0, 3, 0);
    GXSetNumTevStages(1);
    GXSetTevOp(0, 0);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetNumTexGens(1);
    PSMTXIdentity(m);
    GXLoadTexMtxImm(m, 0x1E, 1);
    GXSetTexCoordGen(0, 1, 4, 0x1E);
}

// Texture animation record of id texture `id`; 0 when unknown.
int IdGetAnmAddr(u8 id, TexAnm** ppAnm)
{
    return g_pIdTexSys->GetAnmAddr(id, ppAnm);
}

// Sets the material colour channel from the unit's current col[] (0..255 floats).
void IdChannelSet(ID_UNIT* pIdUnit)
{
    GXColor c;

    GXSetTevOp(0, 0);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 0, 0, 0, 2);
    c.r = (u8) pIdUnit->col[0];
    c.g = (u8) pIdUnit->col[1];
    c.b = (u8) pIdUnit->col[2];
    c.a = (u8) pIdUnit->col[3];
    GXSetChanMatColor(4, c);
}

// Texture work (TPL + animation) of id texture `id`; quiet suppresses the not-found log.
TEX_WK* IdGetTexWk(u8 id, int bNoDispErrMsg)
{
    return g_pIdTexSys->GetTexWk(id, bNoDispErrMsg);
}
