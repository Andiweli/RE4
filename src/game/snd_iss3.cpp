// game/snd_iss3: sound driver new-play of a SE. It resolves the SIT's program in the DLS wavetable
// and programs an AX voice, while sequence SITs (flag 4) go to snd_seq* instead.
#include "snd_drv.h"

// Executes a play request: a sequence SIT (flag 4) starts a MIDI sequence, else a sampled SE voice.
void Snd_req_iss_new_play(SND_REQ* req)
{
    SND_IBLK* blk;
    SND_SIT* sit;

    blk = &Snd_iss_blk[req->blk_no];
    sit = blk->sit_adrs;
    sit += req->req_no;
    if (sit->flag & 0x4) {
        Snd_iss_new_seq_work(blk, sit, req);
    } else {
        iss_new_voice_work(blk, sit, req);
    }
}

// Starts one sampled SE: a voice work (by priority, possibly stealing), an AX voice slot and an AX
// voice (priority 30), all parameters set up from the request / SIT / DLS, MIX channel initialised,
// voice running. Silently drops the SE when any resource is exhausted.
void iss_new_voice_work(SND_IBLK* blk, SND_SIT* sit, SND_REQ* req)
{
    SND_VOICE* vw;
    SND_AXV* axv;
    s8 prio;

    if (req->prio >= 0) {
        prio = req->prio;
    } else {
        prio = sit->prio;
    }
    vw = Snd_voice_work_open_ck(sit, prio);
    if (vw == NULL) {
        return;
    }
    axv = Snd_open_axv_work();
    if (axv == NULL) {
        return;
    }
    axv->ax_voice = AXAcquireVoice(30, cb_drop_voice, 0);
    if (axv->ax_voice == NULL) {
        return;
    }
    iss_voice_work_init(vw, axv, req, prio);
    if (sit->flag & 0x1) {
        axv->se_type = 0;
    } else {
        axv->se_type = 1;
    }
    axv->pcm_adrs = blk->pcm_adrs;
    axv->wt_adrs = blk->wt_adrs;
    axv->sit_ptr = sit;
    iss_ax_set_wt_ptr(axv, sit);
    iss_ax_set_adsr(axv);
    iss_ax_set_vol(axv, req, sit);
    iss_ax_set_pan(axv, req, sit);
    iss_ax_set_aux(axv, req, sit);
    iss_ax_set_pitch(axv, req, sit);
    iss_ax_set_lpf(axv, req);
    iss_ax_set_para(axv, req);
    vw->adsr_rel = axv->adsr_rel;
    MIXInitChannel(axv->ax_voice, 0, axv->ax_vol, axv->ax_aux_a, axv->ax_aux_b, axv->out_pan, axv->out_span, 0);
    AXSetVoiceState(axv->ax_voice, 1);
}

// Links the voice work and the AX voice work for the request (type 1 SE, id, block / number,
// priority, the SIT's se_flag into the AX flags).
void iss_voice_work_init(SND_VOICE* vw, SND_AXV* axv, SND_REQ* req, s8 prio)
{
    vw->be_flag = 1;
    vw->snd_id = req->snd_id;
    vw->srd_type = req->srd_type;
    vw->use_type = 2;
    vw->play_type = 1;
    vw->timer = 0;
    vw->axv_ptr = axv;
    vw->blk_no = req->blk_no;
    vw->req_no = req->req_no;
    vw->vprio = prio;
    axv->be_flag = 1;
    axv->snd_id = req->snd_id;
    axv->srd_type = req->srd_type;
    axv->update = 0;
    axv->req_bit = req->req_bit;
    axv->voice_adrs = vw;
}

