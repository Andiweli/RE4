#ifndef FLR_AT_H
#define FLR_AT_H

#include "types.h"
#include "vec.h"
#include "area.h"

// Per-type payload of a floor attribute record (FLR_AT_DATA + 0x44), one view per FLR_AT_DATA::id. Names and
// layouts are the PS2 FLR_AT_SE_TYPE / FLR_AT_SE_VOLCTRL / FLR_AT_BGM_VOL / FLR_AT_THUNDER_VOL.
struct FLR_AT_SE_TYPE {         // id 0 (foot SE)
    u8 se_type;                 // 0x00  foot SE variation (snd.cpp: SE number += se_type * 30)
    u8 eff_type;                // 0x01  foot effect (EspFootCall); 1 = puddle (est.cpp EspChkInPuddle)
    u8 cartridge_type;          // 0x02  cartridge SE offset (snd.cpp SE 0xF)
    u8 use_kind;                // 0x03  bit mask FlrAtCheck tests against its `flag` argument
};

struct FLR_AT_SE_VOLCTRL {      // id 1 (SE volume control)
    u8 rate[32];                // 0x00
};

struct FLR_AT_BGM_VOL {         // id 2 (BGM volume control)
    u8 blk_no;                  // 0x00  bit i: BGM slot i controlled, 0x10: stream
    u8 sw;                      // 0x01  bit i: set volume (else reset), 0x10: stream play (else stop)
    s8 set_vol[2];              // 0x02  BGM volume per slot
    int time[2];                // 0x04  BGM fade time per slot
    u16 str_blk;                // 0x0C  stream block (0 BGM, 1 VOICE)
    u16 str_no;                 // 0x0E  stream number
    u32 str_fade_time;          // 0x10  SndStrReq time argument
};

struct FLR_AT_THUNDER_VOL {     // id 3 (thunder volume)
    s8 vol;                     // 0x00
    s8 svol;                    // 0x01
};

// Floor attribute record returned by FlrAtCheck (game/flr_at.cpp), 0x84 bytes (PS2 FLR_AT_DATA).
struct FLR_AT_DATA {
    u8 flag;         // 0x00  bit0: active (FlrAtOn / FlrAtOff)  (PS2 be_flg)
    u8 type;         // 0x01  attribute type asked for in FlrAtCheck  (PS2 id)
    u8 no;           // 0x02  record index; (type 2) the BGM control id snd.cpp remembers  (PS2 no)
    u8 group;        // 0x03  group (FlrSys::group 0xFF = any)
    u8 priority;     // 0x04  save order in the tool (15 first)  (PS2 priority)
    u8 padd[15];     // 0x05  (PS2 padd)
    AREA_HIT_DATA area;  // 0x14
    union {          // 0x44  payload by `type`
        u8 dmy[64];
        FLR_AT_SE_TYPE se;
        FLR_AT_SE_VOLCTRL sectrl;
        FLR_AT_BGM_VOL bgmctrl;
        FLR_AT_THUNDER_VOL thunder;
    };
};

// "FSE" room file header (pG->pRoomArc), followed by the FLR_AT_DATA records at 0x10 (PS2 FLR_AT_HEADER).
struct FlrAtHead {
    char magic[4];   // 0x00  "FSE"
    u16 version;     // 0x04  0x103
    u16 num;         // 0x06  record count
    u8 cartridge_type;  // 0x08  default cartridge SE offset (snd.cpp SE 0xF with no record)  (PS2 cartridge_type)
    u8 padd1;        // 0x09
    u16 padd2;       // 0x0A
    u32 padd3;       // 0x0C
};

// Floor system work (`pFlrSys` -> FlrAt_sys, 0x8C bytes).
struct FlrSys {
    void* pData;         // 0x00  room floor attribute data (NULL when the room has none)
    FLR_AT_DATA* pList;        // 0x04  its records
    u8 group;            // 0x08  current group (0xFF = any)
    u8 foot_se[0x41];    // 0x09  foot SE variation per material (FlrAtSetDefVal a)
    u8 foot_esp[0x42];   // 0x4A  foot effect per material (FlrAtSetDefVal b)
};

extern FlrSys* pFlrSys;

FLR_AT_DATA* FlrAtCheck(int id, Vec* pos, int flag);

extern "C" {
void FlrAtInit();
int FlrAtSetDefVal(u32 group, u8 foot_se_set, u8 eff_no);
}

#endif
