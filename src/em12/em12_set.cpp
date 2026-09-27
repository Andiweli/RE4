// em12: the per-enemy object of a Ganado module made of em10.cpp and this file (real file name
// unknown). _prolog registers its Init/Set functions with the DOL and with the shared em10.cpp.

#include "types.h"
#include "atari.h"
#include "global.h"
#include "cManager.h"
#include "em10.h"
#include <dolphin/os.h>
#include "em_mod.h"
#include "arc/em12.h"

void Em12Init(cEm* em);
void Em12Set(cEm10* em);
void Em12WeaponSet(cEm10* em);

// Module entry (SN loader): registers Em12Init as the DOL's enemy constructor (EmInitFunc) and Em12Set as
// em10.cpp's per-enemy set function (Em10SetFunc).
extern "C" void _prolog()
{
    OSReport("em10 prolog Ok\n");
    EmInitFunc = Em12Init;
    Em10SetFunc = Em12Set;
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
void Em12Init(cEm* em)
{
    new (em) cEm10();
}

// Em10SetFunc of this module: the village Ganados (class 0): model types 0 (default), 1, 3 and 4 (chainsaw), voice sets 0..3 like em10. Fills the work's motion table mot[0..40] (body / head / hand
// models, cloth and accessory models, event motions) from the enemy archive for the model type (an
// unknown type is forced to the default), picks the voice table (Em10SetSeTbl), sets the Ganado class
// and calls Em12WeaponSet.
void Em12Set(cEm10* em)
{
    FREE_EM10* w = EM10_WK(em);

    switch (em->type) {
    case 0:
    default:
        em->type = 0;
        w->mot[0] = ARC(EM12_TPL_1BD);
        w->mot[1] = ARC(EM12_BIN_1BC);
        w->mot[2] = ARC(EM12_BIN_1BE);
        w->mot[3] = ARC(EM12_BIN_1BF);
        w->mot[4] = ARC(EM12_BIN_1BF);
        w->mot[5] = ARC(EM12_TPL_1BD);
        w->mot[6] = ARC(EM12_BIN_1C0);
        w->mot[7] = ARC(EM12_BIN_1C1);
        w->mot[8] = ARC(EM12_BIN_1C2);
        w->mot[9] = ARC(EM12_BIN_1C3);
        w->mot[10] = ARC(EM12_BIN_1C4);
        w->mot[11] = ARC(EM12_BIN_1C5);
        w->mot[12] = ARC(EM12_BIN_1C6);
        w->mot[13] = ARC(EM12_BIN_1C7);
        w->mot[14] = ARC(EM12_BIN_1C8);
        w->mot[15] = ARC(EM12_BIN_1C8);
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
        w->mot[0] = ARC(EM12_TPL_1D9);
        w->mot[1] = ARC(EM12_BIN_1D8);
        w->mot[2] = ARC(EM12_BIN_1DA);
        w->mot[5] = ARC(EM12_TPL_1D9);
        w->mot[3] = ARC(EM12_BIN_1DB);
        w->mot[4] = ARC(EM12_BIN_1DB);
        w->mot[6] = ARC(EM12_BIN_1C0);
        w->mot[7] = ARC(EM12_BIN_1C1);
        w->mot[8] = ARC(EM12_BIN_1C2);
        w->mot[9] = ARC(EM12_BIN_1C3);
        w->mot[10] = ARC(EM12_BIN_1C4);
        w->mot[11] = ARC(EM12_BIN_1C5);
        w->mot[12] = ARC(EM12_BIN_1C6);
        w->mot[13] = ARC(EM12_BIN_1C7);
        w->mot[14] = ARC(EM12_BIN_1C8);
        w->mot[15] = ARC(EM12_BIN_1C8);
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
        w->mot[0] = ARC(EM12_TPL_1DD);
        w->mot[1] = ARC(EM12_BIN_1DC);
        w->mot[2] = ARC(EM12_BIN_1DE);
        w->mot[3] = ARC(EM12_BIN_1DF);
        w->mot[4] = ARC(EM12_BIN_1DF);
        w->mot[5] = ARC(EM12_TPL_1DD);
        w->mot[6] = ARC(EM12_BIN_1C0);
        w->mot[7] = ARC(EM12_BIN_1C1);
        w->mot[8] = ARC(EM12_BIN_1C2);
        w->mot[9] = ARC(EM12_BIN_1C3);
        w->mot[10] = ARC(EM12_BIN_1C4);
        w->mot[11] = ARC(EM12_BIN_1C5);
        w->mot[12] = ARC(EM12_BIN_1C6);
        w->mot[13] = ARC(EM12_BIN_1C7);
        w->mot[14] = ARC(EM12_BIN_1C8);
        w->mot[15] = ARC(EM12_BIN_1C8);
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
    case 4:
        w->mot[0] = ARC(EM12_TPL_1E1);
        w->mot[1] = ARC(EM12_BIN_1E0);
        w->mot[2] = ARC(EM12_BIN_1E2);
        w->mot[3] = ARC(EM12_BIN_1E3);
        w->mot[4] = ARC(EM12_BIN_1E3);
        w->mot[5] = ARC(EM12_TPL_1E1);
        w->mot[6] = ARC(EM12_BIN_1C0);
        w->mot[7] = ARC(EM12_BIN_1C1);
        w->mot[8] = ARC(EM12_BIN_1C2);
        w->mot[9] = ARC(EM12_BIN_1C3);
        w->mot[10] = ARC(EM12_BIN_1C4);
        w->mot[11] = ARC(EM12_BIN_1C5);
        w->mot[12] = ARC(EM12_BIN_1C6);
        w->mot[13] = ARC(EM12_BIN_1C7);
        w->mot[14] = ARC(EM12_BIN_1C8);
        w->mot[15] = ARC(EM12_BIN_1C8);
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
        Em10SetSeTbl(em, 3);
        break;
    }
    w->Ganado = 0;
    Em12WeaponSet(em);
}

// Weapon model table of the module: mot[41..78] = the bin / tpl pairs em10MakeWeapon uses (hoe, bucket
// and its motions, sickle, hatchet / flail, chainsaw (the real saw only for the chainsaw type), scythe /
// stun rod, torch, bowgun and arrow, pitchfork); 0 = the weapon does not exist in this village module.
void Em12WeaponSet(cEm10* em)
{
    FREE_EM10* w = EM10_WK(em);

    w->mot[41] = ARC(EM12_BIN_WEAPON_SET_254);
    w->mot[42] = ARC(EM12_TPL_WEAPON_SET_255);
    w->mot[43] = ARC(EM12_BIN_WEAPON_SET_256);
    w->mot[44] = ARC(EM12_TPL_WEAPON_SET_257);
    w->mot[45] = ARC(EM12_MOT_WEAPON_SET_258);
    w->mot[46] = ARC(EM12_MOT_WEAPON_SET_259);
    w->mot[47] = ARC(EM12_MOT_WEAPON_SET_25A);
    w->mot[48] = ARC(EM12_BIN_WEAPON_SET_25B);
    w->mot[49] = ARC(EM12_TPL_WEAPON_SET_25C);
    w->mot[50] = ARC(EM12_MOT_WEAPON_SET_25D);
    w->mot[51] = ARC(EM12_MOT_WEAPON_SET_25E);
    w->mot[52] = ARC(EM12_BIN_WEAPON_SET_25F);
    w->mot[53] = ARC(EM12_TPL_WEAPON_SET_260);
    w->mot[54] = ARC(EM12_TPL_WEAPON_SET_261);
    w->mot[55] = ARC(EM12_BIN_WEAPON_SET_262);
    w->mot[56] = ARC(EM12_TPL_WEAPON_SET_263);
    w->mot[57] = ARC(EM12_TPL_WEAPON_SET_264);
    w->mot[58] = ARC(EM12_BIN_WEAPON_SET_265);
    w->mot[59] = ARC(EM12_TPL_WEAPON_SET_266);
    w->mot[60] = ARC(EM12_TPL_WEAPON_SET_267);
    w->mot[61] = ARC(EM12_BIN_WEAPON_SET_268);
    w->mot[62] = ARC(EM12_TPL_WEAPON_SET_269);
    w->mot[63] = ARC(EM12_BIN_WEAPON_SET_26A);
    w->mot[64] = ARC(EM12_TPL_WEAPON_SET_26B);
    w->mot[65] = ARC(EM12_BIN_WEAPON_SET_26C);
    w->mot[66] = ARC(EM12_TPL_WEAPON_SET_26D);
    w->mot[67] = 0;
    w->mot[68] = 0;
    w->mot[69] = 0;
    w->mot[70] = 0;
    w->mot[71] = ARC(EM12_BIN_WEAPON_SET_270);
    w->mot[72] = ARC(EM12_TPL_WEAPON_SET_271);
    w->mot[73] = 0;
    w->mot[74] = 0;
    w->mot[75] = 0;
    w->mot[76] = 0;
    w->mot[77] = 0;
    w->mot[78] = 0;
}
