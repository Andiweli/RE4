// game/snd_iss2: sound driver audio-frame side of the SE requests. Snd_iss_manager runs every 5 ms
// and applies the global SE controls and the pending requests to the AX voices.
#include "snd_drv.h"

typedef void (*SND_REQ_CMD)(SND_AXV*, SND_REQ*, u16);

// Audio frame: frees finished AX voices, then (unless a reset is in progress) applies the global
// SE controls and executes the request bank; finally pushes the pending AX voice updates.
void Snd_iss_manager(void)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;
    int ret;

    Snd_axv_work_close_check();
    ret = Snd_se_reset_check(ctrl);
    if (ret == 0) {
        se_ctrl_execute(ctrl);
        iss_req_execute(ctrl);
    }
    Snd_axv_work_control();
}

// Runs the se_ctrl bits set by snd_iss1 (fade-outs, pauses, resumes, volume down / up, pan or
// volume reset) and clears them.
void se_ctrl_execute(SND_CTRL* ctrl)
{
    if (ctrl->se_ctrl_flag & 0x200) {
        se_ctrl_fade_out(ctrl, 1);
    }
    if (ctrl->se_ctrl_flag & 0x400) {
        se_ctrl_fade_out(ctrl, 2);
    }
    if (ctrl->se_ctrl_flag & 0x1) {
        se_ctrl_pause_on(ctrl);
    }
    if (ctrl->se_ctrl_flag & 0x2) {
        se_ctrl_pause_on2(ctrl);
    }
    if (ctrl->se_ctrl_flag & 0x4) {
        se_ctrl_pause_on(ctrl);
    }
    if (ctrl->se_ctrl_flag & 0x8) {
        se_ctrl_pause_off(ctrl);
    }
    if (ctrl->se_ctrl_flag & 0x10) {
        se_ctrl_pause_off2(ctrl);
    }
    if (ctrl->se_ctrl_flag & 0x20) {
        se_ctrl_vdown_on(ctrl);
    }
    if (ctrl->se_ctrl_flag & 0x40) {
        se_ctrl_vdown_off(ctrl);
    }
    if (ctrl->se_ctrl_flag & 0x80) {
        se_ctrl_reset_pan_or_vol(ctrl, 3);
    }
    if (ctrl->se_ctrl_flag & 0x100) {
        se_ctrl_reset_pan_or_vol(ctrl, 4);
    }
    ctrl->se_ctrl_flag = 0;
}

// Note-off with a fade (se_fade_time, or the voice's own release) on every SE voice; mode 1 skips
// the protected voices (axv flag 4).
void se_ctrl_fade_out(SND_CTRL* ctrl, int mode)
{
    SND_AXV* axv;
    SND_VOICE* vw;
    int i;
    s32 time;

    for (i = 0; i < SND_VOICE_MAX; i++) {
        vw = &Snd_voice_work[i];
        if (vw->be_flag == 0) {
            continue;
        }
        if (vw->play_type != 1) {
            continue;
        }
        axv = vw->axv_ptr;
        if (axv == NULL) {
            continue;
        }
        if (mode == 1 && (axv->req_bit & 0x4)) {
            continue;
        }
        if (ctrl->se_fout_time == 0) {
            time = vw->adsr_rel;
        } else {
            time = ctrl->se_fout_time;
        }
        Snd_axv_work_note_off(axv, time);
    }
}

// Pauses every AX voice.
void se_ctrl_pause_on(SND_CTRL* ctrl)
{
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        seCtrlPauseOn_sub(&Snd_axv_work[i], ctrl);
    }
}

// Pauses the AX voices of block se_pause_type (-1 = all).
void se_ctrl_pause_on2(SND_CTRL* ctrl)
{
    SND_AXV* axv;
    SND_VOICE* vw;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (ctrl->pause_blk != -1) {
            if (axv->be_flag != 0) {
                vw = axv->voice_adrs;
                if (vw != NULL && vw->blk_no == ctrl->pause_blk) {
                    seCtrlPauseOn_sub(axv, ctrl);
                }
            }
        } else {
            seCtrlPauseOn_sub(axv, ctrl);
        }
    }
}

