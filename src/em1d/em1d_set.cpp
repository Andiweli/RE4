// em1d: the per-enemy object of a Ganado module (the real file name is not in the binary; the
// module is em10.cpp + this file). _prolog registers the enemy's Init/Set functions with the DOL
// (EmInitFunc) and the shared em10.cpp (Em10SetFunc); Em1dInit constructs the shared cEm10 class in
// the manager's work, Em1dSet / Em1dWeaponSet fill the Ganado work's motion table from the enemy
// archive (cEm::subArc) by model type.

#include "types.h"
#include "atari.h"
#include "global.h"
#include "cManager.h"
#include "em10.h"
#include "light.h"
#include "esp.h"
#include <dolphin/os.h>
#include "em_mod.h"
#include "arc/em1d.h"

void Em1dInit(cEm* em);
void Em1dSet(cEm10* em);
void Em1dWeaponSet(cEm10* em);

// Module entry (SN loader): registers Em1dInit as the DOL's enemy constructor (EmInitFunc) and Em1dSet as
// em10.cpp's per-enemy set function (Em10SetFunc).
extern "C" void _prolog()
{
    OSReport("em10 prolog Ok\n");
    EmInitFunc = Em1dInit;
    Em10SetFunc = Em1dSet;
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
void Em1dInit(cEm* em)
{
    new (em) cEm10();
}

// Em10SetFunc of this module: the island soldiers (Ganado class 2, stage 3): model type pairs 14/18 (default, voice 0), 15/19 (voice 2), 16/20 (voice 3), 17/21 (voice 2, shared table) and 2 (the gatling gunner, voice 1); also loads the effect data 0xCC / 0xCD. Fills the work's motion table mot[0..40] (body / head / hand
// models, cloth and accessory models, event motions) from the enemy archive for the model type (an
// unknown type is forced to the default), picks the voice table (Em10SetSeTbl), sets the Ganado class
// and calls Em1dWeaponSet.
void Em1dSet(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

    switch (em->type) {
    case 14:
    case 18:
    default:
        if (em->type == 18) {
            w->mot[0] = ARC(EM1D_TPL_248);
            w->mot[1] = ARC(EM1D_BIN_247);
        } else {
            w->mot[0] = ARC(EM1D_TPL_230);
            w->mot[1] = ARC(EM1D_BIN_22F);
            em->type = 14;
        }
        w->mot[2] = ARC(EM1D_BIN_231);
        w->mot[3] = ARC(EM1D_BIN_233);
        w->mot[4] = ARC(EM1D_BIN_233);
        w->mot[5] = ARC(EM1D_TPL_232);
        w->mot[6] = ARC(EM1D_BIN_234);
        w->mot[7] = ARC(EM1D_BIN_235);
        w->mot[8] = ARC(EM1D_BIN_236);
        w->mot[9] = ARC(EM1D_BIN_237);
        w->mot[10] = ARC(EM1D_BIN_237);
        w->mot[11] = ARC(EM1D_BIN_238);
        w->mot[12] = ARC(EM1D_BIN_239);
        w->mot[13] = ARC(EM1D_BIN_23A);
        w->mot[14] = ARC(EM1D_BIN_23B);
        w->mot[15] = ARC(EM1D_BIN_23C);
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
    case 19:
        if (em->type == 15) {
            w->mot[0] = ARC(EM1D_TPL_230);
            w->mot[1] = ARC(EM1D_BIN_22F);
        } else {
            w->mot[0] = ARC(EM1D_TPL_248);
            w->mot[1] = ARC(EM1D_BIN_247);
        }
        w->mot[2] = ARC(EM1D_BIN_249);
        w->mot[3] = ARC(EM1D_BIN_233);
        w->mot[4] = ARC(EM1D_BIN_233);
        w->mot[5] = ARC(EM1D_TPL_24A);
        w->mot[6] = ARC(EM1D_BIN_234);
        w->mot[7] = ARC(EM1D_BIN_235);
        w->mot[8] = ARC(EM1D_BIN_236);
        w->mot[9] = ARC(EM1D_BIN_237);
        w->mot[10] = ARC(EM1D_BIN_237);
        w->mot[11] = ARC(EM1D_BIN_238);
        w->mot[12] = ARC(EM1D_BIN_239);
        w->mot[13] = ARC(EM1D_BIN_23A);
        w->mot[14] = ARC(EM1D_BIN_23B);
        w->mot[15] = ARC(EM1D_BIN_23C);
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
    case 16:
    case 20:
        if (em->type == 16) {
            w->mot[0] = ARC(EM1D_TPL_230);
            w->mot[1] = ARC(EM1D_BIN_22F);
        } else {
            w->mot[0] = ARC(EM1D_TPL_248);
            w->mot[1] = ARC(EM1D_BIN_247);
        }
        w->mot[2] = ARC(EM1D_BIN_24B);
        w->mot[3] = ARC(EM1D_BIN_233);
        w->mot[4] = ARC(EM1D_BIN_233);
        w->mot[5] = ARC(EM1D_TPL_24C);
        w->mot[6] = ARC(EM1D_BIN_234);
        w->mot[7] = ARC(EM1D_BIN_235);
        w->mot[8] = ARC(EM1D_BIN_236);
        w->mot[9] = ARC(EM1D_BIN_237);
        w->mot[10] = ARC(EM1D_BIN_237);
        w->mot[11] = ARC(EM1D_BIN_238);
        w->mot[12] = ARC(EM1D_BIN_239);
        w->mot[13] = ARC(EM1D_BIN_23A);
        w->mot[14] = ARC(EM1D_BIN_23B);
        w->mot[15] = ARC(EM1D_BIN_23C);
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
    case 17:
        w->mot[0] = ARC(EM1D_TPL_230);
        w->mot[1] = ARC(EM1D_BIN_22F);
        // types 17 and 21 share the rest of the arm through this label: the target's
        // case 21 block starts at mot[2] (its zero-store schedule is the other arms'),
        // which two full copies of the body do not give (the cross-jump happens after sched1)
        goto common17;
    case 21:
        w->mot[0] = ARC(EM1D_TPL_248);
        w->mot[1] = ARC(EM1D_BIN_247);
    common17:
        w->mot[2] = ARC(EM1D_BIN_24D);
        w->mot[3] = ARC(EM1D_BIN_233);
        w->mot[4] = ARC(EM1D_BIN_233);
        w->mot[5] = ARC(EM1D_TPL_24E);
        w->mot[6] = ARC(EM1D_BIN_234);
        w->mot[7] = ARC(EM1D_BIN_235);
        w->mot[8] = ARC(EM1D_BIN_236);
        w->mot[9] = ARC(EM1D_BIN_237);
        w->mot[10] = ARC(EM1D_BIN_237);
        w->mot[11] = ARC(EM1D_BIN_238);
        w->mot[12] = ARC(EM1D_BIN_239);
        w->mot[13] = ARC(EM1D_BIN_23A);
        w->mot[14] = ARC(EM1D_BIN_23B);
        w->mot[15] = ARC(EM1D_BIN_23C);
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
    case 2:
        w->mot[0] = ARC(EM1D_TPL_22C);
        w->mot[1] = ARC(EM1D_BIN_22B);
        w->mot[2] = 0;
        w->mot[3] = 0;
        w->mot[4] = 0;
        w->mot[5] = 0;
        w->mot[6] = 0;
        w->mot[7] = 0;
        w->mot[8] = 0;
        w->mot[9] = 0;
        w->mot[10] = 0;
        w->mot[11] = 0;
        w->mot[12] = 0;
        w->mot[13] = 0;
        w->mot[14] = 0;
        w->mot[15] = 0;
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
    }
    w->Ganado = 2;
    Em1dWeaponSet(em);
    EspDataLoad((u32) ARC(EM1D_EFF_1A4), EFF_OBM66, 0);
    EspDataLoad((u32) ARC(EM1D_EFF_278), EFF_OBM83, 0);
}

// Weapon model table of the module: mot[41..78] = the bin / tpl pairs em10MakeWeapon uses (hoe, bucket
// and its motions, sickle, hatchet / flail, chainsaw (the real saw only for the chainsaw type), scythe /
// stun rod, torch, bowgun and arrow, pitchfork); 0 = the weapon does not exist in this island module.
void Em1dWeaponSet(cEm10* em)
{
    Em10Work* w = EM10_WK(em);

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
    w->mot[63] = ARC(EM1D_BIN_WEAPON_SET_26C);
    w->mot[64] = ARC(EM1D_TPL_WEAPON_SET_26D);
    w->mot[65] = ARC(EM1D_BIN_WEAPON_SET_276);
    w->mot[66] = ARC(EM1D_TPL_WEAPON_SET_277);
    w->mot[67] = 0;
    w->mot[68] = 0;
    w->mot[69] = ARC(EM1D_BIN_WEAPON_SET_26A);
    w->mot[70] = ARC(EM1D_TPL_WEAPON_SET_26B);
    w->mot[71] = ARC(EM1D_BIN_WEAPON_SET_270);
    w->mot[72] = ARC(EM1D_TPL_WEAPON_SET_271);
    w->mot[73] = ARC(EM1D_BIN_WEAPON_SET_272);
    w->mot[74] = ARC(EM1D_TPL_WEAPON_SET_273);
    w->mot[75] = ARC(EM1D_BIN_WEAPON_SET_274);
    w->mot[76] = ARC(EM1D_TPL_WEAPON_SET_275);
    w->mot[77] = 0;
    w->mot[78] = 0;
}
