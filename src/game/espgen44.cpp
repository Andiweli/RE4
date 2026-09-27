// game/espgen44: effect controller 44, screen filter parameter setter (D:/Bio4/Prog/espgen44.cpp).
// The record programs Filter05 (Id 0) or Filter06 (Id != 0) from its colour/size fields when set
// up, and resets the filter to zero when the controller is destroyed; it does nothing per frame.
#include "atari.h"
#include "light.h"
#include "esp.h"
#include "espgen.h"
#include "filter.h"

struct Espgen44Work {
    u8 type;           // 0x14 0 = Filter05, 1 = Filter06
};

// EspgenMoveTbl entry for controller type 0x44: nothing per frame (the filter runs on its own).
void Espgen44_Move(cEspgen* pGen)
{
}

// EspgenTransTbl entry: nothing to draw.
void Espgen44_Trans(cEspgen* pGen)
{
}

// Destruct entry: turns the filter the record programmed back off (Filter05SetParam / Filter06SetParam
// with all-zero parameters).
void Espgen44_Destruct(cEspgen* pGen)
{
    Espgen44Work* p = (Espgen44Work*) pGen->Free.buff;

    switch (p->type) {
    case 0:
        Filter05SetParam(0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0, 0);
        break;
    case 1: {
        Vec zero = {0.0f, 0.0f, 0.0f};
        Filter06SetParam(0, 0, 0, 0, 0, 0.0f, 0.0f, &zero, &zero, 0.0f, 0);
        break;
    }
    }
}

// Programs the filter from the record: level = Work8[0]*100+100, colour Col_start_rgba, Col_d_a,
// scale = Size_base_x*0.005; Filter05 (Id 0) takes Blend_type and kind Tex_id, Filter06 takes
// Speed/R_speed as vectors and kind Work8[1].
int Espgen44_SetFreeWork(cEspgen* pGen, cEspSeqTbl* pSeq, cEspSeqHead* pSeqHed, cModel* pMod, u16 Null_parts_no, Mtx* pMat,
                         Vec* pOffset, Vec* pAng, ESPSEQ_CONTROL* pSct)
{
    Espgen44Work* p = (Espgen44Work*) pGen->Free.buff;
    if (pSeq->Id == 0) {
        p->type = 0;
    } else {
        p->type = 1;
    }

    switch (p->type) {
    case 0: {
        int level = (s8) pSeq->Work8[0] * 100 + 100;
        f32 scale = pSeq->Size_base_x * 0.005f;
        int kind = pSeq->Tex_id;
        Filter05SetParam(level, pSeq->Col_start_r, pSeq->Col_start_g, pSeq->Col_start_b, pSeq->Col_start_a, pSeq->Col_d_a, 0.0f, scale, pSeq->Blend_type, kind);
        break;
    }
    case 1: {
        int level = (s8) pSeq->Work8[0] * 100 + 100;
        f32 scale = pSeq->Size_base_x * 0.005f;
        int kind = pSeq->Work8[1];
        Filter06SetParam(level, pSeq->Col_start_r, pSeq->Col_start_g, pSeq->Col_start_b, pSeq->Col_start_a, pSeq->Col_d_a, 0.0f, &pSeq->Speed, &pSeq->R_speed, scale,
                         kind);
        break;
    }
    }
    return 1;
}

// the split object pads .rodata to 8 bytes
asm(".section .rodata; .balign 8");
