#ifndef ESP_EFM_H
#define ESP_EFM_H

#include "types.h"
#include "vec.h"
#include "obj.h"

// Effect models (game/esp_efm.cpp): room models the effect generators spawn.

// Creates a free-standing effect model from its bin / tpl at pos / rot (embox.cpp: the broken box).
cObj* SetEffModel(void* bin, void* tpl, Vec* pos, Vec* rot);

#endif
