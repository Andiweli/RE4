#ifndef SCE_AT_H
#define SCE_AT_H

#include "types.h"
#include "vec.h"
#include "area.h"
#include "scheduler.h"
#include "item.h"
#include "cFlag.h"

class cObj;
class cModel;
class cEm;
class cSat;

// Scenario trigger areas ("AT", game/sce_at.cpp): the room's AEV/ITA data records and the
// runtime-created ones, linked into an ordering table by their x44 priority.

// Payload structs of the area records. Layout is the GC one (12-byte Vec where PS2 has a 16-byte one, so
// PS2's split x/y/z floats are a Vec here); field names and order are PS2's.

// Item area payload (SCE_AT_ITEM + 0x5C, type 3).
struct SCE_AT_DATA_ITEM {
    Vec item_pos;         // 0x00 (0x5C)
    cModel* pModel;       // 0x0C (0x68)  item model: a cObj (setItemObj) or a cEmItem (shoot-down item)
    Vec eff_offset;       // 0x10 (0x6C)  effect offset
    u16 item_id;          // 0x1C (0x78)
    u16 item_flg;         // 0x1E (0x7A)  ITEM_SET flag (pG->Item_flg), 0 = use the room save flag
    u16 item_num;         // 0x20 (0x7C)
    u16 auto_item_flg;    // 0x22 (0x7E)  room save flag of the "found" state (0 = none)
    u8 eff_type;          // 0x24 (0x80)  item glow effect colour (sceAtCheckItemEffectCol)
    u8 eff_setno;         // 0x25 (0x81)  running effect number (EspPullCoreKind), 0 = none
    u8 player_type;       // 0x26 (0x82)  game modes the item exists in (0 = all)
    u8 pos_set;           // 0x27 (0x83)  bit1 model loaded by itemZoom, bit2 model shown for the zoom, bit3 model kept in p_imodel_bak
    u8 ctrl_flag;         // 0x28 (0x84)  bit0 handed to an enemy, bit1 no auto area, bit2 no hit check, bit3 save work,
                          //              bit4 shoot-down item, bit5 disappear timer running, bit6 dropped item, bit7 no button
    u8 disappear_timer;   // 0x29 (0x85)  halves of a second
    s16 save_no;          // 0x2A (0x86)  pG->save_item index, -1 = none
    f32 radius;           // 0x2C (0x88)  auto area radius (0 = 100)
    f32 ang_x;            // 0x30 (0x8C)  model rotation
    f32 ang_y;            // 0x34 (0x90)
    f32 open_ang;         // 0x38 (0x94)  != 0 (> 0) = apply the rotation
    u8 se_no;             // 0x3C (0x98)  SE when the item is found
    u8 se_no2;            // 0x3D (0x99)  SE when the item is shot
    u8 padd01[2];
};

// Door area payload (type 1).
struct SCE_AT_DATA_DOOR {
    Vec next_pos;         // 0x00 (0x5C)  destination position
    f32 next_ang_y;       // 0x0C (0x68)
    u8 next_stage_no;     // 0x10 (0x6C)
    u8 next_room_no;      // 0x11 (0x6D)
    u8 key_id;            // 0x12 (0x6E)  1 locked, 2 locked until the flag is set (KEY_ID)
    u8 key_flg;           // 0x13 (0x6F)  pG->flags_51DC bit
    TaskFunc pExitFunc;   // 0x14 (0x70)  SceSys.x10 / x14 handed over at the jump (SceAtSetDoorFunc)
    u8 next_part_no;      // 0x18 (0x74)  pG->Part in the destination room
    s8 key_se;            // 0x19 (0x75)  locked door SE
    u8 open_se;           // 0x1A (0x76)  pG->door_se
    u8 fade_eff;          // 0x1B (0x77)  SceSys.m_door_fade_eff (2 = execute now, SceChapterEnd)
    void* pExitParam;     // 0x1C (0x78)
};

// Models inside the frame (type 0).
struct SCE_AT_DATA_NORMAL {
    cModel* pModel[16];   // 0x00 (0x5C)
};

// Camera control area payload (type 0xC).
struct SCE_AT_DATA_CAM_CTRL {
    Vec pos;              // 0x00 (0x5C)
    f32 ang_y;            // 0x0C (0x68)
    u8 pos_set;           // 0x10 (0x6C)  pos / ranges initialised (t_sce_at)
    u8 type;              // 0x11 (0x6D)  0 heading, 1 direction to pos
    u8 padd[2];
    f32 radius;           // 0x14 (0x70)
    f32 out_range;        // 0x18 (0x74)  added to radius while this area is the current one
};

