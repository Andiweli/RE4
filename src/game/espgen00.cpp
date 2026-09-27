// game/espgen00 (D:/Bio4/Prog/espgen00.cpp): effect controller 00, the repeating emitter, which
// espgen runs through EspgenMoveTbl.
#include "atari.h"
#include "light.h"
#include "global.h"
#include "esp.h"
#include "espgen.h"
#include "math_sub.h"
#include "rnd.h"
#include "db_log.h"

typedef struct tagESPGEN00_WK ESPGEN00_WK;

extern "C" {
void espgen00_UpdateMatrix(cEspgen* w);
void espgen00_Update(cEspgen* w);
void espgen00_Move00(cEspgen* w);
void espgen00_Move01(cEspgen* w);
static f32 Calc_D256(ESPGEN00_WK* p, u8 d, f32 rate);
}

// Effect controller 00: emits one esp record repeatedly (Set_num at a time, every Next_max frames).
typedef struct tagESPGEN00_WK {
    cEspSeqTbl* pSeq;   // 0x14
    cModel* pMod;     // 0x18
    u32 Guid_pMod;        // 0x1C model serial the controller was set up with
    u16 Time_cnt;           // 0x20 frame counter
    u16 Life_max;          // 0x22 life time (0 = infinite)
    u8 Parent_no;      // 0x24  unused here
    u8 Next_max;           // 0x25 frames between emissions
    u8 Next_cnt;        // 0x26 frames left until the next emission
    u8 Set_num;            // 0x27 emissions per frame - 1
    u32 Rand_seed;          // 0x28
    u8 Espgen_flg;          // 0x2C bit0: spread the angle, bit1: fixed seed
    u8 Flg;         // 0x2D bit0 parts matrix fixed, bit1 head flag, bit2 pass the position on
    u8 Null_parts_no;          // 0x2E
    u8 R_inter;        // 0x2F random range added to Next_max
    Mtx Mat;           // 0x30
    Vec Offset;           // 0x60
    Vec Ang;           // 0x6C
    u8 D_size;         // 0x78 rate curve parameters (Calc_D256)
    u8 D_speed;           // 0x79
    u8 D_alpha;           // 0x7A
    u8 D_inter;          // 0x7B
    ESPSEQ_CONTROL Sct;     // 0x7C
    ESPSEQ_CONTROL* pSct;   // 0x98
} ESPGEN00_WK;

// Rebuilds the emitter matrix from parts `Null_parts_no` of pMod (parts rotation + Offset/Ang), once
// when Flg bit 0 is not set (bit 1 = keep following every frame). 0xFE = free position (matrix left
// as set up); 0xF8..0xFD/0xFF or a missing parent kill the controller (PushEspgen) with a log.
void espgen00_UpdateMatrix(cEspgen* pEspgen)
{
    ESPGEN00_WK* p = (ESPGEN00_WK*) pEspgen->Free.buff;
    cModel* model = p->pMod;

    if ((p->Null_parts_no >= 0xF8 && p->Null_parts_no <= 0xFD) || p->Null_parts_no == 0xFF) {
        pLog->err(0, 0, "ESP_CTRL : NULL_PARTS_NO[%x] invalid.", p->Null_parts_no);
        PushEspgen(pEspgen);
        return;
    }
    if (p->Null_parts_no == 0xFE) {
        return;
    }
    if (model == NULL) {
        pLog->err(0, 0, "ESP_CTRL : PARTS_NO is set but No Parent.");
        return;
    }
    if (!(p->Flg & 1)) {
        if (p->Null_parts_no < model->nParts) {
            cParts* part;
            Vec ofs;
            Vec r;

            part = model->getPartsPtr(p->Null_parts_no);
            PSMTXIdentity(p->Mat);
            PSVECAdd(&p->Ang, &model->ang, &r);
            RotMatrix(p->Mat, &r);
            PSMTXMultVecSR(p->Mat, &p->Offset, &ofs);
            p->Mat[0][3] = part->mat[0][3] + ofs.x;
            p->Mat[1][3] = part->mat[1][3] + ofs.y;
            p->Mat[2][3] = part->mat[2][3] + ofs.z;
            if (!(p->Flg & 2)) {
                p->Flg |= 1;
            }
        } else {
            pLog->err(0, 0, "ESP_CTRL : PARTS_NO[%d] is invalid(MAX:%d).", p->Null_parts_no, model->nParts);
            PushEspgen(pEspgen);
            return;
        }
    }
}