// Resolves the SIT's program (bank << 8 | note) in the block's DLS: instrument -> key region ->
// articulation, sample and ADPCM coefficients.
void iss_ax_set_wt_ptr(SND_AXV* axv, SND_SIT* sit)
{
    u16 prog;

    prog = sit->note;
    axv->fhp = (WTFILEHEADER*) axv->wt_adrs;
    axv->isp = (WTINST*) (axv->wt_adrs + axv->fhp->offsetMelodicInst);
    axv->isp += (u16) (prog >> 8);
    axv->rgp = (WTREGION*) (axv->wt_adrs + axv->fhp->offsetRegions);
    axv->rgp += axv->isp->keyRegion[prog & 0xFF];
    axv->atp = (WTART*) (axv->wt_adrs + axv->fhp->offsetArticulations);
    axv->atp += axv->rgp->articulationIndex;
    axv->smp = (WTSAMPLE*) (axv->wt_adrs + axv->fhp->offsetSamples);
    axv->smp += axv->rgp->sampleIndex;
    axv->adp = (WTADPCM*) (axv->wt_adrs + axv->fhp->offsetAdpcmContext);
    axv->adp += axv->smp->adpcmIndex;
}

// Envelope from the articulation (when adsr_on): release time from eg1Release, attack ramp in
// 5 ms steps from eg1Attack (status bit1 = attacking); without ADSR full volume at once.
void iss_ax_set_adsr(SND_AXV* axv)
{
    s32 attack;
    u32 steps;

    axv->adsr_atk = 0;
    axv->adsr_rel = 1;
    axv->adsr_vol = 0x7F00;
    axv->adsr_end = 0x7F00;
    axv->adsr_spd = 0;
    axv->adsr_ctr = 0;
    if (axv->se_type != 1) {
        return;
    }
    axv->adsr_rel = (s32) 0xFC400000 / axv->atp->eg1Release;
    attack = axv->atp->eg1Attack;
    if (attack == (s32) 0x80000000) {
        return;
    }
    steps = (u32) (pow(2.0, (f64) attack / 78643200.0) * 1000.0);
    if (steps > 4) {
        axv->adsr_atk = steps / 5;
    } else {
        axv->adsr_atk = 1;
    }
    axv->adsr_vol = 0;
    axv->adsr_spd = axv->adsr_end / axv->adsr_atk;
    axv->be_flag |= 0x2;
}

// Volume / surround volume: request override, else the SIT, else the region attenuation
// (surround = volume); applies a running volume-down (se_state bit1) and computes the AX volume.
void iss_ax_set_vol(SND_AXV* axv, SND_REQ* req, SND_SIT* sit)
{
    s32 vol;

    if (req->vol >= 0) {
        axv->ste_vol = req->vol;
    } else if (sit->vol >= 0) {
        axv->ste_vol = sit->vol;
    } else {
        vol = axv->rgp->attn / 0x10000;
        axv->ste_vol = Snd_vol_ax_to_syn(vol);
    }
    if (req->svol >= 0) {
        axv->srd_vol = req->svol;
    } else if (sit->svol >= 0) {
        axv->srd_vol = sit->svol;
    } else {
        axv->srd_vol = axv->ste_vol;
    }
    axv->ste_vol = axv->ste_vol << 8;
    axv->srd_vol = axv->srd_vol << 8;
    axv->sv_ste_vol = axv->ste_vol;
    axv->sv_srd_vol = axv->srd_vol;
    if ((Snd_ctrl_work.status_flag & 0x2) && !(axv->req_bit & 0x2)) {
        axv->be_flag |= 0x10;
        Snd_axv_work_calc_vdown_vol(axv);
    }
    Snd_axv_work_choice_now_vol(axv);
    Snd_axv_work_calc_ax_vol(axv);
}

// Pan / surround pan: request, else SIT, else the articulation pan / 0x7F.
void iss_ax_set_pan(SND_AXV* axv, SND_REQ* req, SND_SIT* sit)
{
    if (req->pan >= 0) {
        axv->out_pan = req->pan;
    } else if (sit->pan >= 0) {
        axv->out_pan = sit->pan;
    } else {
        axv->out_pan = axv->atp->pan;
    }
    if (req->span >= 0) {
        axv->srd_span = req->span;
    } else if (sit->span >= 0) {
        axv->srd_span = sit->span;
    } else {
        axv->srd_span = 0x7F;
    }
    Snd_axv_work_choice_out_span(axv);
}