// Ladder area payload (type 0x10).
struct SCE_AT_DATA_LADDER {
    Vec pos;              // 0x00 (0x5C)
    f32 ang_y;            // 0x0C (0x68)
    s8 height;            // 0x10 (0x6C)
    u8 type;              // 0x11 (0x6D)
    u8 pos_set;           // 0x12 (0x6E)
    u8 cam_no;            // 0x13 (0x6F)  camera cuts + 1 (0 = none)
    u8 cam_no2;           // 0x14 (0x70)
    u8 cam_no3;           // 0x15 (0x71)
};

// Runtime scenario collision payload (type 0xB).
struct SCE_AT_DATA_SCR_AT {
    cSat* pSat;           // 0x00 (0x5C)
    cSat* pEat;           // 0x04 (0x60)
    u8 set_flg;           // 0x08 (0x64)
    u8 pad[3];
    int sat_attr;         // 0x0C (0x68)
    int eat_attr;         // 0x10 (0x6C)
    u32 ctrl_flag;        // 0x14 (0x70)  bit0 no EatMgr piece, bit1 no SatMgr piece, bit2 keep attr
    int sat_flag;         // 0x18 (0x74)
};

// Field info payload (type 0xD, SceAtCreateFieldAt; emwindow reads it through SceAtCheckFieldInfo).
struct SCE_AT_DATA_FIELD_INFO {
    int id;               // 0x00 (0x5C)  0 = the model inside gets State.SetInRoom(1)
    cModel* pParent;      // 0x04 (0x60)  creator
    u32 free[10];         // 0x08 (0x64)
};

// Damage area payload (type 0xA).
struct SCE_AT_DATA_DAMAGE {
    int dmg_timer;        // 0x00 (0x5C)  frames (0 = 1); its low byte doubles as the setDamage 5th argument
    u8 dmg_type;          // 0x04 (0x60)
    u8 dmg_ctrl;          // 0x05 (0x61)  bit0, bit1 use dmg_ang
    u8 pad[2];
    int dmg_vol;          // 0x08 (0x64)
    f32 dmg_ang;          // 0x0C (0x68)
};

// Hide area payload (type 0x12).
struct SCE_AT_DATA_HIDE {
    u8 type;              // 0x00 (0x5C)  SCEAT_HIDE_TYPE
    u8 pos_set;           // 0x01 (0x5D)
    u8 area_set;          // 0x02 (0x5E)
    u8 status;            // 0x03 (0x5F)
    f32 hide_area[4][2];  // 0x04 (0x60)
    Vec pos;              // 0x24 (0x80)
    void (*func)(int);    // 0x30 (0x8C)  SceAtDataSet_hide
    u8 cam_no;            // 0x34 (0x90)
};

// Flag area payload (type 4).
struct SCE_AT_DATA_FLG {
    u8 flg_id;            // 0x00 (0x5C)  0 event flag, 1 room save flag, 2 pG->flags_51BC (SCEAT_FLG_KIND)
    u8 flg_act;           // 0x01 (0x5D)  1 = clear
    u16 flg_no;           // 0x02 (0x5E)
};

// Shadow display area payload (type 9).
struct SCE_AT_DATA_SHD_DISP {
    u16 shd_no;           // 0x00 (0x5C)
    u8 disp_flg;          // 0x02 (0x5E)
    u8 set_flg;           // 0x03 (0x5F)
};

// Special key area payload (type 0xF, SceAtDataSet_exec fills it).
struct SCE_AT_DATA_SKEY {
    void* pParam;         // 0x00 (0x5C)
    TaskFunc pFunc;       // 0x04 (0x60)
    u8 task_level;        // 0x08 (0x64)
    u8 kind;              // 0x09 (0x65)
};

// Message request handed to SceAtSetMes (sce_com SceUpCut), 0xC bytes (type 5 payload).
struct SCE_AT_DATA_MES {
    s16 mes_type;         // 0x00
    s16 mes_no;           // 0x02  < 0: no message
    u8 cam_no;            // 0x04  camera cut + 1 (CamCtrl.CutCall)
    u8 se_type;           // 0x05  SE block select (0: SndCall block 6, else block 0)
    u16 se_no;            // 0x06  SE + 1
    u8 attr;              // 0x08  bit2: keep the camera cut after the message
    u8 padd[3];
};

