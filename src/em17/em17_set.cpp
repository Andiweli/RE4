// em17: the per-enemy object of a Ganado module made of em10.cpp plus this file (real file name
// unknown). _prolog registers its Init/Set functions with the DOL and with the shared em10.cpp.

#include "types.h"
#include "atari.h"
#include "global.h"
#include "cManager.h"
#include "em10.h"
#include <dolphin/os.h>
#include "em_mod.h"
#include "arc/em17.h"

void Em17Init(cEm* em);
void Em17Set(cEm10* em);
void Em17WeaponSet(cEm10* em);

// Module entry (SN loader): registers Em17Init as the DOL's enemy constructor (EmInitFunc) and Em17Set as
// em10.cpp's per-enemy set function (Em10SetFunc).
extern "C" void _prolog()
{
    OSReport("em10 prolog Ok\n");
    EmInitFunc = Em17Init;
    Em10SetFunc = Em17Set;
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
void Em17Init(cEm* em)
{
    new (em) cEm10();
}

// Em10SetFunc of this module: the village Ganados (class 0): model types 0 (default), 1, 3 and 12 (voice 3). Fills the work's motion table mot[0..40] (body / head / hand
// models, cloth and accessory models, event motions) from the enemy archive for the model type (an
// unknown type is forced to the default), picks the voice table (Em10SetSeTbl), sets the Ganado class
// and calls Em17WeaponSet.
void Em17Set(cEm10* em)
{
    FREE_EM10* w = EM10_WK(em);

    switch (em->type) {
    case 0:
    default:
        em->type = 0;
        w->mot[0] = ARC(EM17_TPL_1BD);
        w->mot[1] = ARC(EM17_BIN_1BC);
        w->mot[2] = ARC(EM17_BIN_1BE);
        w->mot[3] = ARC(EM17_BIN_1BF);
        w->mot[4] = ARC(EM17_BIN_1BF);
        w->mot[5] = ARC(EM17_TPL_1BD);
        w->mot[6] = ARC(EM17_BIN_1C0);
        w->mot[7] = ARC(EM17_BIN_1C1);
        w->mot[8] = ARC(EM17_BIN_1C2);
        w->mot[9] = ARC(EM17_BIN_1C3);
        w->mot[10] = ARC(EM17_BIN_1C4);
        w->mot[11] = ARC(EM17_BIN_1C5);
        w->mot[12] = ARC(EM17_BIN_1C6);
        w->mot[13] = ARC(EM17_BIN_1C7);
        w->mot[14] = ARC(EM17_BIN_1C8);
        w->mot[15] = ARC(EM17_BIN_1C8);
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
    case 1:
        w->mot[0] = ARC(EM17_TPL_1D9);
        w->mot[1] = ARC(EM17_BIN_1D8);
        w->mot[2] = ARC(EM17_BIN_1DA);
        w->mot[5] = ARC(EM17_TPL_1D9);
        w->mot[3] = ARC(EM17_BIN_1DB);
        w->mot[4] = ARC(EM17_BIN_1DB);
        w->mot[6] = ARC(EM17_BIN_1C0);
        w->mot[7] = ARC(EM17_BIN_1C1);
        w->mot[8] = ARC(EM17_BIN_1C2);
        w->mot[9] = ARC(EM17_BIN_1C3);
        w->mot[10] = ARC(EM17_BIN_1C4);
        w->mot[11] = ARC(EM17_BIN_1C5);
        w->mot[12] = ARC(EM17_BIN_1C6);
        w->mot[13] = ARC(EM17_BIN_1C7);
        w->mot[14] = ARC(EM17_BIN_1C8);
        w->mot[15] = ARC(EM17_BIN_1C8);
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
        Em10SetSeTbl(em, 1);
        break;
    case 3:
        w->mot[0] = ARC(EM17_TPL_1DD);
        w->mot[1] = ARC(EM17_BIN_1DC);
        w->mot[2] = ARC(EM17_BIN_1DE);
        w->mot[3] = ARC(EM17_BIN_1DF);
        w->mot[4] = ARC(EM17_BIN_1DF);
        w->mot[5] = ARC(EM17_TPL_1DD);
        w->mot[6] = ARC(EM17_BIN_1C0);
        w->mot[7] = ARC(EM17_BIN_1C1);
        w->mot[8] = ARC(EM17_BIN_1C2);
        w->mot[9] = ARC(EM17_BIN_1C3);
        w->mot[10] = ARC(EM17_BIN_1C4);
        w->mot[11] = ARC(EM17_BIN_1C5);
        w->mot[12] = ARC(EM17_BIN_1C6);
        w->mot[13] = ARC(EM17_BIN_1C7);
        w->mot[14] = ARC(EM17_BIN_1C8);
        w->mot[15] = ARC(EM17_BIN_1C8);
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
    case 12:
        em->type = 12;
        w->mot[0] = ARC(EM17_TPL_1CE);
        w->mot[1] = ARC(EM17_BIN_1CD);
        w->mot[2] = ARC(EM17_BIN_1CF);
        w->mot[3] = ARC(EM17_BIN_1D0);
        w->mot[4] = ARC(EM17_BIN_1D0);
        w->mot[5] = ARC(EM17_TPL_1CE);
        w->mot[6] = ARC(EM17_BIN_1C0);
        w->mot[7] = ARC(EM17_BIN_1C1);
        w->mot[8] = ARC(EM17_BIN_1C2);
        w->mot[9] = ARC(EM17_BIN_1C3);
        w->mot[10] = ARC(EM17_BIN_1C4);
        w->mot[11] = ARC(EM17_BIN_1C5);
        w->mot[12] = ARC(EM17_BIN_1C6);
        w->mot[13] = ARC(EM17_BIN_1C7);
        w->mot[14] = ARC(EM17_BIN_1C8);
        w->mot[15] = ARC(EM17_BIN_1C8);
        w->mot[16] = 0;
        w->mot[17] = 0;
        w->mot[18] = 0;
        w->mot[19] = 0;
        w->mot[20] = 0;
        w->mot[21] = 0;
        w->mot[22] = 0;
        w->mot[23] = ARC(EM17_BIN_1D3);
        w->mot[24] = ARC(EM17_TPL_1D4);
        w->mot[25] = ARC(EM17_TPL_1D5);
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
        Em10SetSeTbl(em, 3);
        break;
    }
    w->Ganado = 0;
    Em17WeaponSet(em);
}

// Weapon model table of the module: mot[41..78] = the bin / tpl pairs em10MakeWeapon uses (hoe, bucket
// and its motions, sickle, hatchet / flail, chainsaw (the real saw only for the chainsaw type), scythe /
// stun rod, torch, bowgun and arrow, pitchfork); 0 = the weapon does not exist in this village module.
void Em17WeaponSet(cEm10* em)
{
    FREE_EM10* w = EM10_WK(em);

    w->mot[41] = ARC(EM17_BIN_WEAPON_SET_254);
    w->mot[42] = ARC(EM17_TPL_WEAPON_SET_255);
    w->mot[43] = 0;
    w->mot[44] = 0;
    w->mot[45] = 0;
    w->mot[46] = 0;
    w->mot[47] = 0;
    w->mot[48] = 0;
    w->mot[49] = 0;
    w->mot[50] = 0;
    w->mot[51] = 0;
    w->mot[52] = ARC(EM17_BIN_WEAPON_SET_25F);
    w->mot[53] = ARC(EM17_TPL_WEAPON_SET_260);
    w->mot[54] = ARC(EM17_TPL_WEAPON_SET_261);
    w->mot[55] = 0;
    w->mot[56] = 0;
    w->mot[57] = 0;
    w->mot[58] = ARC(EM17_BIN_WEAPON_SET_265);
    w->mot[59] = ARC(EM17_TPL_WEAPON_SET_266);
    w->mot[60] = ARC(EM17_TPL_WEAPON_SET_267);
    w->mot[61] = ARC(EM17_BIN_WEAPON_SET_268);
    w->mot[62] = ARC(EM17_TPL_WEAPON_SET_269);
    w->mot[63] = ARC(EM17_BIN_WEAPON_SET_26A);
    w->mot[64] = ARC(EM17_TPL_WEAPON_SET_26B);
    w->mot[65] = ARC(EM17_BIN_WEAPON_SET_26C);
    w->mot[66] = ARC(EM17_TPL_WEAPON_SET_26D);
    w->mot[67] = 0;
    w->mot[68] = 0;
    w->mot[69] = 0;
    w->mot[70] = 0;
    w->mot[71] = ARC(EM17_BIN_WEAPON_SET_270);
    w->mot[72] = ARC(EM17_TPL_WEAPON_SET_271);
    w->mot[73] = 0;
    w->mot[74] = 0;
    w->mot[75] = 0;
    w->mot[76] = 0;
    w->mot[77] = ARC(EM17_BIN_WEAPON_SET_1D6);
    w->mot[78] = ARC(EM17_TPL_WEAPON_SET_1D7);
}
