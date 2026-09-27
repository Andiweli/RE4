// game/texture: cTexSys, a registry of texture slots indexed by texture id, loaded and released per
// owner. Used for the room textures (room_tex.cpp) and the id / cockpit textures (id_sys).
#include "types.h"
#include "vec.h"
#include "gx.h"
#include "tpl.h"
#include "db_log.h"
#include "main_mem.h"
#include "texture.h"

#line 20 "D:/Bio4/Prog/texture.cpp"

int lod_enable = 0;
int tex_dummy = 0;


// Creates a registry `name` with a pool of `num` GX texture objects and their in-use bitmap.
void cTexSys::Init(const char* name, u32 max)
{
    u32 i;

    this->m_name = name;
    nTexObj = max;
#line 53
    if ((pTexObj = (GXTexObj*) MEM_ALLOC(max * sizeof(GXTexObj), 1, 0xD)) == NULL) {
        nTexObj = 0;
        pLog->err(0, 0, "%s::Init(): Memory Allocation Failed.", this->m_name);
        return;
    }
#line 60
    if ((pFlag = (u8*) MEM_ALLOC(nTexObj / 8 + 1, 1, 0xD)) == NULL) {
        nTexObj = 0;
        pLog->err(0, 0, "%s::Init(): Memory Allocation Failed.", this->m_name);
        return;
    }
    for (i = 0; i < nTexObj; i++) {
        SetTexObjFlag(i, 0);
    }
    Clear();
}

// Drops every registered texture and frees the whole object pool.
void cTexSys::Clear()
{
    u32 i;
    TEX_WK* w = m_texw_array;

    for (i = 0; i < 256; i++, w++) {
        w->Owner = 0;
    }
    x5408 = 0;
    for (i = 0; i < nTexObj; i++) {
        SetTexObjFlag(i, 0);
    }
}

// In-use bit of pool object `no`.
int cTexSys::GetTexObjFlag(u32 no)
{
    if (no >= nTexObj) {
        pLog->err(0, 0, "%s::GetTexObjFlag : TexNo over [%d/%d]", m_name, no, nTexObj);
        return 0;
    }
    if ((pFlag[no >> 3] >> (no & 7)) & 1) {
        return 1;
    }
    return 0;
}

// Sets / clears the in-use bit of pool object `no`.
void cTexSys::SetTexObjFlag(u32 no, int flag)
{
    u8 bit;

    if (no >= nTexObj) {
        pLog->err(0, 0, "%s::GetTexObjFlag : TexNo over [%d/%d]", m_name, no, nTexObj);
    }
    bit = 1 << (no & 7);
    if (flag == 1) {
        pFlag[no >> 3] |= bit;
    } else {
        pFlag[no >> 3] &= ~bit;
    }
}

// Registers every texture of a TexData file (version 3) under `owner`; `clamp` forces clamped
// (non-repeating) textures. Returns 0 on a bad file.
int cTexSys::DataLoad(TexData* data, u32 owner, int clamp)
{
    TexIdTbl* ids;
    TexOfsTbl* tpls;
    TexOfsTbl* anms;
    u32 i;
    // COMPILER-DIFF: candidate (local-alloc qty order). The target names the three offset
    // temporaries r9/r11/r0 (ofsId/ofsTpl/ofsAnm), ours r0/r9/r0: a fake-lifetime tie decided by
    // the sched1 position of the third `lwz` (30 statement/base/order forms tried); the two pins
    // give the target's names with the same schedule.
    register u32 oI asm("r9");
    register u32 oT asm("r11");

    if (data->version != 3) {
        pLog->err(0, 0, "%s::DataLoad() : Data Invalid. [0x%x]", m_name, data);
        return 0;
    }
    oI = data->ofsId;
    ids = (TexIdTbl*) ((u8*) data + oI);
    oT = data->ofsTpl;
    tpls = (TexOfsTbl*) ((u8*) data + oT);
    anms = (TexOfsTbl*) ((u8*) data + data->ofsAnm);
    for (i = 0; i < ids->num; i++) {
        TEXPalette* tpl = (TEXPalette*) ((u8*) tpls + tpls->ofs[i]);
        TexAnm* anm = (TexAnm*) ((u8*) anms + anms->ofs[i]);
        u16 id = ids->ent[i].id;
        TexRegist(tpl, anm, id, owner, clamp, 1);
    }
    return 1;
}