// Item use payload (type 0x11): the item that becomes usable from the inventory.
struct SCE_AT_DATA_USE {
    u16 pad_0;
    u16 use_id;           // 0x02 (0x5E)  low half of PS2's u32 use_id
};

// Position jump payload (type 0x13).
struct SCE_AT_DATA_POS_JUMP {
    Vec dest_pos;         // 0x00 (0x5C)
    f32 dest_ang_y;       // 0x0C (0x68)
    u8 pos_set;           // 0x10 (0x6C)
};

// Save point payload (type 8).
struct SCE_AT_DATA_SAVE {
    u32 term_no;          // 0x00 (0x5C)  CardSave argument
};

// Area type (PS2 SCEAT_ID): SCE_AT_DATA::type, the row of sceAtFunc_tbl. SCEAT_ID_ADA_WIRE is PS2-only.
enum SCEAT_ID {
    SCEAT_ID_NORMAL = 0,
    SCEAT_ID_DOOR = 1,
    SCEAT_ID_EXEC = 2,
    SCEAT_ID_ITEM = 3,
    SCEAT_ID_FLG = 4,
    SCEAT_ID_MES = 5,
    SCEAT_ID_PLANTER = 6,
    SCEAT_ID_JUMP = 7,
    SCEAT_ID_SAVE = 8,
    SCEAT_ID_SHD_DISP = 9,
    SCEAT_ID_DAMAGE = 10,
    SCEAT_ID_SCR_AT = 11,
    SCEAT_ID_CAM_CTRL = 12,
    SCEAT_ID_FIELD_INFO = 13,
    SCEAT_ID_STOOP = 14,
    SCEAT_ID_SKEY = 15,
    SCEAT_ID_LADDER = 16,
    SCEAT_ID_USE = 17,
    SCEAT_ID_HIDE = 18,
    SCEAT_ID_POS_JUMP = 19,
    SCEAT_ID_ITEM_PARENT = 20,
    SCEAT_ID_ADA_WIRE = 21,
    SCEAT_ID_MAX = 22
};

// Countries an area is disabled in (SCE_AT_DATA::country, applied at room start).
enum SCEAT_COUNTRY {
    SCEAT_COUNTRY_USA = 0,
    SCEAT_COUNTRY_JPN = 1
};

// The fields every area record starts with, 0x5C bytes (PS2 SCE_AT_UNIT: 0x60, with a u32 pad32 at 0x5C).
struct SCE_AT_UNIT {
    u32 tag;          // 0x00  OTag link
    AREA_HIT_DATA area;    // 0x04 .. 0x34
    u8 be_flg;        // 0x34  bit0 enabled, bit1 set, bit2 allocated (SceAtCreate*), bit3 parent rotation ignored (SCEAT_BE_FLAG)
    u8 id;            // 0x35  SCEAT_ID area type (index into sceAtFunc_tbl)
    u8 no;            // 0x36  area number (SceAtPtr key; ITA records + 0x80)
    u8 hit_type;      // 0x37  bit0 test the front point instead of the position, bit1 angle check
    u8 trg_type;      // 0x38  hit state bits that fire the area (1/2/4), bit3 (8) action button, bit7 disable after use
    u8 target_type;   // 0x39  who may trigger it: 1 player, 2 enemy, 8 partner (SceAtCheck type mask)
    u8 task_level;    // 0x3A  SceExec priority (0 = call func directly)
    u8 trg_type_bak;  // 0x3B  trg_type saved by SceAtDataSet_exec
    void* pParam;     // 0x3C
    TaskFunc pFunc;   // 0x40
    u8 priority;      // 0x44  ordering table index (0..15) passed to SceExec / ActBtn.set
    u8 kind;          // 0x45  SceExec flag
    u8 waiting_type;  // 0x46  1 enemy list entry, 2 etc model (SCEAT_WAITING_TYPE)
    u8 waiting_no;    // 0x47
    s8 hit_dir_ang;   // 0x48  * 2 degrees
    s8 hit_open_ang;  // 0x49  * 2 degrees
    u8 act_type;      // 0x4A  action button kind (ActBtn.set)
    u8 waiting_etc_id;  // 0x4B
    cModel* pParent;  // 0x4C
    s16 parts_no;     // 0x50  -1 = the model itself
    cFlag<u8, SCEAT_COUNTRY> country;  // 0x52
    u8 act_color;     // 0x53  action button colour (1 = alternate, SCEAT_ACT_COL)
    u8 pad2[8];
};

