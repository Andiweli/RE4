#include "types.h"
#include "atari.h"
#include "map_obj.h"
#include "light.h"
#include "widget.h"
#include "dmg.h"
#include "card.h"
#include "event.h"
#include "global.h"
#include "main.h"
#include "main_sub.h"
#include "main_mem.h"
#include "scheduler.h"
#include "libgpu.h"
#include "sce_sys.h"
#include "sce_at.h"
#include "sce.h"
#include "em.h"
#include "em_set.h"
#include "emitem.h"
#include "emhit.h"
#include "obj.h"
#include "player.h"
#include "pl_sub.h"
#include "pl_npc.h"
#include "pl_wep.h"
#include "cam_ctrl.h"
#include "item.h"
#include "examine.h"
#include "mes.h"
#include "snd.h"
#include "pad.h"
#include "est.h"
#include "esp.h"
#include "shadow.h"
#include "room_data.h"
#include "etc_model.h"
#include "route_ck.h"
#include "sscrn.h"
#include "puzzle.h"
#include "dvd.h"
#include "act_btn.h"
#include "geometry.h"
#include "math_sub.h"
#include "eprintf.h"
#include "db_log.h"
#include "ref_access.h"
#include <string.h>
#include <stdio.h>
#include <dolphin/os.h>
#include "em_sub.h"
#include "item_model.h"
#include "read.h"

// Scenario trigger areas: the room's AEV (areas) and ITA (items) records plus the areas created at
// run time, checked against the player, the partner and the enemies every frame. Each record is a
// SCE_AT_DATA whose `type` selects its handler in sceAtFunc_tbl.

int DbMenuActiveCheck();                                 // game/db_menu.cpp
cObj* setItemObj(void* bin, void* tpl, Vec* pos, Vec* rot);  // game/obj19.cpp

// MTX_COPY (vec.h) with `d_` assigned after the declarations: the pointer order sceAtGetArea and
// SceAtItemHitCheck need.
#define MTX_COPY_LATE_DST(src, dst)               \
    {                                    \
        MtxPtr d_;                       \
        MtxPtr s_ = (src);               \
        int i_ = 3;                      \
        int j_;                          \
        f32* sp_;                        \
        f32* dp_;                        \
        d_ = (dst);                      \
        while (i_--) {                   \
            dp_ = *d_;                   \
            sp_ = *s_;                   \
            for (j_ = 0; j_ < 4; j_++) { \
                *dp_++ = *sp_++;         \
            }                            \
            d_++;                        \
            s_++;                        \
        }                                \
    }

// cEm::setItem seen with an int 5th argument (the caller sign-extends the item colour byte).
class cEmSetItemView : public cModel {
public:
    virtual void setItem(u16 a, u16 b, u16 c, u16 d, int e);
};
#define EM_SET_ITEM(em, a, b, c, d, e) ((cEmSetItemView*) (em))->setItem(a, b, c, d, e)

// Bit test evaluated as a value (the original's `xori; andi.` shape).
static inline int bitOff(u32 v)
{
    return !(v & 1);
}

// The item-found flag word (kind 2 of the flag areas).
static inline u32* flags51BC()
{
    return &pG->Scenario_flg[0];
}
// Door unlock bits (SCE_AT_DATA_DOOR key_flg).
static inline u32* doorUnlock()
{
    return pG->Key_flg;
}
// Global ITEM_SET flags (SCE_AT_DATA_ITEM flagNo): the item was taken.
static inline u32* itemFlags()
{
    return pG->Item_flg;
}
// Global item-found flags (Item_flg[4..]): the item was seen / its area found.
static inline u32* itemFindFlags()
{
    return &pG->Item_flg[4];
}
// Halfword fields of the save items: the original forms the address as integer arithmetic with the index
// first (`idx*16 + ((u32)pG + ofs)`): non-struct MEM with an unflagged base, so pG is reloaded after
// every store and the field offset is added to pG before the index (sthx base, idx).
static inline u32 saveItemBase(int ofs)
{
    return (u32) pG + ofs;
}
// Non-struct store at a constant offset from a struct pointer (aliases every following global load, so
// the original's `lwz pPL` stays behind the store).
#define RAW_F32(p, ofs) (*(f32*) ((u32) (p) + (ofs)))
#define RAW_U32(p, ofs) (*(u32*) ((u32) (p) + (ofs)))

#define EM_DEAD_BIT(n, i) (*(u32*) (((i) << 2) + emDeadRow(n)))
#define SAVE_ITEM_HALF(i, f) (*(u16*) (saveItemBase(PG_OFS(item_save[0].f)) + ((i) << 4)))
#define SAVE_ITEM_ROOM(i) SAVE_ITEM_HALF(i, room_no)
#define SAVE_ITEM_ID(i) SAVE_ITEM_HALF(i, item_id)
#define SAVE_ITEM_NUM(i) SAVE_ITEM_HALF(i, item_num)
#define SAVE_ITEM_POS(i, k) (*(s16*) (saveItemBase(PG_OFS(item_save[0].pos[k])) + ((i) << 4)))
#define SAVE_ITEM_TYPE(i) pG->item_save[i].item_type
#define SAVE_ITEM_ATNO(i) pG->item_save[i].item_at
#define SAVE_ITEM_EFF(i) pG->item_save[i].item_eff

// Room save record words: item flags at +8, item-found flags at +0x18 (separate pointer pseudos keep the addi).
static inline u32* roomItemFlags()
{
    return (u32*) (RoomData.getRoomSavePtr(pG->room_id) + 8);
}
// Per-room "found" flags in the room save record (+0x18).
static inline u32* roomItemFindFlags()
{
    return (u32*) (RoomData.getRoomSavePtr(pG->room_id) + 0x18);
}

// One entry of the scenario area system (`SceAtSys`, 0x124 bytes).
struct SceAtReserve {
    cEm* key;         // 0x00
    u8 saveNo;        // 0x04
    u8 pad_5[3];
};

struct SceAtSysWork {
    void* pAtData;             // 0x00   AEV file
    u32 pAtWork;               // 0x04   its records (+0x10), kept as an address
    void* pItemData;           // 0x08   ITA file
    u32 pItemWork;             // 0x0C
    u32 hitFlg[8];             // 0x10   areas hit this frame
    u32 execFlg[8];            // 0x30   areas executed this frame
    u32 ot[16];                // 0x50   ordering table, ot[15] is the list head
    u32 stop;                  // 0x90   pG->flags_170 saved by the semi-auto stop (bit31 = pending)
    u32 m_stop_flag_backup;    // 0x94   pG->Stop_flg saved by the door / skey tasks (PS2 SCE_AT_SYS m_stop_flag_backup)
    u8 hideActive;             // 0x98   a hide area is running
    u8 pad_99[3];
    SceAtReserve reserve[16];  // 0x9C
    u8 m_use_tool_data;        // 0x11C  pAtData was allocated by the tool (t_sce_at) (PS2 m_use_tool_data)
    u8 m_use_tool_data_i;      // 0x11D  (PS2 m_use_tool_data_i)
    u8 pad_11E[2];
    SCE_AT_DATA_CAM_CTRL* pCamAt;      // 0x120  camera control area in effect
};

struct SceAtReleaseModel {
    s16 cnt;          // 0x00
    u8 pad_2[2];
    void* bin;        // 0x04
    void* tpl;        // 0x08
};

// Data file headers.
struct SceAtFileHead {
    char magic[4];    // 0x00  "AEV" / "ITA"
    u16 version;      // 0x04
    u16 num;          // 0x06
    u8 pad_8[8];
    SCE_AT_DATA work[1];  // 0x10
};

// AreaViewCheck's cone scratch: the original frame reserves 0x40 bytes for it (frame 0xC8 with the
// three other locals), not the 0x48 of `GEOM_CONE_REV[2]` (GEOM_CONE_REV was probably 0x20 without `radius` then).
struct SceAtViewCone {
    f32 w[16];
};

struct SceAtFuncTbl {
    int (*func)(SCE_AT_DATA* w, cModel* m);
    u32 exclusive;    // 1 = only one such area fires per check
};

static SceAtSysWork* pS;
static SceAtSysWork SceAtSys;
static SceAtReleaseModel releaseModelTbl[8];
static cModel* p_imodel_bak = NULL;
static void* lbl_80314D6C = NULL;

int sceAtFunc_normal(SCE_AT_DATA* w, cModel* m);
int sceAtFunc_door(SCE_AT_DATA* w, cModel* m);
static int sceAtFunc_exec(SCE_AT_DATA* w, cModel* m);
int sceAtFunc_item(SCE_AT_DATA* w, cModel* m);
int sceAtFunc_flg(SCE_AT_DATA* w, cModel* m);
int sceAtFunc_mes(SCE_AT_DATA* w, cModel* m);
int sceAtFunc_save(SCE_AT_DATA* w, cModel* m);
static int sceAtFunc_shd_disp(SCE_AT_DATA* w, cModel* m);
int sceAtFunc_damage(SCE_AT_DATA* w, cModel* m);
int sceAtFunc_scr_at(SCE_AT_DATA* w, cModel* m);
int sceAtFunc_field_info(SCE_AT_DATA* w, cModel* m);
int sceAtFunc_stoop(SCE_AT_DATA* w, cModel* m);
int sceAtFunc_skey(SCE_AT_DATA* w, cModel* m);
int sceAtFunc_ladder(SCE_AT_DATA* w, cModel* m);
int sceAtFunc_use(SCE_AT_DATA* w, cModel* m);
int sceAtFunc_hide(SCE_AT_DATA* w, cModel* m);
int sceAtFunc_pos_jump(SCE_AT_DATA* w, cModel* m);
void sceInLock(SCE_AT_DATA* w);
static void sceAtSkey(SCE_AT_DATA* w);
void sceAtGetItem(SCE_AT_ITEM* w);
void sceAtGetItem_NoModel(SCE_AT_ITEM* w);
static void sceAtDeleteItem(SCE_AT_ITEM* w);
void initReleaseModelTbl();
void setReleaseModelTbl(void* bin, void* tpl);
void checkReleaseModelTbl();
void sceAtCamCtrlCheck();
void sceAtDebugDisp();
void sceAtItemFindCheck();
static void sceAtDataLoopInit();

static SceAtFuncTbl sceAtFunc_tbl[21] = {
    {sceAtFunc_normal, 0},      // 0x00
    {sceAtFunc_door, 1},        // 0x01
    {sceAtFunc_exec, 0},        // 0x02
    {sceAtFunc_item, 1},        // 0x03
    {sceAtFunc_flg, 0},         // 0x04
    {sceAtFunc_mes, 1},         // 0x05
    {sceAtFunc_normal, 1},      // 0x06
    {sceAtFunc_normal, 0},      // 0x07
    {sceAtFunc_save, 1},        // 0x08
    {sceAtFunc_shd_disp, 0},    // 0x09
    {sceAtFunc_damage, 0},      // 0x0A
    {sceAtFunc_scr_at, 0},      // 0x0B
    {sceAtFunc_normal, 0},      // 0x0C  camera control
    {sceAtFunc_field_info, 0},  // 0x0D
    {sceAtFunc_stoop, 0},       // 0x0E
    {sceAtFunc_skey, 1},        // 0x0F
    {sceAtFunc_ladder, 1},      // 0x10
    {sceAtFunc_use, 0},         // 0x11
    {sceAtFunc_hide, 0},        // 0x12
    {sceAtFunc_pos_jump, 0},    // 0x13
    {sceAtFunc_normal, 0},      // 0x14  item parent
};

// Room start: resets the area system (ordering table, hit / exec flags, reservations) and links the
// room's AEV records (version 0x104) and ITA item records (version 0x105, numbered +0x80) into the
// ordering table by their otNo.
void SceAtInit(void* pHeader, void* pHeader_i)
{
    int i;

    pS = &SceAtSys;
    pS->pAtData = 0;
    pS->pAtWork = 0;
    pS->pCamAt = 0;
    pS->hideActive = 0;
    ClearOTagR(pS->ot, 16);
    initReleaseModelTbl();
    for (i = 0; i < 16; i++) {
        SceAtSys.reserve[i].key = 0;
        SceAtSys.reserve[i].saveNo = 0;
    }
    SceAtWorkLoopInit();
    pS->m_use_tool_data = 0;
    pS->m_use_tool_data_i = 0;
    if (pHeader != 0) {
        if (strcmp((char*) pHeader, "AEV") != 0) {
            pLog->err(0, 0, "THIS DATA IS NOT SCENARIO ATARI DATA");
        } else if (((SceAtFileHead*) pHeader)->version != 0x104) {
            pLog->err(0, 0, "SceAt DATA IS OLD VERSION");
        } else {
            pS->pAtData = pHeader;
            pS->pAtWork = (u32) ((u8*) pHeader + 0x10);
            for (i = ((SceAtFileHead*) pS->pAtData)->num - 1; i >= 0; i--) {
                SCE_AT_DATA* w = (SCE_AT_DATA*) (i * sizeof(SCE_AT_DATA) + pS->pAtWork);

                AddPrim(&pS->ot[w->priority], (u32*) w);
            }
        }
    }
    if (pHeader_i != 0) {
        if (strcmp((char*) pHeader_i, "ITA") != 0) {
            pLog->err(0, 0, "THIS DATA IS NOT ITEM SET DATA");
        } else if (((SceAtFileHead*) pHeader_i)->version != 0x105) {
            pLog->err(0, 0, "SceItem DATA IS OLD VERSION");
        } else {
            pS->pItemData = pHeader_i;
            pS->pItemWork = (u32) ((u8*) pHeader_i + 0x10);
            for (i = ((SceAtFileHead*) pS->pItemData)->num - 1; i >= 0; i--) {
                SCE_AT_DATA* w;

                ((SCE_AT_DATA*) (i * sizeof(SCE_AT_DATA) + pS->pItemWork))->no += 0x80;
                w = (SCE_AT_DATA*) (i * sizeof(SCE_AT_DATA) + pS->pItemWork);
                AddPrim(&pS->ot[w->priority], (u32*) w);
            }
        }
    }
}

// Iteration start for sceAtGetOtAddr: the ordering table head (ot[15]).
SCE_AT_DATA* sceAtSetOtStart()
{
    return (SCE_AT_DATA*) &pS->ot[15];
}

// Next area record in the ordering table after `p` (skips the table's own entries); 0 at the end.
SCE_AT_DATA* sceAtGetOtAddr(SCE_AT_DATA* p)
{
    u32 v;

    while ((v = p->tag) != 0xFFFFFFFF) {
        p = (SCE_AT_DATA*) (v | 0x80000000);
        if ((s32) v < 0) {
            return p;
        }
    }
    return 0;
}

// Clears the "hit this frame" bits.
void SceAtClearHitFlg()
{
    memclr_asm(pS->hitFlg, sizeof(pS->hitFlg));
}

// Marks area `no` as hit this frame (SceAtHitCheck reads it).
void SceAtSetHitFlg(u32 at_no)
{
    u32* f = pS->hitFlg;

    FlagOn(f, at_no);
}

// Clears the "executed this frame" bits.
void SceAtClearExecFlg()
{
    memclr_asm(pS->execFlg, sizeof(pS->execFlg));
}

// Marks area `no` as executed this frame.
void SceAtSetExecFlg(u32 at_no)
{
    u32* f = pS->execFlg;

    FlagOn(f, at_no);
}

// Per frame: clears Room_flg[2..3] (per-frame event flags) and the hit / exec bits.
void SceAtWorkLoopInit()
{
    pG->Room_flg[2] = 0;
    pG->Room_flg[3] = 0;
    SceAtClearHitFlg();
    SceAtClearExecFlg();
}

// Per frame: empties the hit-model lists of the enabled type 0 (normal) areas.
static void sceAtDataLoopInit()
{
    SCE_AT_DATA* w = sceAtSetOtStart();

    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->be_flg)) {
            continue;
        }
        if (w->id == SCEAT_ID_NORMAL) {
            memclr_asm(w->data, sizeof(w->data));
        }
    }
}

// Once per frame (scenario move): the hide sequence, model links, item find / camera areas, then
// tests every enabled area against the player (type 1), the partner (8) and the active enemies
// (2, room enemies below id 0x40 plus the racks 0x45); skipped while Stop_flg 0x00400000 or in
// the debug modes. Clears the action-key status bits at the end.
void SceAtCheck()
{
    u32 i;
    cEm* em;

    if (SceAtCheckHideActive() == 1) {
        if (!StaFlagChk(pG, STA_DIEDEMO)) {
            SceAtCheckHideProc();
        }
    }
    sceAtLink_check();
    if (SpfFlagChk(pG, SPF_SCE_AT)) {
        return;
    }
    checkReleaseModelTbl();
    SceAtWorkLoopInit();
    if (DbgFlagChk(pG, DBG_NO_SCE_EXE)) {
        return;
    }
    sceAtDebugDisp();
    if (DbgFlagChk(pG, DBG_TEST_MODE)) {
        StaFlagOff(pG, STA_PL_CHECK);
        StaFlagOff(pG, STA_PL_CHECK2);
        return;
    }
    ItemMgr.flagclear();
    sceAtDataLoopInit();
    sceAtItemFindCheck();
    sceAtCamCtrlCheck();
    if ((s32) pS->stop < 0) {
        if (StaFlagChk(pG, STA_PL_CHECK)) {
            pS->stop &= 0x7FFFFFFF;
        } else {
            StaFlagOff(pG, STA_PL_CHECK2);
        }
    }
    sceAtCheck_main(pPL, 1);
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        em = EmMgr.fastAt(i);
        if (pSUB != 0 && pSUB == em) {
            sceAtCheck_main(em, 8);
            continue;
        }
        switch (em->id) {
        case 0x00:
        case 0x21:
        case 0x23:
        case 0x24:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2E:
        case 0x3B:
            continue;
        case 0x45:
            break;
        default:
            if (em->id > 0x3F) {
                continue;
            }
            break;
        }
        if (EmMoveActiveCheck(em) != 0) {
            sceAtCheck_main(em, 2);
        }
    }
    StaFlagOff(pG, STA_PL_CHECK);
    StaFlagOff(pG, STA_PL_CHECK2);
}

