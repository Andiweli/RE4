#ifndef ID_SYS_H
#define ID_SYS_H

#include "types.h"
#include "vec.h"
#include "hermite.h"

// Screen id (widget) unit (game/id_sys.cpp), 0x138 bytes.
struct ID_UNIT {
    u8 be_flag;        // 0x00  0xFF: free; 0x01: alive, 0x02: just set, 0x04: move, 0x08: visible, 0x10: drawing
    u8 unitNo;       // 0x01  own number (parent lookup key)
    u8 classNo;         // 0x02  id table type (IDSystem::set parameter)
    u8 markNo;           // 0x03  unit id inside the table
    u8 type;         // 0x04  1: group (children follow)
    u8 levelNo;        // 0x05  depth in the parent tree
    u8 parentNo;     // 0x06
    u8 rowNo;           // 0x07
    Mtx mat;         // 0x08  world matrix
    Mtx l_mat;    // 0x38
    ID_UNIT* pParent;  // 0x68
    u8 texId;        // 0x6C
    u8 maskId;       // 0x6D
    u8 texNo;           // 0x6E  texture frame (stage: digit)
    u8 maskNo;       // 0x6F  mask texture frame
    u8 tex_ptn_no;       // 0x70
    u8 mask_ptn_no;      // 0x71
    u16 timer[4];    // 0x72  (converted as s16)  path / scale / color / rotation curve times
    u8 vtxType;      // 0x7A  low nibble: anchor (IdCalcVertex)
    u8 loop_flag;         // 0x7B  bit n: timer n loops
    u8 size_flag;    // 0x7C  0x10: scale x only, 0x20: y only
    u8 rot_flag;      // 0x7D
    u8 rev_flag;          // 0x7E  bit n: timer n counts up
    u8 tex_flag;     // 0x7F  0x01: mask texture, 0x02: no texture animation, 0x04: no mask animation
    u8 otType;           // 0x80
    u8 otNo;         // 0x81
    u8 trans_type;    // 0x82  0: common, 1: negative, 2/3: shimmer
    u8 pow;     // 0x83
    u8 blend_type;    // 0x84
    u8 anima_state;          // 0x85  bit n: timer n finished
    u8 pad_86[2];
    Vec pos0;         // 0x88  screen position
    Vec pos;         // 0x94  path offset + scr (world position used for drawing)
    Vec ver[4];      // 0xA0
    f32 size_W;       // 0xD0
    f32 size_H;       // 0xD4
    u8 pad_D8[8];
    u8 col0[4];      // 0xE0
    u8 col1[4];      // 0xE4
    f32 col[4];      // 0xE8
    Vec rot0;         // 0xF8
    Vec rot;         // 0x104
    f32 u0;          // 0x110
    f32 u1;          // 0x114
    f32 v0;          // 0x118
    f32 v1;          // 0x11C
    void* path0;     // 0x120  FuncPath data
    void* path1;     // 0x124
    Hermite1* curve[4];  // 0x128
};

// One entry of an id data table (IDSystem::set): ID_DATA_V1 is 0x88 bytes, ID_DATA_V2 0x8C.
struct ID_DATA_V1 {
    u8 pad_0[3];
    u8 flags;        // 0x03
    u8 id;           // 0x04
    u8 no;           // 0x05
    u8 level;        // 0x06
    u8 parentNo;     // 0x07
    u8 rowNo;        // 0x08  -> ID_UNIT::rowNo
    u8 kind;         // 0x09
    u8 Id;           // 0x0A  the game does not read it
    u8 texId;        // 0x0B
    u8 vtxType;      // 0x0C
    u8 loop;         // 0x0D
    u8 scaleType;    // 0x0E
    u8 rotAxis;      // 0x0F
    u8 dir;          // 0x10
    u8 pad_11[3];
    Vec pos;         // 0x14
    Vec vtx[4];      // 0x20
    f32 sizeX;       // 0x50
    f32 sizeY;       // 0x54
    u8 col0[4];      // 0x58
    Vec rot;         // 0x5C
    u8 blendType;    // 0x68
    u8 transType;    // 0x69
    u8 maskId;       // 0x6A
    u8 flags_7F;     // 0x6B
    u8 transSub;     // 0x6C
    u8 pad_6D[3];
    u32 ofs[6];      // 0x70  path0, path1, curve[4] (offsets from the table start, 0 = none)
};

