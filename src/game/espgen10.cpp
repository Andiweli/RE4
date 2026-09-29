// game/espgen10 (D:/Bio4/Prog/espgen10.cpp): effect controller 10, which plays an effect sequence
// record by record. EstSet (est.cpp) creates these controllers.
#include "atari.h"
#include "light.h"
#include "esp.h"
#include "espgen.h"
#include "math_sub.h"
#include "db_log.h"

void espgen10_Update(cEspgen* w);
void espgen10_Move00(cEspgen* w);
void espgen10_Move01(cEspgen* w);

// Spawns record `no` of the sequence: Kind 0 -> one esp via EspSeqSet (pos is passed only when
// flag != 0), Kind 1 -> a controller via EspgenSeqSet. In event mode (Core_flg 0x1000) the parent
// model comes from EspEvModList[Parent_no]. Returns 0 when the spawn failed (pool full / bad kind).
BOOL EspgenDataSet(cEspSeqHead* pSeqHed, u32 seq_ptr, cEffectCore* pCore, u32* pRand_seed, cModel* pMod, u16 Null_Parts_no, Mtx* pMat,
                   Vec* pOffset, Vec* pAng, ESPSEQ_CONTROL* pSct, BOOL bUseOffset)
{
    cEspSeqTbl* rec;
    BOOL ret = 1;

    rec = &pSeqHed->SeqTbl[seq_ptr];
    if (pCore->Core_flg & 0x1000) {
        u32 no = rec->Parent_no;
        pMod = EspEvModList.GetModelPtr(no);
    }

    switch (rec->Kind) {
    case 0: {
        cEsp* esp;
        if (bUseOffset == 0) {
            pOffset = NULL;
        }
        if (EspSeqSet(rec, pCore, pRand_seed, pMod, pMat, 0, 0.0f, &esp, pSct, pOffset) == 0) {
            ret = 0;
        }
        break;
    }
    case 1:
        if (EspgenSeqSet(pSeqHed, seq_ptr, pCore, pMod, Null_Parts_no, pMat, pOffset, pAng, pSct, bUseOffset) == 0) {
            ret = 0;
        }
        break;
    default:
        pLog->err(0, 0, "ESP_CTRL : KIND[%d] is invalid.", rec->Kind);
        ret = 0;
        break;
    }
    return ret;
}
// Fills a cEffectCore owner block: Core_flg = a, Call_no = b, Core_kind = c, Core_pEm = d,
// owner = e (the ids EfmDelete / EspDelete use to find effects by owner).
void SetEspCore(cEffectCore* pCore, int Core_flg, u32 Call_no, u8 Core_kind, void* Core_pEm, int owner)
{
    pCore->Core_flg = Core_flg;
    pCore->Core_kind = Core_kind;
    pCore->Call_no = Call_no;
    pCore->Core_pEm = Core_pEm;
    pCore->owner = owner;
}

// Takes a free controller from the pool (front == 1: from the front, drawn first) and stamps the
// owner info on it. Returns 0 when the pool is empty.
int PullEspEspgen(cEspgen** ppEspgen, int Core_flg, int Core_kind, u32 Call_no, void* Core_pEm, int owner, int type)
{
    int ret;

    if (type == 1) {
        ret = PullEspgenFront(ppEspgen);
    } else {
        ret = PullEspgen(ppEspgen);
    }
    if (ret) {
        SetEspCore(&(*ppEspgen)->Eff_core, Core_flg, Call_no, Core_kind, Core_pEm, owner);
    }
    return ret;
}