// Area test for one model against every enabled area of the matching checkType. Trigger bit3 areas
// register the action button, others fire their handler when the key state matches, and exclusive
// handlers run only once per frame. Returns 1 when a handler fired.
int sceAtCheck_main(cEm* em, int target_type)
{
    Vec pos;
    Vec front;
    char name[21] = {'N', 'D', 'E', 'I', 'F', 'M', 'P', 'J', 'T', 'S', 'd', 's', ' ', 'f', 'C', 'K', 'L', 'U', 'H', ' ', ' '};
    ITEM_INFO info;
    SCE_AT_DATA* w;
    int col = 0;
    int cnt = 0;
    int flag = 1;
    int hit;
    u32 ft;
    u8 t;
    int c;
    int kind;

    em->State.SetInRoom(0);
    pos = em->pos;
    pos.y += 250.0f;
    front.x = 0.0f;
    front.y = 250.0f;
    front.z = 550.0f;
    PSMTXMultVec(em->mat, &front, &front);
    if (target_type & 1) {
        if (StaFlagChk(pG, STA_PL_CHECK)) {
            flag = 3;
        }
        if (StaFlagChk(pG, STA_PL_CHECK2)) {
            flag |= 4;
        }
        SatMgr.hitCheck(&pos, &front, &front, 0, 0, 0);
    }
    hit = 0;
    w = sceAtSetOtStart();
    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->be_flg)) {
            continue;
        }
        if (!(w->target_type & target_type)) {
            continue;
        }
        if (sceAtHitCheck(w, em, &front, &pos) == 0) {
            if (w->id == SCEAT_ID_SHD_DISP) {
                sceAtFunc_shd_disp_reverse(w);
            }
            continue;
        }
        SceAtSetHitFlg(w->no);
        if (w->trg_type & 1) {
            col = 6;
        } else if (w->trg_type & 2) {
            col = 4;
        } else if (w->trg_type & 4) {
            col = 0;
        }
        eprintf2(8, 0x10, cnt * 8 + 0x168, 8, col, 0, "%c", name[w->id]);
        cnt++;
        cnt &= 7;
        t = w->id;
        if (t == SCEAT_ID_EXEC && w->pFunc == 0) {
            continue;
        }
        ft = SCEAT_ID_EXEC;
        if (w->pFunc == 0) {
            ft = t;
        }
        if (w->trg_type & 8) {
            c = 0;
            kind = w->act_type;
            if (w->act_color != 0) {
                c = (w->act_color == 1) << 7;
            }
            if (t == SCEAT_ID_DOOR) {
                c |= ACTCTR_DOOR_COLOR;
            }
            switch (ft) {
            case SCEAT_ID_DOOR:
                c |= ACTCTR_DOOR_COLOR;
                break;
            case SCEAT_ID_STOOP:
                if (PlGetStatus() & 0x8000) {
                    continue;
                }
                break;
            case SCEAT_ID_HIDE:
                if (SceAtCheckHideActive() != 0) {
                    continue;
                }
                if (SubCharHideCheck() != 1) {
                    continue;
                }
                if (RouteCkPosToPosDis(&pPL->pos, &pSUB->pos) < 5000.0f) {
                    ActBtn.set(kind, w->priority, (void*) sceAtFunc_tbl[SCEAT_ID_HIDE].func, w, ACTCTR_WEP_SET_IGNORE, DISP_X, ACT_FUNC_SCE_AT, (void*) em);
                }
                continue;
            case SCEAT_ID_ITEM:
                if (pG->shooting_mode != 0) {
                    itemInfo(((SCE_AT_ITEM*) w)->item.item_id, &info);
                    if (info.type != 7) {
                        continue;
                    }
                }
                break;
            }
            ActBtn.set(kind, w->priority, (void*) sceAtFunc_tbl[ft].func, w, c, DISP_A_NORMAL, ACT_FUNC_SCE_AT, (void*) em);
            continue;
        }
        if (!(t == 1 && w->pFunc == 0 && (w->trg_type & 2) && (flag & 4))) {
            if (!(w->trg_type & flag)) {
                continue;
            }
        }
        if (ft > 0x14) {
            continue;
        }
        if (hit != 0 && sceAtFunc_tbl[ft].exclusive != 0) {
            continue;
        }
        SceAtSetExecFlg(w->no);
        if (sceAtFunc_tbl[ft].func(w, em) == 1) {
            hit = 1;
        }
        if (w->trg_type & 0x80) {
            if (ft == 2 && w->pFunc == 0) {
                continue;
            }
            SceAtSetEnable(w->no, 0);
        }
    }
    return hit;
}

// The area of `w` in world space: its parent's matrix applied (rotation ignored with flag bit3).
// The area in world space: the record's area moved (and rotated unless flag bit3) by the parent
// model / parts matrix when the area follows a parent.
void sceAtGetArea(AREA_HIT_DATA* ret_area, SCE_AT_DATA* w)
{
    Mtx mat;
    Mtx pmat;

    *ret_area = w->area;
    if (w->pParent == 0) {
        return;
    }
    if (w->parts_no >= 0) {
        MTX_COPY_LATE_DST(w->pParent->getPartsPtr(w->parts_no)->mat, pmat);
    } else {
        MTX_COPY_LATE_DST(w->pParent->mat, pmat);
    }
    if (w->be_flg & 8) {
        Vec zero = { 0.0f, 0.0f, 0.0f };
        low_RotMatrix(mat, &zero);
        mat[0][3] = pmat[0][3];
        mat[1][3] = pmat[1][3];
        mat[2][3] = pmat[2][3];
    } else {
        MTX_COPY_LATE_DST(pmat, mat);
    }
    Vec p[4];
    switch (ret_area->type) {
    case 1:
        p[0].y = p[1].y = p[2].y = p[3].y = ret_area->xz4.floor;
        p[0].x = ret_area->xz4.p[0].x;
        p[0].z = ret_area->xz4.p[0].z;
        p[1].x = ret_area->xz4.p[1].x;
        p[1].z = ret_area->xz4.p[1].z;
        p[2].x = ret_area->xz4.p[2].x;
        p[2].z = ret_area->xz4.p[2].z;
        p[3].x = ret_area->xz4.p[3].x;
        p[3].z = ret_area->xz4.p[3].z;
        PSMTXMultVec(mat, &p[0], &p[0]);
        PSMTXMultVec(mat, &p[1], &p[1]);
        PSMTXMultVec(mat, &p[2], &p[2]);
        PSMTXMultVec(mat, &p[3], &p[3]);
        ret_area->xz4.floor = p[0].y;
        ret_area->xz4.p[0].x = p[0].x;
        ret_area->xz4.p[0].z = p[0].z;
        ret_area->xz4.p[1].x = p[1].x;
        ret_area->xz4.p[1].z = p[1].z;
        ret_area->xz4.p[2].x = p[2].x;
        ret_area->xz4.p[2].z = p[2].z;
        ret_area->xz4.p[3].x = p[3].x;
        ret_area->xz4.p[3].z = p[3].z;
        break;
    case 2:
    case 3:
        p[0].x = ret_area->cylinder.x;
        p[0].y = ret_area->cylinder.floor;
        p[0].z = ret_area->cylinder.z;
        PSMTXMultVec(mat, &p[0], &p[0]);
        ret_area->cylinder.x = p[0].x;
        ret_area->cylinder.floor = p[0].y;
        ret_area->cylinder.z = p[0].z;
        break;
    }
}

// Is `m` in area `w`? Eye areas (area type 3) test the view cone and the screen; the others test
// `front` (checkFlag bit0) or `pos`, then the facing angle within angleRange (checkFlag bit1) and,
// for item areas, the item's own hit box.
int sceAtHitCheck(SCE_AT_DATA* w, cModel* pModel, Vec* pos_f, Vec* pos)
{
    AREA_HIT_DATA area;
    f32 ang;
    int ret;
    f32 ry;

    sceAtGetArea(&area, w);
    ang = (f32) (w->hit_dir_ang * 2) * (PI / 180.0f);
    if (w->pParent != 0) {
        // `rot.y` read in both arms: the cross-jumped `lfs` lands ahead of the flag test.
        if (w->parts_no >= 0) {
            ry = w->pParent->getPartsPtr(w->parts_no)->ang.y;
        } else {
            ry = w->pParent->ang.y;
        }
        if (!(w->be_flg & 8)) {
            ang = LIMIT_ANGLE(ang + ry);
        }
    }
    ret = 0;
    if (area.type == 3) {
        Vec cc;
        Vec c = {0.0f, 0.0f, 0.0f};
        SceAtViewCone cone;

        c.x = area.eye_trigger.xz;
        c.y = area.eye_trigger.floor;
        c.z = area.eye_trigger.z;
        cc = c;
        if (AreaViewCheck(&area, (GEOM_CONE_REV*) &cone) == 1) {
            ret = InScreenCheck(&cc) == 1;
        }
    } else {
        // Both arms written out (cross-jumped `AreaHitCheck` tail, the null test stays per arm).
        if (w->hit_type & 1) {
            if (pos_f != 0 && AreaHitCheck(&area, pos_f) == 1) {
                ret = 1;
            }
        } else {
            if (pos != 0 && AreaHitCheck(&area, pos) == 1) {
                ret = 1;
            }
        }
        if (pModel != 0 && (w->hit_type & 2) && ret == 1) {
            f32 d = LIMIT_ANGLE(ang - pModel->ang.y);
            int r = w->hit_open_ang;

            if (d < (f32) (-r * 2) * (PI / 180.0f) || d > (f32) (r * 2) * (PI / 180.0f)) {
                ret = 0;
            }
        }
        if (w->id == SCEAT_ID_ITEM && ret == 1) {
            if (SceAtItemHitCheck((SCE_AT_ITEM*) w, 0) == 0) {
                ret = 0;
            }
        }
    }
    return ret;
}

// Type 0 / 6 / 7 / 0xC / 0x14 handler: records `m` in the area's hitModel list (SceAtCheckHitModel).
int sceAtFunc_normal(SCE_AT_DATA* w, cModel* pModel)
{
    u32 i;

    for (i = 0; i < 16; i++) {
        if (w->normal.pModel[i] == 0) {
            w->normal.pModel[i] = pModel;
            break;
        }
    }
    return 0;
}

