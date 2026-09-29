// game/t_flag: the debug flag editor task (FlagEdit): pages of the game's flag words (fe_data —
// debug, display, status, system, scenario, room, item flags...) shown in hex with per-bit names,
// toggled with the pad.
#include "types.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"
#include "room_data.h"
#include "t_util.h"
#include <string.h>


// Flag editor tool: pages of bit flags with names. PS2 names this task's work struct `tagTEST`/`TEST`,
// like the room-jump and debug-menu tool tasks; the three are unrelated, PS2 just reuses the name.
typedef struct tagTEST {
    s16 id;        // 0x00  fe_data index
    s16 flag_bit;  // 0x02  bit number in the page
    u16 r_no_0;    // 0x04  func_tbl index (0 = move, 1 = die)
    u32 stopflag;  // 0x08  saved pG stop flags
} TEST;

struct FE_DATA {
    const char* name;    // 0x00
    u32* addr;          // 0x04
    s16 max;            // 0x08  flag word bits on this page
    s16 start_bit;            // 0x0A  added to the bit number for display
    const char** bit_name;  // 0x0C
    u32 bit_name_size;       // 0x10
};

static void init(TEST* t);
static void move(TEST* t);
void die(TEST* t);
int CkBit(u32* flags, u32 bit);

static const char* dbg_s[125] = {
    "DBG_TEST_MODE", "DBG_SCR_TEST", "DBG_BACK_CLIP", "DBG_DBG_CAM", "DBG_SAT_DISP", "DBG_EAT_DISP",
    "DBG_EVENT_TOOL", "DBG_SLOW_ON", "DBG_SHADOW_POLYGON", "DBG_SCE_AT_DISP", "DBG_SCR2_TEST",
    "DBG_SHADOW_FRAME", "DBG_MIRROR_POLYGON", "DBG_GROUND_DISP", "DBG_SKELETON_DISP",
    "DBG_ESPTOOL_ONSCR", "DBG_CINESCO_OFF", "DBG_RTP_DISP", "DBG_ROOM_WIRE_DISP",
    "DBG_EM_YARARE_DISP", "DBG_CAM_AREA_OFF", "DBG_CLOTH_AT_DISP", "DBG_WIND_ON",
    "DBG_ESPTOOL_MEM_USE", "DBG_TEX_RENDER_ALL", "DBG_ESPTOOL_ONEM", "DBG_EMINFO_DISP",
    "DBG_LIGHT_TOOL", "", "", "", "", "DBG_COCKPIT_TOOL", "DBG_BOUNDING_DISP", "DBG_ADJUST_CAM",
    "DBG_FLAT_FLOOR", "DBG_OBJ_SKELETON_", "DBG_DRAW_SH_TEX", "DBG_EM_NO_ATK", "DBG_NO_EST_CALL",
    "DBG_IN_ESP_TOOL", "DBG_TERM_TOOL", "DBG_WARN_LEVEL_LOW", "", "", "", "", "DGG_TIMER_STOP", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "DBG_ROOMJMP", "DBG_PROC_BAR",
    "DBG_SCA_VIEW", "DBG_OBA_VIEW", "DBG_SLOW_MODE", "DBG_NO_SCE_EXE", "DBG_SINGLE_DISK",
    "DBG_BUGCHECK_MODE", "DBG_NO_DEATH", "DBG_INF_BULLET", "DBG_NO_ENEMY", "DBG_BGM_STOP",
    "DBG_SE_STOP", "DBG_PL_LOCK_FOLLOW", "DBG_EM_NO_DEATH", "DBG_KAIOUKEN", "DBG_PAD_INFO",
    "DBG_UNDER_CONST", "DBG_EM_WEAK", "DBG_EM_LIFE_DISP", "DBG_SHADOW_LIGHT", "DBG_CAPTION_OFF",
    "DBG_TEST_MODE_CK", "DBG_LIGHT_ERR_CHECK", "DBG_EST_CALL_CHK", "DBG_GX_WARN_ALL",
    "DBG_GX_WARN_MIDIUM", "DBG_GX_WARN_SEVERE", "DBG_PL_NOHIT", "DBG_SE_ERR_ALL", "DBG_5e",
    "DBG_AV_TEST", "DBG_INF_BULLET2", "DBG_BATTLE_CAM", "DBG_62", "DBG_63", "DBG_SCISSOR_OFF",
    "DBG_LOG_OFF", "DBG_SCR_CHECK", "DBG_OBJ_SERVER", "DBG_START_ST2", "DBG_ERRORL_CK",
    "DBG_APP_USE_DBMEM", "DBG_REFRACT_CK", "DBG_EM_NO_DIE_FLAG", "DBG_START_ST3", "DBG_6e",
    "DBG_DOOR_SET_MODE", "DBG_EFF_NUM_DISP", "DBG_SET_HITMARK_ALL", "DBG_FOG_FAR_GREEN", "DBG_73",
    "DBG_NO_ETC_SET", "DBG_NO_DEATH2", "DBG_NO_PARASITE", "DBG_ESP_CHK", "DBG_NO_EVENT",
    "DBG_NO_LASER_LINE", "", "DBG_SHOP_FULL", "DBG_ADA_OMAKE_EV",
};