// One sequence frame: kills the controller when the model died or was reused; rebuilds Mat from the
// parts (or Offset/Ang for 0xFE) unless Flg bit 0 says it is fixed; then spawns every record whose
// Set_time == Time_cnt (records must be sorted, otherwise "no SORT" error) and ends the controller
// after the last record.
void espgen10_Update(cEspgen* pEspgen)
{
    ESPGEN10_WK* p = (ESPGEN10_WK*) pEspgen->Free.buff;
    cEspSeqHead* head = p->head;
    cEspSeqTbl* rec = &head->SeqTbl[p->Seq_ptr];
    cModel* model = p->pMod;

    if (model != NULL) {
        if (!model->isAlive() || model->guid != p->Guid_pMod) {
            PushEspgen(pEspgen);
            return;
        }
    }
    if ((p->Null_parts_no >= 0xF8 && p->Null_parts_no <= 0xFD) || p->Null_parts_no == 0xFF) {
        pLog->err(0, 0, "ESP_CTRL10 : PARTS_NO[%x] invalid.", p->Null_parts_no);
        PushEspgen(pEspgen);
        return;
    }
    if (p->Null_parts_no == 0xFE) {
        PSMTXIdentity(p->Mat);
        RotMatrix(p->Mat, &p->Ang);
        p->Mat[0][3] = p->Offset.x;
        p->Mat[1][3] = p->Offset.y;
        p->Mat[2][3] = p->Offset.z;
    } else {
        if (p->pMod == NULL) {
            pLog->err(0, 0, "ESP_CTRL10 : PARTS_NO is set but No Parent.");
            PushEspgen(pEspgen);
            return;
        }
        if (!(p->Flg & 1)) {
            cParts* part;
            Vec ofs;
            Vec r;

            if (p->Null_parts_no >= model->nParts) {
                pLog->err(0, 0, "ESP_CTRL10 : PARTS_NO[%d] is invalid(MAX:%d).", p->Null_parts_no, model->nParts);
                PushEspgen(pEspgen);
                return;
            }
            part = model->getPartsPtr(p->Null_parts_no);
            PSMTXIdentity(p->Mat);
            PSVECAdd(&p->Ang, &model->ang, &r);
            RotMatrix(p->Mat, &r);
            PSMTXMultVecSR(p->Mat, &p->Offset, &ofs);
            p->Mat[0][3] = part->mat[0][3] + ofs.x;
            p->Mat[1][3] = part->mat[1][3] + ofs.y;
            p->Mat[2][3] = part->mat[2][3] + ofs.z;
            if (!(head->flags & 1)) {
                p->Flg |= 1;
            }
        }
    }
    if (rec->Set_time < p->Time_cnt) {
        pLog->err(0, 0, "ESP_ESTSET : DATA[%d] is no SORT.", p->Seq_ptr);
        PushEspgen(pEspgen);
        return;
    }
    while (rec->Set_time == p->Time_cnt) {
        int flag = 0;
        if (p->Flg & 2) {
            flag = 1;
        }
        if (!EspgenDataSet(head, p->Seq_ptr, &pEspgen->Eff_core, &p->Rand_seed, p->pMod, p->Null_parts_no, &p->Mat, &p->Offset, &p->Ang, p->p8,
                           flag)) {
            return;
        }
        p->Seq_ptr++;
        rec++;
        if (p->Seq_ptr >= head->num) {
            PushEspgen(pEspgen);
            break;
        }
    }
    p->Time_cnt++;
}

// Step 0 of Espgen10MoveTbl: first frame, then step 1.
void espgen10_Move00(cEspgen* pEspgen)
{
    espgen10_Update(pEspgen);
    pEspgen->Rno0 = 1;
}

// Step 1 of Espgen10MoveTbl: steady state.
void espgen10_Move01(cEspgen* pEspgen)
{
    espgen10_Update(pEspgen);
}

// EspgenMoveTbl entry for controller type 0x10: dispatches on w->Rno0.
void Espgen10_Move(cEspgen* pEspgen)
{
    static void (*Espgen10MoveTbl[])(cEspgen*) = {espgen10_Move00, espgen10_Move01};

    Espgen10MoveTbl[pEspgen->Rno0](pEspgen);
}
