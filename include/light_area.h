#ifndef LIGHT_AREA_H
#define LIGHT_AREA_H

#include "types.h"

// Light areas (game/light_area.cpp): per-room volumes that recolour the characters inside them. The
// data header type is the unit's own; callers hand the room file's address over.

struct LightAreaHed;

void LightAreaInit();
int LightAreaDataLoad(LightAreaHed* p);
void LightAreaUpdate();

#endif