// Pauses one AX voice (status bit3, update 0x100) unless it is releasing, unpausable (flag 1, when
// not forced by se_ctrl 4), stopped, or a one-shot within 800 samples of its end.
void seCtrlPauseOn_sub(SND_AXV* axv, SND_CTRL* ctrl)
{
    SND_VOICE* vw;
    u32 cur;
    u32 end;

    if (axv->be_flag == 0) {
        return;
    }
    if (axv->be_flag & 0x4) {
        return;
    }
    if (!(ctrl->se_ctrl_flag & 0x4) && (axv->req_bit & 0x1)) {
        return;
    }
    if (axv->ax_voice->pb.state == 0) {
        return;
    }
    if (axv->ax_voice->pb.addr.loopFlag == 0) {
        cur = *(u32*) &axv->ax_voice->pb.addr.currentAddressHi;
        end = *(u32*) &axv->ax_voice->pb.addr.endAddressHi;
        if (end - cur <= 800) {
            return;
        }
    }
    axv->be_flag |= 0x8;
    axv->update |= 0x100;
    vw = axv->voice_adrs;
    if (vw != NULL) {
        vw->be_flag |= 0x2;
    }
}

// Resumes every AX voice; se_state bit0 off.
void se_ctrl_pause_off(SND_CTRL* ctrl)
{
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        seCtrlPauseOff_sub(&Snd_axv_work[i]);
    }
    ctrl->status_flag &= ~0x1;
}

// Resumes the AX voices of block se_pause_type (-1 = all).
void se_ctrl_pause_off2(SND_CTRL* ctrl)
{
    SND_AXV* axv;
    SND_VOICE* vw;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (ctrl->pause_blk != -1) {
            if (axv->be_flag != 0) {
                vw = axv->voice_adrs;
                if (vw != NULL && vw->blk_no == ctrl->pause_blk) {
                    seCtrlPauseOff_sub(axv);
                }
            }
        } else {
            seCtrlPauseOff_sub(axv);
        }
    }
}

// Resumes one paused AX voice (update 0x200).
void seCtrlPauseOff_sub(SND_AXV* axv)
{
    SND_VOICE* vw;

    if (axv->be_flag == 0) {
        return;
    }
    if (axv->be_flag & 0x8) {
        axv->be_flag &= ~0x8;
        axv->update |= 0x200;
        vw = axv->voice_adrs;
        if (vw != NULL) {
            vw->be_flag &= ~0x2;
        }
    }
}

// Volume-down on every AX voice not flagged exempt (flag 2): status bit4, volume recomputed.
void se_ctrl_vdown_on(SND_CTRL* ctrl)
{
    SND_AXV* axv;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axv->be_flag == 0) {
            continue;
        }
        if (axv->be_flag & 0x4) {
            continue;
        }
        if (axv->req_bit & 0x2) {
            continue;
        }
        axv->be_flag |= 0x10;
        axv->update |= 0x1;
        Snd_axv_work_calc_vdown_vol(axv);
    }
}

// Ends the volume-down on every AX voice; se_state bit1 off.
void se_ctrl_vdown_off(SND_CTRL* ctrl)
{
    SND_AXV* axv;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axv->be_flag == 0) {
            continue;
        }
        if ((axv->be_flag & 0x10) == 0) {
            continue;
        }
        axv->be_flag &= ~0x10;
        axv->update |= 0x1;
    }
    ctrl->status_flag &= ~0x2;
}

// Marks every AX voice for a pan (mode 3) or volume (4) recomputation (output mode change).
void se_ctrl_reset_pan_or_vol(SND_CTRL* ctrl, int mode)
{
    SND_AXV* axv;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axv->be_flag == 0) {
            continue;
        }
        if (axv->be_flag & 0x4) {
            continue;
        }
        if (mode == 3) {
            axv->update |= 0x2;
        } else {
            axv->update |= 0x1;
        }
    }
}

// Executes every request of the back bank (commands or new plays), then swaps the banks.
void iss_req_execute(SND_CTRL* ctrl)
{
    SND_REQ* req;
    int i;

    for (i = 0; i < SND_REQ_MAX; i++) {
        req = &Snd_req_work[ctrl->req_exec_idx][i];
        if (req->be_flag == 0) {
            continue;
        }
        if (req->use_type & 0x4) {
            iss_req_command(req);
        } else {
            Snd_req_iss_new_play(req);
        }
        req->be_flag = 0;
    }
    ctrl->req_push_idx ^= 1;
    ctrl->req_exec_idx ^= 1;
}

// A type 4 request: cmd 0 stop, else set parameters.
void iss_req_command(SND_REQ* req)
{
    if (req->cmd_no == 0) {
        req_cmd_se_stop(req);
    } else {
        req_cmd_se_para(req);
    }
}