// Rate curve: d >= 0 fades 1 -> 1 - d/128 over Life_max, d < 0 grows 1 -> 1 + 10 * -d/128.
static f32 Calc_D256(ESPGEN00_WK* p, u8 d, f32 rate)
{
    s8 v = d;
    f32 t;
    f32 ret;

    // t is assigned in both arms: a global pseudo, so local-alloc does not tie the
    // (f32)d / constant operands to the product (d -> f13, 1/128 -> f12, 1.0 -> f11)
    if (v >= 0) {
        t = (f32) d * 0.0078125f;
        ret = 1.0f - rate * t;
    } else {
        t = (f32) v * -0.0078125f;
        ret = rate * t * 10.0f + 1.0f;
    }
    return ret;
}

// One emitter frame: kills the controller when the model died or was reused (serial mismatch), then
// when Next_cnt reaches 0 emits Set_num+1 copies of the record through EspSeqSet (Espgen_flg bit 0
// spreads them over 2pi), scaling size/speed/alpha by the Life_max-rate curves D_size/D_speed/D_alpha
// and reloading Next_cnt from Next_max (+D_inter curve, +-R_inter). Ends itself after `Life_max` frames.
void espgen00_Update(cEspgen* pEspgen)
{
    ESPGEN00_WK* p = (ESPGEN00_WK*) pEspgen->Free.buff;
    f32 spdR = 0.0f;
    f32 scaleR = 0.0f;
    f32 colR = 0.0f;
    int bScale = 0;
    int bSpd = 0;
    int bCol = 0;
    int add = 0;
    cModel* model = p->pMod;

    if (model != NULL) {
        if (!model->isAlive() || model->guid != p->Guid_pMod) {
            PushEspgen(pEspgen);
            return;
        }
    }
    espgen00_UpdateMatrix(pEspgen);
    if (p->Life_max != 0) {
        f32 rate = (f32) p->Time_cnt / (f32) (int) p->Life_max;

        if (p->D_size) {
            scaleR = Calc_D256(p, p->D_size, rate);
            bScale = 1;
        }
        if (p->D_speed) {
            spdR = Calc_D256(p, p->D_speed, rate);
            bSpd = 1;
        }
        if (p->D_alpha) {
            colR = Calc_D256(p, p->D_alpha, rate);
            bCol = 1;
        }
        if (p->D_inter) {
            add = (int) (rate * (f32) (s8) p->D_inter);
        }
    }
    if (p->Next_cnt == 0) {
        cEspSeqTbl* pSeq = p->pSeq;
        int n;
        int i;

        n = p->Next_max + add;
        if (n < 0) {
            p->Next_cnt = 0;
        } else {
            p->Next_cnt = n;
        }
        if (p->R_inter) {
            int r = Rnd() % (p->R_inter * 2) - p->R_inter;

            if (p->Next_cnt + r < 0) {
                p->Next_cnt = 0;
            } else if (p->Next_cnt + r > 255) {
                p->Next_cnt = 255;
            } else {
                p->Next_cnt = p->Next_cnt + r;
            }
        }
        if (g_pEspSys->nEsp - g_pEspSys->ActiveEspNum < (u32) (p->Set_num + 1)) {
            return;
        }
        {
            f32 step = 6.28f / (f32) (p->Set_num + 1);
            f32 ang = 0.0f;

            for (i = 0; i < p->Set_num + 1; i++) {
                cEsp* esp;
                Vec* pos = NULL;
                int ret;

                if (p->Flg & 4) {
                    pos = &p->Offset;
                }
                if (p->Espgen_flg & 1) {
                    ret = EspSeqSet(pSeq, &pEspgen->Eff_core, &p->Rand_seed, p->pMod, &p->Mat, 1, ang, &esp, p->pSct, pos);
                    ang += step;
                } else {
                    ret = EspSeqSet(pSeq, &pEspgen->Eff_core, &p->Rand_seed, p->pMod, &p->Mat, 0, 0.0f, &esp, p->pSct, pos);
                }
                if (ret) {
                    if (bScale) {
                        esp->m_Size_base_x *= scaleR;
                        esp->m_Size_base_y *= scaleR;
                    }
                    if (bSpd) {
                        PSVECScale(&esp->m_Speed, &esp->m_Speed, spdR);
                    }
                    if (bCol) {
                        f32 a = (f32) (int) esp->m_Col_start_a * colR;

                        if (a > 255.0f) {
                            a = 255.0f;
                        }
                        esp->m_Col_start_a = (u8) a;
                        esp->m_Col_a *= colR;
                    }
                }
            }
        }
    } else {
        p->Next_cnt--;
    }
    p->Time_cnt++;
    if (p->Life_max != 0 && p->Life_max <= p->Time_cnt) {
        PushEspgen(pEspgen);
    }
}

