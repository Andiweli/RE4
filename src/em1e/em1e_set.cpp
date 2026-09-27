// em1e: the per-enemy object of a Ganado module, linked with the shared em10.cpp (real file name
// unknown). It registers Em1eInit and Em1eSet, which fill the cEm10 motion table by model type.

#include "types.h"
#include "atari.h"
#include "global.h"
#include "cManager.h"
#include "em10.h"
#include "light.h"
#include "esp.h"
#include <dolphin/os.h>
#include "em_mod.h"
#include "arc/em1e.h"

void Em1eInit(cEm* em);
void Em1eSet(cEm10* em);
void Em1eWeaponSet(cEm10* em);

// Module entry (SN loader): registers Em1eInit as the DOL's enemy constructor (EmInitFunc) and Em1eSet as
// em10.cpp's per-enemy set function (Em10SetFunc).
extern "C" void _prolog()
{
    OSReport("em10 prolog Ok\n");
    EmInitFunc = Em1eInit;
    Em10SetFunc = Em1eSet;
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
void Em1eInit(cEm* em)
{
    new (em) cEm10();
}

// Em10SetFunc of this module: the island soldiers (class 2) plus the robed type 6 (class 0): model types 14 (default, voice 0), 15 / 23 / 25 (voice 2) and 6 (voice 0). Fills the work's motion table mot[0..40] (body / head / hand
// models, cloth and accessory models, event motions) from the enemy archive for the model type (an
// unknown type is forced to the default), picks the voice table (Em10SetSeTbl), sets the Ganado class
// and calls Em1eWeaponSet.
void Em1eSet(cEm10* em)
{
    FREE_EM10* w = EM10_WK(em);

    switch (em->type) {
    case 14:
    default:
        w->mot[0] = ARC(EM1E_TPL_230);
        w->mot[1] = ARC(EM1E_BIN_22F);
        em->type = 14;
        w->mot[2] = ARC(EM1E_BIN_231);
        w->mot[3] = ARC(EM1E_BIN_233);
        w->mot[4] = ARC(EM1E_BIN_233);
        w->mot[5] = ARC(EM1E_TPL_232);
        w->mot[6] = ARC(EM1E_BIN_234);
        w->mot[7] = ARC(EM1E_BIN_235);
        w->mot[8] = ARC(EM1E_BIN_236);
        w->mot[9] = ARC(EM1E_BIN_237);
        w->mot[10] = ARC(EM1E_BIN_237);
        w->mot[11] = ARC(EM1E_BIN_238);
        w->mot[12] = ARC(EM1E_BIN_239);
        w->mot[13] = ARC(EM1E_BIN_23A);
        w->mot[14] = ARC(EM1E_BIN_23B);
        w->mot[15] = ARC(EM1E_BIN_23C);
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
        w->mot[26] = 0;
        w->mot[27] = 0;
        w->mot[28] = 0;
        w->mot[29] = 0;
        w->mot[30] = 0;
        w->mot[31] = 0;
        w->mot[32] = 0;
        w->mot[33] = 0;
        w->mot[34] = 0;
        w->mot[35] = 0;
        w->mot[36] = 0;
        w->mot[37] = 0;
        w->mot[38] = 0;
        w->mot[39] = 0;
        w->mot[40] = 0;
        Em10SetSeTbl(em, 0);
        break;
    case 15:
        w->mot[0] = ARC(EM1E_TPL_230);
        w->mot[1] = ARC(EM1E_BIN_22F);
        w->mot[2] = ARC(EM1E_BIN_249);
        w->mot[3] = ARC(EM1E_BIN_233);
        w->mot[4] = ARC(EM1E_BIN_233);
        w->mot[5] = ARC(EM1E_TPL_24A);
        w->mot[6] = ARC(EM1E_BIN_234);
        w->mot[7] = ARC(EM1E_BIN_235);
        w->mot[8] = ARC(EM1E_BIN_236);
        w->mot[9] = ARC(EM1E_BIN_237);
        w->mot[10] = ARC(EM1E_BIN_237);
        w->mot[11] = ARC(EM1E_BIN_238);
        w->mot[12] = ARC(EM1E_BIN_239);
        w->mot[13] = ARC(EM1E_BIN_23A);
        w->mot[14] = ARC(EM1E_BIN_23B);
        w->mot[15] = ARC(EM1E_BIN_23C);
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
        w->mot[26] = 0;
        w->mot[27] = 0;
        w->mot[28] = 0;
        w->mot[29] = 0;
        w->mot[30] = 0;
        w->mot[31] = 0;
        w->mot[32] = 0;
        w->mot[33] = 0;
        w->mot[34] = 0;
        w->mot[35] = 0;
        w->mot[36] = 0;
        w->mot[37] = 0;
        w->mot[38] = 0;
        w->mot[39] = 0;
        w->mot[40] = 0;
        Em10SetSeTbl(em, 2);
        break;
    case 23:
        w->mot[0] = ARC(EM1E_TPL_248);
        w->mot[1] = ARC(EM1E_BIN_247);
        w->mot[2] = ARC(EM1E_BIN_24D);
        w->mot[3] = ARC(EM1E_BIN_233);
        w->mot[4] = ARC(EM1E_BIN_233);
        w->mot[5] = ARC(EM1E_TPL_24E);
        w->mot[6] = ARC(EM1E_BIN_234);
        w->mot[7] = ARC(EM1E_BIN_235);
        w->mot[8] = ARC(EM1E_BIN_236);
        w->mot[9] = ARC(EM1E_BIN_237);
        w->mot[10] = ARC(EM1E_BIN_237);
        w->mot[11] = ARC(EM1E_BIN_238);
        w->mot[12] = ARC(EM1E_BIN_239);
        w->mot[13] = ARC(EM1E_BIN_23A);
        w->mot[14] = ARC(EM1E_BIN_23B);
        w->mot[15] = ARC(EM1E_BIN_23C);
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
        w->mot[26] = 0;
        w->mot[27] = 0;
        w->mot[28] = 0;
        w->mot[29] = 0;
        w->mot[30] = 0;
        w->mot[31] = 0;
        w->mot[32] = 0;
        w->mot[33] = 0;
        w->mot[34] = 0;
        w->mot[35] = 0;
        w->mot[36] = 0;
        w->mot[37] = 0;
        w->mot[38] = 0;
        w->mot[39] = 0;
        w->mot[40] = 0;
        Em10SetSeTbl(em, 2);
        break;
    case 25:
        w->mot[0] = ARC(EM1E_TPL_248);
        w->mot[1] = ARC(EM1E_BIN_247);
        w->mot[2] = ARC(EM1E_BIN_24B);
        w->mot[3] = ARC(EM1E_BIN_233);
        w->mot[4] = ARC(EM1E_BIN_233);
        w->mot[5] = ARC(EM1E_TPL_24C);
        w->mot[6] = ARC(EM1E_BIN_234);
        w->mot[7] = ARC(EM1E_BIN_235);
        w->mot[8] = ARC(EM1E_BIN_236);
        w->mot[9] = ARC(EM1E_BIN_237);
        w->mot[10] = ARC(EM1E_BIN_237);
        w->mot[11] = ARC(EM1E_BIN_238);
        w->mot[12] = ARC(EM1E_BIN_239);
        w->mot[13] = ARC(EM1E_BIN_23A);
        w->mot[14] = ARC(EM1E_BIN_23B);
        w->mot[15] = ARC(EM1E_BIN_23C);
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
        w->mot[26] = 0;
        w->mot[27] = 0;
        w->mot[28] = 0;
        w->mot[29] = 0;
        w->mot[30] = 0;
        w->mot[31] = 0;
        w->mot[32] = 0;
        w->mot[33] = 0;
        w->mot[34] = 0;
        w->mot[35] = 0;
        w->mot[36] = 0;
        w->mot[37] = 0;
        w->mot[38] = 0;
        w->mot[39] = 0;
        w->mot[40] = 0;
        Em10SetSeTbl(em, 2);
        break;
    case 6:
        w->mot[0] = ARC(EM1E_TPL_1E8);
        w->mot[1] = ARC(EM1E_BIN_1E7);
        w->mot[2] = ARC(EM1E_BIN_1E9);
        w->mot[3] = ARC(EM1E_BIN_1E9);
        w->mot[4] = ARC(EM1E_BIN_1E9);
        w->mot[16] = ARC(EM1E_BIN_1F0);
        w->mot[5] = ARC(EM1E_TPL_1EA);
        w->mot[17] = ARC(EM1E_TPL_1F1);
        w->mot[6] = ARC(EM1E_BIN_1F4);
        w->mot[7] = ARC(EM1E_BIN_1F5);
        w->mot[8] = ARC(EM1E_BIN_1F6);
        w->mot[9] = ARC(EM1E_BIN_1F7);
        w->mot[10] = ARC(EM1E_BIN_1F7);
        w->mot[11] = ARC(EM1E_BIN_1F8);
        w->mot[12] = ARC(EM1E_BIN_1F9);
        w->mot[13] = ARC(EM1E_BIN_1FA);
        w->mot[14] = ARC(EM1E_BIN_1FB);
        w->mot[15] = ARC(EM1E_BIN_1FB);
        w->mot[18] = ARC(EM1E_BIN_1ED);
        w->mot[19] = ARC(EM1E_BIN_1EE);
        w->mot[20] = ARC(EM1E_BIN_1EF);
        w->mot[21] = 0;
        w->mot[22] = 0;
        w->mot[23] = 0;
        w->mot[24] = 0;
        w->mot[25] = 0;
        w->mot[26] = 0;
        w->mot[27] = 0;
        w->mot[28] = 0;
        w->mot[29] = 0;
        w->mot[30] = 0;
        w->mot[31] = 0;
        w->mot[32] = 0;
        w->mot[33] = 0;
        w->mot[34] = 0;
        w->mot[35] = 0;
        w->mot[36] = 0;
        w->mot[37] = 0;
        w->mot[38] = 0;
        w->mot[39] = 0;
        w->mot[40] = 0;
        Em10SetSeTbl(em, 0);
        break;
    }
    if (em->type == 6) {
        w->Ganado = 0;
    } else {
        w->Ganado = 2;
    }
    Em1eWeaponSet(em);
    EspDataLoad((u32) ARC(EM1E_EFF_278), EFF_OBM83, 0);
}

// Weapon model table of the module: mot[41..78] = the bin / tpl pairs em10MakeWeapon uses (hoe, bucket
// and its motions, sickle, hatchet / flail, chainsaw (the real saw only for the chainsaw type), scythe /
// stun rod, torch, bowgun and arrow, pitchfork); 0 = the weapon does not exist in this island module.
void Em1eWeaponSet(cEm10* em)
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
    w->mot[63] = ARC(EM1E_BIN_WEAPON_SET_26C);
    w->mot[64] = ARC(EM1E_TPL_WEAPON_SET_26D);
    w->mot[65] = ARC(EM1E_BIN_WEAPON_SET_276);
    w->mot[66] = ARC(EM1E_TPL_WEAPON_SET_277);
    w->mot[67] = 0;
    w->mot[68] = 0;
    w->mot[69] = ARC(EM1E_BIN_WEAPON_SET_26A);
    w->mot[70] = ARC(EM1E_TPL_WEAPON_SET_26B);
    w->mot[71] = ARC(EM1E_BIN_WEAPON_SET_270);
    w->mot[72] = ARC(EM1E_TPL_WEAPON_SET_271);
    w->mot[73] = ARC(EM1E_BIN_WEAPON_SET_272);
    w->mot[74] = ARC(EM1E_TPL_WEAPON_SET_273);
    w->mot[75] = ARC(EM1E_BIN_WEAPON_SET_274);
    w->mot[76] = ARC(EM1E_TPL_WEAPON_SET_275);
    w->mot[77] = 0;
    w->mot[78] = 0;
}