const char* spf_s[27] = {
    "SPF_KEY", "SPF_CAMERA", "SPF_EM", "SPF_PL", "SPF_ESP", "SPF_OBJ", "SPF_CTRL", "SPF_LIGHT",
    "SPF_SCE", "SPF_SCE_AT", "SPF_CCHG", "SPF_PL_CCHG", "SPF_NOTSUBSCR", "SPF_WATER",
    "SPF_SPECULAR", "SPF_EARTHQUAKE", "SPF_VIBRATION", "SPF_CINESCO", "SPF_MIST", "SPF_SUBCHAR",
    "SPF_SE", "SPF_EVT", "SPF_BLOCK", "SPF_ACTBTN", "SPF_DATAREAD_AT", "SPF_ID_SYSTEM",
    "SPF_ESP_AREA",
};

const char* sta_s[105] = {
    "STA_BG_OFF", "STA_PL_CHECK", "STA_PL_CHECK2", "STA_MOVIE_ON", "STA_CUTCHG", "STA_MOVIE2_ON",
    "STA_SSCRN_ENABLE", "STA_CINESCO", "STA_PL_FIRE", "STA_09", "STA_ACT_DONT_FIRE", "STA_DIEDEMO",
    "STA_BLUR", "STA_SUB_SCRN", "STA_CARD_ACCESS", "STA_PAD_SENSITIVE", "STA_10", "STA_PL_ACTION",
    "STA_PL_INVISIBLE", "STA_EVENT", "STA_ASHLEY_HIDE", "STA_CAM_SHOULDER", "STA_WATER_ALIVE",
    "STA_CAMERA", "STA_BLACKOUT", "STA_SCOPE_CAMERA", "STA_RIDE_GONDOLA", "STA_1b",
    "STA_PL_JUMP_OFF", "STA_MIRROR", "STA_SAND_ALIVE", "STA_SELF_SHADOW", "STA_PL_SE_FOOT",
    "STA_PL_SE_WHISTLE", "STA_SE_BURST", "STA_SUSPEND", "STA_TEX_RENDER", "STA_THERMO_GRAPH",
    "STA_CAMERA_IN_ROOM", "STA_NO_LIGHTMASK", "STA_PL_SPEAR_SET", "STA_PL_SWIM", "STA_PL_BOAT",
    "STA_WATER_CAMERA", "STA_PL_SWIM_CAMERA", "STA_PL_LADDER", "STA_CRITICAL", "STA_TAKEAWAY",
    "STA_PL_CATCHED", "STA_SHADOW_EQCOL", "STA_PL_CATCHHOLD", "STA_NEARCLIP_TOUCH",
    "STA_CAMERA_SET_ROOM", "STA_ROOM_RAIN", "STA_USE_CAST_SHADOW", "STA_PROC_SHD_TEX",
    "STA_ALPHA_DRAW2", "STA_SET_BG_COLOR", "STA_ESPGEN45_SET", "STA_EFFEM2D_TEXRND",
    "STA_SUB_LADDER", "STA_SUBCHAR_CTRL", "STA_ITEM_GET", "STA_LASERSITE_NOADD", "STA_PL_DONT_FIRE",
    "STA_PL_EM_ACTION", "STA_SUB_CATCHED", "STA_CUT_CHANGE", "STA_NO_FENCE", "STA_SSCRN_REQUEST",
    "STA_ESP_COMPULSION_NOSUSPEND", "STA_PL_MISS_SHOT", "STA_SUB_BULLDOZER", "STA_LIT_NO_UPDATE",
    "STA_MAP_DISABLE", "STA_USE_SHADOW_LIGHT", "STA_EVENT_SYSYTEM", "STA_INTO_SHOP",
    "STA_TIMER_NO_PAUSE", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "STA_SAVEDATA_NO_UPDATE", "STA_BEHIND_CAM", "STA_62", "STA_SCISSOR", "STA_SLOW",
    "STA_SUB_ASHLEY", "STA_BIG_MARKER", "", "STA_KLAUSER_TRANSFORM",
};

