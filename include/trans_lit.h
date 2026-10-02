#ifndef TRANS_LIT_H
#define TRANS_LIT_H

#include "types.h"
#include "vec.h"
#include "model.h"
#include "light.h"

// Per-model light setup of the model trans (game/trans_lit.cpp).

void LightSetInit();
void LightSetModel(cModel* pMod);
// Fill `list` (n entries) with the lights a cloth / water surface / effect picks up.
void commonClothLightSet(cLight** list, int n, Vec pos, f32 size);
void commonWaterLightSet(cLight** pLightData, int data_num, u32 pow);
void commonEspLightSet(cLight** list, int n);

#endif