// Task for a locked door: the locked SE and message 0xA ("locked") or 0xB ("unlocked with the
// key", lockType 2 sets the unlock bit); then re-enables the area and restores Stop_flg.
void sceInLock(SCE_AT_DATA* w)
{
    switch (w->door.key_id) {
    case 1:
        SndCall(6, (s8) w->door.key_se, &pPL->pos, 0, 0, 0);
        SceMesSet(0xA, 0x11, 1, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        while (cMes.GetMesStatus(0) & 1) {
            TaskSleep(1);
        }
        break;
    case 2:
        SndCall(6, (s8) w->door.key_se, &pPL->pos, 0, 0, 0);
        SceMesSet(0xB, 0x11, 1, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        while (cMes.GetMesStatus(0) & 1) {
            TaskSleep(1);
        }
        FlagOn(doorUnlock(), w->door.key_flg);
        break;
    }
    SceAtStopSemiautoCheck();
    pG->Stop_flg = pS->m_stop_flag_backup;
    w->be_flg |= 1;
    TaskExit();
}

// 1 when Ashley is around and can follow through a door: within 5000 by route, not hiding, not in
// the carried / no-follow states.
int CheckAshleyActive()
{
    if (pSUB == 0) {
        return 0;
    }
    if (RouteCkPosToPosDis(&pPL->pos, &pSUB->pos) > 5000.0f || SceAtCheckHideActive() == 1 || (StaFlagChk(pG, STA_SUB_CATCHED)) ||
        (SubCharGetStatus() & 0x02000000)) {
        return 0;
    }
    return 1;
}

// May the player take a door now? Blocked when Ashley is present (and the "left behind" flag
// Scenario_flg[0] 0x80 is not set) but cannot follow.
int CheckDoorJumpWithAshley()
{
    if (pSUB != 0 && !ScfFlagChk(pG, SCF_NO_ASHLEY_DIST_CK)) {
        if (CheckAshleyActive() == 0) {
            return 0;
        }
    }
    return 1;
}

// Type 1 handler (door): with Ashley too far, message 0x67 instead; a locked door (lockType with
// its unlock bit clear) runs sceInLock; else hands the door function to SceSys and sets the next
// room (NextPos / NextY, next_stage / next_room_no / next_point, door_no) and the game routine 4
// (room change).
int sceAtFunc_door(SCE_AT_DATA* w, cModel* pModel)
{
    u8 lt;

    if (DbMenuActiveCheck() == 1) {
        return 0;
    }
    if (CheckDoorJumpWithAshley() == 0) {
        cMes.MesSet(0x67, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1, 1, 0, 0, 4);
        return 1;
    }
    pS->m_stop_flag_backup = pG->Stop_flg;
    KeyStop(0xEFCF0000);
    pG->Stop_flg = -1;
    lt = w->door.key_id;
    if (lt != 0 && !(doorUnlock()[w->door.key_flg >> 5] & (0x80000000 >> (w->door.key_flg & 31)))) {
        switch (lt) {
        case 1:
        case 2:
            TaskExec(1, (TaskFunc) sceInLock, w);
            w->be_flg &= ~1;
            return 1;
        }
    }
    if (w->door.pExitFunc != 0) {
        SceSys.pDoorFunc = w->door.pExitFunc;
        SceSys.pDoorParam = w->door.pExitParam;
        w->door.pExitFunc = 0;
    }
    SceSys.m_door_fade_eff = w->door.fade_eff;
    pG->NextPos.x = w->door.next_pos.x;
    pG->NextPos.y = w->door.next_pos.y;
    pG->NextPos.z = w->door.next_pos.z;
    pG->NextY = w->door.next_ang_y;
    pG->room_id_prev = pG->room_id;
    pG->Part_old = pG->Part;
    pG->Stage_next = w->door.next_stage_no;
    pG->Room_next = w->door.next_room_no;
    pG->Part_next = w->door.next_part_no;
    pG->door_se = w->door.open_se;
    pG->Rno0 = 4;
    pG->Rno1 = 0;
    pG->Rno2 = 0;
    pG->Rno3 = 0;
    pG->r_continue_cnt = 0;
    SysFlagOff(pG, SYS_START_EVT_SKIP);
    return 1;
}

// Type 2 handler: runs the area's func — directly with `arg` when prio is 0, else as a scenario
// task (SceExec at prio / otNo with the model).
static int sceAtFunc_exec(SCE_AT_DATA* w, cModel* pModel)
{
    if (w->pFunc == 0) {
        return 0;
    }
    if (w->task_level == 0) {
        SetTaskModelPtr(pModel, 0);
        ((void (*)(void*)) w->pFunc)(w->pParam);
    } else {
        SceExec(w->task_level, w->pFunc, w->pParam, w->kind, w->priority, pModel);
    }
    return 1;
}

// Empties the deferred item-model free list.
void initReleaseModelTbl()
{
    memclr_asm(releaseModelTbl, sizeof(releaseModelTbl));
}

// Queues an item model's bin / tpl to be freed 3 frames later (after the GPU is done with it).
void setReleaseModelTbl(void* bin, void* tpl)
{
    u32 i;

    for (i = 0; i < 8; i++) {
        SceAtReleaseModel* t = &releaseModelTbl[i];

        if (t->cnt == 0) {
                        t->bin = bin;
            t->tpl = tpl;
            t->cnt = 3;
break;
        }
    }
}

// Per frame: counts the deferred frees down and frees the buffers.
void checkReleaseModelTbl()
{
    u32 i;

    for (i = 0; i < 8; i++) {
        SceAtReleaseModel* t = &releaseModelTbl[i];

        if (t->cnt > 0) {
            t->cnt -= 1;
            if (t->cnt <= 0) {
                if (t->bin != 0) {
                    Mem_free(t->bin);
                }
                if (t->tpl != 0) {
                    Mem_free(t->tpl);
                }
                t->bin = 0;
                t->tpl = 0;
                t->cnt = 0;
            }
        }
    }
}

#line 990 "D:/Bio4/Prog/sce_at.cpp"
// Prepares the pick-up zoom of an item area: loads the item's model from disc (the treasure map
// items 0x95 / 0x97 replace their existing model) into a setItemObj object attached to the area.
// Returns 0 on a load failure.
int itemZoom(SCE_AT_ITEM* w)
{
    SCE_AT_DATA_ITEM* it = &w->item;
    char name[0x20];
    char name2[0x20];
    void* bin;
    void* tpl;
    cObj* obj;
    int ret;

    if (it->item_id == 0x95 || it->item_id == 0x97) {
        if (it->pModel != 0) {
            p_imodel_bak = it->pModel;
            it->pModel = 0;
            w->item.pos_set |= 8;
        }
    }
    if (it->pModel == 0) {
        sprintf(name, "SS/cmn/itm%02x.bin", it->item_id);
        sprintf(name2, "SS/cmn/itm%02x.tpl", it->item_id);
        bin = 0;
        tpl = 0;
#line 1008 "D:/Bio4/Prog/sce_at.cpp"
        ret = DvdReadN(name, 0, 0, 0, 0, 5, __FILE__, __LINE__);
        if (Dvd.ReadCheck(ret, 0, 0, &bin) < 0) {
            pLog->err(0, 0, "Item model \"%s\" load faild!", name);
            return 0;
        }
#line 1018 "D:/Bio4/Prog/sce_at.cpp"
        ret = DvdReadN(name2, 0, 0, 0, 0, 5, __FILE__, __LINE__);
        if (Dvd.ReadCheck(ret, 0, 0, &tpl) < 0) {
            if (bin != 0) {
                Mem_free(bin);
            }
            pLog->err(0, 0, "Item model \"%s\" load faild!", name2);
            return 0;
        }
        obj = setItemObj(bin, tpl, (Vec*) &vecZero, (Vec*) &vecZero);
        if (obj == 0) {
            pLog->err(0, 0, "Item %d set faild!", it->item_id);
            if (bin != 0) {
                Mem_free(bin);
            }
            if (tpl != 0) {
                Mem_free(tpl);
            }
            return 0;
        }
        SceAtSetItemModel(w, obj);
        w->item.pos_set |= 2;
    }
    return 1;
}

// Undoes itemZoom: destroys the zoom model (its buffers freed later) and restores a replaced model
// (hidden unless `keep`).
void releaseModel(SCE_AT_ITEM* w, int disp_flg)
{
    if ((w->item.pos_set & 2) && w->item.pModel != 0) {
        cObj* obj = (cObj*) w->item.pModel;
        void* bin = obj->pModelInfo->model_addr;
        void* tpl = obj->pModelInfo->tpl_addr;

        ObjMgr.destroy(obj);
        setReleaseModelTbl(bin, tpl);
        w->item.pModel = 0;
        w->item.pos_set &= ~2;
    }
    if (w->item.pos_set & 8) {
        w->item.pModel = p_imodel_bak;
        p_imodel_bak = 0;
        w->item.pos_set &= ~8;
        if (disp_flg == 0) {
            w->item.pModel->be_flag &= ~2;
        }
    }
}

#define ITEM_CANCEL()                                     \
    {                                                     \
        itemExam.reset();                                 \
        it->pModel->setNoSuspend(0);                      \
        if (it->pos_set & 4) {                            \
            it->pModel->be_flag &= ~2;                    \
            it->pos_set &= ~4;                            \
        }                                                 \
        releaseModel(w, 1);                               \
        StaFlagOff(pG, STA_ITEM_GET);                        \
        StaFlagOn(pG, STA_CUT_CHANGE);                \
        SceSys.m_item_get = 0;                                   \
        SceUpCutEnd();                                    \
        return;                                           \
    }

// Scenario task of an item pick-up with a model (SceExec 5 from sceAtFunc_item): adds the item,
// shows the "got X" message with the item zoom, and opens the sub screen when the case is full.
void sceAtGetItem(SCE_AT_ITEM* w)
{
    static int disp_flag_bak;
    static int sub_screen_open;
    static int swep_flag;
    SCE_AT_DATA_ITEM* it = &w->item;
    cModel* model = w->item.pModel;
    int fh = cMes.getFontHeight(0);
    int ls = cMes.getLineGap(0);
    int y = 0x129 - fh - ls;
    // `cancel` is the newest zero when `swep_flag = 0` is expanded (sel has no initializer), so
    // cse stores its r25 there and cancel lives from the top: it then ranks below sel in global
    // alloc (sel r29, cancel r25, as in the original).
    int cancel = 0;
    int mes = 0;
    int put = 1;
    int sel;
    int i;
    ITEM_INFO info;
    cItem tmp;

    SceUpCutStart();
    swep_flag = 0;
    SpfFlagOn(pG, SPF_CAMERA);
    itemInfo(it->item_id, &info);
    switch (info.type) {
    case 0:
    case 4:
    case 5:
    case 7:
    case 0xC:
        put = ItemMgr.get(it->item_id, it->item_num);
        itemInfo(it->item_id, &info);
        switch (info.type) {
        case 7:
            SndCall(0, 0, 0, 0, 0, 0);
            break;
        case 5:
        case 0xC:
            SndCall(0, 0x13, 0, 0, 0, 0);
            break;
        }
        cMes.MesSet(0x15, 0x64, y, 0x11, 0, 0, 4);
        cMes.SetItemName(0, it->item_id);
        itemInfo(it->item_id, &info);
        if (info.type == 7) {
            cMes.SetBttnWait(0, 0x1E);
        }
        mes = 0;
        break;
    case 8: {
        // COMPILER-DIFF: 13 (global-alloc rotation it/ItemMgr/money): money pinned to the
        // original's r29 settles the other two (it r31, the ItemMgr high r30).
        register u32 money asm("r29") = pG->peseta;

        put = ItemMgr.get(it->item_id, it->item_num);
        if (it->item_id == 0x73) {
            cMes.MesSetNumber(0, ItemMgr.m_bonus_time, 0);
            cMes.MesSet(0x94, 0x64, y, 0x10000011, 0, 0, 4);
        } else if (it->item_id == 0x75) {
            cMes.MesSetNumber(0, ItemMgr.m_bonus_point, 0);
            cMes.MesSet(0x95, 0x64, y, 0x10000011, 0, 0, 4);
        } else {
            if ((s32) money < (s32) pG->peseta) {
                cMes.MesSetNumber(0, pG->peseta - money, 0);
            }
            cMes.MesSet(0x14, 0x64, y, 0x10000011, 0, 0, 4);
        }
        mes = 0;
        SndCall(0, 0x10, 0, 0, 0, 0);
        break;
    }
    case 3:
        itemInfo(ItemMgr.weaponId(), &info);
        if (info.type == 3) {
            swep_flag = put;
        }
    case 1:
    case 9:
        mes = 1;
        cMes.MesSet(0x11, 0x64, y, 0x111, 0, 0, 4);
        cMes.SetItemName(0, it->item_id);
        break;
    case 2:
        cMes.MesSet(0x13, 0x64, y, 0x211, 0, 0, 4);
        cMes.SetItemName(0, it->item_id);
        if (it->item_num != 0) {
            cMes.MesSetNumber(0, it->item_num, 0);
        } else {
            itemInfo(it->item_id, &info);
            cMes.MesSetNumber(0, info.defNum, 0);
        }
        mes = 1;
        break;
    case 0xE:
        cMes.MesSet(0x11, 0x64, y, 0x411, 0, 0, 4);
        mes = 1;
        cMes.SetItemName(0, it->item_id);
        break;
    case 6:
        switch (it->item_id) {
        case 5:
        case 6:
        case 8:
        case 9:
        case 0xA:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x95:
        case 0x97:
            if ((s16) pG->pl_life >= (s16) pG->pl_life_max) {
                cMes.MesSet(0x11, 0x64, y, 0x411, 0, 0, 4);
            } else {
                cMes.MesSet(0x12, 0x64, y, 0x411, 0, 0, 4);
            }
            break;
        default:
            cMes.MesSet(0x11, 0x64, y, 0x411, 0, 0, 4);
            break;
        }
        cMes.SetItemName(0, it->item_id);
        mes = 1;
        break;
    case 0xA:
        if (SubScreenOpen(SS_OPEN_FILE, 0) == 0) {
            SceSleep(1);
        }
        SubScreenWk.get_item_id = it->item_id;
        sub_screen_open = put;
        SubScreenWk.get_item_num = put;
        break;
    }
    itemExam.setup();
    SceSleep(1);
    if (it->pModel->isTrans() == 0) {
        it->pModel->be_flag |= 2;
        it->pos_set |= 4;
    }
    sel = 0;
    cancel = 0;
    StaFlagOn(pG, STA_ITEM_GET);
    disp_flag_bak = pG->Disp_flg;
    pG->Disp_flg = -1;
    DpfFlagOff(pG, DPF_COCKPIT);
    DpfFlagOff(pG, DPF_ESP);
    DpfFlagOff(pG, DPF_ID_SYSTEM);
    DpfFlagOff(pG, DPF_MESSAGE);
    itemExam.init(w->item.item_id, model, 0);
    LightMgr.offScr(0x20);
    LightMgr.create(0, 9, -2, 0);
    sub_screen_open = sel;
    if (mes != 0) {
        while (cMes.GetSelectMessage(0) == 0 && cancel == 0) {
            itemExam.move();
            itemExam.trans();
            if (Key.trg & 0x40000000) {
                cMes.Clear();
                put = 0;
                cancel = 1;
            }
            SceSleep(1);
        }
        if (cancel == 0) {
            // COMPILER-DIFF: candidate #12 (r0 pin): cse1 follows the `bne` into the else arm and would
            // canonicalise `res == 2` to sel; canon_reg never replaces a hard register, so the pinned res
            // keeps `cmpwi r0,2` and sel (a pseudo: preferred as class head) keeps `mr; cmpwi sel,1`.
            register int res asm("r0") = cMes.GetSelectMessage(0);

            sel = res;
            if (sel == 1) {
                put = PutInCase(it->item_id, it->item_num, (s8) SubScreenWk.board_size);
                if (put != 1) {
                    if (SubScreenOpen(SS_OPEN_PZZL, 0) == 0) {
                        SceSleep(1);
                    }
                    SubScreenWk.get_item_id = it->item_id;
                    SubScreenWk.get_item_num = it->item_num;
                    sub_screen_open = sel;
                }
            } else if (res == 2) {
                put = 0;
            } else {
                u16 n = it->item_num;

                tmp.id = it->item_id;
                if (n == 0) {
                    itemInfo(it->item_id, &info);
                    tmp.num = info.defNum;
                } else {
                    tmp.num = n;
                }
                ItemMgr.setToWhom(0);
                // `put = 1` after the call (as in sceAtGetItem_NoModel): sched2 hoists the
                // callee-saved li above the call with the highest LUID, so `addi r4,&tmp` issues first.
                ItemMgr.use(&tmp);
                put = 1;
            }
        }
    } else {
        while (cMes.GetMesStatus(0) & 1) {
            itemExam.move();
            itemExam.trans();
            SceSleep(1);
        }
    }
    itemExam.quit();
    pG->Disp_flg = disp_flag_bak;
    if (CamCtrl.areaNo != -1) {
        LightMgr.update(CamCtrl.areaNo, 0);
    } else {
        LightMgr.update(0, 0);
    }
    EffectEspDelete(1, ESP_CORE_KIND_ITEM, model, 0);
    EffectEspgenDelete(1, ESP_CORE_KIND_ITEM, model);
    EffectEfmDelete(1, ESP_CORE_KIND_ITEM, model);
    if (sub_screen_open != 0) {
        while (SubScreenWk.close_flag == 0) {
            SceSleep(1);
        }
        if (SubScreenWk.model_flag == 0) {
            ITEM_CANCEL();
        }
    } else {
        if (put == 0) {
            ITEM_CANCEL();
        }
    }
    sceAtItemFlgOn(it);
    if (swep_flag != 0) {
        PlReloadBullet();
    }
    SceAtSetEnable(w->no, 0);
    releaseModel(w, 0);
    if ((w->item.ctrl_flag & 8) && w->item.save_no >= 0) {
        memclr_asm(&pG->item_save[w->item.save_no], sizeof(ITEM_SAVE_WORK));
    }
    if (w->be_flg & 4) {
        Mem_free(w);
        DelPrim(&pS->ot[15], (u32*) w);
    }
    SceSys.m_item_get = 0;
    SceUpCutEnd();
    StaFlagOff(pG, STA_ITEM_GET);
    StaFlagOn(pG, STA_CUT_CHANGE);
}

#define ITEM_CANCEL_NOMODEL()                             \
    {                                                     \
        SceSys.m_item_get = 0;                                   \
        StaFlagOn(pG, STA_CUT_CHANGE);                \
        SceUpCutEnd();                                    \
        return;                                           \
    }

// The same pick-up sequence without a model to zoom (the item's model failed to load or is a
// no-model item): messages, case placement (PutInCase) or the sub screen, flags and clean-up.
void sceAtGetItem_NoModel(SCE_AT_ITEM* w)
{
    static int sub_screen_open;
    static int swep_flag;
    SCE_AT_DATA_ITEM* it = &w->item;
    int fh = cMes.getFontHeight(0);
    int ls = cMes.getLineGap(0);
    int y = 0x129 - fh - ls;
    int cancel = 0;
    int mes = 0;
    int put = 1;
    int sel;
    // COMPILER-DIFF: codeless def: a mention of sel before the result block so cse1 makes sel the
    // head of the `sel = res` class (a block-local sel is replaced by the sign-extend temp: 4 refs, r31).
    asm("" : "=r"(sel));
    int i;
    ITEM_INFO info;
    cItem tmp;

    SceUpCutStart();
    pPL->setNoSuspend(1);
    DpfFlagOff(pG, DPF_PL);
    swep_flag = 0;
    itemInfo(it->item_id, &info);
    switch (info.type) {
    case 0:
    case 4:
    case 5:
    case 7:
    case 0xC:
        put = ItemMgr.get(it->item_id, it->item_num);
        itemInfo(it->item_id, &info);
        switch (info.type) {
        case 7:
            SndCall(0, 0, 0, 0, 0, 0);
            break;
        case 5:
        case 0xC:
            SndCall(0, 0x13, 0, 0, 0, 0);
            break;
        }
        cMes.MesSet(0x15, 0x64, y, 0x11, 0, 0, 4);
        cMes.SetItemName(0, it->item_id);
        itemInfo(it->item_id, &info);
        if (info.type == 7) {
            cMes.SetBttnWait(0, 0x1E);
        }
        mes = 0;
        break;
    case 8: {
        u32 money = pG->peseta;

        put = ItemMgr.get(it->item_id, it->item_num);
        if (it->item_id == 0x73) {
            cMes.MesSetNumber(0, ItemMgr.m_bonus_time, 0);
            cMes.MesSet(0x94, 0x64, y, 0x10000011, 0, 0, 4);
        } else if (it->item_id == 0x75) {
            cMes.MesSetNumber(0, ItemMgr.m_bonus_point, 0);
            cMes.MesSet(0x95, 0x64, y, 0x10000011, 0, 0, 4);
        } else {
            if ((s32) money < (s32) pG->peseta) {
                cMes.MesSetNumber(0, pG->peseta - money, 0);
            }
            cMes.MesSet(0x14, 0x64, y, 0x10000011, 0, 0, 4);
        }
        mes = 0;
        SndCall(0, 0x10, 0, 0, 0, 0);
        break;
    }
    case 3:
        itemInfo(ItemMgr.weaponId(), &info);
        if (info.type == 3) {
            swep_flag = put;
        }
    case 1:
    case 9:
        mes = 1;
        cMes.MesSet(0x11, 0x64, y, 0x111, 0, 0, 4);
        cMes.SetItemName(0, it->item_id);
        break;
    case 2:
        cMes.MesSet(0x13, 0x64, y, 0x211, 0, 0, 4);
        cMes.SetItemName(0, it->item_id);
        if (it->item_num != 0) {
            cMes.MesSetNumber(0, it->item_num, 0);
        } else {
            itemInfo(it->item_id, &info);
            cMes.MesSetNumber(0, info.defNum, 0);
        }
        mes = 1;
        break;
    case 0xE:
        cMes.MesSet(0x11, 0x64, y, 0x411, 0, 0, 4);
        mes = 1;
        cMes.SetItemName(0, it->item_id);
        break;
    case 6:
        switch (it->item_id) {
        case 5:
        case 6:
        case 8:
        case 9:
        case 0xA:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x95:
        case 0x97:
            if ((s16) pG->pl_life >= (s16) pG->pl_life_max) {
                cMes.MesSet(0x11, 0x64, y, 0x411, 0, 0, 4);
            } else {
                cMes.MesSet(0x12, 0x64, y, 0x411, 0, 0, 4);
            }
            break;
        default:
            cMes.MesSet(0x11, 0x64, y, 0x411, 0, 0, 4);
            break;
        }
        cMes.SetItemName(0, it->item_id);
        mes = 1;
        break;
    case 0xA:
        if (SubScreenOpen(SS_OPEN_FILE, 0) == 0) {
            SceSleep(1);
        }
        SubScreenWk.get_item_id = it->item_id;
        sub_screen_open = put;
        SubScreenWk.get_item_num = put;
        break;
    }
    sub_screen_open = 0;
    cancel = 0;
    if (mes != 0) {
        while (cMes.GetSelectMessage(0) == 0 && cancel == 0) {
            if (Key.trg & 0x40000000) {
                cMes.Clear();
                put = 0;
                cancel = 1;
            }
            SceSleep(1);
        }
        if (cancel == 0) {
            // COMPILER-DIFF: candidate #12 (r0 pin): cse1 follows the `bne` into the else arm and would
            // canonicalise `res == 2` to sel; canon_reg never replaces a hard register, so the pinned res
            // keeps `cmpwi r0,2` and sel (a pseudo: preferred as class head) keeps `mr; cmpwi sel,1`.
            register int res asm("r0") = cMes.GetSelectMessage(0);

            sel = res;
            if (sel == 1) {
                put = PutInCase(it->item_id, it->item_num, (s8) SubScreenWk.board_size);
                if (put != 1) {
                    if (SubScreenOpen(SS_OPEN_PZZL, 0) == 0) {
                        SceSleep(1);
                    }
                    SubScreenWk.get_item_id = it->item_id;
                    SubScreenWk.get_item_num = it->item_num;
                    sub_screen_open = sel;
                }
            } else if (res == 2) {
                ITEM_CANCEL_NOMODEL();
            } else {
                u16 n = it->item_num;

                tmp.id = it->item_id;
                if (n == 0) {
                    itemInfo(it->item_id, &info);
                    tmp.num = info.defNum;
                } else {
                    tmp.num = n;
                }
                ItemMgr.setToWhom(0);
                ItemMgr.use(&tmp);
                put = 1;
            }
        }
    } else {
        while (cMes.GetMesStatus(0) & 1) {
            SceSleep(1);
        }
    }
    if (sub_screen_open != 0) {
        while (SubScreenWk.close_flag == 0) {
            SceSleep(1);
        }
        if (SubScreenWk.model_flag == 0) {
            ITEM_CANCEL_NOMODEL();
        }
    } else {
        if (put == 0) {
            ITEM_CANCEL_NOMODEL();
        }
    }
    sceAtItemFlgOn(it);
    if (swep_flag != 0) {
        PlReloadBullet();
    }
    SceAtSetEnable(w->no, 0);
    if ((w->item.ctrl_flag & 8) && w->item.save_no >= 0) {
        memclr_asm(&pG->item_save[w->item.save_no], sizeof(ITEM_SAVE_WORK));
    }
    if (w->be_flg & 4) {
        Mem_free(w);
        DelPrim(&pS->ot[15], (u32*) w);
    }
    pPL->setNoSuspend(0);
    SceSys.m_item_get = 0;
    StaFlagOn(pG, STA_CUT_CHANGE);
    SceUpCutEnd();
}

// Type 3 handler (item): keys blocked, the item model prepared (itemZoom) and the pick-up task
// started (sceAtGetItem or the no-model variant); SceSys.m_item_get = 1 while it runs.
int sceAtFunc_item(SCE_AT_DATA* w, cModel* m)
{
    SCE_AT_DATA_ITEM* it = &((SCE_AT_ITEM*) w)->item;
    int ret;
    SCE_TASK* p;

    KeyClear(0xEFCF0000);
    ret = itemZoom((SCE_AT_ITEM*) w);
    if (ret == 1) {
        p = SceExec(5, (TaskFunc) sceAtGetItem, w, 0, SCE_PRIO_15, 0);
        if (p != 0) {
            SceSys.m_item_get = ret;
            p->setNoSuspend(1);
            it->pModel->setNoSuspend(1);
        }
    } else {
        p = SceExec(5, (TaskFunc) sceAtGetItem_NoModel, w, 0, SCE_PRIO_15, 0);
        if (p != 0) {
            SceSys.m_item_get = 1;
            p->setNoSuspend(1);
        }
    }
    return 1;
}

// Room save flag helpers as the original flag_rsf.h has them (HALT lines 17 / 21).
static inline u32* RsfFlags(u16 room)
{
    return (u32*) (RoomData.getRoomSavePtr(room) + 4);
}

// Sets room save flag `no` (0..31) of `room`.
static inline void RsfSet(u16 room, int no)
{
    if (no > 0x1F) {
#line 17 "D:/Bio4/Prog/flag_rsf.h"
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);
        *(volatile u32*) 0x11111111 = 0;
    }
    RsfFlags(room)[(u32) no >> 5] |= 0x80000000 >> (no & 31);
}

// Clears room save flag `no` of `room`.
static inline void RsfClear(u16 room, int no)
{
    if (no > 0x1F) {
#line 21 "D:/Bio4/Prog/flag_rsf.h"
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);
        *(volatile u32*) 0x11111111 = 0;
    }
    RsfFlags(room)[(u32) no >> 5] &= ~(0x80000000 >> (no & 31));
}
#line 1400 "D:/Bio4/Prog/sce_at.cpp"

// Type 4 handler (flag): sets or clears (flg.off) flag `no` of kind 0 event flags (Room_flg),
// 1 room save flags, 2 Scenario_flg[0].
int sceAtFunc_flg(SCE_AT_DATA* w, cModel* pModel)
{
    SCE_AT_DATA_FLG* f = &w->flg;

    switch (f->flg_id) {
    case 0:
        if (f->flg_act == 0) {
            FlagOn(eventFlags(), f->flg_no);
        } else {
            FlagOff(eventFlags(), f->flg_no);
        }
        break;
    case 1: {
        u8 off = f->flg_act;

        if (off == 0) {
            u16 no = f->flg_no;
            u16 room = pG->room_id;

            RsfSet(room, no);
        } else {
            u16 no = f->flg_no;
            u16 room = pG->room_id;

            RsfClear(room, no);
        }
        break;
    }
    case 2:
        if (f->flg_act == 0) {
            FlagOn(flags51BC(), f->flg_no);
        } else {
            FlagOff(flags51BC(), f->flg_no);
        }
        break;
    }
    return 0;
}

// Type 5 handler (message): shows the message at once, or as a scenario task when a camera cut is
// requested.
int sceAtFunc_mes(SCE_AT_DATA* w, cModel* pModel)
{
    SCE_AT_DATA_MES* d = &w->mes;

    if (d->cam_no != 0) {
        SceExec(5, (TaskFunc) SceAtSetMes, d, 0, SCE_PRIO_DEF_2, 0);
    } else {
        SceAtSetMes(d);
    }
    return 1;
}

