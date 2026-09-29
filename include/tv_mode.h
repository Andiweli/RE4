#ifndef TV_MODE_H
#define TV_MODE_H

#include "types.h"
#include "gx.h"

// game/tv_mode.cpp: progressive-scan prompt task run at boot.
struct TV_MODE {
    u8 Rno0;                 // 0x00  index into tvModeFuncTbl
    u8 Rno1;                 // 0x01
    u8 Status;               // 0x02
    s8 Cursor;               // 0x03  unused by this build's task
    GXRenderModeObj* pRmode; // 0x04
};

extern TV_MODE* pTv;
extern u8 tv_mode_cnt;

void SetTvMode(GXRenderModeObj* pRmode);
void tvModeCheckTask();
void tvModeTrigger(TV_MODE* pTv);
void tvModeMenu_progressive(TV_MODE* pTv);
void tvModeExit(TV_MODE* pTv);

#endif
