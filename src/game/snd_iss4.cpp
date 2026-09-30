// game/snd_iss4: sound driver AX voice works (SND_AXV, one per hardware voice of a SE). Their
// parameter updates are deferred and pushed to the AX / MIX libraries by Snd_axv_work_control.
#include "snd_drv.h"

// Clears the 64 AX voice works (numbered).
void Snd_axv_work_clear(void)
{
    SND_AXV* axv;
    u32 i;
    u32 j;
    u8* p;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        p = (u8*) axv;
        for (j = 0; j < sizeof(SND_AXV); j++) {
            *p++ = 0;
        }
        axv->work_id = i;
    }
}

// A free AX voice work, NULL (with a report) when all 64 are used.
SND_AXV* Snd_open_axv_work(void)
{
    SND_AXV* axv;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axv->be_flag == 0) {
            return axv;
        }
    }
    OSReport("Snd_axv_work is full !!\n");
    return NULL;
}

// Audio frame: frees AX voices whose hardware voice has stopped, and ages the voice works (count).
void Snd_axv_work_close_check(void)
{
    SND_AXV* axv;
    SND_VOICE* vw;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axv->be_flag != 0) {
            axv_close_ck_main(axv);
        }
    }
    for (i = 0; i < SND_VOICE_MAX; i++) {
        vw = &Snd_voice_work[i];
        if (vw->be_flag == 0) {
            continue;
        }
        if (vw->timer != -1) {
            vw->timer++;
        }
    }
}

// Releases one AX voice whose pb.state is stopped (not while paused): MIX channel and AX voice
// freed, the voice work unlinked.
void axv_close_ck_main(SND_AXV* axv)
{
    SND_VOICE* vw;

    if (axv->be_flag & 0x8) {
        return;
    }
    if (axv->ax_voice->pb.state != 0) {
        return;
    }
    MIXReleaseChannel(axv->ax_voice);
    AXFreeVoice(axv->ax_voice);
    vw = axv->voice_adrs;
    if (vw != NULL) {
        vw->be_flag = 0;
        vw->axv_ptr = NULL;
    }
    axv->be_flag = 0;
    axv->voice_adrs = NULL;
    axv->ax_voice = NULL;
}

// Starts the release: envelope ramps to 0 over `time` 5 ms steps, AX priority lowered to 1, the
// voice work is freed at once (status bit2 = releasing).
void Snd_axv_work_note_off(SND_AXV* axv, s32 time)
{
    SND_VOICE* vw;
    u16 mask;
    s16 diff;

    if (axv == NULL) {
        return;
    }
    axv->adsr_rel = time;
    axv->adsr_end = 0;
    diff = axv->adsr_end - axv->adsr_vol;
    axv->adsr_spd = diff / axv->adsr_rel;
    if (axv->adsr_spd == 0) {
        if (diff > 0) {
            axv->adsr_spd = 1;
        } else {
            axv->adsr_spd = -1;
        }
    }
    axv->adsr_ctr = 0;
    AXSetVoicePriority(axv->ax_voice, 1);
    vw = axv->voice_adrs;
    if (vw != NULL) {
        vw->be_flag = 0;
        vw->axv_ptr = NULL;
    }
    mask = 0xA;
    axv->be_flag &= ~mask;
    axv->be_flag |= 0x4;
    axv->voice_adrs = NULL;
}

// 1 when the voice uses the surround (DPL2) parameters: DPL2 output and a surround-type SE.
int Snd_axv_work_get_out_mode(SND_AXV* axv)
{
    if (Snd_get_sound_mode() == 2) {
        if (axv->srd_type == 0) {
            return 0;
        } else {
            return 1;
        }
    } else {
        return 0;
    }
}

// Picks the effective volume: surround or normal, volume-down variant while status bit4.
void Snd_axv_work_choice_now_vol(SND_AXV* axv)
{
    if (Snd_axv_work_get_out_mode(axv) == 1) {
        if (axv->be_flag & 0x10) {
            axv->now_vol = axv->vd_srd_vol;
        } else {
            axv->now_vol = axv->srd_vol;
        }
    } else {
        if (axv->be_flag & 0x10) {
            axv->now_vol = axv->vd_ste_vol;
        } else {
            axv->now_vol = axv->ste_vol;
        }
    }
}

// Volume-down volumes = source volumes scaled by se_vdown_vol / 127.
void Snd_axv_work_calc_vdown_vol(SND_AXV* axv)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;

    axv->vd_ste_vol = axv->sv_ste_vol / 127 * ctrl->vdown_value;
    axv->vd_srd_vol = axv->sv_srd_vol / 127 * ctrl->vdown_value;
}

// AX attenuation from system SE volume x master x the voice volume x the envelope (8.8 fixed).
void Snd_axv_work_calc_ax_vol(SND_AXV* axv)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;

    axv->out_vol = ctrl->vol_mas_se / 127 * (ctrl->vol_iss_se >> 8);
    axv->out_vol = axv->out_vol / 127 * (axv->now_vol >> 8);
    axv->out_vol = axv->out_vol / 127 * (axv->adsr_vol >> 8);
    axv->ax_vol = Snd_vol_syn_to_ax((s16) (axv->out_vol >> 8));
}