const char* cfg_s[6] = {
    "CFG_AIM_REVERSE", "CFG_WIDE_MODE", "CFG_LOCK_ON", "CFG_BONUS_GET", "CFG_VIBRATION",
    "CFG_KNIFE_MODE",
};

// The EXTRA page lists cfg_s names (num_names 2), yet the original .rodata carries these two
// strings between the CFG_ and SCF_ names with no table referencing them. An unused inline
// keeps them in that spot (GCC 2.95 emits string literals at parse time).
static inline const char* ext_name(int no)
{
    return no == 0 ? "EXT_COSTUME" : "EXT_HARD_MODE";
}

const char* scf_s[64] = {
    "SCF_KEY_ID_A_GET", "SCF_KEY_ID_B_GET", "SCF_KEY_ID_C_GET", "SCF_HOOK_STALKING_R10A",
    "SCF_R10E_BATTLE_END", "SCF_R104_ACT_STATUE", "SCF_R01E_TEST", "SCF_R100_TEST00",
    "SCF_R100_TEST01", "SCF_R100_TEST02", "SCF_R106_EVENT", "SCF_R117_ASHLEY_FIND",
    "SCF_R100_DOG_RUN", "SCF_ST1_SUB_MISSION", "SCF_R11C_BESIEGED_EVENT", "SCF_R201_EVENT00",
    "SCF_R108_PUZZLE_CLEAR", "SCF_R100_KILL_GANADE_1ST", "SCF_R101_ENTER", "SCF_R103_ENTER",
    "SCF_R106_ENTER", "SCF_R106_CONFINEED_WITH_LUIS", "SCF_R108_CHECK_DOOR", "SCF_R10C_GET_CREST",
    "SCF_NO_ASHLEY_DIST_CK", "SCF_R11C_BESIEGED_END_EVENT", "SCF_R103_MANURE_RECEPTACLE",
    "SCF_R103_ITEM_IN_MANURE_RECEPTACLE", "SCF_R11B_END_SALAMANDER", "SCF_ST1_MAP_DAY",
    "SCF_ST1_MAP_NIGHT", "SCF_ST2_MAP", "SCF_ST3_MAP", "SCF_R217_PUZZLE_CLEAR", "",
    "SCF_R206_ASHLEY_RESCUE", "SCF_R101_IMPRISON", "SCF_R103_OPEN_COVER",
    "SCF_R20D_END_OF_ASHLEY_PLAY", "SCF_ST1_NIGHT", "SCF_ST2_IN", "SCF_STOCK_ST1_DAY",
    "SCF_STOCK_ST1_NIGHT", "SCF_STOCK_ST2", "SCF_R108_OPERATOR", "SCF_R204_ASHLEY_SPLIT",
    "SCF_R11C_OPERATOR", "SCF_ST3_IN", "SCF_R119_DOOR_CLOSE", "SCF_R10C_TO_R10E",
    "SCF_ST1_NIGHT_LV_ADD", "SCF_R307_REGENERATER_APPEAR", "SCF_R316_TO_R30A_CUTBACK_EVENT",
    "SCF_R30D_ENTER", "", "", "", "", "SCF_R317_LEON_WOUND", "", "SCF_R22C_BONUS_1",
    "SCF_R22C_BONUS_2", "SCF_R22C_BONUS_3", "SCF_R22C_BONUS_4",
};

const char* itf_s[16] = {
    "ITF_DUMMY", "", "", "", "", "", "", "", "", "", "", "", "", "ITF_R101_IDCARD_A",
    "ITF_R10D_IDCARD_B", "ITF_R10B_IDCARD_C",
};

