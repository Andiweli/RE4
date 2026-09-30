// game/snd_seq3: the sound driver's sequence works (SND_SEQ), from starting a sequence on an
// ISS play request to closing it when the last note has died.
#include "snd_drv.h"

// Clears the 8 sequence works (numbered).
void Snd_seq_work_clear(void)
{
    SND_SEQ* seq;
    u32 i;
    u32 j;
    u8* p;

    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        p = (u8*) seq;
        for (j = 0; j < sizeof(SND_SEQ); j++) {
            *p++ = 0;
        }
        seq->work_id = i;
    }
}

// The active sequence work with sound id `snd_id`, or NULL.
SND_SEQ* Snd_search_seq_work_snd_id(u32 snd_id)
{
    SND_SEQ* seq;
    int i;

    if (snd_id == 0) {
        return NULL;
    }
    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        if (seq->be_flag == 0) {
            continue;
        }
        if (seq->snd_id != snd_id) {
            continue;
        }
        return seq;
    }
    return NULL;
}

// Game-frame tick: a sequence that has stopped (status bit4 off) is closed once its synth has no
// active notes left.
void Snd_seq_work_close_check(void)
{
    SND_SEQ* seq;
    int i;

    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        if (seq->be_flag == 0) {
            continue;
        }
        if (seq->be_flag & 0x10) {
            continue;
        }
        if (SYNGetActiveNotes(&seq->synth) != 0) {
            continue;
        }
        SYNQuitSynth(&seq->synth);
        seq->be_flag = 0;
    }
}

// Reads a MIDI variable-length delta time (up to 4 bytes) at seq_pos.
u32 Snd_seq_get_delta(SND_SEQ* seq)
{
    u32 d;
    u32 c;

    d = *seq->now_seq_ptr++;
    if (d & 0x80) {
        c = *seq->now_seq_ptr++;
        d = ((d & 0x7F) << 7) | (c & 0x7F);
        if (c & 0x80) {
            c = *seq->now_seq_ptr++;
            d = (d << 7) | (c & 0x7F);
            if (c & 0x80) {
                c = *seq->now_seq_ptr++;
                d = (d << 7) | (c & 0x7F);
            }
        }
    }
    return d;
}

// Forwards the current 3-byte message to the sequence's synth.
void Snd_seq_send_midi(SND_CTRL* msg, SND_SEQ* seq)
{
    int old;

    old = OSDisableInterrupts();
    SYNMidiInput(&seq->synth, msg->seq_data);
    OSRestoreInterrupts(old);
}

// Sends one MIDI message to a synth.
void Snd_send_midi(SYNSYNTH* synth, u8 status, u8 data1, u8 data2)
{
    u8 msg[3];
    int old;

    msg[0] = status;
    msg[1] = data1;
    msg[2] = data2;
    old = OSDisableInterrupts();
    SYNMidiInput(synth, msg);
    OSRestoreInterrupts(old);
}

// AX volume from the system BGM (type 2) or SE volume x master x the sequence's 8.8 volume.
void Snd_seq_work_calc_ax_vol(SND_SEQ* seq)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;

    if (seq->seq_type == 2) {
        seq->out_vol = ctrl->vol_mas_se / 127 * (ctrl->vol_iss_se >> 8);
    } else {
        seq->out_vol = ctrl->vol_mas_bgm / 127 * (ctrl->vol_iss_bgm >> 8);
    }
    seq->out_vol = seq->out_vol / 127 * (seq->now_vol >> 8);
    seq->ax_vol = Snd_vol_syn_to_ax((s16) (seq->out_vol >> 8));
}

// Starts a sequence from a play request: a free work, tracks reset, synth on the block's DLS and
// ARAM, sequence data = the block's sequence table entry for the SIT's bank, first delta read,
// volume from the request or the SIT; status 0x11 running.
void Snd_iss_new_seq_work(SND_IBLK* blk, SND_SIT* sit, SND_REQ* req)
{
    SND_SEQ* seq;
    u8* tbl;
    u32 ofs;
    u32 bank;

    seq = open_seq_work();
    if (seq == NULL) {
        return;
    }
    seq->be_flag = 0x11;
    seq->snd_id = req->snd_id;
    seq->seq_type = req->use_type;
    seq_work_init_track(seq);
    seq->pcm_adrs = blk->pcm_adrs;
    seq->wt_adrs = blk->wt_adrs;
    seq->sit_ptr = sit;
    SYNInitSynth(&seq->synth, seq->wt_adrs, seq->pcm_adrs, Snd_ctrl_work.zero_adrs, 30, 30, 1);
    bank = (u16) ((u16) (sit->note >> 8) & 0xFF);
    tbl = blk->seq_adrs;
    ofs = ((u32*) tbl)[bank + 1];
    seq->top_seq_ptr = blk->seq_adrs + ofs;
    seq->now_seq_ptr = seq->top_seq_ptr;
    seq->lop_seq_ptr = seq->top_seq_ptr;
    seq->time = Snd_seq_get_delta(seq);
    if (req->vol >= 0) {
        seq->now_vol = req->vol << 8;
    } else {
        seq->now_vol = sit->vol << 8;
    }
    seq->update |= 0x1;
    seq->be_flag |= 0x10;
}

// A free sequence work, NULL (with a report) when all 8 are used.
SND_SEQ* open_seq_work(void)
{
    SND_SEQ* seq;
    int i;

    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        if (seq->be_flag != 0) {
            continue;
        }
        return seq;
    }
    OSReport("Snd_seq_work is full.\n");
    return NULL;
}

// Resets the playback state (tempo 1000, division 480) and the 16 channels' remembered controllers.
void seq_work_init_track(SND_SEQ* seq)
{
    u32 i;

    seq->update = 0;
    seq->req_flag = 0;
    seq->req_vol = 0;
    seq->out_vol = 0;
    seq->now_vol = 0;
    seq->req_fade_time = 0;
    seq->req_fade_end = 0;
    seq->nml_fade_spd = 0;
    seq->nml_fade_end = 0;
    seq->tempo = 1000;
    seq->tpm = 480;
    seq->time = 0;
    seq->tpr_num = 0;
    for (i = 0; i < 16; i++) {
        seq->flag[i] = 0;
        seq->prio[i] = 0;
        seq->prog[i] = -1;
        seq->vol[i] = 0;
        seq->exp[i] = 0;
        seq->pan[i] = -1;
        seq->pitch_m[i] = 0;
        seq->pitch_l[i] = 0;
        seq->pitch_sen_m[i] = -1;
        seq->pitch_sen_l[i] = -1;
        seq->modulation[i] = 0;
        seq->hold[i] = 0;
        seq->aux_a[i] = 0;
        seq->aux_b[i] = 0;
    }
}
