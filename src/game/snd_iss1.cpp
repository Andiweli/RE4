// game/snd_iss1: sound driver SE control requests — parameter updates, stop, end check and the
// global SE controls (fade-out all, pause / resume by block type, volume-down); each only sets a
// bit in Snd_ctrl_work.se_ctrl or queues a type 4 request that the audio frame executes (snd_iss2).
#include "snd_drv.h"

// Applies the parameters in Snd_ctrl_work (ovr_flag bits: pan, volume, AUX, filter, pitch) to the
// playing SE `snd_id`. Returns 1 when the request bank is full.
int Snd_se_set_paras(u32 snd_id)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = se_set_paras_sub(snd_id);
    OSRestoreInterrupts(old);
    return ret;
}

// Queues the type 4 / cmd 1 (set parameters) request with the control work copied in.
int se_set_paras_sub(u32 snd_id)
{
    SND_REQ* req;

    req = Snd_open_req_work();
    if (req == NULL) {
        return 1;
    }
    req->be_flag = 1;
    req->use_type = 4;
    req->cmd_no = 1;
    req->snd_id = snd_id;
    Snd_req_work_copy_para(&Snd_ctrl_work, req);
    return 0;
}

// -1 while the SE is still queued, 1 while a voice plays it, 0 when it is gone.
int Snd_se_end_check(u32 snd_id)
{
    SND_REQ* req;
    SND_VOICE* voice;
    int old;

    old = OSDisableInterrupts();
    req = Snd_search_req_work_snd_id(snd_id, 2);
    OSRestoreInterrupts(old);
    if (req != NULL) {
        return -1;
    }
    old = OSDisableInterrupts();
    voice = Snd_search_voice_work_snd_id(snd_id);
    OSRestoreInterrupts(old);
    if (voice == NULL) {
        return 0;
    }
    return 1;
}

// Stops SE `snd_id` (type 4 / cmd 0 request). Returns 1 when the bank is full.
int Snd_se_stop_one(u32 snd_id)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = se_cmd_req_work(0, snd_id, 0);
    OSRestoreInterrupts(old);
    return ret;
}

// Queues a type 4 command request (0 stop, 1 set parameters) for a sound id.
int se_cmd_req_work(u16 cmd, u32 snd_id, u16 para)
{
    SND_REQ* req;

    req = Snd_open_req_work();
    if (req == NULL) {
        return 1;
    }
    req->be_flag = 1;
    req->use_type = 4;
    req->cmd_no = cmd;
    req->snd_id = snd_id;
    req->para = para;
    return 0;
}

// Fades every SE out over `time` (5 ms units), except the ones flagged to survive (axv flag 4).
int Snd_se_fade_out_all(s16 time)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = se_ctrl_sub(0x200, time);
    OSRestoreInterrupts(old);
    return ret;
}

// Fades every SE out, including the protected ones.
int Snd_se_fade_out_all2(s16 time)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = se_ctrl_sub(0x400, time);
    OSRestoreInterrupts(old);
    return ret;
}

// Sets a global SE control bit for the next audio frame: 0x200 / 0x400 fade-out (para = time),
// 1 pause all, 2 pause block type `para`, 4 pause all unconditionally, 8 resume all, 0x10 resume
// block type, 0x20 volume down to `para`, 0x40 volume back. Refused (1) during a reset.
int se_ctrl_sub(u16 cmd, s16 para)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;

    if (ctrl->reset_flag & 0x10) {
        return 1;
    }
    switch (cmd) {
    case 0x200:
        ctrl->se_ctrl_flag |= 0x200;
        ctrl->se_fout_time = para;
        break;
    case 0x400:
        ctrl->se_ctrl_flag |= 0x400;
        ctrl->se_fout_time = para;
        break;
    case 0x1:
        ctrl->se_ctrl_flag |= 0x1;
        ctrl->status_flag |= 0x1;
        break;
    case 0x2:
        ctrl->se_ctrl_flag |= 0x2;
        ctrl->pause_blk = para;
        break;
    case 0x4:
        ctrl->se_ctrl_flag |= 0x4;
        ctrl->pause_blk = para;
        break;
    case 0x8:
        ctrl->se_ctrl_flag |= 0x8;
        break;
    case 0x10:
        ctrl->se_ctrl_flag |= 0x10;
        ctrl->pause_blk = para;
        break;
    case 0x20:
        ctrl->se_ctrl_flag |= 0x20;
        ctrl->status_flag |= 0x2;
        ctrl->vdown_value = para;
        break;
    case 0x40:
        ctrl->se_ctrl_flag |= 0x40;
        break;
    }
    return 0;
}

// Non-zero while any SE is queued (0x10) or any AX voice is sounding (1).
int Snd_se_pronounce_ck_all(void)
{
    SND_CTRL* ctrl;
    int old;
    int ret;

    old = OSDisableInterrupts();
    ctrl = &Snd_ctrl_work;
    ret = 0;
    ret |= se_pro_ck_req_work(ctrl->req_push_idx);
    ret |= se_pro_ck_req_work(ctrl->req_exec_idx);
    OSRestoreInterrupts(old);
    old = OSDisableInterrupts();
    ret |= se_pro_ck_axv_work();
    OSRestoreInterrupts(old);
    return ret;
}

// 0x10 when the request bank holds a pending SE play (type 2 with a SE SIT).
int se_pro_ck_req_work(int bank)
{
    SND_REQ* req;
    int i;

    for (i = 0; i < SND_REQ_MAX; i++) {
        req = &Snd_req_work[bank][i];
        if (req->be_flag == 0) {
            continue;
        }
        if (req->use_type & 0x4) {
            continue;
        }
        if (!(req->use_type & 0x2)) {
            continue;
        }
        if (req->sit_ptr->flag & 0x3) {
            return 0x10;
        }
    }
    return 0;
}

// 1 when any AX voice is in use.
int se_pro_ck_axv_work(void)
{
    SND_AXV_WORK* axv;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axv->status != 0) {
            return 1;
        }
    }
    return 0;
}

// Pauses the SEs of block `type` (-1 = all) at the next audio frame.
int Snd_se_pause_on2(s16 type)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = se_ctrl_sub(0x2, type);
    OSRestoreInterrupts(old);
    return ret;
}

// Pauses all SEs, including those flagged unpausable (axv flag 1).
int Snd_se_pause_on3(void)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = se_ctrl_sub(0x4, 0);
    OSRestoreInterrupts(old);
    return ret;
}

// Resumes the SEs of block `type` (-1 = all).
int Snd_se_pause_off2(s16 type)
{
    int old;
    int ret;

    old = OSDisableInterrupts();
    ret = se_ctrl_sub(0x10, type);
    OSRestoreInterrupts(old);
    return ret;
}
