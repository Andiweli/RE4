#ifndef FOOT_SHADOW_H
#define FOOT_SHADOW_H

#include "types.h"
#include "vec.h"
#include "gx.h"

// One foot shadow entry of a character (game/foot_shadow_tbl.cpp), 8 bytes.
struct FOOTSHADOW_DATA {
    u8 joint;   // 0x00  parts the shadow is drawn under
    u8 div;     // 0x01  shadows interpolated between this and the previous linked entry
    u8 flag;    // 0x02  bit0: link the next entry to this one
    u8 color;   // 0x03
    f32 size;   // 0x04
};

// Foot shadow table (cEm::pFsdTbl): entry count and the entries.
struct FootShadowTbl {
    u32 nTbl;
    FOOTSHADOW_DATA* dat;
};

// game/foot_shadow.cpp
void DrawFootShadow(class cModel* pMod);
void drawShadowParts(GXTexObj* tex, Vec* pos, f32 size, f32 alpha);

// game/foot_shadow_tbl.cpp: the per-character tables the enemy modules point pFsdTbl at.
extern FootShadowTbl Em10_fs_tbl;   // Ganado (em10..em20, em18, em30, em34 reuse it)
extern FootShadowTbl Em2b_fs_tbl;
extern FootShadowTbl Em2c_fs_tbl;   // event.cpp ExePacket_SetOm
extern FootShadowTbl Em39_fs_tbl;

#endif