static const char* dpf_s[21] = {
    "DPF_EM", "DPF_PL", "DPF_SUBCHAR", "DPF_OBJ", "DPF_SCR", "DPF_ESP", "DPF_SHADOW", "DPF_WATER",
    "DPF_MIRROR", "DPF_CTRL", "DPF_CINESCO", "DPF_FILTER\t", "DPF_GLB_ILM", "DPF_CAST_SHADOW",
    "DPF_CLOTH", "DPF_COCKPIT", "DPF_SELF_SHADOW", "DPF_FOG", "DPF_ID_SYSTEM", "DPF_ACTBTN",
    "DPF_MESSAGE",
};

static const char* sys_s[29] = {
    "SYS_OMAKE_ADA_GAME", "SYS_OMAKE_ETC_GAME", "SYS_EXCEPTION", "SYS_RENDER_END", "SYS_SP_USED",
    "SYS_SOFT_RESET", "SYS_DATA_READ", "SYS_ROOMJUMP", "SYS_INVISIBLE", "SYS_DOOR_AFTER",
    "SYS_DOORDEMO", "SYS_TRANS_STOP", "SYS_CONTINUE", "SYS_SET_BLACK", "SYS_SN_PC_READ",
    "SYS_SN_PC_READ_TOOL", "SYS_HARD_RESET", "SYS_SCREEN_SHOT", "SYS_NEW_GAME", "SYS_TYPEWRITER",
    "SYS_SCISSOR_ON", "SYS_SCREEN_STOP", "SYS_CARD_ACCESS", "SYS_LOAD_GAME", "SYS_CONTINUE_AFTER",
    "SYS_START_EVT_SKIP", "SYS_HARD_MODE", "SYS_MESSAGE_INIT", "SYS_PUBLICITY_VER",
};
const char* kyf_s[1] = {""};

static FE_DATA fe_data[12] = {
    {"DEBUG", &Global.Debug_flg[0], 128, 0, dbg_s, 125},
    {"STOP", &Global.Stop_flg, 32, 0, spf_s, 27},
    {"STATUS", &Global.Status_flg[0], 128, 0, sta_s, 105},
    {"SYSTEM", &Global.System_flg, 32, 0, sys_s, 29},
    {"ITEM_SET", &Global.Item_flg[0], 128, 0, itf_s, 16},
    {"SCENARIO", &Global.Scenario_flg[0], 256, 0, scf_s, 64},
    {"KEY_LOCK", &Global.Key_flg[0], 64, 0, kyf_s, 1},
    {"ROOM", &Global.Room_flg[0], 128, 0, NULL, 0},
    {"ROOM_SAVE", NULL, 32, 0, NULL, 0},
    {"EXTRA", &SystemSave.Extra_flg, 32, 0, cfg_s, 2},
    {"CONFIG", &SystemSave.Config_flg, 32, 0, cfg_s, 6},
    {"DISP", &Global.Disp_flg, 32, 0, dpf_s, 21},
};

TEST Test;

// Debug flag editor task: pages of the game's flag words (debug / disp / status / scenario / room
// flags... in fe_data) with the bit names; runs until B.
void FlagEdit()
{
    static void (*func_tbl[2])(TEST*) = {move, die};

    Test.stopflag = pG->Stop_flg;
    BitOn(pG->Stop_flg, ~0x4000);
    init(&Test);
    TaskSleep(1);
    while (1) {
        pG->Stop_flg = Test.stopflag;
        func_tbl[Test.r_no_0](&Test);
        Test.stopflag = pG->Stop_flg;
        BitOn(pG->Stop_flg, ~0x4000);
        TaskSleep(1);
    }
}

// Editor start: page 0, cursor 0, the game frozen (Stop_flg saved).
static void init(TEST* t)
{
    Test.r_no_0 = 0;
    t->flag_bit = 0;
    t->id = 0;
    DbgFlagOn(pG, DBG_TEST_MODE);
}

