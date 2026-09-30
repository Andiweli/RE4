// game/snd_str3: sound driver stream AX voices — acquiring the voice works and AX voices (left,
// and right for stereo) at priority 31, programming their ADPCM parameters from the stream header
// (SHD), start / stop / recovery / free, and the voice-drop callback that aborts the stream.
#include "snd_drv.h"

// Takes the voice works (slots start..start+num) and AX voices, sets the initial ARAM addresses
// (short streams: the whole data with the real loop; long: the ring) and programs the voices.
// Returns 1 when no voice is free.
int Snd_str_ax_voice_init(SND_STR* str, s8 start, s8 num)
{
    int ret;

    ret = str_secure_voice_work(str, start, num);
    if (ret == 1) {
        return 1;
    }
    str->ax_start_l = str->ar_buff_nbl_l + 2;
    str->ax_start_r = str->ar_buff_nbl_r + 2;
    if (str->short_flag & 0x1) {
        str_ax_adrs_set_short(str);
    } else {
        str_ax_adrs_set_long(str);
    }
    str->aram_nbl = str->ax_start_l - str->ar_buff_nbl_l;
    str_ax_voice_para_set(str);
    return 0;
}

// Long stream: loop over the whole ARAM ring (128 KB mono / 64 KB stereo per channel).
void str_ax_adrs_set_long(SND_STR* str)
{
    u32 len;

    str->ax_lptop_l = str->ax_start_l;
    str->ax_lptop_r = str->ax_start_r;
    if (str->shd_flag & 0x1) {
        len = 0x3FFFF;
    } else {
        len = 0x7FFFF;
    }
    str->ax_lpend_l = str->ar_buff_nbl_l + len;
    str->ax_lpend_r = str->ar_buff_nbl_r + len;
}

// Short (fully resident) stream: loop at the real loop start (or the zero table when not
// looping), end at the loop end.
void str_ax_adrs_set_short(SND_STR* str)
{
    u32 zero;

    zero = Snd_ctrl_work.aram_base * 2 + 2;
    if (str->short_flag & 0x2) {
        str->ax_lptop_l = str->ar_buff_nbl_l + str->lptop_nbl;
        str->ax_lptop_r = str->ar_buff_nbl_r + str->lptop_nbl;
    } else {
        str->ax_lptop_l = zero;
        str->ax_lptop_r = zero;
    }
    str->ax_lpend_l = str->ar_buff_nbl_l + str->lpend_nbl;
    str->ax_lpend_r = str->ar_buff_nbl_r + str->lpend_nbl;
}

