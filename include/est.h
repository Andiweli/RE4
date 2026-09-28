#ifndef EST_H
#define EST_H

#include "types.h"
#include "vec.h"
#include "model.h"
#include "esp.h"

// game/est.cpp: effect set table (est) helpers. EstSet itself is declared in esp.h.
void AreaSstSet(int id);
int GetSstDispFlag(u32 id);
void SetSstDispFlag(u32 id, int flg);
void SetSstAddAreaFlag(u32 flg);
void SstSet(u32 owner, int blk_no, ESP_CORE_KIND kind, int start_id, int end_id, int bTimeLoop);
void EffectEspDelete(int a, int b, void* c, cModel* pMod);
void EffectEspgenDelete(int Core_flg, int Core_kind, void* c);
void EffectEfmDelete(int Core_flg, int Core_kind, void* c);
void EffectDeleteAll();
void EffectEventDelete();
void EspDelete(int a, int b, void* c, cModel* pMod);
void EspDeleteEvent();
void EspSetWaterBomb(Vec* pos);
void EspSetWaterHitmark(Vec* pos);
int EspChkInPuddle(Vec* pos, Vec* nor);
void EspSetEatEffect(Vec* pos, Vec* nor, int type, u8 wepNo);
void EventCutEstSet(int owner, u32 cut_no);
void EventCutEffDelete();
void EventAllEffDelete();
int ChkWaterEffectEnable(Vec* pos);
void EstSetEm10WaterFall(Vec* pMod);

// Drop effect (owner a, kind b) in all three effect systems (r318, r31b, r31c).
static inline void EffectDelete(int a, int b)
{
    EffectEspDelete(a, b, 0, 0);
    EffectEspgenDelete(a, b, 0);
    EffectEfmDelete(a, b, 0);
}

#endif