// Shows a message request: optional up-cut camera (camCut - 1), message `no` (type 0 plain, else
// flag bit0), optional SE (block 6 or 0), then waits for the message and returns the camera unless
// flag bit2.
void SceAtSetMes(SCE_AT_DATA_MES* pMes)
{
    u32 flags = 0x10;

    if (pMes->cam_no != 0) {
        SceUpCutStart();
        CamCtrl.CutCall((s8) (pMes->cam_no - 1));
        flags = 0x30;
    }
    if (pMes->mes_no >= 0) {
        if (pMes->mes_type == 0) {
            SceMesSet(pMes->mes_no, flags, 1, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        } else {
            SceMesSet(pMes->mes_no, flags | 1, 1, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1);
        }
    }
    if (pMes->se_no != 0) {
        if (pMes->se_type == 0) {
            SndCall(6, pMes->se_no - 1, 0, 0, 0, 0);
        } else {
            SndCall(0, pMes->se_no - 1, 0, 0, 0, 0);
        }
    }
    if (pMes->cam_no != 0) {
        if (pMes->mes_no >= 0) {
            SceMesWait();
        }
        if (!(pMes->attr & 4)) {
            CamCtrl.Comeback(0);
        }
        SceUpCutEnd();
    }
}

// Type 8 handler (typewriter): saves to the memory card (CardSave, slot `value`), refused with
// message 0x97 while Ashley is carried / away.
int sceAtFunc_save(SCE_AT_DATA* w, cModel* pModel)
{
    if (pSUB != 0 && (StaFlagChk(pG, STA_SUB_CATCHED) || (SubCharGetStatus() & 0x02000000))) {
        cMes.MesSet(0x97, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1, 1, 0, 0, 4);
    } else {
        CardSave(w->save.term_no, 1);
    }
    return 1;
}

// Type 9 handler (shadow display): turns shadow object `no` on / off (be_flag 0x80) once while the
// model is inside (`done`).
static int sceAtFunc_shd_disp(SCE_AT_DATA* w, cModel* pModel)
{
    SCE_AT_DATA_SHD_DISP* s = &w->shd_disp;

    if (s->set_flg == 1) {
        return 0;
    }
    if (s->disp_flg == 1) {
        ShdGetObjPtr(w->shd_disp.shd_no)->be_flag |= 0x80;
    } else {
        ShdGetObjPtr(w->shd_disp.shd_no)->be_flag &= ~0x80;
    }
    s->set_flg = 1;
    return 0;
}

// Leaving a shadow display area restores the shadow object's previous state.
void sceAtFunc_shd_disp_reverse(SCE_AT_DATA* w)
{
    SCE_AT_DATA_SHD_DISP* s = &w->shd_disp;

    if (s->set_flg != 0) {
        if (s->disp_flg == 1) {
            ShdGetObjPtr(w->shd_disp.shd_no)->be_flag &= ~0x80;
        } else {
            ShdGetObjPtr(w->shd_disp.shd_no)->be_flag |= 0x80;
        }
        s->set_flg = 0;
    }
}

// Type 0xA handler (damage area): damages the player (checkType bit0) / partner (bit3) through
// setDamage(kind, arg, power or 123 = no direction, flags bit0, time) when alive and not already
// dying, and registers a DmgMgr area of the same shape for the enemies (bit1).
int sceAtFunc_damage(SCE_AT_DATA* w, cModel* pModel)
{
    Vec pt[4];
    Vec c;
    int time = w->damage.dmg_timer;

    if (time == 0) {
        time = 1;
    }
    if (w->target_type & 1) {
        int dead = 1;

        if (!pPL->dmg.m_Flag && !pPL->dmg.m_Timer) {
            dead = 0;
        }
        if (dead == 0 && (s16) pG->pl_life > 0) {
            u8 fl = w->damage.dmg_ctrl;
            int a = 0;
            int b = 0xFF;

            if (fl & 1) {
                a = 1;
            }
            if (w->damage.dmg_timer != 0) {
                b = (u8) w->damage.dmg_timer;
            }
            if (fl & 2) {
                pPL->setDamage(w->damage.dmg_type, w->damage.dmg_vol, a, b, w->damage.dmg_ang);
            } else {
                pPL->setDamage(w->damage.dmg_type, w->damage.dmg_vol, a, b, 123.0f);
            }
        }
    }
    if (w->target_type & 8) {
        if (pSUB != 0) {
            int dead = 1;

            if (!pSUB->dmg.m_Flag && !pSUB->dmg.m_Timer) {
                dead = 0;
            }
            if (dead == 0 && (s16) pG->ashley_life > 0) {
                u8 fl = w->damage.dmg_ctrl;
                int a = 0;
                int b = 0xFF;

                if (fl & 1) {
                    a = 1;
                }
                if (w->damage.dmg_timer != 0) {
                    b = (u8) w->damage.dmg_timer;
                }
                if (fl & 2) {
                    SUB_CHAR()->setDamage(w->damage.dmg_type, w->damage.dmg_vol, a, b, w->damage.dmg_ang);
                } else {
                    SUB_CHAR()->setDamage(w->damage.dmg_type, w->damage.dmg_vol, a, b, 123.0f);
                }
            }
        }
    }
    if (w->target_type & 2) {
        switch (w->area.type) {
        case 1:
            pt[0].x = w->area.xz4.p[0].x;
            pt[0].y = w->area.xz4.floor;
            pt[0].z = w->area.xz4.p[0].z;
            pt[1].x = w->area.xz4.p[3].x;
            pt[1].y = w->area.xz4.floor;
            pt[1].z = w->area.xz4.p[3].z;
            pt[2].x = w->area.xz4.p[2].x;
            pt[2].y = w->area.xz4.floor;
            pt[2].z = w->area.xz4.p[2].z;
            pt[3].x = w->area.xz4.p[1].x;
            pt[3].y = w->area.xz4.floor;
            pt[3].z = w->area.xz4.p[1].z;
            DmgMgr.set(w->damage.dmg_type, time, pt, w->area.xz4.height);
            break;
        case 2:
            c.x = w->area.cylinder.x;
            c.y = w->area.cylinder.floor;
            c.z = w->area.cylinder.z;
            DmgMgr.set(w->damage.dmg_type, time, &c, w->area.cylinder.radius, w->area.cylinder.height);
            break;
        }
    }
    return 0;
}

// Type 0xB (runtime scenario collision) has no trigger action.
int sceAtFunc_scr_at(SCE_AT_DATA* w, cModel* pModel)
{
    return 0;
}

// Type 0xD handler (field info): value 0 flags the model inside (State in-room flag, dark area).
int sceAtFunc_field_info(SCE_AT_DATA* w, cModel* pModel)
{
    if (w->field.id == 0) {
        ((cEm*) pModel)->State.SetInRoom(1);
    }
    return 0;
}

// Type 0xE handler (stoop): the player crouches (low passage).
int sceAtFunc_stoop(SCE_AT_DATA* w, cModel* pModel)
{
    PlSetCrouch();
    return 0;
}

// Type 0xF handler (special key): stops the game and shows the "needs a key" message task.
int sceAtFunc_skey(SCE_AT_DATA* w, cModel* pModel)
{
    pS->m_stop_flag_backup = pG->Stop_flg;
    KeyStop(0xEFCF0000);
    pG->Stop_flg = -1;
    TaskExec(1, (TaskFunc) sceAtSkey, w);
    return 0;
}

// Task: message 0xC, then restores Stop_flg.
static void sceAtSkey(SCE_AT_DATA* w)
{
    cMes.MesSet(0xC, 0x64, 336 - cMes.getLineGap(0) - cMes.getFontHeight(0) - 1, 1, 0, 0, 4);
    while (cMes.GetMesStatus(0) & 1) {
        TaskSleep(1);
    }
    pG->Stop_flg = pS->m_stop_flag_backup;
    TaskExit();
}

// Ladder camera task: plays the area's up to three camera cuts, waits until the player has left
// the ladder routine, returns the camera. SceSys.pLadderTask is cleared at the end.
void sceAtLadder(SCE_AT_DATA* w)
{
    CamCtrl.CutCall((s8) (w->ladder.cam_no - 1));
    while (CamCtrl.IsMotionEnd() == 0) {
        SceSleep(1);
    }
    if (w->ladder.cam_no2 != 0) {
        CamCtrl.CutCall((s8) (w->ladder.cam_no2 - 1));
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
    }
    if (w->ladder.cam_no3 != 0) {
        CamCtrl.CutCall((s8) (w->ladder.cam_no3 - 1));
        while (CamCtrl.IsMotionEnd() == 0) {
            SceSleep(1);
        }
    }
    while (PlGetStatus() & 0x40000) {
        SceSleep(1);
    }
    CamCtrl.Comeback(0);
    SceSys.pLadderTask = 0;
}

// Type 0x10 handler (ladder): puts the player on the ladder (PlSetLadder at the area's foot
// position / angle / level) and starts the camera task when cut1 is set.
int sceAtFunc_ladder(SCE_AT_DATA* w, cModel* pModel)
{
    Vec pos;
    f32 ang;

    sceAtGetLadderPos(&w->ladder, &pos, &ang);
    PlSetLadder(&pos, ang, w->ladder.height);
    if (w->ladder.cam_no != 0) {
        SceSys.pLadderTask = SceExec(5, (TaskFunc) sceAtLadder, w, 0, SCE_PRIO_DEF_2, 0);
    }
    return 0;
}

// Where the player stands to use the ladder: 300 in front of the ladder record, facing it.
void sceAtGetLadderPos(SCE_AT_DATA_LADDER* ladder, Vec* pos, f32* ladder_ang)
{
    Vec ofs = {0.0f, 0.0f, 300.0f};
    Vec rot;
    Vec tmp = {0.0f, 0.0f, 0.0f};
    Mtx mat;

    tmp.y = ladder->ang_y;
    rot = tmp;
    low_RotMatrix(mat, &rot);
    TransMatrix(mat, &ladder->pos);
    PSMTXMultVec(mat, &ofs, pos);
    *ladder_ang = ladder->ang_y + PI;
    *ladder_ang = LIMIT_ANGLE(*ladder_ang);
}

// 1 when another enemy (id <= 0x20) stands within 500 of the ladder's foot (someone is using it).
int sceAtCheckLadderUp(SCE_AT_DATA_LADDER* ladder, cModel* pEm)
{
    AREA_HIT_DATA area;
    Vec pos;
    f32 ang;
    u32 i;

    sceAtGetLadderPos(ladder, &pos, &ang);
    AreaDataInit(&area, &pos, 500.0f, 2000.0f, 2);
    for (i = 0; i < EmMgr.getArrayNum(); i++) {
        cEm* em = EmMgr.fastAt(i);

        if (em->id <= 0x20 && pEm != em) {
            if (AreaHitCheck(&area, &em->pos) == 1) {
                return 0;
            }
        }
    }
    return 1;
}

// Type 0x11 handler (use item): the item useItem[1] becomes usable from the inventory while the
// player stands here (ItemMgr.available).
int sceAtFunc_use(SCE_AT_DATA* w, cModel* pModel)
{
    ItemMgr.available(w->use.use_id);
    return 0;
}

// Type 0x12 handler (hide spot, action button): sends Ashley to hide at hide.pos (SubCharCtrlHide
// with hide.mode), starts the hide sequence (step 1) with its SE.
int sceAtFunc_hide(SCE_AT_DATA* w, cModel* pModel)
{
    SubCharCtrlHide(&w->hide.pos, w->hide.type);
    w->hide.status = 1;
    SndCall(6, 0x5E, 0, 0, 0, 0);
    return 0;
}

// Room: registers the scenario function of hide area `no` (called with 0 when she is hidden, 1
// when called back).
void SceAtDataSet_hide(int no, void (*func)(int))
{
    SCE_AT_DATA* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtDataSet_hide(): AT NOT FOUND");
    } else if (w->id != SCEAT_ID_HIDE) {
        pLog->err(0, 0, "SceAtDataSet_hide(): ID is not HIDE");
    } else {
        w->hide.func = func;
    }
}

// 1 while Ashley is hiding / going to hide (partner status bits) or a hide sequence runs.
int SceAtCheckHideActive()
{
    if ((SubCharGetStatus() & 0x04000000) || (SubCharGetStatus() & 0x08000000) || (SubCharGetStatus() & 0x10000000) ||
        pS->hideActive != 0) {
        return 1;
    }
    return 0;
}

// Per frame (from SceAtCheck): drives the active hide area's sequence — step 1 waits until she is
// hidden and runs func(0); 2 waits for the whistle (PlSetWhistle); 3 after 25 frames runs func(1),
// calls her back and plays the area's camera cut; 4 waits for the cut to end and releases.
void SceAtCheckHideProc()
{
    static int timer;
    SCE_AT_DATA* w = sceAtSetOtStart();
    int off;
    u8 step;
    SCE_TASK* p;

    while ((w = sceAtGetOtAddr(w)) != 0) {
        off = !(w->be_flg & 1);
        if (off) {
            continue;
        }
        if (w->id != SCEAT_ID_HIDE) {
            continue;
        }
        step = w->hide.status;
        if (step != 0) {
            goto FOUND;
        }
    }
    return;
FOUND:
    switch (step) {
    case 0:
        break;
    case 1:
        if (SubCharGetStatus() & 0x08000000) {
            if (w->hide.func != 0) {
                SceKill(w->hide.func);
                SceExec(0x12, (TaskFunc) w->hide.func, 0, 0, SCE_PRIO_DEF_2, 0);
            }
            w->hide.status++;
            pS->hideActive = step;
        }
        break;
    case 2:
        if (PlSetWhistle() != 0) {
            w->hide.status++;
            KeyStop(0xEFCF0000);
            timer = 0x19;
        }
        break;
    case 3:
        if (timer == 0) {
            p = 0;
            if (w->hide.func != 0) {
                SceKill(w->hide.func);
                p = SceExec(0x12, (TaskFunc) w->hide.func, (void*) 1, 0, SCE_PRIO_DEF_2, 0);
            }
            SpfFlagOff(pG, SPF_KEY);
            SubCharCtrlHide(&pPL->pos, 0);
            if (w->hide.cam_no != 0) {
                SceUpCutStart();
                DpfFlagOff(pG, DPF_SUBCHAR);
                pSUB->setNoSuspend(1);
                SpfFlagOff(pG, SPF_SUBCHAR);
                if (p != 0) {
                    p->setNoSuspend(1);
                }
                CamCtrl.CutCall((s8) (w->hide.cam_no - 1));
                w->hide.status++;
            } else {
                w->hide.status = off;
                pS->hideActive = off;
            }
        }
        timer--;
        break;
    case 4:
        if (CamCtrl.IsMotionEnd() == 1) {
            pSUB->setNoSuspend(0);
            SceUpCutEnd();
            CamCtrl.Comeback(0);
            w->hide.status = off;
            pS->hideActive = off;
        }
        break;
    }
}

// OPEN (register only): the original loads dstAngle into f0 and the 0.0 constant into f13 (ours
// swapped: local-alloc qty priority); store orders, chains and a zero local tried.
// Type 0x13 handler (position jump): teleports the player to jumpPos / dstAngle and re-seats the
// quasi-FPS camera.
int sceAtFunc_pos_jump(SCE_AT_DATA* w, cModel* pModel)
{
    Vec rot;
    // COMPILER-DIFF: #13. The original's 0.0 is a reload-materialised constant (f13, the FPR after
    // the local-alloc'd dstAngle load in f0); ours allocates the 3-ref 0.0 first (f0). Both values
    // pinned: the angle too, so the y store gets the same call anti-dependent as the z/x stores
    // and the three stores keep the source order.
    register f32 a asm("fr0");
    f32 z;

    pPL->setPos(&w->pos_jump.dest_pos);
    a = w->door.next_ang_y;
    z = 0.0f;
    rot.y = a;
    rot.x = z;
    rot.z = z;
    pPL->setAng(&rot);
    CamCtrl.m_QuasiFPS.setPlayerLocation(pPL->mat, pPL->pFloor_norm);
    return 0;
}

// Marks a pending action-key release (pS->stop bit31): the held key must be released before the
// next held-trigger area fires.
void SceAtStopSemiautoCheck()
{
    pS->stop |= 0x80000000;
}

// Room start after SceAtInit: creates the runtime collision pieces (type 0xB), gives the door /
// message / stoop / typewriter / ladder / hide areas their action button kind and ordering slot
// when the data did not, sets up every enabled item area (item 0x1000 also preloads enemy module
// 0x24), and disables the areas excluded for the current language (country).
void SceAtRoomSet()
{
    SCE_AT_DATA* w = sceAtSetOtStart();

    while ((w = sceAtGetOtAddr(w)) != 0) {
        u8 t = w->id;

        switch (t) {
        case 0xB:
            if (w->be_flg & 1) {
                sceAtSetScrAt(w);
            }
            break;
        case 1:
            if (bitOff(w->trg_type) && !(w->trg_type & 8)) {
                w->act_color = 1;
                w->act_type = 0x10;
                w->trg_type = (w->trg_type & 0x80) | 8;
                w->priority = 2;
            }
            break;
        case 5:
            if (!(w->trg_type & 8)) {
                w->act_type = 1;
                w->trg_type = (w->trg_type & 0x80) | 8;
                w->priority = 2;
            }
            break;
        case 0xE:
            if (!(w->trg_type & 8)) {
                w->act_type = 0x13;
                w->trg_type = (w->trg_type & 0x80) | 8;
                w->priority = 5;
            }
            break;
        case 8:
            if (!(w->trg_type & 8)) {
                w->act_type = 0x2F;
                w->trg_type = (w->trg_type & 0x80) | 8;
                w->priority = 5;
            }
            break;
        case 0x10:
            if (!(w->trg_type & 8)) {
                w->trg_type = (w->trg_type & 0x80) | 8;
                if (w->ladder.height > 0) {
                    w->act_type = 8;
                } else {
                    w->act_type = 9;
                }
                w->priority = 5;
            }
            break;
        case 0x12:
            if (!(w->trg_type & 8)) {
                w->act_type = 0x20;
                w->trg_type = (w->trg_type & 0x80) | 8;
                w->priority = 5;
            }
            break;
        case 0x11:
            w->trg_type = 1;
            break;
        }
    }
    w = sceAtSetOtStart();
    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->be_flg)) {
            continue;
        }
        if (w->id == SCEAT_ID_ITEM) {
            if (((SCE_AT_ITEM*) w)->item.item_id == 0x1000) {
                EmReadSearch(0x24, 0, 0);
            }
            sceAtSetItem((SCE_AT_ITEM*) w);
        }
        if (pG->game_country == 0) {
            if (w->country.check(SCEAT_COUNTRY_JPN)) {
                SceAtSetEnable(w->no, 0);
            }
        } else {
            if (w->country.check(SCEAT_COUNTRY_USA)) {
                SceAtSetEnable(w->no, 0);
            }
        }
    }
}

// Creates the scenario (SatMgr, unless scr.flags bit1; attribute 0x40 added unless bit2) and
// effect (EatMgr, unless bit0) collision pieces of a type 0xB area from its quad (scaled and placed
// by the parent when it has one).
void sceAtSetScrAt(SCE_AT_DATA* w)
{
    Vec pos;
    f32 h;

    if (w->area.type != 1) {
        return;
    }
    if (w->scr_at.set_flg != 0) {
        return;
    }
    Vec rot = { 0.0f, 0.0f, 0.0f };
    Vec poly[4];
    if (w->pParent != 0) {
        cModel* p = w->pParent;

        pos = p->pos;
        poly[0].x = p->scale.x * w->area.xz4.p[0].x;
        poly[0].y = p->scale.y * w->area.xz4.floor;
        poly[0].z = p->scale.z * w->area.xz4.p[0].z;
        poly[1].x = p->scale.x * w->area.xz4.p[1].x;
        poly[1].y = p->scale.y * w->area.xz4.floor;
        poly[1].z = p->scale.z * w->area.xz4.p[1].z;
        poly[2].x = p->scale.x * w->area.xz4.p[2].x;
        poly[2].y = p->scale.y * w->area.xz4.floor;
        poly[2].z = p->scale.z * w->area.xz4.p[2].z;
        poly[3].x = p->scale.x * w->area.xz4.p[3].x;
        poly[3].y = p->scale.y * w->area.xz4.floor;
        poly[3].z = p->scale.z * w->area.xz4.p[3].z;
    } else {
        pos.x = w->area.xz4.p[0].x;
        pos.y = w->area.xz4.floor;
        pos.z = w->area.xz4.p[0].z;
        poly[0].x = 0.0f;
        poly[0].y = 0.0f;
        poly[0].z = 0.0f;
        poly[1].x = w->area.xz4.p[1].x - w->area.xz4.p[0].x;
        poly[1].y = 0.0f;
        poly[1].z = w->area.xz4.p[1].z - w->area.xz4.p[0].z;
        poly[2].x = w->area.xz4.p[2].x - w->area.xz4.p[0].x;
        poly[2].y = 0.0f;
        poly[2].z = w->area.xz4.p[2].z - w->area.xz4.p[0].z;
        poly[3].x = w->area.xz4.p[3].x - w->area.xz4.p[0].x;
        poly[3].y = 0.0f;
        poly[3].z = w->area.xz4.p[3].z - w->area.xz4.p[0].z;
    }
    h = w->area.xz4.height;
    if (!(w->scr_at.ctrl_flag & 2)) {
        if (!(w->scr_at.ctrl_flag & 4)) {
            w->scr_at.sat_attr |= 0x40;
        }
        w->scr_at.pSat = SatMgr.create(&pos, &rot, poly, h, w->scr_at.sat_attr, w->scr_at.sat_flag);
    }
    if (bitOff(w->scr_at.ctrl_flag)) {
        w->scr_at.pEat = EatMgr.create(&pos, &rot, poly, h, w->scr_at.eat_attr, w->scr_at.sat_flag);
    }
    w->scr_at.set_flg = 1;
}

