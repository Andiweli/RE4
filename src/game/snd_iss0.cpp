// game/snd_iss0: sound driver request entry — Snd_iss_req_para turns a block / SE number (an
// ISS block's SIT entry) into a request in the request bank that the audio frame picks up
// (snd_iss1); chained SITs (flag 0x2000) queue several requests under one sound id.
#include "snd_drv.h"

// Plays SE / sequence `req_no` of ISS block `blk_no` with the parameters left in Snd_ctrl_work
// (pan / volume / pitch overrides, ovr_flag); `para` (optional) receives the surround type.
// Returns the new sound id, 0 when nothing was queued.
int Snd_iss_req_para(u16 blk_no, u16 req_no, u8* para)
{
    int ret;

    ret = req_iss_main(blk_no, req_no, para);
    return ret;
}

// Allocates a sound id and queues one request per SIT of the chain (flag 0x2000 = the next SIT
// belongs to the same sound); dummy SITs (0x8000) and unknown numbers are refused.
int req_iss_main(u16 blk_no, u16 req_no, u8* para)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;
    SND_SIT* sit;

    ctrl->IsLink = 0;
    if (req_no >= Snd_iss_blk[blk_no].sit_num) {
        OSReport("SND REQ_NO is not found.\n");
        OSReport("BLK_NO : %d / REQ_NO : %d\n", blk_no, req_no);
        return 0;
    }
    ctrl->snd_id++;
    if (ctrl->snd_id == 0) {
        ctrl->snd_id++;
    }
    while (1) {
        sit = Snd_iss_blk[blk_no].sit_adrs;
        sit += req_no;
        if (sit->flag & 0x8000) {
            OSReport("SND REQ_NO is dummy data.\n");
            OSReport("BLK_NO : %d / REQ_NO : %d\n", blk_no, req_no);
            return 0;
        }
        req_set_srd_type(ctrl, sit, para);
        if (req_iss_one(ctrl, sit, blk_no, req_no) == 1) {
            return 0;
        }
        if (!(sit->flag & 0x2000)) {
            return ctrl->snd_id;
        }
        ctrl->IsLink = 1;
        req_no++;
    }
}

// The surround type of the request: the override (ovr_flag 0x100) or the SIT's.
void req_set_srd_type(SND_CTRL* ctrl, SND_SIT* sit, u8* para)
{
    if (ctrl->para_flag & 0x100) {
        ctrl->now_srd_type = ctrl->srd_type;
    } else {
        ctrl->now_srd_type = sit->srd_type;
    }
    if (para != NULL) {
        *para = ctrl->now_srd_type;
    }
}

// Queues one request with interrupts off. Returns 1 when the request bank is full.
int req_iss_one(SND_CTRL* ctrl, SND_SIT* sit, u16 blk_no, u16 req_no)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = req_iss_one_sub(ctrl, sit, blk_no, req_no);
    OSRestoreInterrupts(old);
    return ret;
}

// Fills a free request slot: type 1 (one-shot SE, SIT flag 0x100) or 2, block / number / id / SIT
// and the control parameters copied in.
int req_iss_one_sub(SND_CTRL* ctrl, SND_SIT* sit, u16 blk_no, u16 req_no)
{
    SND_REQ* req;

    req = Snd_open_req_work();
    if (req == NULL) {
        return 1;
    }
    req->be_flag = 1;
    if (sit->flag & 0x100) {
        req->use_type = 1;
    } else {
        req->use_type = 2;
    }
    req->srd_type = ctrl->now_srd_type;
    req->blk_no = blk_no;
    req->req_no = req_no;
    req->snd_id = ctrl->snd_id;
    req->sit_ptr = sit;
    Snd_req_work_copy_para(ctrl, req);
    return 0;
}

// What sound id `snd_id` is: 1 SE (pending or on a voice), 2 sequence, 4 stream, 0 unknown / done.
int Snd_get_play_type(u32 snd_id)
{
    SND_REQ* req;
    SND_VOICE* voice;
    SND_SEQ* seq;
    SND_STR* str;
    int old;
    int type = 0;

    old = OSDisableInterrupts();
    req = Snd_search_req_work_snd_id(snd_id, 3);
    if (req != NULL) {
        if (req->sit_ptr->flag & 0x4) {
            type = 2;
        } else {
            type = 1;
        }
    }
    OSRestoreInterrupts(old);
    if (type != 0) {
        return type;
    }
    old = OSDisableInterrupts();
    voice = Snd_search_voice_work_snd_id(snd_id);
    OSRestoreInterrupts(old);
    if (voice != NULL) {
        return 1;
    }
    seq = Snd_search_seq_work_snd_id(snd_id);
    if (seq != NULL) {
        return 2;
    }
    str = Snd_search_str_work_snd_id(snd_id);
    if (str != NULL) {
        return 4;
    }
    return 0;
}
