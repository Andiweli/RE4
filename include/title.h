#ifndef TITLE_H
#define TITLE_H

#include "types.h"

struct IdUnit;

// Frames of the opening timeline (PS2 TITLE_FRAME): TITLE_WORK::counter at which each logo starts; the
// TTL_CANCEL_* ints in title.cpp are the earliest frame START may skip to the next one.
enum TITLE_FRAME {
    TTL_START_WARNING = 0,
    TTL_START_CAPCOM = 105,
    TTL_START_CRI = 230,
    TTL_START_DOLBY = 330,
    TTL_START_BIO4 = 585,
    TTL_START_MENU = 615,
    TTL_FADE_OUT = 15,
    TTL_FADE_IN = 15
};

// Title screen task work (game/title.cpp, mem_calloc'd 0x9C bytes by Title_task).
struct TITLE_WORK {
    s8 Rno0;          // 0x00  titleFuncTbl index (0 init, 1 wait, 2 nintendo, 3 warning, 4 logo, 5 main, 6 sub/omake, 7 exit)
    s8 Rno1;          // 0x01  state inside the mode
    s8 Rno2;           // 0x02  sub state (demo movie steps, stage select)
    u8 Rno3;            // 0x03
    u8 demo_no;        // 0x04  alternates between the two demo movies
    u8 pad_5[3];
    int sndFlag;      // 0x08  1 = title.snd read done, BGM not started yet (titleLogo)
    u8 xC;            // 0x0C
    u8 pad_D[7];
    int req;          // 0x14  DvdReadN request
    u32 se_id;        // 0x18  SndCall handle of the title BGM
    struct TitleArc* pIdDat;  // 0x1C  title.dat (offset table)
    int counter;          // 0x20  frame counter (setTime reads its low half)
    int menu_num;      // 0x24  menu entries
    int cursor;       // 0x28
    IdUnit* p_menu[5];  // 0x2C  menu id units
    int scroll;       // 0x40  1 = the background scroll follows the stick (titleLoop)
    f32 scroll_add;        // 0x44  background scroll speed
    int dbg_mode;          // 0x48  1 = the title logo time was pushed forward (debug menu)
    struct TitleArc* pOmk;  // 0x4C  omk_tX.dat (offset table)
    int data_size;      // 0x50
    s8 omk_menu_no;     // 0x54  omake menu: 0 start, 1 back
    s8 omk_char_no;       // 0x55  mercenaries character select (0..4)
    s8 omk_stage_no;      // 0x56  mercenaries stage select (0..3)
    u8 Rno1_bak;      // 0x57  mode 5 state saved while the omake screens run
    u8 Rno2_bak;       // 0x58
    u8 Rno3_bak;        // 0x59
    u8 pad_5A[2];
    int counter_bak;      // 0x5C
    s8 Stage;      // 0x60  debug menu: stage
    s8 Room[10];   // 0x61  debug menu: room index per stage
    s8 JumpPoint;      // 0x6B  debug menu: jump point
    s8 c_pos;     // 0x6C  debug menu: line (0..20)
    s8 em_list_no;     // 0x6D  debug menu: enemy list
    u8 load_no;        // 0x6E  debug menu: load slot (1..10)
    u8 pad_6F;
    s16 menu_x;         // 0x70  debug menu position
    s16 menu_y;         // 0x72
    u8 pad_74[0x9C - 0x74];
};

// Offset table at the head of title.dat / omk_tX.dat: byte offsets of the sub-files.
struct TitleArc {
    u32 ofs[0x10];
};
#define TITLE_ARC_PTR(arc, no) ((void*) ((arc)->ofs[no] + (u32) (arc)))

extern "C" void Title_task();

#endif