// `num` consecutive free pool objects, marked in use; NULL when the pool is full.
GXTexObj* cTexSys::PullTexObj(u32 num)
{
    u32 start = 0;
    u32 cnt = 0;
    u32 i;
    GXTexObj* obj;

    while (cnt != num) {
        if (GetTexObjFlag(start + cnt) == 0) {
            cnt++;
        } else {
            start++;
            cnt = 0;
        }
        if (start + cnt >= nTexObj) {
            goto full;
        }
    }
    obj = &pTexObj[start];
    for (i = 0; i < num; i++) {
        SetTexObjFlag(start + i, 1);
    }
    goto done;

full:
    pLog->err(0, 0, "%s::PullTexObj(): TEXOBJ MAX!!", m_name);
    return NULL;

done:
    return obj;
}

// Relocates a TPL's offsets into pointers (once: skipped when the descriptor pointer is already a
// real address).
void cTexSys::CalcTplAddr(TEXPalette* tpl)
{
    u32 i;
    TEXDescriptor* desc;

    if (tpl == NULL) {
        return;
    }
    if ((s32) tpl->descriptorArray < 0) {
        return;
    }
    tpl->descriptorArray = (TEXDescriptor*) ((u32) tpl->descriptorArray + (u32) tpl);
    for (i = 0; i < tpl->numDescriptors; i++) {
        desc = &tpl->descriptorArray[i];
        desc->textureHeader = (TEXHeader*) ((u8*) tpl + (u32) desc->textureHeader);
        desc->textureHeader->data = (u8*) tpl + (u32) desc->textureHeader->data;
        if (desc->CLUTHeader != NULL) {
            desc->CLUTHeader = (CLUTHeader*) ((u8*) tpl + (u32) desc->CLUTHeader);
            desc->CLUTHeader->data = (u8*) tpl + (u32) desc->CLUTHeader->data;
        }
    }
}

// Registers texture `id`: pool objects for the animation's frame count, one GX texture object per
// TPL image (repeat wrap when power-of-two and not clamped; CI formats also load the palette),
// optional LOD setup. 0 when the id is taken (error when `check`) or the pool is full.
int cTexSys::TexRegist(TEXPalette* tpl, TexAnm* anm, u8 id, u32 owner, int clamp, int check)
{
    TEX_WK* w = &m_texw_array[id];
    TEXDescriptor* desc;
    TEXHeader* hdr;
    GXTexObj* obj;
    int i;

    if (w->Owner != 0) {
        if (check != 0) {
            pLog->err(0, 0, "%s::TexRegist():TexId[%x] id already used.", m_name, id);
        }
        return 0;
    }
    CalcTplAddr(tpl);
    desc = TEXGet(tpl, 0);
    w->nTexObj = anm->numTex;
    w->pTex_obj_start = PullTexObj(w->nTexObj);
    if (w->pTex_obj_start == NULL) {
        pLog->err(0, 0, "%s : ID[%02x] PullTexObj() work full!!", m_name, id);
        return 0;
    }
    w->texHdr = desc->textureHeader;
    w->Anm_addr = anm;
    w->Owner = owner;
    w->Tpl_addr = tpl;
    for (i = 0; i < w->nTexObj; i++) {
        obj = &w->pTex_obj_start[i];
        desc = TEXGet(tpl, i);
        hdr = desc->textureHeader;
        if (hdr->format - 8 <= 1) {
            if (clamp == 0 && ((hdr->width & (hdr->width - 1)) == 0) && ((hdr->height & (hdr->height - 1)) == 0)) {
                GXInitTexObjCI(obj, hdr->data, hdr->width, hdr->height, hdr->format, 1, 1, 0, 0);
            } else {
                GXInitTexObjCI(obj, desc->textureHeader->data, desc->textureHeader->width, desc->textureHeader->height,
                               desc->textureHeader->format, 0, 0, 0, 0);
            }
            GXInitTlutObj(&w->tlut, desc->CLUTHeader->data, desc->CLUTHeader->format, desc->CLUTHeader->numEntries);
            GXLoadTlut(&w->tlut, 0);
        } else {
            if (clamp == 0 && ((hdr->width & (hdr->width - 1)) == 0) && ((hdr->height & (hdr->height - 1)) == 0)) {
                GXInitTexObj(obj, hdr->data, hdr->width, hdr->height, hdr->format, 1, 1, 0);
            } else {
                GXInitTexObj(obj, desc->textureHeader->data, desc->textureHeader->width, desc->textureHeader->height,
                             desc->textureHeader->format, 0, 0, 0);
            }
        }
        if (lod_enable) {
            TEXHeader* h = desc->textureHeader;
            GXInitTexObjLOD(obj, 0, 0, (f32) h->minLOD, (f32) h->maxLOD, h->LODBias, 0, h->edgeLODEnable, 0);
        }
    }
    PSMTXIdentity(w->_Mtx);
    return 1;
}