// One AEV area record (0x9C bytes; PS2 SCE_AT_DATA is 0xA0).
struct SCE_AT_DATA : SCE_AT_UNIT {
    union {
        u8 data[0x40];
        SCE_AT_DATA_DOOR door;
        SCE_AT_DATA_NORMAL normal;
        SCE_AT_DATA_FLG flg;
        SCE_AT_DATA_MES mes;
        SCE_AT_DATA_SHD_DISP shd_disp;
        SCE_AT_DATA_DAMAGE damage;
        SCE_AT_DATA_SCR_AT scr_at;
        SCE_AT_DATA_CAM_CTRL cam_ctrl;
        SCE_AT_DATA_FIELD_INFO field;
        SCE_AT_DATA_SKEY skey;
        SCE_AT_DATA_LADDER ladder;
        SCE_AT_DATA_USE use;
        SCE_AT_DATA_HIDE hide;
        SCE_AT_DATA_POS_JUMP pos_jump;
        SCE_AT_DATA_SAVE save;
    };
};

// One ITA item record (0x9C bytes; PS2 SCE_AT_ITEM is 0xB0).
struct SCE_AT_ITEM : SCE_AT_UNIT {
    SCE_AT_DATA_ITEM item;   // 0x5C
};

void SceAtInit(void* pHeader, void* pHeader_i);
SCE_AT_DATA* sceAtSetOtStart();
SCE_AT_DATA* sceAtGetOtAddr(SCE_AT_DATA* p);
void SceAtClearHitFlg();
void SceAtSetHitFlg(u32 at_no);
void SceAtClearExecFlg();
void SceAtSetExecFlg(u32 at_no);
void SceAtWorkLoopInit();
void SceAtCheck();
int sceAtCheck_main(cEm* em, int target_type);
void sceAtGetArea(AREA_HIT_DATA* ret_area, SCE_AT_DATA* w);
int sceAtHitCheck(SCE_AT_DATA* w, cModel* pModel, Vec* pos_f, Vec* pos);
int CheckAshleyActive();
int CheckDoorJumpWithAshley();
int itemZoom(SCE_AT_ITEM* w);
void releaseModel(SCE_AT_ITEM* w, int disp_flg);
void SceAtSetMes(SCE_AT_DATA_MES* pMes);
void sceAtFunc_shd_disp_reverse(SCE_AT_DATA* w);
void sceAtLadder(SCE_AT_DATA* w);
void sceAtGetLadderPos(SCE_AT_DATA_LADDER* ladder, Vec* pos, f32* ladder_ang);
int sceAtCheckLadderUp(SCE_AT_DATA_LADDER* ladder, cModel* pEm);
void SceAtDataSet_hide(int no, void (*func)(int));
int SceAtCheckHideActive();
void SceAtCheckHideProc();
void SceAtStopSemiautoCheck();
void SceAtRoomSet();
void sceAtSetScrAt(SCE_AT_DATA* w);
void sceAtDeleteScrAt(SCE_AT_DATA* w);
void SceAtCheckMoveScrAt();
SCE_AT_DATA* SceAtPtr(int at_no);
int sceAtPullAtNo(u8* out);
void SceAtSetDoorFunc(int no, TaskFunc func, void* arg);
// Area `no`: run `func(obj)` (prio, otPrio) when the player enters it.
enum SCE_LEVEL {
    SCE_NO_TASK = 0,
    SCE_LEVEL_EV = 7,
    SCE_LEVEL00 = 8,
    SCE_LEVEL01 = 9,
    SCE_LEVEL02 = 10,
    SCE_LEVEL03 = 11,
    SCE_LEVEL04 = 12,
    SCE_LEVEL05 = 13,
    SCE_LEVEL06 = 14,
    SCE_LEVEL07 = 15,
    SCE_LEVEL08 = 16,
    SCE_LEVEL09 = 17,
    SCE_LEVEL10 = 18,
    SCE_LEVEL11 = 19,
    SCE_LEVEL_ANY = 20
};

