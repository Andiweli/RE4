// em11: the per-enemy object of a Ganado module, linked with the shared em10.cpp (the real file
// name is not in the binary). Em11Set / Em11WeaponSet fill the Ganado motion table by model type.

#include "types.h"
#include "atari.h"
#include "global.h"
#include "cManager.h"
#include "em10.h"
#include <dolphin/os.h>
#include "em_mod.h"
#include "arc/em11.h"

void Em11Init(cEm* em);
void Em11Set(cEm10* em);
void Em11WeaponSet(cEm10* em);

// Module entry (SN loader): registers Em11Init as the DOL's enemy constructor (EmInitFunc) and Em11Set as
// em10.cpp's per-enemy set function (Em10SetFunc).
extern "C" void _prolog()
{
    OSReport("em10 prolog Ok\n");
    EmInitFunc = Em11Init;
    Em10SetFunc = Em11Set;
}

// Module exit: nothing to undo.
extern "C" void _epilog()
{
}

// SN loader stub for unresolved imports: nothing.
extern "C" void _unresolved()
{
}

// EmInitFunc of the module: constructs the shared cEm10 class in the manager's work (em10_R0_Init then
// builds the enemy through Em10SetFunc).
void Em11Init(cEm* em)
{
    new (em) cEm10();
}

// Em10SetFunc of this module: the castle zealots (Ganado class 1): model types 7 (default; voice set 0 or 2 by emset_no bit0), 8 (voice 3) and 9 (voice 0 / 2). Fills the work's motion table mot[0..40] (body / head / hand
// models, cloth and accessory models, event motions) from the enemy archive for the model type (an
// unknown type is forced to the default), picks the voice table (Em10SetSeTbl), sets the Ganado class
// and calls Em11WeaponSet.
void Em11Set(cEm10* em)
{
    FREE_EM10* w = EM10_WK(em);

    switch (em->type) {
    case 7:
    default:
        em->type = 7;
        w->mot[0] = ARC(EM11_TPL_1FD);
        w->mot[1] = ARC(EM11_BIN_1FC);
        w->mot[2] = ARC(EM11_BIN_1FE);
        w->mot[3] = ARC(EM11_BIN_1FF);
        w->mot[4] = ARC(EM11_BIN_200);
        w->mot[5] = ARC(EM11_TPL_201);
        w->mot[6] = ARC(EM11_BIN_204);
        w->mot[7] = ARC(EM11_BIN_205);
        w->mot[8] = ARC(EM11_BIN_206);
        w->mot[9] = ARC(EM11_BIN_207);
        w->mot[10] = ARC(EM11_BIN_207);
        w->mot[11] = ARC(EM11_BIN_208);
        w->mot[12] = ARC(EM11_BIN_209);
        w->mot[13] = ARC(EM11_BIN_20A);
        w->mot[14] = ARC(EM11_BIN_20B);
        w->mot[15] = ARC(EM11_BIN_20C);
        w->mot[16] = 0;
        w->mot[17] = 0;
        w->mot[18] = 0;
        w->mot[19] = 0;
        w->mot[20] = 0;
        w->mot[21] = 0;
        w->mot[22] = 0;
        w->mot[23] = 0;
        w->mot[24] = 0;
        w->mot[25] = 0;
        w->mot[26] = ARC(EM11_BIN_202);
        w->mot[27] = ARC(EM11_BIN_203);
        w->mot[28] = ARC(EM11_BIN_20D);
        w->mot[29] = ARC(EM11_TPL_20E);
        w->mot[30] = ARC(EM11_BIN_20F);
        w->mot[31] = ARC(EM11_TPL_210);
        w->mot[32] = ARC(EM11_BIN_211);
        w->mot[33] = ARC(EM11_TPL_212);
        w->mot[34] = ARC(EM11_BIN_213);
        w->mot[35] = ARC(EM11_TPL_214);
        w->mot[36] = ARC(EM11_BIN_215);
        w->mot[37] = ARC(EM11_TPL_216);
        w->mot[38] = ARC(EM11_BIN_217);
        w->mot[39] = ARC(EM11_TPL_218);
        if (em->emset_no & 1) {
            Em10SetSeTbl(em, 0);
        } else {
            Em10SetSeTbl(em, 2);
        }
        break;
    case 8:
        em->type = 8;
        w->mot[0] = ARC(EM11_TPL_21A);
        w->mot[1] = ARC(EM11_BIN_1FC);
        w->mot[2] = ARC(EM11_BIN_21B);
        w->mot[3] = ARC(EM11_BIN_1FF);
        w->mot[4] = ARC(EM11_BIN_200);
        w->mot[5] = ARC(EM11_TPL_21C);
        w->mot[6] = ARC(EM11_BIN_204);
        w->mot[7] = ARC(EM11_BIN_205);
        w->mot[8] = ARC(EM11_BIN_206);
        w->mot[9] = ARC(EM11_BIN_207);
        w->mot[10] = ARC(EM11_BIN_207);
        w->mot[11] = ARC(EM11_BIN_208);
        w->mot[12] = ARC(EM11_BIN_209);
        w->mot[13] = ARC(EM11_BIN_20A);
        w->mot[14] = ARC(EM11_BIN_20B);
        w->mot[15] = ARC(EM11_BIN_20C);
        w->mot[16] = 0;
        w->mot[17] = 0;
        w->mot[18] = 0;
        w->mot[19] = 0;
        w->mot[20] = 0;
        w->mot[21] = 0;
        w->mot[22] = 0;
        w->mot[23] = 0;
        w->mot[24] = 0;
        w->mot[25] = 0;
        w->mot[26] = ARC(EM11_BIN_202);
        w->mot[27] = ARC(EM11_BIN_203);
        w->mot[28] = ARC(EM11_BIN_20D);
        w->mot[29] = ARC(EM11_TPL_20E);
        w->mot[30] = ARC(EM11_BIN_20F);
        w->mot[31] = ARC(EM11_TPL_210);
        w->mot[32] = ARC(EM11_BIN_211);
        w->mot[33] = ARC(EM11_TPL_212);
        w->mot[34] = ARC(EM11_BIN_213);
        w->mot[35] = ARC(EM11_TPL_214);
        w->mot[36] = ARC(EM11_BIN_215);
        w->mot[37] = ARC(EM11_TPL_216);
        w->mot[38] = ARC(EM11_BIN_217);
        w->mot[39] = ARC(EM11_TPL_218);
        w->mot[40] = ARC(EM11_TPL_219);
        Em10SetSeTbl(em, 3);
        break;
    case 9:
        w->mot[0] = ARC(EM11_TPL_21E);
        w->mot[1] = ARC(EM11_BIN_21D);
        w->mot[2] = ARC(EM11_BIN_21F);
        w->mot[3] = ARC(EM11_BIN_1FF);
        w->mot[4] = ARC(EM11_BIN_200);
        w->mot[5] = ARC(EM11_TPL_201);
        w->mot[6] = ARC(EM11_BIN_204);
        w->mot[7] = ARC(EM11_BIN_205);
        w->mot[8] = ARC(EM11_BIN_206);
        w->mot[9] = ARC(EM11_BIN_207);
        w->mot[10] = ARC(EM11_BIN_207);
        w->mot[11] = ARC(EM11_BIN_208);
        w->mot[12] = ARC(EM11_BIN_209);
        w->mot[13] = ARC(EM11_BIN_20A);
        w->mot[14] = ARC(EM11_BIN_20B);
        w->mot[15] = ARC(EM11_BIN_20C);
        w->mot[16] = 0;
        w->mot[17] = 0;
        w->mot[18] = 0;
        w->mot[19] = 0;
        w->mot[20] = 0;
        w->mot[21] = 0;
        w->mot[22] = 0;
        w->mot[23] = 0;
        w->mot[24] = 0;
        w->mot[25] = 0;
        w->mot[26] = ARC(EM11_BIN_202);
        w->mot[27] = ARC(EM11_BIN_203);
        w->mot[28] = ARC(EM11_BIN_20D);
        w->mot[29] = ARC(EM11_TPL_20E);
        w->mot[30] = ARC(EM11_BIN_20F);
        w->mot[31] = ARC(EM11_TPL_210);
        w->mot[32] = ARC(EM11_BIN_211);
        w->mot[33] = ARC(EM11_TPL_212);
        w->mot[34] = ARC(EM11_BIN_213);
        w->mot[35] = ARC(EM11_TPL_214);
        w->mot[36] = ARC(EM11_BIN_215);
        w->mot[37] = ARC(EM11_TPL_216);
        w->mot[38] = ARC(EM11_BIN_217);
        w->mot[39] = ARC(EM11_TPL_218);
        w->mot[40] = ARC(EM11_TPL_219);
        if (em->emset_no & 1) {
            Em10SetSeTbl(em, 0);
        } else {
            Em10SetSeTbl(em, 2);
        }
        break;
    }
    w->Ganado = 1;
    Em11WeaponSet(em);
}