// Surround pan actually sent: the voice's span in DPL2 surround mode, else 0x7F.
void Snd_axv_work_choice_out_span(SND_AXV* axv)
{
    if (Snd_axv_work_get_out_mode(axv) == 1) {
        axv->out_span = axv->srd_span;
    } else {
        axv->out_span = 0x7F;
    }
}

// Audio frame: envelope step and pending parameter update of every active AX voice.
void Snd_axv_work_control(void)
{
    SND_AXV* axv;
    int i;

    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axv->be_flag == 0) {
            continue;
        }
        axv_work_adsr(axv);
        axv_work_update(axv);
    }
}

// One envelope step while attacking (status bit1) or releasing (bit2): moves env_vol toward
// env_target; at the target the attack ends or the released voice is stopped.
void axv_work_adsr(SND_AXV* axv)
{
    AXVPB* voice;
    int v;

    if ((axv->be_flag & 0x6) == 0) {
        return;
    }
    voice = axv->ax_voice;
    if (voice->pb.state == 0) {
        return;
    }
    if (axv->adsr_vol == axv->adsr_end) {
        if (axv->be_flag & 0x2) {
            axv->be_flag &= ~0x2;
        } else {
            AXSetVoiceState(voice, 0);
        }
        return;
    }
    v = axv->adsr_vol + axv->adsr_spd;
    if (axv->adsr_spd > 0) {
        if (v > axv->adsr_end) {
            v = axv->adsr_end;
        }
    } else {
        if (v < axv->adsr_end) {
            v = axv->adsr_end;
        }
    }
    axv->adsr_vol = v;
    axv->adsr_ctr++;
    axv->update |= 0x1;
}

// Pushes the upd bits to the hardware: pause (0x100: input muted, voice stopped) / resume (0x200),
// then volume / pan, AUX, LPF, pitch; clears upd.
void axv_work_update(SND_AXV* axv)
{
    if (axv->update & 0x100) {
        axv->ax_vol = Snd_vol_syn_to_ax(0);
        MIXSetInput(axv->ax_voice, axv->ax_vol);
        AXSetVoiceState(axv->ax_voice, 0);
    }
    if (axv->update & 0x200) {
        axv->update |= 0x1;
        AXSetVoiceState(axv->ax_voice, 1);
    }
    axv_work_update_vol_pan(axv);
    axv_work_update_aux(axv);
    axv_work_update_lpf(axv);
    axv_work_update_pitch(axv);
    axv->update = 0;
}

// upd 1: recompute and set the MIX input volume; upd 2: pan and surround pan.
void axv_work_update_vol_pan(SND_AXV* axv)
{
    if (axv->update & 0x1) {
        Snd_axv_work_choice_now_vol(axv);
        Snd_axv_work_calc_ax_vol(axv);
        if (axv->ax_voice->pb.state != 0) {
            MIXSetInput(axv->ax_voice, axv->ax_vol);
        }
    }
    if (axv->update & 0x2) {
        Snd_axv_work_choice_out_span(axv);
        MIXSetPan(axv->ax_voice, axv->out_pan);
        MIXSetSPan(axv->ax_voice, axv->out_span);
    }
}

// upd 4 / 8: AUX A / B send levels.
void axv_work_update_aux(SND_AXV* axv)
{
    if (axv->update & 0x4) {
        axv->ax_aux_a = Snd_vol_syn_to_ax(axv->out_aux_a);
        MIXSetAuxA(axv->ax_voice, axv->ax_aux_a);
    }
    if (axv->update & 0x8) {
        axv->ax_aux_b = Snd_vol_syn_to_ax(axv->out_aux_b);
        MIXSetAuxB(axv->ax_voice, axv->ax_aux_b);
    }
}

// upd 0x10: LPF on / off with the table coefficients; 0x20: new coefficients only.
void axv_work_update_lpf(SND_AXV* axv)
{
    AXPBLPF lpf;
    u16 a0;
    u16 b0;

    if (axv->update & 0x10) {
        if (axv->lpf_flag != 0) {
            lpf.on = 1;
            lpf.yn1 = 0;
            lpf.a0 = Snd_lpf_tbl[axv->lpf_freq].a0;
            lpf.b0 = Snd_lpf_tbl[axv->lpf_freq].b0;
        } else {
            lpf.on = 0;
            lpf.yn1 = 0;
            lpf.a0 = 0;
            lpf.b0 = 0;
        }
        AXSetVoiceLpf(axv->ax_voice, &lpf);
    }
    if (axv->update & 0x20) {
        a0 = Snd_lpf_tbl[axv->lpf_freq].a0;
        b0 = Snd_lpf_tbl[axv->lpf_freq].b0;
        AXSetVoiceLpfCoefs(axv->ax_voice, a0, b0);
    }
}

// upd 0x40: new sample-rate ratio from the pitch in cents (clamped to 4x).
void axv_work_update_pitch(SND_AXV* axv)
{
    f64 ratio;
    u32 r;

    if ((axv->update & 0x40) == 0) {
        return;
    }
    ratio = pow(2.0, (f64) axv->out_pitch / 1200.0);
    ratio = axv->sample_rate * (f32) ratio / 32000.0f;
    r = (u32) (ratio * 65536.0);
    if (r > 0x40000) {
        OSReport("SND Sample Rate Over.\n");
    }
    AXSetVoiceSrcRatio(axv->ax_voice, ratio);
}