// Destroys the collision pieces created by sceAtSetScrAt.
void sceAtDeleteScrAt(SCE_AT_DATA* w)
{
    if (w->scr_at.set_flg == 1) {
        if (!(w->scr_at.ctrl_flag & 2)) {
            SatMgr.destroy(w->scr_at.pSat);
        }
        if (bitOff(w->scr_at.ctrl_flag)) {
            EatMgr.destroy(w->scr_at.pEat);
        }
        w->scr_at.pSat = 0;
        w->scr_at.pEat = 0;
        w->scr_at.set_flg = 0;
    }
}

// Per frame (stage move): parented type 0xB collision pieces follow their parent's position / yaw.
void SceAtCheckMoveScrAt()
{
    SCE_AT_DATA* w = sceAtSetOtStart();

    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->be_flg)) {
            continue;
        }
        if (w->id != SCEAT_ID_SCR_AT) {
            continue;
        }
        if (w->pParent == 0) {
            continue;
        }
        if (w->scr_at.pSat != 0) {
            w->scr_at.pSat->setCoord(&w->pParent->pos, &w->pParent->ang);
        }
        if (w->scr_at.pEat != 0) {
            w->scr_at.pEat->setCoord(&w->pParent->pos, &w->pParent->ang);
        }
    }
}

// The area record numbered `no` (ITA items are 0x80 + index), or 0.
SCE_AT_DATA* SceAtPtr(int at_no)
{
    SCE_AT_DATA* w = sceAtSetOtStart();

    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (w->no == at_no) {
            return w;
        }
    }
    return 0;
}


// A free area number (0..255 not used by any record); 0 when none.
int sceAtPullAtNo(u8* out)
{
    u32 used[8];
    SCE_AT_DATA* w;
    u32 i;

    memclr_asm(used, sizeof(used));
    w = sceAtSetOtStart();
    while ((w = sceAtGetOtAddr(w)) != 0) {
        ((u32*) used)[w->no >> 5] |= 0x80000000 >> (w->no & 31);
    }
    for (i = 0; i < 256; i++) {
        if (!FlagChkVar((u32) used, i)) {
            *out = i;
            return 1;
        }
    }
    return 0;
}

// Room: the function SceSys runs after door `no` has been taken (hand-over to the next room).
void SceAtSetDoorFunc(int no, TaskFunc func, void* arg)
{
    SCE_AT_DATA* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtSetDoorFunc(): AT NOT FOUND");
    } else {
        w->door.pExitFunc = func;
        w->door.pExitParam = arg;
    }
}

// Room: makes area `no` run `func(obj)` when triggered — as a scenario task at `prio` (max 0x12, 0
// = direct call) with SceExec flag `b`; `a` != 0 replaces the trigger bits (the old ones saved in
// prioBak). For special-key areas (0xF) fills the skey payload instead.
void SceAtDataSet_exec(int no, int prio, int a, TaskFunc func, void* obj, int b)
{
    SCE_AT_DATA* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtDataSet_exec(): AT NOT FOUND");
        return;
    }
    if (w->id == SCEAT_ID_SKEY) {
        w->skey.pParam = obj;
        w->skey.pFunc = func;
        w->skey.kind = b;
        if (w->task_level > 0x12) {
            w->skey.task_level = 0x12;
        } else {
            w->skey.task_level = prio;
        }
        return;
    }
    if (a != 0) {
        if (w->trg_type_bak == 0) {
            w->trg_type_bak = w->trg_type;
        }
        w->trg_type = a;
    }
    if (w->task_level > 0x12) {
        w->task_level = 0x12;
    } else {
        w->task_level = prio;
    }
    w->pFunc = func;
    w->pParam = obj;
    w->kind = b;
}

// Undoes SceAtDataSet_exec: trigger restored, no function.
void SceAtDataReset(int at_no)
{
    SCE_AT_DATA* w = SceAtPtr(at_no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtDataReset(): AT NOT FOUND");
        return;
    }
    if (w->trg_type_bak != 0) {
        w->trg_type = w->trg_type_bak;
        w->trg_type_bak = 0;
    }
    w->task_level = 0;
    w->pFunc = 0;
}

// Enables / disables area `no` (flag bit0); item areas create / remove their model and effect,
// collision areas their pieces.
void SceAtSetEnable(int at_no, int sw)
{
    SCE_AT_DATA* w = SceAtPtr(at_no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtSetEnable(): AT NOT FOUND");
        return;
    }
    if (sw == 1) {
        w->be_flg |= 1;
    } else {
        w->be_flg &= ~1;
    }
    switch (w->id) {
    case SCEAT_ID_ITEM:
        if (sw == 1) {
            sceAtSetItem((SCE_AT_ITEM*) w);
        } else {
            sceAtDeleteItem((SCE_AT_ITEM*) w);
        }
        break;
    case SCEAT_ID_SCR_AT:
        if (sw == 1) {
            sceAtSetScrAt(w);
        } else {
            sceAtDeleteScrAt(w);
        }
        break;
    }
}

// Never-called inline the original kept the string of.
static inline int SceAtCheckEnable(int at_no)
{
    SCE_AT_DATA* w = SceAtPtr(at_no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtCheckEnable(): AT NOT FOUND");
        return 0;
    }
    return w->be_flg & 1;
}

// 1 when area `no` was hit (someone inside) this frame.
int SceAtHitCheck(u32 at_no)
{
    u32* f = pS->hitFlg;

    if (f[at_no >> 5] & (0x80000000 >> (at_no & 31))) {
        return 1;
    }
    return 0;
}

// Fires area `no` now (its type handler with no model).
void SceAtExecute(int at_no)
{
    SCE_AT_DATA* w = SceAtPtr(at_no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtExecute(): AT NOT FOUND");
        return;
    }
    sceAtFunc_tbl[w->id].func(w, 0);
}

// 1 when model `m` is inside normal area `no` this frame (hitModel list).
int SceAtCheckHitModel(int at_no, cModel* pModel)
{
    SCE_AT_DATA* w = SceAtPtr(at_no);
    int i;

    if (w == 0) {
        pLog->err(0, 0, "SceAtCheckHitModel(): AT NOT FOUND");
        return 0;
    }
    for (i = 0; i < 16; i++) {
        if (w->normal.pModel[i] == pModel) {
            return 1;
        }
    }
    return 0;
}

// Action button colour of area `no` (1 = the alternate colour).
void SceAtSetActColor(int at_no, int col)
{
    SCE_AT_DATA* w = SceAtPtr(at_no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtSetActColor(): AT NOT FOUND");
    } else {
        w->act_color = col;
    }
}

// World centre of area `no`.
void SceAtGetCenterPos(Vec* ret_pos, int at_no)
{
    AREA_HIT_DATA area;
    SCE_AT_DATA* w = SceAtPtr(at_no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtGetCenterPos(): AT NOT FOUND");
    } else {
        sceAtGetArea(&area, w);
        AreaGetCenterPos(ret_pos, &area);
    }
}

// Attaches area `w` to `parent`: the area (and an item's position / offset / model) is converted
// into the parent's local frame (divided by its scale); flag is or-ed into w->flag (8 = ignore
// the parent rotation). Returns 1 when attached, 0 when already attached / unsupported shape.
int SceAtSetParent(SCE_AT_DATA* w, cModel* parent, int flag)
{
    if (parent == 0) {
        pLog->err(0, 0, "sceAtSetParent(): pParent == NULL");
        return 0;
    }
    if (w->pParent == parent) {
        return 0;
    }
    Vec inv = { 0.0f, 0.0f, 0.0f };
    if (parent->scale.x != 0.0f) {
        inv.x = 1.0f / parent->scale.x;
    }
    if (parent->scale.y != 0.0f) {
        inv.y = 1.0f / parent->scale.y;
    }
    if (parent->scale.z != 0.0f) {
        inv.z = 1.0f / parent->scale.z;
    }
    w->be_flg |= flag;
    w->parts_no = -1;
    w->pParent = parent;
    switch (w->area.type) {
    case 1:
        w->area.xz4.floor -= parent->pos.y;
        w->area.xz4.p[0].x -= parent->pos.x;
        w->area.xz4.p[0].z -= parent->pos.z;
        w->area.xz4.p[1].x -= parent->pos.x;
        w->area.xz4.p[1].z -= parent->pos.z;
        w->area.xz4.p[2].x -= parent->pos.x;
        w->area.xz4.p[2].z -= parent->pos.z;
        w->area.xz4.p[3].x -= parent->pos.x;
        w->area.xz4.p[3].z -= parent->pos.z;
        w->area.xz4.floor *= inv.y;
        w->area.xz4.p[0].x *= inv.x;
        w->area.xz4.p[0].z *= inv.z;
        w->area.xz4.p[1].x *= inv.x;
        w->area.xz4.p[1].z *= inv.z;
        w->area.xz4.p[2].x *= inv.x;
        w->area.xz4.p[2].z *= inv.z;
        w->area.xz4.p[3].x *= inv.x;
        w->area.xz4.p[3].z *= inv.z;
        break;
    case 2:
    case 3:
        w->area.cylinder.x -= parent->pos.x;
        w->area.xz4.floor -= parent->pos.y;
        w->area.cylinder.z -= parent->pos.z;
        w->area.cylinder.x *= inv.x;
        w->area.xz4.floor *= inv.y;
        w->area.cylinder.z *= inv.z;
        break;
    default:
        return 0;
    }
    if (w->id == SCEAT_ID_ITEM) {
        ((SCE_AT_ITEM*) w)->item.item_pos.x -= parent->pos.x;
        ((SCE_AT_ITEM*) w)->item.item_pos.y -= parent->pos.y;
        ((SCE_AT_ITEM*) w)->item.item_pos.z -= parent->pos.z;
        ((SCE_AT_ITEM*) w)->item.item_pos.x *= inv.x;
        ((SCE_AT_ITEM*) w)->item.item_pos.y *= inv.y;
        ((SCE_AT_ITEM*) w)->item.item_pos.z *= inv.z;
        ((SCE_AT_ITEM*) w)->item.eff_offset.x *= inv.x;
        ((SCE_AT_ITEM*) w)->item.eff_offset.y *= inv.y;
        ((SCE_AT_ITEM*) w)->item.eff_offset.z *= inv.z;
        if (((SCE_AT_ITEM*) w)->item.eff_setno != 0) {
            sceAtItemEffDelete(&((SCE_AT_ITEM*) w)->item);
            sceAtItemEffSet((SCE_AT_ITEM*) w, 0);
        }
        if (((SCE_AT_ITEM*) w)->item.pModel != 0) {
            ((SCE_AT_ITEM*) w)->item.pModel->pos.x -= w->pParent->pos.x;
            ((SCE_AT_ITEM*) w)->item.pModel->pos.y -= w->pParent->pos.y;
            ((SCE_AT_ITEM*) w)->item.pModel->pos.z -= w->pParent->pos.z;
            ((SCE_AT_ITEM*) w)->item.pModel->pos.x *= inv.x;
            ((SCE_AT_ITEM*) w)->item.pModel->pos.y *= inv.y;
            ((SCE_AT_ITEM*) w)->item.pModel->pos.z *= inv.z;
            sceAtSetItemModelParent((SCE_AT_ITEM*) w);
        }
    }
    return 1;
}

// SceAtSetParent for area number `no`; 0 when the area does not exist.
int SceAtSetParent(int no, cModel* parent, int flag)
{
    SCE_AT_DATA* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "sceAtSetParent(): AT NOT FOUND");
        return 0;
    }
    return SceAtSetParent(w, parent, flag);
}

// Dead-stripped by the original linker (STRIP_UNUSED): only its constant pool (1e10) survives in
// `.rodata` between SceAtSetParent(int, cObj*, int) and InScreenCheck.
static int sceAtFarCheck(Vec* a, Vec* b)
{
    f32 dx = a->x - b->x;
    f32 dz = a->z - b->z;

    if (dx * dx + dz * dz > 10000000000.0f) {
        return 1;
    }
    return 0;
}

// 1 when the world point projects into the middle of the screen (25..75 % wide, 10..90 % high).
int InScreenCheck(Vec* pos)
{
    Vec scr;
    Vec p = *pos;

    GetScreenPos(&p, &scr);
    if (Screen.width * 0.25f < scr.x && Screen.width * 0.75f > scr.x && Screen.height * 0.1f < scr.y &&
        Screen.height * 0.9f > scr.y) {
        return 1;
    }
    return 0;
}

// Scenario: room change to `room` (stage << 8 | no) arriving at pos / rot.y, part `a` — a door
// area made up on the spot and fired.
void SceAtExecRoomJump(u16 room, Vec* pos, Vec* rot, int a)
{
    SCE_AT_DATA w;

    w.country.reset();
    w.door.next_stage_no = room >> 8;
    w.door.next_room_no = room;
    w.door.next_pos.x = pos->x;
    w.door.next_pos.y = pos->y;
    w.door.next_pos.z = pos->z;
    w.door.next_ang_y = rot->y;
    w.door.next_part_no = a;
    w.door.pExitFunc = 0;
    sceAtFunc_door(&w, 0);
}

// The field-info payload of the enabled type 0xD area containing `pos`, or 0 (emwindow uses it
// to decide the lighting of thrown things).
SCE_AT_DATA_FIELD_INFO* SceAtCheckFieldInfo(Vec* pos)
{
    SCE_AT_DATA* w;

    if (pS == 0) {
        return 0;
    }
    w = sceAtSetOtStart();
    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->be_flg)) {
            continue;
        }
        if (w->id != SCEAT_ID_FIELD_INFO) {
            continue;
        }
        if (sceAtHitCheck(w, 0, pos, pos) == 1) {
            return &w->field;
        }
    }
    return 0;
}

// Is `m` in an enabled ladder area that nobody else is using? Returns 1 with the ladder's foot
// position / facing / level (enemies climbing).
int SceAtCheckLadder(cModel* pEm, Vec* pos, f32* ladder_ang, u8* ladder_height)
{
    SCE_AT_DATA* w;
    SCE_AT_DATA_LADDER* l;
    Vec* mp;

    if (pS == 0) {
        return 0;
    }
    if (pEm == 0) {
        return 0;
    }
    w = sceAtSetOtStart();
    mp = &pEm->pos;
    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->be_flg)) {
            continue;
        }
        if (w->id != SCEAT_ID_LADDER) {
            continue;
        }
        if (sceAtHitCheck(w, 0, mp, mp) != 1) {
            continue;
        }
        l = &w->ladder;
        if (sceAtCheckLadderUp(l, pEm) == 1) {
            sceAtGetLadderPos(l, pos, ladder_ang);
            *ladder_height = w->ladder.height;
            return 1;
        }
    }
    return 0;
}

// Nearest free ladder within 2000 of `m`; returns 1 with its foot position / facing / level.
int SceAtSearchLadder(cModel* pEm, Vec* pos, f32* ladder_ang, u8* ladder_height)
{
    SCE_AT_DATA* w;
    SCE_AT_DATA_LADDER* l;
    SCE_AT_DATA_LADDER* found;
    f32 best;
    f32 d;

    if (pS == 0 || pEm == 0) {
        return 0;
    }
    best = 4000000.0f;
    found = 0;
    w = sceAtSetOtStart();
    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->be_flg)) {
            continue;
        }
        if (!(w->id & 0x10)) {
            continue;
        }
        l = &w->ladder;
        if (sceAtCheckLadderUp(l, pEm) != 1) {
            continue;
        }
        d = PSVECSquareDistance(&pEm->pos, &l->pos);
        if (best > d) {
            best = d;
            found = l;
        }
    }
    if (found == 0) {
        return 0;
    }
    sceAtGetLadderPos(found, pos, ladder_ang);
    *ladder_height = found->height;
    return 1;
}

// Per frame: picks the camera-control area the player stands in and hands it to the quasi-FPS
// camera. Without one, a corner found by PlCornerCheck makes a temporary area at the player.
void sceAtCamCtrlCheck()
{
    static SCE_AT_DATA_CAM_CTRL auto_work;
    SCE_AT_DATA_CAM_CTRL* found = 0;
    SCE_AT_DATA_CAM_CTRL* c;
    SCE_AT_DATA* w;
    f32 best = 100000000.0f;
    f32 r;
    f32 r2;
    f32 d;
    f32 dy;
    f32 a;
    int ret;

    w = sceAtSetOtStart();
    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->be_flg)) {
            continue;
        }
        if (w->id != SCEAT_ID_CAM_CTRL) {
            continue;
        }
        c = &w->cam_ctrl;
        // `r * r` in both arms: jump2 cross-jumps the shared `fmuls` into the join, ahead of the pPL load.
        if (pS->pCamAt == c) {
            r = pS->pCamAt->radius + pS->pCamAt->out_range;
            r2 = r * r;
        } else {
            r = c->radius;
            r2 = r * r;
        }
        d = PSVECSquareDistance(&pPL->pos, &c->pos);
        if (!(r2 > d)) {
            continue;
        }
        if (!(best > d)) {
            continue;
        }
        dy = pPL->pos.y - c->pos.y;
        if (dy * dy > 250000.0f) {
            continue;
        }
        switch (c->type) {
        case 0:
            a = LIMIT_ANGLE(c->ang_y - pPL->ang.y);
            if (pS->pCamAt == c) {
                if (a < -1.3962634f || a > 1.3962634f) {
                    continue;
                }
            } else {
                if (a < -1.0471976f || a > 1.0471976f) {
                    continue;
                }
            }
            break;
        case 1:
            a = GetXZAngleLocal(&pPL->pos, &c->pos, pPL->ang.y);
            if (pS->pCamAt == c) {
                if (a < -2.3561945f || a > 0.17453292f) {
                    continue;
                }
            } else {
                if (a < -1.5707964f || a > -0.17453292f) {
                    continue;
                }
            }
            break;
        }
        best = d;
        found = c;
    }
    if (found != 0) {
        RAW_U32(pS, 0x120) = (u32) found;
        CamCtrl.m_QuasiFPS.LRinfo(found);
        return;
    }
    ret = PlCornerCheck();
    if (ret < 0) {
        return;
    }
    if (ret > 1) {
        if (ret == 2) {
            RAW_U32(pS, 0x120) = (u32) &auto_work;
            auto_work.pos = pPL->pos;
            auto_work.ang_y = pPL->ang.y;
            CamCtrl.m_QuasiFPS.LRinfo(&auto_work);
        }
    } else {
        if (pS->pCamAt != 0) {
            f32 lim = 250000.0f;

            if (lim < PSVECSquareDistance(&pPL->pos, &pS->pCamAt->pos)) {
                RAW_U32(pS, 0x120) = 0;
            } else {
                a = LIMIT_ANGLE(pS->pCamAt->ang_y - pPL->ang.y);
                if (a < -1.2217305f || a > 1.2217305f) {
                    RAW_U32(pS, 0x120) = 0;
                }
            }
        }
        CamCtrl.m_QuasiFPS.LRinfo(pS->pCamAt);
    }
}

// Debug (debug_mode 0x11 / Debug_flg[0] 0x00400000): draws every enabled area (and the items'
// eye triggers) with its number, type letter and state.
void sceAtDebugDisp()
{
    AREA_HIT_DATA eye;
    Mtx mat;
    Mtx pmat;
    SCE_AT_DATA* w;
    // COMPILER-DIFF: gcse PRE pseudo numbering. One extra pseudo before the matrix copies: without it the
    // second copy's `s_ + 16` / `d_ + 16` expressions (regs 123/124) hash to buckets 76/0 of the 77-bucket
    // table, so the dst giv is numbered (and allocated, r8) before the src giv; the original has src in r8.
    int dead = 0;

    if (pG->debug_mode != 0x11 && !DbgFlagChk(pG, DBG_SCE_AT_DISP)) {
        return;
    }
    w = sceAtSetOtStart();
    while ((w = sceAtGetOtAddr(w)) != 0) {
        if (bitOff(w->be_flg)) {
            continue;
        }
        eprintf(0xC8, 0x20, 0, 0x11, "[SCENARIO ATARI VIEW]");
        if (w->pParent == 0) {
            AreaDataDisp(&w->area, 0x80808080, 1, 0);
        } else {
            if (w->parts_no >= 0) {
                MTX_COPY_LATE_DST(w->pParent->getPartsPtr(w->parts_no)->mat, pmat);
            } else {
                MTX_COPY_LATE_DST(w->pParent->mat, pmat);
            }
            if (w->be_flg & 8) {
                Vec zero = { 0.0f, 0.0f, 0.0f };
                low_RotMatrix(mat, &zero);
                mat[0][3] = pmat[0][3];
                mat[1][3] = pmat[1][3];
                mat[2][3] = pmat[2][3];
            } else {
                MTX_COPY_LATE_DST(pmat, mat);
            }
            AreaDataDisp(&w->area, 0x80808080, 1, mat);
        }
        if (w->id == SCEAT_ID_ITEM && (((SCE_AT_ITEM*) w)->item.pos_set & 1)) {
            SceAtDataEyeTriggreCopy(&eye, w);
            AreaDataDisp(&eye, 0x80808080, 1, 0);
        }
    }
}