// TPL of texture `id`; 0 when unregistered.
int cTexSys::GetTplAddr(u32 id, TEXPalette** tpl_addr)
{
    TEX_WK* w = &m_texw_array[id];

    if (w->Owner == 0) {
        return 0;
    }
    *tpl_addr = w->Tpl_addr;
    return 1;
}

// GX texture object `no` (animation frame) of texture `id`; 0 when unregistered.
int cTexSys::GetTexObj(u32 id, u32 no, GXTexObj** texobj)
{
    TEX_WK* w = &m_texw_array[id];

    if (w->Owner == 0) {
        return 0;
    }
    *texobj = &w->pTex_obj_start[no];
    return 1;
}

// Animation table of texture `id`; 0 when unregistered.
int cTexSys::GetAnmAddr(u32 id, TexAnm** anm)
{
    TEX_WK* w = &m_texw_array[id];

    if (w->Owner == 0) {
        return 0;
    }
    *anm = w->Anm_addr;
    return 1;
}

// Palette object of a CI texture `id`; 0 when unregistered or not paletted.
int cTexSys::GetTlutObj(u32 id, GXTlutObj** out)
{
    TEX_WK* w = &m_texw_array[id];

    if (w->Owner == 0) {
        return 0;
    }
    if (w->texHdr->format - 8 <= 1) {
        *out = &w->tlut;
        return 1;
    }
    *out = NULL;
    return 0;
}

// The registry slot of texture `id`, NULL (error unless `quiet`) when unregistered.
TEX_WK* cTexSys::GetTexWk(u32 id, int bNoDispErrMsg)
{
    TEX_WK* w = &m_texw_array[id];

    if (w->Owner != 0) {
        return w;
    }
    if (bNoDispErrMsg == 0) {
        pLog->err(0, 0, "GetTexWk(): TexId[%x] No such texture", id);
    }
    return NULL;
}

// Unregisters every texture of `owner` and frees its pool objects. Returns the count released.
int cTexSys::TexRelease(u32 owner)
{
    TEX_WK* w;
    u32 i;
    u32 j;
    u32 base;

    for (w = m_texw_array, i = 0; i < 256; w++, i++) {
        if (w->Owner == owner) {
            w->Owner = 0;
            base = w->pTex_obj_start - pTexObj;
            for (j = base; j < base + w->nTexObj; j++) {
                SetTexObjFlag(j, 0);
            }
        }
    }
    return 1;
}