// AUX A / B send levels: request, else SIT, else 0; converted to AX attenuation.
void iss_ax_set_aux(SND_AXV* axv, SND_REQ* req, SND_SIT* sit)
{
    if (req->aux_a >= 0) {
        axv->out_aux_a = req->aux_a;
    } else if (sit->aux_a >= 0) {
        axv->out_aux_a = sit->aux_a;
    } else {
        axv->out_aux_a = 0;
    }
    if (req->aux_b >= 0) {
        axv->out_aux_b = req->aux_b;
    } else if (sit->aux_b >= 0) {
        axv->out_aux_b = sit->aux_b;
    } else {
        axv->out_aux_b = 0;
    }
    axv->ax_aux_a = Snd_vol_syn_to_ax(axv->out_aux_a);
    axv->ax_aux_b = Snd_vol_syn_to_ax(axv->out_aux_b);
}

// Pitch in cents: (note - unity note) * 100 + fine tune + the request's pitch / pitch_add as the
// base, plus the request's pitch offset.
void iss_ax_set_pitch(SND_AXV* axv, SND_REQ* req, SND_SIT* sit)
{
    WTREGION* rgn;
    int cents;

    rgn = axv->rgp;
    axv->sample_rate = (f64) axv->smp->sampleRate;
    cents = (u16) (sit->note & 0xFF) - rgn->unityNote;
    cents *= 100;
    cents += rgn->fineTune;
    cents += req->rnd_pitch;
    cents += req->pitch;
    axv->org_pitch = cents;
    axv->dop_pitch = req->dop_p;
    axv->out_pitch = axv->org_pitch + axv->dop_pitch;
}

// Low-pass filter number from the request (-1 = off).
void iss_ax_set_lpf(SND_AXV* axv, SND_REQ* req)
{
    if (req->lpf == -1) {
        axv->lpf_flag = 0;
    } else {
        axv->lpf_flag = 1;
    }
    axv->lpf_freq = req->lpf;
}

