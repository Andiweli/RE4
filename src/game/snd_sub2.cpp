// game/snd_sub2: sound driver request works (SND_REQ, two banks of 64 — the game fills one
// while the audio frame executes the other): reset handling, copying the game's control
// parameters (Snd_ctrl_work ovr_flag bits) into a request, allocation and lookup by sound id.
#include "snd_drv.h"

// Audio frame: on a reset request drops all pending requests, releases every AX voice (1 step)
// and clears the SE controls (reset_flag bit4 = SE side done). Returns 1 while resetting.
int Snd_se_reset_check(SND_CTRL* ctrl)
{
    SND_AXV* axv;
    int i;

    if (ctrl->reset_flag & 0x10) {
        return 1;
    }
    if (ctrl->reset_flag & 0x1) {
        Snd_req_work_clear();
        for (i = 0; i < SND_AXV_MAX; i++) {
            axv = &Snd_axv_work[i];
            if (axv->be_flag != 0) {
                Snd_axv_work_note_off(axv, 1);
            }
        }
        ctrl->reset_flag |= 0x10;
        ctrl->status_flag = 0;
        ctrl->se_ctrl_flag = 0;
        return 1;
    }
    return 0;
}

// Clears both request banks (numbered).
void Snd_req_work_clear(void)
{
    SND_REQ* req;
    u32 i;
    u32 j;
    u32 k;
    u8* p;

    for (i = 0; i < SND_REQ_BANK_MAX; i++) {
        for (j = 0; j < SND_REQ_MAX; j++) {
            req = &Snd_req_work[i][j];
            p = (u8*) req;
            for (k = 0; k < sizeof(SND_REQ); k++) {
                *p++ = 0;
            }
            req->work_id = j;
        }
    }
}

// Copies the game's overrides into the request: ovr_flag bit0 priority, 1 pan, 2 span, 3 vol, 4
// svol, 5 / 6 AUX A / B, 7 LPF, 9 pitch add, 10 pitch offset, 11 se_flag (others -1 = use the
// SIT / DLS); a play request also draws its random pitch (shared across a chained request).
void Snd_req_work_copy_para(SND_CTRL* ctrl, SND_REQ* req)
{
    SND_SIT* sit;

    sit = Snd_iss_blk[req->blk_no].sit_adrs;
    sit += req->req_no;
    req->prio = -1;
    req->pan = -1;
    req->span = -1;
    req->vol = -1;
    req->svol = -1;
    req->aux_a = -1;
    req->aux_b = -1;
    req->lpf = -1;
    req->pitch = 0;
    req->dop_p = 0;
    req->req_bit = 0;
    req->flag = ctrl->para_flag;
    if (req->flag & 0x1) {
        req->prio = ctrl->prio;
    }
    if (req->flag & 0x2) {
        req->pan = ctrl->pan;
    }
    if (req->flag & 0x4) {
        req->span = ctrl->span;
    }
    if (req->flag & 0x8) {
        req->vol = ctrl->vol;
    }
    if (req->flag & 0x10) {
        req->svol = ctrl->svol;
    }
    if (req->flag & 0x20) {
        req->aux_a = ctrl->aux_a;
    }
    if (req->flag & 0x40) {
        req->aux_b = ctrl->aux_b;
    }
    if (req->flag & 0x80) {
        req->lpf = ctrl->lpf;
    }
    if (req->flag & 0x200) {
        req->pitch = ctrl->pitch;
    }
    if (req->flag & 0x400) {
        req->dop_p = ctrl->dop_p;
    }
    if (req->flag & 0x800) {
        req->req_bit = ctrl->req_bit;
    }
    if (req->use_type != 4) {
        if (ctrl->IsLink == 1) {
            req->rnd_pitch = ctrl->rnd_pitch;
        } else {
            req->rnd_pitch = Snd_get_rnd_pitch(sit);
        }
    }
}

// A free slot in the game-side request bank, NULL when full or during a reset.
SND_REQ* Snd_open_req_work(void)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;
    SND_REQ* req;
    int i;

    if (ctrl->reset_flag & 0x10) {
        return NULL;
    }
    for (i = 0; i < SND_REQ_MAX; i++) {
        req = &Snd_req_work[ctrl->req_push_idx][i];
        if (req->be_flag == 0) {
            return req;
        }
    }
    return NULL;
}

// A pending play request (type mask 1 SE / 2 sequence) with sound id `snd_id`, or NULL.
SND_REQ* Snd_search_req_work_snd_id(u32 snd_id, u8 type)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;
    SND_REQ* req;
    int i;
    int j;

    for (i = 0; i < SND_REQ_BANK_MAX; i++) {
        for (j = 0; j < SND_REQ_MAX; j++) {
            req = &Snd_req_work[i][j];
            if (req->be_flag == 0) {
                continue;
            }
            if (req->use_type & 0x4) {
                continue;
            }
            if (!(req->use_type & type)) {
                continue;
            }
            if (req->snd_id != snd_id) {
                continue;
            }
            return req;
        }
    }
    return NULL;
}