// Programs the left (and right) AX voice: sample rate ratio, no LPF, loop / end / current
// addresses, ADPCM coefficients and initial state from the header.
void str_ax_voice_para_set(SND_STR* str)
{
    SND_SHD* shd;
    AXPBADDR addr;
    AXPBSRC src;
    AXPBADPCM adpcm;
    AXPBLPF lpf;
    u32 i;
    u32 loop;
    u32 ratio;
    u32 srctype;
    f32 f;

    shd = str->shd_adrs;
    f = str->smp_rate / 32000.0f;
    ratio = (u32) (f * 65536.0f);
    if (str->smp_rate == 32000.0f) {
        srctype = 0;
    } else {
        srctype = 1;
    }
    src.ratioHi = ratio >> 16;
    src.ratioLo = ratio;
    src.currentAddressFrac = 0;
    src.last_samples[0] = 0;
    src.last_samples[1] = 0;
    src.last_samples[2] = 0;
    src.last_samples[3] = 0;
    lpf.on = 0;
    lpf.yn1 = 0;
    lpf.a0 = 0;
    lpf.b0 = 0;
    if (str->short_flag & 0x4) {
        loop = 0;
    } else {
        loop = 1;
    }
    addr.loopFlag = loop;
    addr.format = 0;
    addr.loopAddressHi = str->ax_lptop_l >> 16;
    addr.loopAddressLo = str->ax_lptop_l;
    addr.endAddressHi = str->ax_lpend_l >> 16;
    addr.endAddressLo = str->ax_lpend_l;
    addr.currentAddressHi = str->ax_start_l >> 16;
    addr.currentAddressLo = str->ax_start_l;
    for (i = 0; i < 8; i++) {
        adpcm.a[i][0] = shd->coef[i];
        adpcm.a[i][1] = shd->coef[i + 8];
    }
    adpcm.gain = shd->gain[0];
    adpcm.pred_scale = shd->ps[0];
    adpcm.yn1 = shd->yn1[0];
    adpcm.yn2 = shd->yn2[0];
    if (loop == 1) {
        AXSetVoiceType(str->ax_voice_l, 1);
    } else {
        AXSetVoiceType(str->ax_voice_l, 0);
    }
    AXSetVoiceAddr(str->ax_voice_l, &addr);
    AXSetVoiceAdpcm(str->ax_voice_l, &adpcm);
    AXSetVoiceLpf(str->ax_voice_l, &lpf);
    AXSetVoiceSrc(str->ax_voice_l, &src);
    AXSetVoiceSrcType(str->ax_voice_l, srctype);
    if (str->shd_flag & 0x2) {
        return;
    }
    addr.loopFlag = loop;
    addr.format = 0;
    addr.loopAddressHi = str->ax_lptop_r >> 16;
    addr.loopAddressLo = str->ax_lptop_r;
    addr.endAddressHi = str->ax_lpend_r >> 16;
    addr.endAddressLo = str->ax_lpend_r;
    addr.currentAddressHi = str->ax_start_r >> 16;
    addr.currentAddressLo = str->ax_start_r;
    for (i = 0; i < 8; i++) {
        adpcm.a[i][0] = shd->coefR[i];
        adpcm.a[i][1] = shd->coefR[i + 8];
    }
    adpcm.gain = shd->gain[1];
    adpcm.pred_scale = shd->ps[1];
    adpcm.yn1 = shd->yn1[1];
    adpcm.yn2 = shd->yn2[1];
    if (loop == 1) {
        AXSetVoiceType(str->ax_voice_r, 1);
    } else {
        AXSetVoiceType(str->ax_voice_r, 0);
    }
    AXSetVoiceAddr(str->ax_voice_r, &addr);
    AXSetVoiceAdpcm(str->ax_voice_r, &adpcm);
    AXSetVoiceLpf(str->ax_voice_r, &lpf);
    AXSetVoiceSrc(str->ax_voice_r, &src);
    AXSetVoiceSrcType(str->ax_voice_r, srctype);
}

// AX voice-drop callback: the stream loses a voice — flags the stream for abort (status 0x4000
// plus 0x1000 / 0x2000 for the lost left / right voice) and frees the voice work.
void cb_str_voice_drop(void* voice)
{
    int old;
    AXVPB* axvpb;
    SND_STR* str;
    SND_VOICE* vw;
    u32 i;

    axvpb = (AXVPB*) voice;
    vw = NULL;
    OSReport("Str Voice Drop 0x%08x\n", voice);
    old = OSDisableInterrupts() ? TRUE : FALSE;
    for (i = 0; i < SND_STR_MAX; i++) {
        str = &Snd_str_work[i];
        if (str->be_flag != 0) {
            str->be_flag |= 0x4000;
            if (str->ax_voice_l == axvpb) {
                str->be_flag |= 0x1000;
                MIXReleaseChannel(axvpb);
                vw = str->voice_adrs_l;
                if (vw != NULL) {
                    vw->be_flag = 0;
                    vw->axv_ptr = NULL;
                }
            } else if (str->ax_voice_r == axvpb) {
                str->be_flag |= 0x2000;
                MIXReleaseChannel(axvpb);
                vw = str->voice_adrs_r;
                if (vw != NULL) {
                    vw->be_flag = 0;
                    vw->axv_ptr = NULL;
                }
            }
        }
    }
    OSRestoreInterrupts(old);
}