struct ID_DATA_V2 {
    u8 pad_0[3];
    u8 flags;        // 0x03
    u8 id;           // 0x04
    u8 no;           // 0x05
    u8 level;        // 0x06
    u8 parentNo;     // 0x07
    u8 rowNo;        // 0x08  -> ID_UNIT::rowNo
    u8 kind;         // 0x09
    u8 Id;           // 0x0A  the game does not read it
    u8 texId;        // 0x0B
    u8 vtxType;      // 0x0C
    u8 loop;         // 0x0D
    u8 scaleType;    // 0x0E
    u8 rotAxis;      // 0x0F
    u8 dir;          // 0x10
    u8 pad_11[3];
    Vec pos;         // 0x14
    Vec vtx[4];      // 0x20
    f32 sizeX;       // 0x50
    f32 sizeY;       // 0x54
    u8 col0[4];      // 0x58
    u8 col1[4];      // 0x5C
    Vec rot;         // 0x60
    u8 blendType;    // 0x6C
    u8 transType;    // 0x6D
    u8 maskId;       // 0x6E
    u8 flags_7F;     // 0x6F
    u8 transSub;     // 0x70
    u8 pad_71[3];
    u32 ofs[6];      // 0x74
};

// Id data table header (IDSystem::set's argument), entries from 0x08.
typedef struct _ID_FILE_HEADER {
    char Version[4];  // 0x00  "1.00" / "2.00"
    u8 GroupNo;       // 0x04
    u8 UnitNum;       // 0x05
    u8 dummy[2];      // 0x06
} ID_FILE_HEADER;

// Id class (PS2 ID_CLASS): the `type` / classNo of IDSystem::set/kill/setCk/dispSw/unitPtr and the IdSet*
// helpers; IDC_NUM_00..IDC_NUM_61 are the 62 digit classes, IDC_ANY matches every class.
enum ID_CLASS {
    IDC_SSCRN_MAIN_MENU = 0,
    IDC_SSCRN_BACK_GROUND = 1,
    IDC_SSCRN_PESETA = 2,
    IDC_SSCRN_CONFIRM = 3,
    IDC_SSCRN_ETC = 4,
    IDC_SSCRN_NEAR_0 = 16,
    IDC_SSCRN_NEAR_1 = 17,
    IDC_SSCRN_NEAR_2 = 18,
    IDC_SSCRN_NEAR_3 = 19,
    IDC_SSCRN_0 = 20,
    IDC_SSCRN_1 = 21,
    IDC_SSCRN_2 = 22,
    IDC_SSCRN_3 = 23,
    IDC_SSCRN_FAR_0 = 24,
    IDC_SSCRN_FAR_1 = 25,
    IDC_SSCRN_FAR_2 = 26,
    IDC_SSCRN_FAR_3 = 27,
    IDC_SSCRN_CKPT_0 = 28,
    IDC_SSCRN_CKPT_1 = 29,
    IDC_SSCRN_CKPT_2 = 30,
    IDC_SSCRN_CKPT_3 = 31,
    IDC_ACT_BUTTON = 32,
    IDC_LIFE_METER = 33,
    IDC_GAUGE = 34,
    IDC_COUNT_DOWN = 35,
    IDC_BINOCULAR = 36,
    IDC_SCOPE = 37,
    IDC_EXAMINE = 38,
    IDC_DATA = 39,
    IDC_TITLE = 40,
    IDC_TITLE_MENU = 41,
    IDC_OPTION = 42,
    IDC_OPTION_BG = 43,
    IDC_EVENT = 44,
    IDC_DEAD = 45,
    IDC_CONTINUE = 46,
    IDC_MSG_WINDOW = 47,
    IDC_CINESCO = 48,
    IDC_WIP = 49,
    IDC_BLLT_ICON = 50,
    IDC_SUB_MISSION = 51,
    IDC_LASER_GAUGE = 52,
    IDC_BATTERY_TARGET = 53,
    IDC_NUM_00 = 64,
    IDC_NUM_61 = 125,
    IDC_PRICE_00 = 128,
    IDC_PRICE_01 = 129,
    IDC_PRICE_02 = 130,
    IDC_PRICE_03 = 131,
    IDC_PRICE_04 = 132,
    IDC_TOOL = 254,
    IDC_ANY = 255
};