// Builds the eye (view cone) area of an item: 100 radius at the item position, cone from its rot
// (x / y angles, z = opening).
void SceAtDataEyeTriggreCopy(AREA_HIT_DATA* area, SCE_AT_DATA* w)
{
    SCE_AT_DATA_ITEM* it;

    area->be_flag = 1;
    area->type = 3;
    if (w->id != SCEAT_ID_ITEM) {
        return;
    }
    it = &((SCE_AT_ITEM*) w)->item;
    area->eye_trigger.floor = it->item_pos.y;
    area->eye_trigger.radius = 100.0f;
    area->eye_trigger.xz = ((SCE_AT_ITEM*) w)->item.item_pos.x;
    area->eye_trigger.z = it->item_pos.z;
    area->eye_trigger.ang_x = it->ang_x;
    area->eye_trigger.ang_y = it->ang_y;
    area->eye_trigger.open_ang = it->open_ang;
}

// Per frame: for each enabled item area — a shoot-down item (flag2 bit4) that was hit plays its
// damage SE and, once landed, becomes a normal item (found flag, auto area, glow effect 2); a
// dropped item (bit6) falls to the floor the same way; a disappearing item (bit5) counts its
// timer in half seconds (fade effect at 6) and is removed at 0.
void sceAtItemFindCheck()
{
    Vec pos;
    Vec rot;
    SCE_AT_DATA* w = sceAtSetOtStart();
    SCE_AT_DATA_ITEM* it;
    int off;
    cModel* pm;

    while ((w = sceAtGetOtAddr(w)) != 0) {
        off = !(w->be_flg & 1);
        if (off) {
            continue;
        }
        if (w->id != SCEAT_ID_ITEM) {
            continue;
        }
        it = &((SCE_AT_ITEM*) w)->item;
        sceAtItemFindFlgCk(it);
        if ((it->ctrl_flag & 0x10) && it->pModel != 0) {
            cEmItem* em = (cEmItem*) it->pModel;

            if (em->ckStatus() == 3 && it->se_no2 != 0) {
                pos = it->pModel->pos;
                pos.y -= 300.0f;
                SndCall(6, it->se_no2, &pos, 0, 0, 0);
                it->se_no2 = off;
            }
            if (em->ckStatus() == 1) {
                it->ctrl_flag &= ~0x10;
                it->item_pos = it->pModel->pos;
                it->item_pos.y += 100.0f;
                sceAtItemFindFlgOn(it);
                if (it->se_no != 0) {
                    pos = it->pModel->pos;
                    pos.y += 300.0f;
                    SndCall(6, it->se_no, &pos, 0, 0, 0);
                }
                if (!(it->ctrl_flag & 2)) {
                    SceAtItemAutoArea(&w->area, &em->pos, it->radius);
                }
                pm = it->pModel;
                if (pm != 0) {
                    Vec* rp = &rot;

                    rp->x = 0.0f;
                    rp->y = 0.0f;
                    rp->z = 0.0f;
                    pm->setAng(rp);
                }
                sceAtItemEffDelete(it);
                it->eff_type = 2;
                sceAtItemEffSet((SCE_AT_ITEM*) w, it->pModel);
                sceAtCheckItemModelParent((SCE_AT_ITEM*) w);
            }
        }
        if ((it->ctrl_flag & 0x40) && it->pModel != 0) {
            cEm* m = (cEm*) ((SCE_AT_ITEM*) w)->item.pModel;
            f32 fl = EatMgr.getFloor(&m->pos, 0, 0.0f, 100000.0f, 0);

            m->pos.y -= m->dmg.m_PosFrom.y;
            m->dmg.m_PosFrom.y += 10.0f;
            if (m->pos.y < fl) {
                m->pos.y = fl;
                it->ctrl_flag &= ~0x40;
                sceAtItemFindFlgOn(it);
                if (it->se_no != 0) {
                    pos = it->pModel->pos;
                    pos.y += 300.0f;
                    SndCall(6, it->se_no, &pos, 0, 0, 0);
                }
                if (!(it->ctrl_flag & 2)) {
                    SceAtItemAutoArea(&w->area, &m->pos, it->radius);
                }
                if (it->pModel != 0) {
                    it->pModel->ang.x = 0.0f;
                    it->pModel->ang.y = 0.0f;
                    it->pModel->ang.z = 0.0f;
                }
                sceAtItemEffDelete(it);
                it->eff_type = 2;
                sceAtItemEffSet((SCE_AT_ITEM*) w, it->pModel);
                sceAtCheckItemModelParent((SCE_AT_ITEM*) w);
            }
        }
        if (it->ctrl_flag & 0x20) {
            if (it->disappear_timer != 0) {
                if (pG->Frame_cnt % 30 == 0) {
                    it->disappear_timer--;
                    if (it->disappear_timer == 6) {
                        sceAtItemEffDelete(it);
                        sceAtItemDisappearEffSet((SCE_AT_ITEM*) w, 0);
                    }
                }
            } else {
                ((SCE_AT_ITEM*) w)->item.ctrl_flag &= ~0x20;
                SceAtSetEnable(w->no, 0);
                if (w->be_flg & 4) {
                    Mem_free(w);
                    DelPrim(&pS->ot[15], (u32*) w);
                }
            }
        }
    }
}

// Marks an item taken: global ITEM_SET flag `flagNo`, or room save item flag `saveNo` when flagNo is 0.
void SceAtItemFlgOn(u16 item_flg, u16 auto_item_flg)
{
    if (item_flg != 0) {
        FlagOn(itemFlags(), item_flg);
    } else if (auto_item_flg != 0) {
        if (RoomData.getRoomSavePtr(pG->room_id) != 0) {
            FlagOn(roomItemFlags(), auto_item_flg);
        }
    }
}

// 1 when the item (global flag, or the room save flag) has been taken.
int SceAtItemFlgCk(u16 item_flg, u16 auto_item_flg)
{
    u32 r = 0;

    if (item_flg != 0) {
        r = itemFlags()[item_flg >> 5] & (0x80000000 >> (item_flg & 31));
    } else if (auto_item_flg != 0) {
        if (RoomData.getRoomSavePtr(pG->room_id) != 0) {
            r = roomItemFlags()[auto_item_flg >> 5] & (0x80000000 >> (auto_item_flg & 31));
        }
    }
    if (r != 0) {
        return 1;
    }
    return 0;
}

// 1 when item area `no` has been taken.
int SceAtItemFlgCk(int at_no)
{
    SCE_AT_DATA* w = SceAtPtr(at_no);

    if (w != 0 && w->id == SCEAT_ID_ITEM) {
        return sceAtItemFlgCk(&((SCE_AT_ITEM*) w)->item);
    }
    return 0;
}

// 1 when item area `no` has been found (seen / knocked down).
int SceAtItemFindFlgCk(int at_no)
{
    SCE_AT_DATA* w = SceAtPtr(at_no);

    if (w != 0 && w->id == SCEAT_ID_ITEM) {
        return sceAtItemFindFlgCk(&((SCE_AT_ITEM*) w)->item);
    }
    return 0;
}

// Marks the item taken (flagNo, else the room flag findFlagNo).
void sceAtItemFlgOn(SCE_AT_DATA_ITEM* it)
{
    u16 no = it->item_flg;

    if (no != 0) {
        FlagOn(itemFlags(), no);
    } else if (it->auto_item_flg != 0) {
        if (RoomData.getRoomSavePtr(pG->room_id) != 0) {
            FlagOn(roomItemFlags(), it->auto_item_flg);
        }
    }
}

// 1 when the item has been taken.
int sceAtItemFlgCk(SCE_AT_DATA_ITEM* it)
{
    u32 r = 0;
    u16 no = it->item_flg;

    if (no != 0) {
        r = itemFlags()[no >> 5] & (0x80000000 >> (no & 31));
    } else if (it->auto_item_flg != 0) {
        if (RoomData.getRoomSavePtr(pG->room_id) != 0) {
            r = roomItemFlags()[it->auto_item_flg >> 5] & (0x80000000 >> (it->auto_item_flg & 31));
        }
    }
    if (r != 0) {
        return 1;
    }
    return 0;
}

// Marks the item found (Item_flg[4..] by flagNo, else the room record's found flags).
void sceAtItemFindFlgOn(SCE_AT_DATA_ITEM* it)
{
    u16 no = it->item_flg;

    if (no != 0) {
        FlagOn(itemFindFlags(), no);
    } else if (it->auto_item_flg != 0) {
        if (RoomData.getRoomSavePtr(pG->room_id) != 0) {
            FlagOn(roomItemFindFlags(), it->auto_item_flg);
        }
    }
}

// 1 when the item has been found.
int sceAtItemFindFlgCk(SCE_AT_DATA_ITEM* it)
{
    u32 r = 0;
    u16 no = it->item_flg;

    if (no != 0) {
        r = itemFindFlags()[no >> 5] & (0x80000000 >> (no & 31));
    } else if (it->auto_item_flg != 0) {
        if (RoomData.getRoomSavePtr(pG->room_id) != 0) {
            r = roomItemFindFlags()[it->auto_item_flg >> 5] & (0x80000000 >> (it->auto_item_flg & 31));
        }
    }
    if (r != 0) {
        return 1;
    }
    return 0;
}

// Removes area `no`: disabled, unlinked from the ordering table, freed if it was created at run time.
int SceAtDestroy(int at_no)
{
    SCE_AT_DATA* w = SceAtPtr(at_no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtDestroy(): AT NOT FOUND");
        return 0;
    }
    SceAtSetEnable(w->no, 0);
    DelPrim(&pS->ot[15], (u32*) w);
    if (w->be_flg & 4) {
        Mem_free(w);
    }
    return 1;
}

#line 3850 "D:/Bio4/Prog/sce_at.cpp"
// Creates a type 2 (exec) area at run time on model `m`: quad of the four `pos` corners (floor =
// their mean y, height h), checkFlag a, trigger b, checkType c, otNo d, facing angle / range (radians),
// action button kind e, task prio / func / arg / flag. Returns the area number, -1 on failure.
int SceAtCreateExecAt(cModel* m, Vec* pos, f32 h, int a, int b, int c, int d, f32 ang, f32 range, int e, int prio, TaskFunc func, void* arg, u8 flag)
{
    SCE_AT_DATA* w;

#line 3859 "D:/Bio4/Prog/sce_at.cpp"
    w = (SCE_AT_DATA*) MEM_CALLOC(sizeof(SCE_AT_DATA), 1, 13);
    if (w == 0) {
        return -1;
    }
    if (sceAtPullAtNo(&w->no) == 0) {
        return -1;
    }
    w->hit_type = a;
    w->trg_type = b;
    w->target_type = c;
    w->act_type = e;
    w->priority = d;
    w->task_level = prio;
    w->kind = flag;
    w->pParent = m;
    w->pFunc = func;
    w->pParam = arg;
    w->be_flg = 7;
    w->id = 2;
    w->parts_no = -1;
    w->hit_dir_ang = (s8) (ang * 0.5f * 57.295776f);
    w->hit_open_ang = (s8) (range * 0.5f * 57.295776f);
    AreaDataInit(&w->area, &m->pos, 1500.0f, h, 1);
    w->area.xz4.floor = (pos[0].y + pos[1].y + pos[2].y + pos[3].y) * 0.25f;
    w->area.xz4.p[0].x = pos[0].x;
    w->area.xz4.p[0].z = pos[0].z;
    w->area.xz4.p[1].x = pos[1].x;
    w->area.xz4.p[1].z = pos[1].z;
    w->area.xz4.p[2].x = pos[2].x;
    w->area.xz4.p[2].z = pos[2].z;
    w->area.xz4.p[3].x = pos[3].x;
    w->area.xz4.p[3].z = pos[3].z;
    AddPrim(&pS->ot[w->priority], (u32*) w);
    return w->no;
}

#line 3936 "D:/Bio4/Prog/sce_at.cpp"
// Creates a type 0xD (field info) area on model `m` (same shape arguments as SceAtCreateExecAt)
// carrying `val`; *out receives the payload. Returns the area number, -1 on failure.
int SceAtCreateFieldAt(cModel* m, Vec* pos, f32 h, int a, int b, int c, int d, f32 ang, f32 range, int e, int val, SCE_AT_DATA_FIELD_INFO** out)
{
    SCE_AT_DATA* w;

#line 3946 "D:/Bio4/Prog/sce_at.cpp"
    w = (SCE_AT_DATA*) MEM_CALLOC(sizeof(SCE_AT_DATA), 1, 13);
    *out = 0;
    if (w == 0) {
        return -1;
    }
    if (sceAtPullAtNo(&w->no) == 0) {
        return -1;
    }
    w->hit_type = a;
    w->trg_type = b;
    w->target_type = c;
    w->act_type = e;
    w->priority = d;
    w->task_level = 0;
    w->pFunc = 0;
    w->pParam = 0;
    w->kind = 0;
    w->be_flg = 7;
    w->id = 0xD;
    w->parts_no = -1;
    w->pParent = m;
    w->hit_dir_ang = (s8) (ang * 0.5f * 57.295776f);
    w->hit_open_ang = (s8) (range * 0.5f * 57.295776f);
    AreaDataInit(&w->area, &m->pos, 1500.0f, h, 1);
    w->area.xz4.floor = (pos[0].y + pos[1].y + pos[2].y + pos[3].y) * 0.25f;
    w->area.xz4.p[0].x = pos[0].x;
    w->area.xz4.p[0].z = pos[0].z;
    w->area.xz4.p[1].x = pos[1].x;
    w->area.xz4.p[1].z = pos[1].z;
    w->area.xz4.p[2].x = pos[2].x;
    w->area.xz4.p[2].z = pos[2].z;
    w->area.xz4.p[3].x = pos[3].x;
    w->area.xz4.p[3].z = pos[3].z;
    w->field.id = val;
    w->field.pParent = m;
    AddPrim(&pS->ot[w->priority], (u32*) w);
    *out = &w->field;
    return w->no;
}

#line 3995 "D:/Bio4/Prog/sce_at.cpp"
// Drops an item into the room at run time (enemy drops, broken crates). Persistent items get a
// save_item record so they survive a room change, and the others disappear after 61 half-seconds.
// Returns the area number, -1 on failure.
int SceAtCreateItemAt(Vec* pos, ITEM_ID id, int num, int effType, int saveNo, cModel* parent, int parts)
{
    SCE_AT_DATA* w;
    void* bin;
    void* tpl;
    int ok;
    cObj* obj;

#line 4005 "D:/Bio4/Prog/sce_at.cpp"
    w = (SCE_AT_DATA*) MEM_CALLOC(sizeof(SCE_AT_DATA), 1, 13);
    if (w == 0) {
        return -1;
    }
    if (sceAtPullAtNo(&w->no) == 0) {
        return -1;
    }
    if (saveNo < 0) {
        if (sceAtCheckSaveItem(id) == 1) {
            saveNo = sceAtPullItemSaveWork();
            if (saveNo >= 0) {
                SAVE_ITEM_ROOM(saveNo) = pG->room_id;
                SAVE_ITEM_TYPE(saveNo) = 0;
                SAVE_ITEM_ATNO(saveNo) = 0;
                SAVE_ITEM_ID(saveNo) = id;
                SAVE_ITEM_NUM(saveNo) = num;
                SAVE_ITEM_EFF(saveNo) = effType;
                SAVE_ITEM_POS(saveNo, 0) = (s16) (pos->x / 10.0f);
                SAVE_ITEM_POS(saveNo, 1) = (s16) (pos->y / 10.0f);
                pG->item_save[saveNo].pos[2] = (s16) (pos->z / 10.0f);
                ((SCE_AT_ITEM*) w)->item.ctrl_flag |= 8;
            } else {
                pLog->err(0, 0, "SceAtCreateItemAt(): lack save work");
            }
        } else if (saveNo == -1) {
            ((SCE_AT_ITEM*) w)->item.disappear_timer = 0x3D;
            ((SCE_AT_ITEM*) w)->item.ctrl_flag |= 0x20;
        }
    } else {
        ((SCE_AT_ITEM*) w)->item.ctrl_flag |= 8;
    }
    w->pParent = parent;
    w->parts_no = parts;
    w->be_flg = 7;
    w->hit_type |= 1;
    w->id = 3;
    w->target_type = 1;
    w->trg_type = 8;
    w->priority = 8;
    w->act_type = 0x28;
    SceAtItemAutoArea(&w->area, pos, 0.0f);
    ((SCE_AT_ITEM*) w)->item.item_num = num;
    ((SCE_AT_ITEM*) w)->item.item_id = id;
    ((SCE_AT_ITEM*) w)->item.item_flg = 0;
    ((SCE_AT_ITEM*) w)->item.auto_item_flg = 0;
    switch (effType) {
    case -1:
    case 0:
    case 6:
        ((SCE_AT_ITEM*) w)->item.eff_type = sceAtCheckItemEffectCol(id);
        break;
    default:
        ((SCE_AT_ITEM*) w)->item.eff_type = effType;
        break;
    }
    ((SCE_AT_ITEM*) w)->item.pModel = 0;
    ((SCE_AT_ITEM*) w)->item.save_no = saveNo;
    if (((SCE_AT_ITEM*) w)->item.eff_type == 8) {
        ((SCE_AT_ITEM*) w)->item.item_pos.x = pos->x;
        ((SCE_AT_ITEM*) w)->item.item_pos.y = pos->y + 1000.0f;
        ((SCE_AT_ITEM*) w)->item.item_pos.z = pos->z;
    } else {
        ((SCE_AT_ITEM*) w)->item.item_pos.x = pos->x;
        ((SCE_AT_ITEM*) w)->item.item_pos.y = pos->y + 100.0f;
        ((SCE_AT_ITEM*) w)->item.item_pos.z = pos->z;
    }
    ok = ItemGetBinTplAddr(id, &bin, &tpl) ? 1 : 0;
    if (ok == 1) {
        obj = setItemObj(bin, tpl, (Vec*) &vecZero, (Vec*) &vecZero);
        SceAtSetItemModel((SCE_AT_ITEM*) w, obj);
    }
    if (((SCE_AT_ITEM*) w)->item.eff_type != 8 && ((SCE_AT_ITEM*) w)->item.pModel != 0) {
        ((SCE_AT_ITEM*) w)->item.pModel->be_flag &= ~2;
    }
    sceAtCheckItemModelParent((SCE_AT_ITEM*) w);
    sceAtItemEffSet((SCE_AT_ITEM*) w, 0);
    AddPrim(&pS->ot[w->priority], (u32*) w);
    return w->no;
}

