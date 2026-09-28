#ifndef GX_SUB_H
#define GX_SUB_H

#include "types.h"
#include "gx.h"

// game/gx_sub.cpp
void bio4_GXSetCopyClear(GXColor clear_clr, u32 clear_z);
// Draws the real background colour over the cleared frame (trans.cpp).
void bio4_AddBgColor();

extern GXColor g_sysBgColor;

#endif
