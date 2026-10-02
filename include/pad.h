#ifndef PAD_H
#define PAD_H

#include "types.h"
#include "joy.h"

// Dolphin PAD (the SDK header pulls in the CodeWarrior libc).
struct PADStatus {
    u16 button;       // 0x00
    s8 stickX;        // 0x02
    s8 stickY;        // 0x03
    s8 substickX;     // 0x04
    s8 substickY;     // 0x05
    u8 triggerLeft;   // 0x06
    u8 triggerRight;  // 0x07
    u8 analogA;       // 0x08
    u8 analogB;       // 0x09
    s8 err;           // 0x0A
    u8 pad_B;
};

#define PAD_CHAN0_BIT 0x80000000
#define PAD_ERR_NONE 0
#define PAD_ERR_NO_CONTROLLER -1
#define PAD_ERR_NOT_READY -2
#define PAD_ERR_TRANSFER -3

// Vibration pattern table (VibSetData): offsets from the table start to VIB_INFO blocks.
struct VIB_DATA {
    u16 flag;     // 0x00
    u16 delay;    // 0x02
    u16 time;     // 0x04
    u8 s_level;   // 0x06  start level
    u8 e_level;   // 0x07  end level
};

struct VIB_INFO {
    u32 num;
    VIB_DATA e[1];
};

struct VibDataTbl {
    u32 num;
    u32 ofs[1];
};

extern "C" {
void PADInit();
u32 PADRead(PADStatus* status);
void PADClamp(PADStatus* status);
BOOL PADReset(u32 mask);
void PADRecalibrate(u32 mask);
void PADControlMotor(int chan, u32 cmd);
void PADSetAnalogMode(u32 mode);
}

// game/pad.cpp
void PadInit();
void PadRead();
void KeyStop(u64 un_stop_bit);
void KeyClear(u64 un_stop_bit);
void VibControl();
PAD_MOTOR* PullVibWork();
void VibSet(u32 time, u32 level, u16 delay, u16 flag);
void VibSetDataCore(VIB_INFO* pInfo, u32 flag);
void VibSetData(VibDataTbl* t, u32 no, u32 type);
void VibSetClearType(u32 type);
int PadCheckStatus(JOY* joy);
void Pad_test();

extern u32 Key_type_tbl[2][64];

#endif