// Pre-allocates a save_item record for a persistent item that an enemy / event will drop later
// (`key` identifies the reservation), so the drop cannot be lost to a room change.
void SceAtReserveItemAt(cEm* pEm, Vec* pos, ITEM_ID item_id, int item_num, int item_eff, int save_no)
{
    int i;

    if (save_no >= 0) {
        return;
    }
    if (sceAtCheckSaveItem(item_id) != 1) {
        return;
    }
    for (i = 0; i < 16; i++) {
        if (SceAtSys.reserve[i].key == pEm) {
            return;
        }
    }
    for (i = 0; i < 16; i++) {
        if (SceAtSys.reserve[i].key == 0) {
            break;
        }
    }
    if (i == 16) {
        pLog->err(0, 0, "SceAtReserveItemAt(): reserve work over");
        return;
    }
    save_no = sceAtPullItemSaveWork();
    SceAtSys.reserve[i].saveNo = save_no;
    SceAtSys.reserve[i].key = pEm;
    if (save_no >= 0) {
        SAVE_ITEM_ROOM(save_no) = pG->room_id;
        SAVE_ITEM_TYPE(save_no) = 0;
        SAVE_ITEM_ATNO(save_no) = 0;
        SAVE_ITEM_ID(save_no) = item_id;
        SAVE_ITEM_NUM(save_no) = item_num;
        SAVE_ITEM_EFF(save_no) = item_eff;
        SAVE_ITEM_POS(save_no, 0) = (s16) (pos->x / 10.0f);
        SAVE_ITEM_POS(save_no, 1) = (s16) (pos->y / 10.0f);
        pG->item_save[save_no].pos[2] = (s16) (pos->z / 10.0f);
    } else {
        pLog->err(0, 0, "SceAtReserveItemAt(): save work over");
    }
}

// Releases a reservation made by SceAtReserveItemAt (the item was not dropped after all).
void SceAtCancelItemAt(cEm* pEm)
{
    int i;

    for (i = 0; i <= 15; i++) {
        if (SceAtSys.reserve[i].key == pEm) {
            memclr_asm(&pG->item_save[SceAtSys.reserve[i].saveNo], sizeof(ITEM_SAVE_WORK));
            SceAtSys.reserve[i].key = 0;
            SceAtSys.reserve[i].saveNo = 0;
            break;
        }
    }
}

// Item glow colour by item type: 5 ammo / weapons (types 1-4), 4 treasure (6), 2 recovery /
// key / money (0, 5, 7), 3 the rest; 8 for item 0x8C.
int sceAtCheckItemEffectCol(ITEM_ID item_id)
{
    ITEM_INFO info;

    if (item_id == 0x8C) {
        return 8;
    }
    itemInfo(item_id, &info);
    switch (info.type) {
    case 1:
    case 2:
    case 3:
    case 4:
        return 5;
    case 6:
        return 4;
    case 0:
    case 5:
    case 7:
        return 2;
    case 8:
        return 3;
    case 0xC:
        return 3;
    default:
        return 3;
    }
}

// 1 when the item type (5 key, 7 money) must survive a room change (save_item record).
int sceAtCheckSaveItem(u16 id)
{
    ITEM_INFO info;

    itemInfo(id, &info);
    if (info.type == 5 || info.type == 7) {
        return 1;
    }
    return 0;
}

// Never emitted (only its string literal reaches .rodata, ahead of SceAtLinkEtcDead's).
static inline void SceAtLinkEmFlag(int no)
{
    if (SceAtPtr(no) == 0) {
        pLog->err(0, 0, "SceAtLinkEmFlag(): AT NOT FOUND");
    }
}

// Links area `no` to breakable etc model `etcNo`: while it is intact the area is (on == 1)
// disabled / (0) enabled, and sceAtLink_check flips it when the model breaks. No link when the
// model is already broken.
void SceAtLinkEtcDead(int at_no, int etc_no, int on_off)
{
    cEm* em;
    SCE_AT_DATA* w;

    if (0) {
        SceAtLinkEmFlag(at_no);
    }
    w = SceAtPtr(at_no);
    if (w == 0) {
        pLog->err(0, 0, "SceAtLinkEtcDead(): AT NOT FOUND");
        return;
    }
    if (getRoomEtcBreak(etc_no, &em, 1) != 1) {
        return;
    }
    if (bitOff(*GetEtcFlgPtr(etc_no, pG->room_id))) {
        w->waiting_no = etc_no;
        w->waiting_type = 2;
        if (on_off == 1) {
            SceAtSetEnable(at_no, 0);
        } else {
            SceAtSetEnable(at_no, 1);
        }
    } else {
        w->waiting_type = 0;
        w->waiting_no = 0;
        if (on_off == 1) {
            SceAtSetEnable(at_no, 1);
        } else {
            SceAtSetEnable(at_no, 0);
        }
    }
}

// Per frame: resolves the enemy and etc-model links, updating each area once its enemy is done or
// its etc model breaks. An item still linked to a living enemy is handed to it (SceAtSetEmItem).
void sceAtLink_check()
{
    cEm* em;
    SCE_AT_DATA* w = sceAtSetOtStart();
    int flag;

    while ((w = sceAtGetOtAddr(w)) != 0) {
        switch (w->waiting_type) {
        case 0:
            break;
        case 1:
            em = GetEmPtrFromList(w->waiting_no);
            if (em != 0) {
                if (w->id == SCEAT_ID_ITEM) {
                    flag = em->checkStatus(EM_STATUS_ITEMSET) == 1;
                } else {
                    flag = em->checkStatus(EM_STATUS_ACTIVE) == 0;
                }
            } else {
                u32 d;
                int no = w->waiting_no;

                if (pG->em_list_no >= 0) {
                    d = EM_DEAD_BIT(pG->em_list_no, no >> 5) & (0x80000000 >> (no & 31));
                } else {
                    d = 0;
                }
                flag = 0;
                if (d != 0) {
                    flag = 1;
                }
            }
            if (flag == 1) {
                if (w->be_flg & 1) {
                    SceAtSetEnable(w->no, 0);
                } else if (w->id == SCEAT_ID_ITEM) {
                    if (((SCE_AT_ITEM*) w)->item.eff_type == 0 || ((SCE_AT_ITEM*) w)->item.eff_type == 6) {
                        ((SCE_AT_ITEM*) w)->item.eff_type = sceAtCheckItemEffectCol(((SCE_AT_ITEM*) w)->item.item_id);
                    }
                    if (sceAtCheckSaveItem(((SCE_AT_ITEM*) w)->item.item_id) == 0) {
                        SceAtSetEnable(w->no, 1);
                        sceAtItemFlgOn(&((SCE_AT_ITEM*) w)->item);
                        ((SCE_AT_ITEM*) w)->item.disappear_timer = 0x3D;
                        ((SCE_AT_ITEM*) w)->item.ctrl_flag |= 0x20;
                    } else {
                        SceAtSetEnable(w->no, 1);
                    }
                } else {
                    SceAtSetEnable(w->no, 1);
                }
                w->waiting_type = 0;
                w->waiting_no = 0;
            }
            if (w->id == SCEAT_ID_ITEM && bitOff(((SCE_AT_ITEM*) w)->item.ctrl_flag)) {
                em = GetEmPtrFromList(w->waiting_no);
                if (em != 0 && bitOff(((SCE_AT_ITEM*) w)->item.ctrl_flag)) {
                    SceAtSetEmItem(em, (SCE_AT_ITEM*) w);
                }
            }
            break;
        case 2: {
            cEm* etc;

            if (getRoomEtcBreak(w->waiting_no, &etc, 0) == 1) {
                if (*GetEtcFlgPtr(w->waiting_no, pG->room_id) & 1) {
                    if (w->be_flg & 1) {
                        SceAtSetEnable(w->no, 0);
                    } else {
                        SceAtSetEnable(w->no, 1);
                    }
                    w->waiting_type = 0;
                    w->waiting_no = 0;
                }
            } else {
                pLog->err(4, 0, "ITEM SET[%d] failed: ETC[%d] not found", w->no - 0x80, w->waiting_no);
            }
            break;
        }
        }
    }
}

// Hands item area `no` to enemy `em` as its drop; 0 when the area does not exist.
int SceAtSetEmItem(cEm* em, int no)
{
    SCE_AT_DATA* w = SceAtPtr(no);

    if (w == 0) {
        return 0;
    }
    return SceAtSetEmItem(em, (SCE_AT_ITEM*) w);
}

// Hands the item (id, num, flags, glow) to `em` (cEm::setItem) and clears the link. 0 without em.
int SceAtSetEmItem(cEm* em, SCE_AT_ITEM* w)
{
    if (em == 0) {
        return 0;
    }
    EM_SET_ITEM(em, w->item.item_id, w->item.item_num, w->item.item_flg, w->item.auto_item_flg, (s8) w->item.eff_type);
    w->waiting_type = 0;
    return 1;
}

// Room start: re-creates the persistent items saved for this room (type 0 records) and restores
// the contents of the ITA item areas that were changed (type 1 records).
void SceAtSetSaveItem()
{
    Vec pos;
    int i;
    SCE_AT_DATA* w;

    for (i = 0; i <= 0xFF; i++) {
        if (pG->item_save[i].room_no == 0) {
            continue;
        }
        if (pG->item_save[i].room_no != pG->room_id) {
            continue;
        }
        switch (SAVE_ITEM_TYPE(i)) {
        case 0:
            pos.x = (f32) pG->item_save[i].pos[0] * 10.0f;
            pos.y = (f32) pG->item_save[i].pos[1] * 10.0f;
            pos.z = (f32) pG->item_save[i].pos[2] * 10.0f;
            SceAtCreateItemAt(&pos, pG->item_save[i].item_id, pG->item_save[i].item_num, SAVE_ITEM_EFF(i), i, 0, -1);
            break;
        case 1:
            w = SceAtPtr(SAVE_ITEM_ATNO(i));
            U16Set(((SCE_AT_ITEM*) w)->item.item_id, pG->item_save[i].item_id);
            ((SCE_AT_ITEM*) w)->item.item_num = SAVE_ITEM_NUM(i);
            ((SCE_AT_ITEM*) w)->item.ctrl_flag |= 8;
            ((SCE_AT_ITEM*) w)->item.save_no = i;
            break;
        }
    }
}

// A free save_item record (room_no 0), -1 when all 256 are used.
int sceAtPullItemSaveWork()
{
    int i;

    for (i = 0; i < 256; i++) {
        if (pG->item_save[i].room_no == 0) {
            return i;
        }
    }
    return -1;
}

// New game: clears all save_item records.
void SceAtInitSaveItem()
{
    memclr_asm(pG->item_save, sizeof(pG->item_save));
}

// 1 when a save_item record for item `id` exists anywhere.
int SceAtCheckSaveItemId(int id)
{
    int i;

    for (i = 0; i < 256; i++) {
        if (pG->item_save[i].room_no != 0 && pG->item_save[i].item_id == id) {
            return 1;
        }
    }
    return 0;
}

// An unparented item lying inside a type 0x14 (item parent) area is attached to that area's
// parent model (items on moving platforms).
void sceAtCheckItemModelParent(SCE_AT_ITEM* w)
{
    SCE_AT_DATA* p;
    SCE_AT_DATA_ITEM* it;

    if (w->pParent != 0) {
        return;
    }
    p = sceAtSetOtStart();
    it = &w->item;
    while ((p = sceAtGetOtAddr(p)) != 0) {
        if (bitOff(p->be_flg)) {
            continue;
        }
        if (p->id != 0x14) {
            continue;
        }
        if (p->pParent == 0) {
            continue;
        }
        if (sceAtHitCheck(p, 0, &it->item_pos, &it->item_pos) != 1) {
            continue;
        }
        SceAtSetParent((SCE_AT_DATA*) w, p->pParent, 0);
    }
}

// Attaches the item's model to the area's parent model (inverse-scaled so it keeps its size).
void sceAtSetItemModelParent(SCE_AT_ITEM* w)
{
    if (w->pParent == 0) {
        return;
    }
    Vec inv = { 0.0f, 0.0f, 0.0f };
    if (w->pParent->scale.x != 0.0f) {
        inv.x = 1.0f / w->pParent->scale.x;
    }
    if (w->pParent->scale.y != 0.0f) {
        inv.y = 1.0f / w->pParent->scale.y;
    }
    if (w->pParent->scale.z != 0.0f) {
        inv.z = 1.0f / w->pParent->scale.z;
    }
    w->item.pModel->be_flag &= ~0x4000;
    w->item.pModel->pList->scale = inv;
    w->item.pModel->setParent(w->pParent, &w->item.pModel->pos, &w->item.pModel->ang);
}

// SceAtSetItemModel for area number `no`.
int SceAtSetItemModel(int no, cModel* m)
{
    SCE_AT_DATA* w = SceAtPtr(no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtSetItemModel(): AT NOT FOUND");
        return 0;
    }
    SceAtSetItemModel((SCE_AT_ITEM*) w, m);
    return 1;
}

// Makes `m` the item area's model: placed at item.pos with the record's rotation (when rot.z > 0),
// ot_type 1 for item 0xAF, parented like the area.
int SceAtSetItemModel(SCE_AT_ITEM* w, cModel* m)
{
    if (w == 0) {
        pLog->err(0, 0, "SceAtSetItemModel(): pData == NULL");
        return 0;
    }
    if (m == 0) {
        pLog->err(0, 0, "SceAtSetItemModel(): pObj == NULL");
        return 0;
    }
    w->item.pModel = m;
    if (w->item.item_id == 0xAF) {
        m->ot_type = 1;
    }
    m->pos.x = w->item.item_pos.x;
    m->pos.y = w->item.item_pos.y;
    m->pos.z = w->item.item_pos.z;
    if (w->item.open_ang > 0.0f) {
        m->ang.x = w->item.ang_x;
        m->ang.y = w->item.ang_y;
        m->ang.z = 0.0f;
    }
    m->setNoSuspend(0);
    sceAtSetItemModelParent(w);
    return 1;
}

// Creates the cEmItem of a shoot-down item (hanging items the player must shoot) at the item
// position; the lanterns 0x58 / 0x59 get a box hit volume. Returns 0 when creation fails.
int SceAtSetShootDownItem(SCE_AT_ITEM* w, void* bin, void* tpl)
{
    Vec rot = { 0.0f, 0.0f, 0.0f };
    cEmItem* em;

    if (w->item.open_ang > 0.0f) {
        rot.x = w->item.ang_x;
        rot.y = w->item.ang_y;
    }
    em = SetEmItem(bin, tpl, &w->item.item_pos, &rot, 0, 0);
    if (em == 0) {
        w->item.pModel = em;
        return 0;
    }
    switch (w->item.item_id) {
    case 0x58:
    case 0x59:
        YarareInitCube(em, 0.0f, -85.0f, 0.0f, 85.0f, 170.0f, 300.0f, 0, YAT_FLAG_ON);
        break;
    }
    em->setNoSuspend(1);
    w->item.pModel = em;
    return 1;
}

// The model of item area `no` (0 when missing or not an item area).
cModel* SceAtItemModelPtr(int at_no)
{
    SCE_AT_DATA* w = SceAtPtr(at_no);

    if (w == 0) {
        pLog->err(0, 0, "SceAtItemModelPtr(): AT NOT FOUND");
        return 0;
    }
    if (w->id != SCEAT_ID_ITEM) {
        pLog->err(0, 0, "SceAtItemModelPtr(): not ID == ITEM");
        return 0;
    }
    return ((SCE_AT_ITEM*) w)->item.pModel;
}

// May the item area fire? Not while it still hangs (flag2 bit4); yes while the pick-up zoom shows
// it (bit2); else the item must be within the eye cone / screen and not hidden by a wall.
int SceAtItemHitCheck(SCE_AT_ITEM* w, Vec* pos)
{
    Vec p;
    SCE_AT_DATA_ITEM* it = &w->item;

    if (it->ctrl_flag & 0x10) {
        return 0;
    }
    if (it->ctrl_flag & 4) {
        return 1;
    }
    if (pos == 0) {
        p = it->item_pos;
        if (w->pParent != 0) {
            Mtx mat;
            Mtx pmat;

            if (w->parts_no >= 0) {
                MTX_COPY_LATE_DST(w->pParent->getPartsPtr(w->parts_no)->mat, pmat);
            } else {
                MTX_COPY_LATE_DST(w->pParent->mat, pmat);
            }
            if (w->be_flg & 8) {
                Vec zero = { 0.0f, 0.0f, 0.0f };
                low_RotMatrix(mat, &zero);
                mat[0][3] = pmat[0][3];
                mat[1][3] = pmat[1][3];
                mat[2][3] = pmat[2][3];
            } else {
                MTX_COPY_LATE_DST(pmat, mat);
            }
            PSMTXMultVec(mat, &p, &p);
        }
    } else {
        p = *pos;
    }
    {
        Vec pl;
        Vec q;

        pl = pPL->pos;
        q = p;
        pl.y = q.y = q.y + 200.0f;
        if (EatMgr.hitCheck(&q, &pl, 0, 0, 0, 0) == 0) {
            return 1;
        }
        pl = pPL->pos;
        q = p;
        pl.y = q.y = q.y + 600.0f;
        if (EatMgr.hitCheck(&q, &pl, 0, 0, 0, 0) == 0) {
            return 1;
        }
        pl = pPL->pos;
        q = p;
        q.y += 200.0f;
        pl.y += 1800.0f;
        return EatMgr.hitCheck(&q, &pl, 0, 0, 0, 0) == 0;
    }
}

// Resolves the special item ids of the ITA data: 0x1000 places enemy 0x24 (a crow / chicken egg
// layer) instead of an item (returns 0), 0x1001 / 0x1002 roll a random item (RandomItemCk tables),
// 0x1003..0x1005 give ammo for the current character's weapons; ids below 0x1000 pass through.
// Returns 1 with the item id / count, 0 when nothing is to be placed.
int SceAtCheckSystemItemSet(u32 id, int* outId, int* outNum, Vec* pos, Vec* rot)
{
    EM_LIST d;
    int num;
    int no;

    switch (id) {
    case 0x1000:
        d.id = 0x24;
        d.type = 0;
        d.set = 0;
        d.flag = 1;
        d.pos[0] = (s16) (pos->x / 10.0f);
        d.pos[1] = (s16) (pos->y / 10.0f);
        d.pos[2] = (s16) (pos->z / 10.0f);
        if (rot->z > 0.0f) {
            d.rot[0] = (s16) (rot->x * 57.295776f) * 0x8000 / 360;
            d.rot[1] = (s16) (rot->y * 57.295776f) * 0x8000 / 360;
            d.rot[2] = 0;
        } else {
            d.rot[0] = 0;
            d.rot[1] = 0;
            d.rot[2] = 0;
        }
        d.hp = 1000;
        d.Guard_r = 1;
        d.Character = 0;
        EmSetEvent(&d);
        goto fail;
    // The fail tail is written out in both RandomItemCk arms: jump2 first cross-jumps each copy into
    // the `fail:` block (fall-through candidate) and only then finds the 0x1002 arm's `b fail` equal
    // to the 0x1001 arm's, so the 0x1001 copy of `bl; cmpwi; beq; b` survives (a shared `goto fail`
    // makes the 0x1001 arm the scanned one and keeps the 0x1002 copy).
    case 0x1001:
        if (RandomItemCk(0x10, outId, outNum, 0) != 1) {
            *outId = 0xFFFF;
            *outNum = 0;
            return 0;
        }
        break;
    case 0x1002:
        if (RandomItemCk(0x10, outId, outNum, 1) != 1) {
            *outId = 0xFFFF;
            *outNum = 0;
            return 0;
        }
        break;
    case 0x1003:
        // The switches run on the result variables (the case-5 `num = 5` of 0x1004 folds into the
        // switch register, 0x1005's arms load straight into `no`).
        no = pG->pl_type;
        switch (no) {
        default:
        case 0:
            no = 0x18;
            num = 0xF;
            break;
        case 2:
            no = 0x20;
            num = 0x64;
            break;
        case 4:
            no = 0x72;
            num = 0x14;
            break;
        case 3:
        case 5:
            *outId = 1;
            *outNum = 1;
            goto ok;
        }
        *outId = no;
        *outNum = num;
        break;
    case 0x1004:
        num = pG->pl_type;
        switch (num) {
        default:
        case 0:
            no = 0x18;
            num = 0xA;
            break;
        case 2:
            no = 7;
            num = 0xA;
            break;
        case 4:
            no = 0x72;
            num = 0xA;
            break;
        case 3:
            no = 0x20;
            num = 0x32;
            break;
        case 5:
            no = 0;
            num = 5;
            break;
        }
        *outId = no;
        *outNum = num;
        break;
    case 0x1005:
        no = pG->pl_type;
        switch (no) {
        case 0:
            no = 4;
            num = 0x14;
            break;
        case 2:
            no = 4;
            num = 0x14;
            break;
        case 4:
            no = 0xE;
            num = 1;
            break;
        case 3:
            no = 0x20;
            num = 0x19;
            break;
        case 5:
            no = 4;
            num = 0x14;
            break;
        default:
            no = 4;
            num = 0x14;
            break;
        }
        *outId = no;
        *outNum = num;
        break;
    default:
        goto other;
    }
ok:
    return 1;
other:
    // The plain-id fallback sits behind the `return 1` in the original layout.
    if (id <= 0xFFF) {
        *outId = id;
        *outNum = 0;
        goto ok;
    }
fail:
    *outId = 0xFFFF;
    *outNum = 0;
    return 0;
}