// A voice work in slots start..start+num and an AX voice (priority 31) for the left and, for
// stereo, the right channel. Returns 1 when one is unavailable.
int str_secure_voice_work(SND_STR* str, s8 start, s8 num)
{
    SND_VOICE* vw;
    int i;

    vw = NULL;
    for (i = 0; i <= num; i++) {
        vw = Snd_open_voice_work_str(str, start + (s8) i);
        if (vw != NULL) {
            break;
        }
    }
    if (vw == NULL) {
        OSReport("STR SND_VOICE L is not available\n");
        return 1;
    }
    str->voice_adrs_l = vw;
    str->ax_voice_l = AXAcquireVoice(31, cb_str_voice_drop, 0);
    if (str->ax_voice_l == NULL) {
        OSReport("STR AX_VOICE_L is not available\n");
        return 1;
    }
    if (str->shd_flag & 0x2) {
        return 0;
    }
    for (i = 0; i <= num; i++) {
        vw = Snd_open_voice_work_str(str, start + (s8) i);
        if (vw != NULL) {
            break;
        }
    }
    if (vw == NULL) {
        OSReport("STR SND_VOICE R is not available\n");
        return 1;
    }
    str->voice_adrs_r = vw;
    str->ax_voice_r = AXAcquireVoice(31, cb_str_voice_drop, 0);
    if (str->ax_voice_r == NULL) {
        OSReport("STR AX_VOICE_R is not available\n");
        return 1;
    }
    return 0;
}

// Starts playback: volume (or a fade-in from 0 when fade_time is set), pan / surround pan, AUX
// sends into MIX channels (stereo: hard left / right), voices running.
void Snd_str_ax_voice_play(SND_STR* str)
{
    if (str->req_fade_time == 0) {
        str->now_vol = str->vol << 8;
    } else {
        str->now_vol = 0;
        str->nml_fade_end = str->req_fade_end << 8;
        str->nml_fade_spd = str->nml_fade_end / str->req_fade_time;
        str->be_flag |= 0x100;
    }
    Snd_str_work_calc_ax_vol(str);
    str->srd_span = str->span;
    Snd_str_work_choice_out_span(str);
    str->ax_aux_a = Snd_vol_syn_to_ax(str->aux_a);
    str->ax_aux_b = Snd_vol_syn_to_ax(str->aux_b);
    if (str->shd_flag & 0x1) {
        MIXInitChannel(str->ax_voice_l, 0, str->ax_vol, str->ax_aux_a, str->ax_aux_b, 0, str->out_span, 0);
        MIXInitChannel(str->ax_voice_r, 0, str->ax_vol, str->ax_aux_a, str->ax_aux_b, 0x7F, str->out_span, 0);
        AXSetVoiceState(str->ax_voice_l, 1);
        AXSetVoiceState(str->ax_voice_r, 1);
    } else {
        MIXInitChannel(str->ax_voice_l, 0, str->ax_vol, str->ax_aux_a, str->ax_aux_b, str->pan, str->out_span, 0);
        AXSetVoiceState(str->ax_voice_l, 1);
    }
}

// Mutes and stops the voices.
void Snd_str_ax_voice_stop(SND_STR* str)
{
    str->now_vol = 0;
    Snd_str_work_calc_ax_vol(str);
    if (str->ax_voice_l->pb.state != 0) {
        MIXSetInput(str->ax_voice_l, str->ax_vol);
        AXSetVoiceState(str->ax_voice_l, 0);
    }
    if (str->shd_flag & 0x2) {
        return;
    }
    if (str->ax_voice_r->pb.state != 0) {
        MIXSetInput(str->ax_voice_r, str->ax_vol);
        AXSetVoiceState(str->ax_voice_r, 0);
    }
}

// After a DVD error: restarts the voices with an error fade pending (status 0x200).
void Snd_str_ax_voice_recovery(SND_STR* str)
{
    str->be_flag |= 0x200;
    Snd_str_work_calc_ax_vol(str);
    MIXSetInput(str->ax_voice_l, str->ax_vol);
    AXSetVoiceState(str->ax_voice_l, 1);
    if (str->shd_flag & 0x2) {
        return;
    }
    MIXSetInput(str->ax_voice_r, str->ax_vol);
    AXSetVoiceState(str->ax_voice_r, 1);
}

// Releases the MIX channels, AX voices and voice works.
void Snd_str_ax_voice_free(SND_STR* str)
{
    SND_VOICE* vw;

    if (str->be_flag & 0x20) {
        MIXReleaseChannel(str->ax_voice_l);
    }
    AXFreeVoice(str->ax_voice_l);
    str->ax_voice_l = NULL;
    vw = str->voice_adrs_l;
    vw->be_flag = 0;
    if (str->shd_flag & 0x2) {
        return;
    }
    if (str->be_flag & 0x20) {
        MIXReleaseChannel(str->ax_voice_r);
    }
    AXFreeVoice(str->ax_voice_r);
    str->ax_voice_r = NULL;
    vw = str->voice_adrs_r;
    vw->be_flag = 0;
}
