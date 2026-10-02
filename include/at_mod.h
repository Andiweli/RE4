#ifndef AT_MOD_H
#define AT_MOD_H

#include "types.h"
#include "vec.h"
#include "em.h"

// Character-to-character collision (game/at_mod.cpp): pushes the cAtariInfo rectangles /
// cylinders of the enemies and objects apart, and line checks against their hit boxes.
void EmAtCheck(cModel* pMod);
// A leading `__` cannot be demangled by the GNU v2 demangler the tools use, so the mangled name
// would not match its sym_map row.
extern "C" void __em_at_core(cModel* pMod, cModel* pMod2);
u32 At_em_rect_rect_ck(cModel* pMod, cModel* pMod2);
int em_rect2_ck_sub(cModel* pMod, cModel* pMod2);
u32 At_em_sphere_rect_ck(cModel* pMod, cModel* pMod2);
u32 At_em_sphere_sphere_ck(cModel* pMod, cModel* pMod2);
// Segment a-b against every enemy / object with collision (flag bit0: rectangles as cubes,
// bit1: cylinders, bit2: the player too). The nearest hit point / normal go to `hit` / `nrm`.
BOOL EmHitCheck(Vec* ret_cross, Vec* ret_norm, Vec* pos0, Vec* pos1, u32 flag);
int ObjHitCheck(Vec* ret_cross, Vec* ret_norm, Vec* pos0, Vec* pos1, u32 flag);
int ComnHitCheck(Vec* cross, Vec* norm, cModel* pMod, Vec* pos0, Vec* pos1, u32 flag);
void DrawOba(cModel* pMod);
BOOL ObaLineHitChk(cModel* m, cAtariInfo* info, const Vec& pos10, const Vec& pos11, Vec& hit, Vec& nrm);

#endif