// Sets up or refreshes an item area. An item linked to an enemy or etc model waits until it dies or
// breaks and then appears where it died. Taken items and those excluded by modeMask are skipped.
void sceAtSetItem(SCE_AT_ITEM* w)
{
    ITEM_INFO info;
    Vec rot;
    cEm* em = 0;
    int id;
    int num;
    void* bin;
    void* tpl;
    SCE_AT_DATA_ITEM* it = &w->item;
    int ok = 1;
    int mask = 2;
    int r;
    int ok2;
    cObj* obj;

    if (pG->pl_type == 0) {
        mask = 1;
    }
    switch (w->waiting_type) {
    case 1: {
        u32 d;
        int no;
        cEm* p;

        p = GetEmPtrFromList(w->waiting_no);
        no = w->waiting_no;
        if (pG->em_list_no >= 0) {
            d = EM_DEAD_BIT(pG->em_list_no, no >> 5) & (0x80000000 >> (no & 31));
        } else {
            d = 0;
        }
        if (d == 0) {
            ok = 0;
        } else if (bitOff(it->ctrl_flag)) {
            if (p == 0) {
                ok = 0;
            } else {
                it->item_pos = p->pos;
            }
        }
        break;
    }
    case 2:
        if (getRoomEtcBreak(w->waiting_no, &em, 1) == 0) {
            ok = 0;
        } else if (bitOff(*GetEtcFlgPtr(w->waiting_no, pG->room_id))) {
            ok = 0;
        } else if (bitOff(it->ctrl_flag) && em != 0) {
            it->item_pos = em->pos;
            it->item_pos.y += 50.0f;
            if (it->open_ang == 0.0f) {
                RAW_F32(it, 0x30) = 0.0f;
                RAW_F32(it, 0x38) = 1.0f;
                it->ang_y = GetXZAngle(&em->pos, &pPL->pos);
            }
        }
        break;
    }
    if (sceAtItemFlgCk(it) != 0) {
        goto disable;
    }
    if ((it->player_type & mask) == 0 && it->player_type != 0) {
        goto disable;
    }
    if (ok != 1) {
        goto disable;
    }
    rot.x = it->ang_x;
    rot.y = it->ang_y;
    rot.z = it->open_ang;
    r = SceAtCheckSystemItemSet(it->item_id, &id, &num, &it->item_pos, &rot);
    if (r != 1) {
        w->be_flg &= ~1;
        sceAtItemFlgOn(it);
        return;
    }
    if (it->item_id != id) {
        int s;

        it->item_id = id;
        it->item_num = num;
        s = sceAtPullItemSaveWork();
        if (s >= 0) {
            SAVE_ITEM_ROOM(s) = pG->room_id;
            SAVE_ITEM_TYPE(s) = r;
            SAVE_ITEM_ATNO(s) = w->no;
            SAVE_ITEM_ID(s) = id;
            SAVE_ITEM_NUM(s) = num;
            SAVE_ITEM_EFF(s) = -1;
        }
    }
    w->waiting_type = 0;
    w->waiting_no = 0;
    if (!(it->ctrl_flag & 0x80)) {
        if (!(w->trg_type & 8)) {
            w->trg_type = 8;
            w->act_type = 0x28;
        } else {
            w->trg_type &= 0x7F;
        }
    } else {
        w->trg_type = 2;
    }
    if (bitOff(it->ctrl_flag)) {
        if (it->ctrl_flag & 0x10) {
            if (it->pModel != 0) {
                it->item_pos = it->pModel->pos;
            }
            if (sceAtItemFindFlgCk(it) == 1) {
                it->ctrl_flag &= ~0x10;
                it->item_pos.y = EatMgr.getFloor(&it->item_pos, 0, 0.0f, 100000.0f, 0);
                it->open_ang = 0.0f;
                it->eff_type = 2;
            }
        }
        if (it->ctrl_flag & 0x40) {
            if (sceAtItemFindFlgCk(it) == 1) {
                it->ctrl_flag &= ~0x40;
                it->item_pos.y = EatMgr.getFloor(&it->item_pos, 0, 0.0f, 100000.0f, 0);
                it->eff_type = 2;
            }
        }
    }
    if (!(it->ctrl_flag & 2)) {
        SceAtItemAutoArea(&w->area, &it->item_pos, it->radius);
    }
    if (it->eff_type == 6) {
        it->eff_type = sceAtCheckItemEffectCol(it->item_id);
    }
    if (it->pModel == 0) {
        ok2 = ItemGetBinTplAddr(it->item_id, &bin, &tpl) ? 1 : 0;
        if (ok2 == 0) {
            bin = (void*) (pG->pCore->ofs_20 + (u32) pG->pCore);
            tpl = (void*) (pG->pCore->ofs_24 + (u32) pG->pCore);
        }
        if (it->ctrl_flag & 0x10) {
            SceAtSetShootDownItem(w, bin, tpl);
            if (it->eff_setno == 0) {
                sceAtItemEffSet(w, w->item.pModel);
            }
        } else if (it->ctrl_flag & 0x40) {
            obj = setItemObj(bin, tpl, (Vec*) &vecZero, (Vec*) &vecZero);
            SceAtSetItemModel(w, obj);
            if (ok2 == 0 && it->eff_type == 0) {
                it->eff_type = 1;
            }
            ((cEm*) w->item.pModel)->dmg.m_PosFrom.y = 0.0f;
            if (it->eff_setno == 0) {
                sceAtItemEffSet(w, w->item.pModel);
            }
        } else {
            if (ok2 == 1) {
                obj = setItemObj(bin, tpl, (Vec*) &vecZero, (Vec*) &vecZero);
                SceAtSetItemModel(w, obj);
            } else if (it->eff_type == 0) {
                it->eff_type = 1;
            }
            if (it->eff_setno == 0) {
                sceAtItemEffSet(w, 0);
            }
        }
    } else {
        it->pModel->be_flag |= 2;
        if (it->eff_setno == 0) {
            sceAtItemEffSet(w, 0);
        }
    }
    sceAtCheckItemModelParent(w);
    return;

disable:
    w->be_flg &= ~1;
    if (it->pModel != 0) {
        it->pModel->be_flag &= ~2;
    }
}

// The pick-up area of an item: a cylinder of radius 2 * size (1500 default) and height 3000 from
// 2000 below `pos`.
void SceAtItemAutoArea(AREA_HIT_DATA* area, Vec* pos, f32 radius)
{
    Vec p = *pos;

    p.y -= 2000.0f;
    {
        f32 h = 3000.0f;

        if (radius == 0.0f) {
            radius = 1500.0f;
        }
        AreaDataInit(area, &p, radius + radius, h, 2);
    }
}

// Disabling an item area: glow effect gone, model hidden.
static void sceAtDeleteItem(SCE_AT_ITEM* w)
{
    SCE_AT_DATA_ITEM* it = &w->item;

    sceAtItemEffDelete(it);
    if (it->pModel != 0) {
        it->pModel->be_flag &= ~2;
    }
}

// Removes the item's glow effect (all three effect kinds under effNo).
void sceAtItemEffDelete(SCE_AT_DATA_ITEM* it)
{
    if (it->eff_type != 0 && it->eff_setno != 0) {
        EffectEspDelete(0, it->eff_setno, 0, 0);
        EffectEspgenDelete(0, it->eff_setno, 0);
        EffectEfmDelete(0, it->eff_setno, 0);
        it->eff_setno = 0;
    }
}

// Starts the item's glow effect for its effType (1 plain glow 0x21 / on a model 0x2D, 2 recovery
// 0x33 + glow, 3 0x2C, 4 treasure 0x31, 5 ammo 0x2F, 7 0x46, 8 falling 0x33 800 below, 9 0x4D) at
// item.pos + ofs (or on model `m`); items on a parent get the parts-relative variants (only the
// parts 2 / 4 / 8 cases of rooms 30F / 21B). Nothing in shooting-range mode.
void sceAtItemEffSet(SCE_AT_ITEM* w, cModel* pModel)
{
    Vec p;
    Vec q;
    SCE_AT_DATA_ITEM* it = &w->item;
    cModel* parent;
    int c;
    int kind;

    if ((s8) pG->shooting_mode != 0) {
        return;
    }
    it->eff_setno = 0;
    if (it->eff_type == 0) {
        return;
    }
    it->eff_setno = EspPullCoreKind();
    if (it->eff_setno == 0) {
        return;
    }
    parent = w->pParent;
    if (parent == 0) {
        if (pModel == 0) {
            p.x = w->item.item_pos.x + it->eff_offset.x;
            p.y = it->item_pos.y + it->eff_offset.y;
            p.z = it->item_pos.z + it->eff_offset.z;
            switch (it->eff_type) {
            case 1:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x21, 0xC00, it->eff_setno, parent, 0);
                break;
            case 3:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x2C, 0xC00, it->eff_setno, parent, 0);
                break;
            case 5:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x2F, 0xC00, it->eff_setno, parent, 0);
                break;
            case 4:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x31, 0xC00, it->eff_setno, parent, 0);
                break;
            case 2:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x33, 0xC00, it->eff_setno, parent, 0);
                EstSet(0, -1, &p, 0, EFF_CORE, 0x21, 0xC00, it->eff_setno, parent, 0);
                break;
            case 7:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x46, 0xC00, it->eff_setno, parent, 0);
                EstSet(0, -1, &p, 0, EFF_CORE, 0x21, 0xC00, it->eff_setno, parent, 0);
                break;
            case 8:
                q = p;
                q.y -= 800.0f;
                EstSet(0, -1, &q, 0, EFF_CORE, 0x33, 0xC00, it->eff_setno, parent, 0);
                EstSet(0, -1, &p, 0, EFF_CORE, 0x21, 0xC00, it->eff_setno, parent, 0);
                break;
            case 9:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x4D, 0xC00, it->eff_setno, parent, 0);
                break;
            case 6:
                break;
            }
        } else {
            if (it->eff_type == 1) {
                p.x = it->eff_offset.x;
                p.y = it->eff_offset.y;
                p.z = it->eff_offset.z;
            } else {
                p.x = pModel->pos.x + it->eff_offset.x;
                p.y = pModel->pos.y + it->eff_offset.y;
                p.z = pModel->pos.z + it->eff_offset.z;
            }
            switch (it->eff_type) {
            case 1:
                EstSet(pModel, -1, &p, 0, EFF_CORE, 0x2D, 0xC00, it->eff_setno, 0, 0);
                break;
            case 3:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x2C, 0xC00, it->eff_setno, 0, 0);
                break;
            case 5:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x2F, 0xC00, it->eff_setno, 0, 0);
                break;
            case 4:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x31, 0xC00, it->eff_setno, 0, 0);
                break;
            case 2:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x33, 0xC00, it->eff_setno, 0, 0);
                EstSet(0, -1, &p, 0, EFF_CORE, 0x21, 0xC00, it->eff_setno, 0, 0);
                break;
            case 7:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x46, 0xC00, it->eff_setno, 0, 0);
                EstSet(0, -1, &p, 0, EFF_CORE, 0x21, 0xC00, it->eff_setno, 0, 0);
                break;
            case 8:
                q = p;
                q.y -= 800.0f;
                EstSet(0, -1, &q, 0, EFF_CORE, 0x33, 0xC00, it->eff_setno, 0, 0);
                EstSet(0, -1, &p, 0, EFF_CORE, 0x21, 0xC00, it->eff_setno, 0, 0);
                break;
            case 9:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x4D, 0xC00, it->eff_setno, 0, 0);
                break;
            case 6:
                break;
            }
        }
        return;
    }
    switch (w->parts_no) {
    case -1:
    case 0:
        break;
    case 2:
        if (pG->room_id != 0x30F) {
            return;
        }
        break;
    case 4:
    case 8:
        if (pG->room_id != 0x21B) {
            return;
        }
        break;
    default:
        return;
    }
    parent = w->pParent;
    p.x = it->item_pos.x + it->eff_offset.x;
    p.y = it->item_pos.y + it->eff_offset.y;
    p.z = it->item_pos.z + it->eff_offset.z;
    kind = 0;
    c = 0;
    switch (it->eff_type) {
    case 3:
        switch (w->parts_no) {
        case -1:
        case 0:
            kind = 0x48;
            break;
        case 2:
            c = 1;
            kind = 0x1C;
            break;
        case 4:
            c = 1;
            kind = 6;
            break;
        case 8:
            c = 1;
            kind = 0xC;
            break;
        }
        EstSet(parent, -1, &p, 0, c, kind, 0xC00, it->eff_setno, 0, 0);
        break;
    case 5:
        switch (w->parts_no) {
        case -1:
        case 0:
            kind = 0x49;
            break;
        case 2:
            c = 1;
            kind = 0x1D;
            break;
        case 4:
            c = 1;
            kind = 8;
            break;
        case 8:
            c = 1;
            kind = 0xE;
            break;
        }
        EstSet(parent, -1, &p, 0, c, kind, 0xC00, it->eff_setno, 0, 0);
        break;
    case 4:
        switch (w->parts_no) {
        case -1:
        case 0:
            kind = 0x4A;
            break;
        case 2:
            c = 1;
            kind = 0x1E;
            break;
        case 4:
            c = 1;
            kind = 0xA;
            break;
        case 8:
            c = 1;
            kind = 0x10;
            break;
        }
        EstSet(parent, -1, &p, 0, c, kind, 0xC00, it->eff_setno, 0, 0);
        break;
    case 2:
        switch (w->parts_no) {
        case -1:
        case 0:
            kind = 0x4B;
            break;
        case 2:
            c = 1;
            kind = 0x1F;
            break;
        case 4:
        case 8:
            return;
        }
        EstSet(parent, -1, &p, 0, c, kind, 0xC00, it->eff_setno, 0, 0);
        // fall through
    case 1:
    case 7:
    case 8:
    case 9:
        EstSet(parent, -1, &p, 0, EFF_CORE, 0x2D, 0xC00, it->eff_setno, 0, 0);
        break;
    case 6:
        break;
    }
}

// Starts the fade-out variant of the glow (0x2E / 0x30 / 0x32 / 0x34 / 0x47 by effType) when a
// dropped item is about to disappear.
void sceAtItemDisappearEffSet(SCE_AT_ITEM* w, cModel* pModel)
{
    Vec p;
    SCE_AT_DATA_ITEM* it = &w->item;
    int c;
    int kind;

    if ((s8) pG->shooting_mode != 0) {
        return;
    }
    it->eff_setno = 0;
    if (it->eff_type == 0) {
        return;
    }
    it->eff_setno = EspPullCoreKind();
    if (it->eff_setno == 0) {
        return;
    }
    if (w->pParent == 0) {
        if (pModel == 0) {
            p.x = w->item.item_pos.x + it->eff_offset.x;
            p.y = it->item_pos.y + it->eff_offset.y;
            p.z = it->item_pos.z + it->eff_offset.z;
            switch (it->eff_type) {
            case 3:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x2E, 0xC00, it->eff_setno, pModel, 0);
                break;
            case 5:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x30, 0xC00, it->eff_setno, pModel, 0);
                break;
            case 4:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x32, 0xC00, it->eff_setno, pModel, 0);
                break;
            case 2:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x34, 0xC00, it->eff_setno, pModel, 0);
                break;
            case 7:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x47, 0xC00, it->eff_setno, pModel, 0);
                break;
            case 8:
                p.y -= 800.0f;
                EstSet(0, -1, &p, 0, EFF_CORE, 0x34, 0xC00, it->eff_setno, pModel, 0);
                break;
            case 1:
            case 6:
            case 9:
                break;
            }
        } else {
            p.x = pModel->pos.x + it->eff_offset.x;
            p.y = pModel->pos.x + it->eff_offset.y;
            p.z = pModel->pos.x + it->eff_offset.z;
            switch (it->eff_type) {
            case 3:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x2E, 0xC00, it->eff_setno, 0, 0);
                break;
            case 5:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x30, 0xC00, it->eff_setno, 0, 0);
                break;
            case 4:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x32, 0xC00, it->eff_setno, 0, 0);
                break;
            case 2:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x34, 0xC00, it->eff_setno, 0, 0);
                break;
            case 7:
                EstSet(0, -1, &p, 0, EFF_CORE, 0x47, 0xC00, it->eff_setno, 0, 0);
                break;
            case 8:
                p.y -= 800.0f;
                EstSet(0, -1, &p, 0, EFF_CORE, 0x34, 0xC00, it->eff_setno, 0, 0);
                break;
            case 1:
            case 6:
            case 9:
                break;
            }
        }
        return;
    }
    switch (w->parts_no) {
    case -1:
    case 0:
        break;
    case 2:
        if (pG->room_id != 0x30F) {
            return;
        }
        break;
    case 4:
    case 8:
        if (pG->room_id != 0x21B) {
            return;
        }
        break;
    default:
        return;
    }
    p.x = it->item_pos.x + it->eff_offset.x;
    p.y = it->item_pos.y + it->eff_offset.y;
    p.z = it->item_pos.z + it->eff_offset.z;
    kind = 0;
    c = 0;
    pModel = w->pParent;
    switch (it->eff_type) {
    case 3:
        switch (w->parts_no) {
        case -1:
        case 0:
            kind = 0x53;
            break;
        case 2:
            c = 1;
            kind = 0x20;
            break;
        case 4:
            c = 1;
            kind = 7;
            break;
        case 8:
            c = 1;
            kind = 0xD;
            break;
        }
        EstSet(pModel, -1, &p, 0, c, kind, 0xC00, it->eff_setno, 0, 0);
        break;
    case 5:
        switch (w->parts_no) {
        case -1:
        case 0:
            kind = 0x54;
            break;
        case 2:
            c = 1;
            kind = 0x21;
            break;
        case 4:
            c = 1;
            kind = 9;
            break;
        case 8:
            c = 1;
            kind = 0xF;
            break;
        }
        EstSet(pModel, -1, &p, 0, c, kind, 0xC00, it->eff_setno, 0, 0);
        break;
    case 4:
        switch (w->parts_no) {
        case -1:
        case 0:
            kind = 0x55;
            break;
        case 2:
            c = 1;
            kind = 0x22;
            break;
        case 4:
            c = 1;
            kind = 0xB;
            break;
        case 8:
            c = 1;
            kind = 0x11;
            break;
        }
        EstSet(pModel, -1, &p, 0, c, kind, 0xC00, it->eff_setno, 0, 0);
        break;
    case 2:
        switch (w->parts_no) {
        case -1:
        case 0:
            kind = 0x56;
            break;
        case 2:
            c = 1;
            kind = 0x23;
            break;
        default:
            return;
        }
        EstSet(pModel, -1, &p, 0, c, kind, 0xC00, it->eff_setno, 0, 0);
        break;
    case 1:
    case 6:
    case 7:
    case 8:
    case 9:
        break;
    }
}