// Weapon model table of the module: mot[41..78] = the bin / tpl pairs em10MakeWeapon uses (hoe, bucket
// and its motions, sickle, hatchet / flail, chainsaw (the real saw only for the chainsaw type), scythe /
// stun rod, torch, bowgun and arrow, pitchfork); 0 = the weapon does not exist in this castle module.
void Em11WeaponSet(cEm10* em)
{
    FREE_EM10* w = EM10_WK(em);

    w->mot[41] = 0;
    w->mot[42] = 0;
    w->mot[43] = 0;
    w->mot[44] = 0;
    w->mot[45] = 0;
    w->mot[46] = 0;
    w->mot[47] = 0;
    w->mot[48] = 0;
    w->mot[49] = 0;
    w->mot[50] = 0;
    w->mot[51] = 0;
    w->mot[52] = 0;
    w->mot[53] = 0;
    w->mot[54] = 0;
    w->mot[55] = 0;
    w->mot[56] = 0;
    w->mot[57] = 0;
    w->mot[58] = 0;
    w->mot[59] = 0;
    w->mot[60] = 0;
    w->mot[61] = 0;
    w->mot[62] = 0;
    w->mot[63] = 0;
    w->mot[64] = 0;
    w->mot[65] = ARC(EM11_BIN_WEAPON_SET_276);
    w->mot[66] = ARC(EM11_TPL_WEAPON_SET_277);
    w->mot[67] = 0;
    w->mot[68] = 0;
    w->mot[69] = ARC(EM11_BIN_WEAPON_SET_26E);
    w->mot[70] = ARC(EM11_TPL_WEAPON_SET_26F);
    w->mot[71] = ARC(EM11_BIN_WEAPON_SET_270);
    w->mot[72] = ARC(EM11_TPL_WEAPON_SET_271);
    w->mot[73] = ARC(EM11_BIN_WEAPON_SET_272);
    w->mot[74] = ARC(EM11_TPL_WEAPON_SET_273);
    w->mot[75] = ARC(EM11_BIN_WEAPON_SET_274);
    w->mot[76] = ARC(EM11_TPL_WEAPON_SET_275);
    w->mot[77] = 0;
    w->mot[78] = 0;
}