// Programs the AX voice: ADPCM sample addresses in ARAM (nibble addressing, 14 samples per 16
// bytes; loop points or the silent zero table as loop for one-shots), coefficients, sample rate
// ratio from the pitch (clamped to 4x), LPF coefficients from Snd_lpf_tbl.
void iss_ax_set_para(SND_AXV* axv, SND_REQ* req)
{
    WTREGION* rgn;
    WTSAMPLE* sample;
    WTADPCM* adpcm;
    AXPBADDR addr;
    AXPBSRC src;
    AXPBADPCM ax_adpcm;
    AXPBADPCMLOOP loop;
    AXPBLPF lpf;
    u32 cur;
    u32 loop_addr;
    u32 end_addr;
    u32 loop_end;
    u32 ratio;
    f64 ratio_f;

    rgn = axv->rgp;
    sample = axv->smp;
    adpcm = axv->adp;
    cur = axv->pcm_adrs * 2 + sample->offset;
    cur += 2;
    if (rgn->loopLength == 0) {
        loop_addr = Snd_ctrl_work.zero_adrs * 2 + 2;
        end_addr = cur;
        end_addr += sample->length / 14 * 16 + sample->length % 14;
        addr.loopFlag = 0;
        addr.format = 0;
    } else {
        loop_addr = cur;
        loop_addr += rgn->loopStart / 14 * 16 + rgn->loopStart % 14;
        loop_end = rgn->loopStart + rgn->loopLength;
        end_addr = cur;
        end_addr += loop_end / 14 * 16 + loop_end % 14;
        addr.loopFlag = 1;
        addr.format = 0;
        loop.loop_pred_scale = adpcm->loop_pred_scale;
        loop.loop_yn1 = adpcm->loop_yn1;
        loop.loop_yn2 = adpcm->loop_yn2;
        AXSetVoiceAdpcmLoop(axv->ax_voice, &loop);
    }
    addr.loopAddressHi = loop_addr >> 16;
    addr.loopAddressLo = loop_addr;
    addr.endAddressHi = end_addr >> 16;
    addr.endAddressLo = end_addr;
    addr.currentAddressHi = cur >> 16;
    addr.currentAddressLo = cur;
    ax_adpcm.a[0][0] = adpcm->a[0][0];
    ax_adpcm.a[1][0] = adpcm->a[1][0];
    ax_adpcm.a[2][0] = adpcm->a[2][0];
    ax_adpcm.a[3][0] = adpcm->a[3][0];
    ax_adpcm.a[4][0] = adpcm->a[4][0];
    ax_adpcm.a[5][0] = adpcm->a[5][0];
    ax_adpcm.a[6][0] = adpcm->a[6][0];
    ax_adpcm.a[7][0] = adpcm->a[7][0];
    ax_adpcm.a[0][1] = adpcm->a[0][1];
    ax_adpcm.a[1][1] = adpcm->a[1][1];
    ax_adpcm.a[2][1] = adpcm->a[2][1];
    ax_adpcm.a[3][1] = adpcm->a[3][1];
    ax_adpcm.a[4][1] = adpcm->a[4][1];
    ax_adpcm.a[5][1] = adpcm->a[5][1];
    ax_adpcm.a[6][1] = adpcm->a[6][1];
    ax_adpcm.a[7][1] = adpcm->a[7][1];
    ax_adpcm.gain = adpcm->gain;
    ax_adpcm.pred_scale = adpcm->pred_scale;
    ax_adpcm.yn1 = adpcm->yn1;
    ax_adpcm.yn2 = adpcm->yn2;
    ratio_f = pow(2.0, (f64) axv->out_pitch / 1200.0);
    ratio_f = axv->sample_rate * (f32) ratio_f / 32000.0f;
    ratio = (u32) (ratio_f * 65536.0);
    if (ratio > 0x40000) {
        OSReport("SND Sample Rate Over.\n");
        OSReport("BLK_NO : %d / REQ_NO : %d\n", req->blk_no, req->req_no);
        ratio = 0x40000;
    }
    src.ratioHi = ratio >> 16;
    src.ratioLo = ratio;
    src.currentAddressFrac = 0;
    src.last_samples[0] = 0;
    src.last_samples[1] = 0;
    src.last_samples[2] = 0;
    src.last_samples[3] = 0;
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
    AXSetVoiceAddr(axv->ax_voice, &addr);
    AXSetVoiceAdpcm(axv->ax_voice, &ax_adpcm);
    AXSetVoiceLpf(axv->ax_voice, &lpf);
    AXSetVoiceSrc(axv->ax_voice, &src);
    AXSetVoiceSrcType(axv->ax_voice, 1);
}

// AX voice-drop callback: the hardware took the voice back — frees its AX and voice works.
void cb_drop_voice(void* voice)
{
    AXVPB* axvpb;
    SND_AXV* axv;
    SND_VOICE* vw;
    u32 i;
    int old;

    axvpb = (AXVPB*) voice;
    OSReport("ISS Voice Drop 0x%08x\n", voice);
    old = OSDisableInterrupts();
    for (i = 0; i < SND_AXV_MAX; i++) {
        axv = &Snd_axv_work[i];
        if (axvpb != axv->ax_voice) {
            continue;
        }
        MIXReleaseChannel(axv->ax_voice);
        vw = axv->voice_adrs;
        if (vw != NULL) {
            vw->be_flag = 0;
            vw->axv_ptr = NULL;
        }
        axv->be_flag = 0;
        axv->voice_adrs = NULL;
        axv->ax_voice = NULL;
        break;
    }
    OSRestoreInterrupts(old);
}