class IDSystem {
private:
    s32 m_maxId;          // 0x00
    s32 m_nId;       // 0x04
    s32 m_levelMax;     // 0x08
    u32 m_set_flag[8];        // 0x0C  table types set
    u32 m_disp_off[8];      // 0x2C  table types hidden
public:
    ID_UNIT* m_IdUnit;    // 0x4C

    static Mtx m_scrn_mat;

    void gameInit(int maxId);
    void roomInit();
    void free();
    int setCk(ID_CLASS classNo);
    void dispSw(ID_CLASS classNo, int sw);
    void unitPush(ID_UNIT* pIdUnit);
    ID_UNIT* unitPull();
    void unitLevel(ID_UNIT* u, u8 level);
    void unitParent(ID_UNIT* pParent, ID_UNIT* pChild);
    ID_UNIT* unitPtr(u8 markNo, ID_CLASS classNo);
    void set(ID_FILE_HEADER* data, u8 markNo, ID_CLASS classNo, u8 otType, u8 otNo, u32 Attr);
    void kill(u8 markNo, ID_CLASS classNo);
    void stop();
    void move();
    void beMove(ID_UNIT* pParent, int on_off);
    void setTime(ID_UNIT* pParent, s16 time);
    void movePos(ID_UNIT* pParent);
    void trans();
    void unitTrans(ID_UNIT* pIdUnit);
};

extern IDSystem IdSys;
extern void* g_pIdBuff;
extern int IdBuffType;

// game/id_tex.cpp
typedef struct tagTEX_WK TEX_WK;
struct TexAnm;
void IdTexSet(u8 id, u8 no);
int IdGetAnmAddr(u8 id, TexAnm** ppAnm);
void IdChannelSet(ID_UNIT* pIdUnit);
TEX_WK* IdGetTexWk(u8 id, int bNoDispErrMsg);

void idSysMove00(ID_UNIT* u);
void IdCalcVertex(ID_UNIT* u);
void idSysMove01(ID_UNIT* u);
void idSysMove02(ID_UNIT* u);
void idSysMove03(ID_UNIT* u);
void idSysMove04(ID_UNIT* u);
void IdGeneralTrans(ID_UNIT* u);
void IdCommonTrans(ID_UNIT* u);
void IdNegativeTrans(ID_UNIT* u, u32 pow);
void IdShimmerTrans(ID_UNIT* u, int u_pow, int Refract_type);
void IdAllocBuffer();
void IdFreeBuffer();
void IdDebugAllocBuffer();
void IdDebugFreeBuffer();
void* IdGetBufferAddr(int type);
void IdSetBufferType(int type);
void IdTexGameInit();
void IdTexRoomInit();
enum TEX_OWNER {
    TEX_OWNER_NONE = 0,
    TEX_OWNER_CORE = 1,
    TEX_OWNER_ROOM = 2,
    TEX_OWNER_ID_TOOL = 3,
    TEX_OWNER_ID_COCKPIT = 4,
    TEX_OWNER_ID_CINESCO = 5,
    TEX_OWNER_ID_EVENT = 6,
    TEX_OWNER_ID_TITLE = 7,
    TEX_OWNER_ID_SHARE = 8,
    TEX_OWNER_ID_SSCRN = 9,
    TEX_OWNER_ID_DEAD = 10,
    TEX_OWNER_ID_SCOPE = 11,
    TEX_OWNER_MAX = 12
};

void IdTexRelease(int owner);
int IdTexDataLoad(void* data, int id);

#endif