void SceAtDataSet_exec(int no, int prio, int a, TaskFunc func, void* obj, int b);
void SceAtDataReset(int at_no);
void SceAtSetEnable(int at_no, int sw);
int SceAtHitCheck(u32 at_no);
void SceAtExecute(int at_no);
int SceAtCheckHitModel(int at_no, cModel* pModel);
void SceAtSetActColor(int at_no, int col);
void SceAtGetCenterPos(Vec* ret_pos, int at_no);
int SceAtSetParent(SCE_AT_DATA* w, cModel* parent, int flag);
int InScreenCheck(Vec* pos);
void SceAtExecRoomJump(u16 room, Vec* pos, Vec* rot, int a);
SCE_AT_DATA_FIELD_INFO* SceAtCheckFieldInfo(Vec* pos);
int SceAtCheckLadder(cModel* pEm, Vec* pos, f32* ladder_ang, u8* ladder_height);
int SceAtSearchLadder(cModel* pEm, Vec* pos, f32* ladder_ang, u8* ladder_height);
void SceAtDataEyeTriggreCopy(AREA_HIT_DATA* area, SCE_AT_DATA* w);
void SceAtItemFlgOn(u16 item_flg, u16 saveFlagNo);
int SceAtItemFlgCk(u16 item_flg, u16 saveFlagNo);
int SceAtItemFindFlgCk(int at_no);
void sceAtItemFlgOn(SCE_AT_DATA_ITEM* it);
int sceAtItemFlgCk(SCE_AT_DATA_ITEM* it);
void sceAtItemFindFlgOn(SCE_AT_DATA_ITEM* it);
int sceAtItemFindFlgCk(SCE_AT_DATA_ITEM* it);
int SceAtDestroy(int at_no);
// Area of the four corners `pos` around `m`: (x37, x38, x39, height, x44, angle, angle range, x4A, prio, func, arg, flag).
int SceAtCreateExecAt(cModel* m, Vec* pos, f32 h, int a, int b, int c, int d, f32 ang, f32 range, int e, int prio, TaskFunc func, void* arg, u8 flag);
int SceAtCreateFieldAt(cModel* m, Vec* pos, f32 h, int a, int b, int c, int d, f32 ang, f32 range, int e, int val, SCE_AT_DATA_FIELD_INFO** out);
int SceAtCreateItemAt(Vec* pos, ITEM_ID id, int num, int effType, int saveNo, cModel* parent, int parts);
void SceAtReserveItemAt(cEm* pEm, Vec* pos, ITEM_ID item_id, int item_num, int item_eff, int save_no);
void SceAtCancelItemAt(cEm* pEm);
int sceAtCheckItemEffectCol(ITEM_ID item_id);
int sceAtCheckSaveItem(u16 id);
void SceAtLinkEtcDead(int at_no, int etc_no, int on_off);
void sceAtLink_check();
int SceAtSetEmItem(cEm* em, int no);
void SceAtSetSaveItem();
int sceAtPullItemSaveWork();
void SceAtInitSaveItem();
int SceAtCheckSaveItemId(int id);
void sceAtCheckItemModelParent(SCE_AT_ITEM* w);
void sceAtSetItemModelParent(SCE_AT_ITEM* w);
int SceAtSetItemModel(int no, cModel* m);
int SceAtSetShootDownItem(SCE_AT_ITEM* w, void* bin, void* tpl);
cModel* SceAtItemModelPtr(int at_no);
int SceAtItemHitCheck(SCE_AT_ITEM* w, Vec* pos);
int SceAtCheckSystemItemSet(u32 id, int* outId, int* outNum, Vec* pos, Vec* rot);
void sceAtSetItem(SCE_AT_ITEM* w);
void SceAtItemAutoArea(AREA_HIT_DATA* area, Vec* pos, f32 radius);
void sceAtItemEffDelete(SCE_AT_DATA_ITEM* it);
void sceAtItemEffSet(SCE_AT_ITEM* w, cModel* pModel);
void sceAtItemDisappearEffSet(SCE_AT_ITEM* w, cModel* pModel);

// Overloads of the functions above.
// Area `no` follows parts `parts` of `obj`; 0 when the area does not exist.
int SceAtSetParent(int no, cModel* parent, int parts);
int SceAtItemFlgCk(int at_no);
int SceAtSetEmItem(cEm* em, SCE_AT_ITEM* w);
int SceAtSetItemModel(SCE_AT_ITEM* w, cModel* m);

#endif