// Step 0 of Espgen00MoveTbl: first frame, then moves to step 1.
void espgen00_Move00(cEspgen* pEspgen)
{
    espgen00_Update(pEspgen);
    pEspgen->Rno0 = 1;
}

// Step 1 of Espgen00MoveTbl: steady state, one Update per frame.
void espgen00_Move01(cEspgen* pEspgen)
{
    espgen00_Update(pEspgen);
}

// EspgenMoveTbl entry for controller type 0: dispatches on w->Rno0; while Status_flg[1] bit
// 0x10000000 (event pause) is set a controller whose model has be_flag 0x800 clear does not run.
void Espgen00_Move(cEspgen* pEspgen)
{
    static void (*Espgen00MoveTbl[])(cEspgen*) = {espgen00_Move00, espgen00_Move01};
    cModel* model = ((ESPGEN00_WK*) pEspgen->Free.buff)->pMod;

    if (model != NULL && (StaFlagChk(pG, STA_SUSPEND))) {
        int susp = !(model->be_flag & 0x800);
        if (susp) {
            return;
        }
    }
    Espgen00MoveTbl[pEspgen->Rno0](pEspgen);
}

// Fills the emitter from the controller record: Life_max (Espgen_work16[0]), Next_max (x10C),
// Set_num (x10D), the D curves (Espgen_work8_2), Espgen_flg, random Next_max range; head flag bit 0
// keeps following the parts, `flag` == 1 passes the position on to the children; fixed seed
// 0x12345678+x10E when Espgen_flg bit 1. Copies the optional ESPSEQ_CONTROL. Always returns 1.
int Espgen00_SetFreeWork(cEspgen* w, cEspSeqTbl* rec, cEspSeqHead* head, cModel* model, u16 parts, Mtx* mtx,
                         Vec* pos, Vec* rot, ESPSEQ_CONTROL* pSct, int flag)
{
    ESPGEN00_WK* p = (ESPGEN00_WK*) w->Free.buff;

    p->pSeq = rec;
    p->pMod = model;
    if (model != NULL) {
        p->Guid_pMod = model->guid;
    } else {
        p->Guid_pMod = (u32) model;
    }
    p->Next_cnt = p->Time_cnt = 0;
    p->Life_max = rec->Espgen_work16[0];
    p->Next_max = rec->Espgen_work8[0];
    p->Set_num = rec->Espgen_work8[1];
    p->D_size = rec->Espgen_work8_2[0];
    p->D_speed = rec->Espgen_work8_2[1];
    p->D_alpha = rec->Espgen_work8_2[2];
    p->D_inter = rec->Espgen_work8_2[3];
    p->Espgen_flg = rec->Espgen_flg;
    p->R_inter = rec->Espgen_work8_3[0];
    if (p->R_inter) {
        p->Next_max += (u32) Rnd() % p->R_inter;
    }
    if (head->flags & 1) {
        p->Flg |= 2;
    }
    if (flag == 1) {
        p->Flg |= 4;
    }
    p->Null_parts_no = parts;
    p->Offset = *pos;
    p->Ang = *rot;
    if (p->Espgen_flg & 2) {
        p->Rand_seed = 0x12345678 + rec->Espgen_work8[2];
    } else {
        p->Rand_seed = Rnd() | (Rnd() << 8) | (Rnd() << 16);
    }
    PSMTXCopy(*mtx, p->Mat);
    if (pSct != NULL) {
        p->pSct = &p->Sct;
        p->Sct = *pSct;
    } else {
        p->pSct = pSct;
    }
    return 1;
}