// Editor frame: d-pad moves the cursor bit, C-stick / L / R change the page, A toggles the bit;
// prints the page's words in hex, the cursor bit's value, number and name. B -> die.
static void move(TEST* pTest)
{
    JOY* joy = GetBugCheckController();
    FE_DATA* p;
    int i;
    int line;
    int a, b, c, d;
    u16 w;
    int bit = 0;
    u32 cur;
    s16 sh;

    if (joy->rep & JOY_RIGHT) {
        pTest->flag_bit++;
    }
    if (joy->rep & JOY_LEFT) {
        pTest->flag_bit--;
    }
    if (joy->rep & JOY_UP) {
        pTest->flag_bit -= 16;
    }
    if (joy->rep & JOY_DOWN) {
        pTest->flag_bit += 16;
    }
    if (joy->rep & 0x20000) {
        pTest->flag_bit++;
    } else if (joy->rep & 0x10000) {
        pTest->flag_bit--;
    } else if (joy->rep & 0x80000) {
        pTest->flag_bit -= 16;
    } else if (joy->rep & 0x40000) {
        pTest->flag_bit += 16;
    }
    if (pTest->flag_bit >= fe_data[pTest->id].max) {
        pTest->flag_bit -= fe_data[pTest->id].max;
        pTest->id++;
        if (pTest->id > 11) {
            pTest->id = 0;
        }
    }
    if (pTest->flag_bit < 0) {
        pTest->id--;
        if (pTest->id < 0) {
            pTest->id = 11;
        }
        pTest->flag_bit += fe_data[pTest->id].max;
    }
    if (joy->rep & JOY_R) {
        pTest->flag_bit = 0;
        pTest->id++;
        if (pTest->id > 11) {
            pTest->id = 0;
        }
    }
    if (joy->rep & JOY_L) {
        pTest->flag_bit = 0;
        pTest->id--;
        if (pTest->id < 0) {
            pTest->id = 11;
        }
    }
    if (strcmp(fe_data[pTest->id].name, "ROOM_SAVE") == 0) {
        if (RoomData.getRoomSavePtr(G_ROOM_ID) != NULL) {
            fe_data[pTest->id].addr = (u32*) (RoomData.getRoomSavePtr(G_ROOM_ID) + 4);
        } else {
            fe_data[pTest->id].addr = NULL;
        }
    }
    p = &fe_data[pTest->id];
    if (fe_data[pTest->id].addr != NULL) {
        for (i = 0; i < p->max / 16; i++) {
            w = ((u16*) p->addr)[i];
            a = BtoX(w >> 12);
            b = BtoX((w >> 8) & 0xF);
            c = BtoX((w >> 4) & 0xF);
            d = BtoX(w & 0xF);
            line = i / 4 + 6;
            eprintf(184, (i + line) * 14, 0, 0, "%04x %04x %04x %04x", a, b, c, d);
        }
        bit = pTest->flag_bit;
        cur = bit;
        {
            // COMPILER-DIFF: #17. The original keeps `cur & 0xF` in scratch r11, untied from x's
            // chain (`add r29,r11,r9`), so y is allocated first (r30) and x second (r29); ours ties
            // the mask into x (r30) and gives y r29. With the mask pinned, `cur >> 6` still loses r0
            // to the mask's dependents in sched1 (m has two consumers, u one) unless it is pinned
            // too. Both pins are codeless.
            register u32 m asm("r11");
            register u32 u asm("r0");
            u = cur >> 6;
            int y = ((cur >> 4) + u + 6) * 14;
            m = cur & 0xF;
            int x = (m + (m >> 2) + 23) * 8;
            eprintf(x, y, 2, 0, "%01x", CkBit(p->addr, cur));
        }
        bit = pTest->flag_bit;
        if (joy->trg & JOY_A) {
            sh = bit % 32;
            p->addr[bit / 32] ^= 0x80000000 >> sh;
        }
    }
    bit += p->start_bit;
    eprintf(184, 56, 6, 0, "[%s]", p->name);
    eprintf(184, 70, 0, 0, " (0x%02x)", bit, bit);
    if ((u32) bit < p->bit_name_size) {
        eprintf(264, 70, 4, 0, "%s", p->bit_name[bit]);
    }
    if (joy->trg & JOY_B) {
        pTest->r_no_0++;
    }
}

// Editor end: restores Stop_flg, ends the task.
void die(TEST* p)
{
    DbgFlagOff(pG, DBG_TEST_MODE);
    pG->Stop_flg = Test.stopflag;
    TaskSignal(0);
    TaskExit();
}

// Value (0 / 1) of bit `bit` in a big-endian bit array (bit 0 = 0x80000000 of word 0).
int CkBit(u32* pDat, u32 bit)
{
    pDat += bit / 32;
    if (*pDat & (0x80000000 >> (bit % 32))) {
        return 1;
    }
    return 0;
}

// The split object carries the 8-byte .sdata alignment of the following unit.
asm(".section .sdata; .balign 8");