// Stops every SE voice with the request's sound id.
void req_cmd_se_stop(SND_REQ* req)
{
    SND_VOICE* vw;
    int i;

    for (i = 0; i < SND_VOICE_MAX; i++) {
        vw = &Snd_voice_work[i];
        if (vw->be_flag == 0) {
            continue;
        }
        if (vw->snd_id != req->snd_id) {
            continue;
        }
        if (vw->play_type != 1) {
            continue;
        }
        Snd_stop_voice_work(vw);
    }
}

// Applies the request's parameter bits (req->flag: pan, span, vol, svol, AUX A / B, filter, pitch
// add / offset) to every AX voice of the sound id.
void req_cmd_se_para(SND_REQ* req)
{
    SND_VOICE* vw;
    SND_AXV* axv;
    int i;
    int j;
    u16 bit;
    static SND_REQ_CMD req_cmd_se_tbl[] = {
        NULL,           req_cmd_se_pan, req_cmd_se_pan, req_cmd_se_vol,   req_cmd_se_vol,   req_cmd_se_aux, req_cmd_se_aux,
        req_cmd_se_lpf, NULL,           req_cmd_se_pitch, req_cmd_se_pitch, NULL,           NULL,
    };

    for (i = 0; i < SND_VOICE_MAX; i++) {
        vw = &Snd_voice_work[i];
        if (vw->be_flag == 0) {
            continue;
        }
        if (vw->snd_id != req->snd_id) {
            continue;
        }
        if (vw->play_type != 1) {
            continue;
        }
        axv = vw->axv_ptr;
        if (axv == NULL) {
            continue;
        }
        bit = 2;
        for (j = 1; j <= 10; j++) {
            if ((req->flag & bit) && req_cmd_se_tbl[j] != NULL) {
                req_cmd_se_tbl[j](axv, req, bit);
            }
            bit <<= 1;
        }
    }
}

// New pan (bit 2) or surround pan (bit 4).
void req_cmd_se_pan(SND_AXV* axv, SND_REQ* req, u16 bit)
{
    if (bit == 0x2) {
        axv->out_pan = req->pan;
    } else {
        axv->srd_span = req->span;
    }
    axv->update |= 0x2;
}

// New volume (bit 8) or surround volume (bit 0x10), also as the volume-down source.
void req_cmd_se_vol(SND_AXV* axv, SND_REQ* req, u16 bit)
{
    if (bit == 0x8) {
        axv->ste_vol = req->vol << 8;
        axv->sv_ste_vol = axv->ste_vol;
    } else {
        axv->srd_vol = req->svol << 8;
        axv->sv_srd_vol = axv->srd_vol;
    }
    axv->update |= 0x1;
}

// New AUX A (bit 0x20) or AUX B (0x40) send.
void req_cmd_se_aux(SND_AXV* axv, SND_REQ* req, u16 bit)
{
    if (bit == 0x20) {
        axv->out_aux_a = req->aux_a;
        axv->update |= 0x4;
    } else {
        axv->out_aux_b = req->aux_b;
        axv->update |= 0x8;
    }
}

// Low-pass filter on / off / changed (lpf_no -1 = off).
void req_cmd_se_lpf(SND_AXV* axv, SND_REQ* req, u16 bit)
{
    if (axv->lpf_flag == 0) {
        if (req->lpf == -1) {
            return;
        }
        axv->lpf_flag = 1;
        axv->lpf_freq = req->lpf;
        axv->update |= 0x10;
    } else {
        if (req->lpf == -1) {
            axv->lpf_flag = 0;
            axv->lpf_freq = -1;
            axv->update |= 0x10;
        } else {
            axv->lpf_freq = req->lpf;
            axv->update |= 0x20;
        }
    }
}

// Pitch: bit 0x200 adds to the base, 0x400 sets the offset; total clamped to +-2400 cents.
void req_cmd_se_pitch(SND_AXV* axv, SND_REQ* req, u16 bit)
{
    if (bit == 0x200) {
        axv->org_pitch += req->pitch;
    } else {
        axv->dop_pitch = req->dop_p;
    }
    axv->out_pitch = axv->org_pitch + axv->dop_pitch;
    if (axv->out_pitch > 2400) {
        OSReport("OUT PITCH HIGH over : %d\n", axv->out_pitch);
        axv->out_pitch = 2400;
    }
    if (axv->out_pitch < -2400) {
        OSReport("OUT PITCH LOW  over : %d\n", axv->out_pitch);
        axv->out_pitch = -2400;
    }
    axv->update |= 0x40;
}
