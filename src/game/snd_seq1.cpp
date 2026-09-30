// game/snd_seq1: the audio-frame side of the sound driver's MIDI sequencer. Snd_midi_sequencer
// advances every running sequence and feeds its due MIDI events to the SYN synthesizer.
#include "snd_drv.h"

// Audio frame (every 4.995 ms): accumulates the milliseconds elapsed and runs every playing
// sequence (status bit4) for that many milliseconds; a reset request ends them all.
void Snd_midi_sequencer(void)
{
    SND_CTRL* ctrl = &Snd_ctrl_work;
    SND_SEQ* seq;
    u32 i;
    u32 t0;
    u32 t1;

    if (ctrl->reset_flag & 0x1) {
        ctrl->reset_flag |= 0x40;
    }
    t0 = ctrl->seq_adjust / 1000;
    ctrl->seq_adjust += 4995;
    t1 = ctrl->seq_adjust / 1000;
    ctrl->seq_proc = t1 - t0;
    if (ctrl->seq_adjust == 999000) {
        ctrl->seq_adjust = 0;
    }
    for (i = 0; i < SND_SEQ_MAX; i++) {
        seq = &Snd_seq_work[i];
        if (seq->be_flag & 0x10) {
            seq_player(ctrl, seq);
        }
    }
}

// One audio frame of a sequence: reset check, track overrides, requests, fade step, then the MIDI
// events for each elapsed millisecond, and the master volume update.
void seq_player(SND_CTRL* ctrl, SND_SEQ* seq)
{
    u32 i;

    if (seq_reset_check(seq) != 0) {
        return;
    }
    seq_tpr_check(seq);
    seq_req_check(seq);
    if (seq_fade_check(seq) != 0) {
        return;
    }
    for (i = 0; i < ctrl->seq_proc; i++) {
        seq_one_msec(ctrl, seq);
    }
    seq_play_update(seq);
}

// During a driver reset the sequence is ended (returns 1).
int seq_reset_check(SND_SEQ* seq)
{
    if ((Snd_ctrl_work.reset_flag & 0x40) == 0) {
        return 0;
    }
    seq_play_end(seq);
    return 1;
}

// Applies the queued per-track parameter changes (kind 8 = volume CC 7, else pan CC 10) to the
// channels in each entry's mask.
void seq_tpr_check(SND_SEQ* seq)
{
    int i;
    u16 mask;
    u16 ch;
    u8 status;
    u8 cc;
    u8 val;

    if (seq->tpr_num == 0) {
        return;
    }
    for (i = 0; i < seq->tpr_num; i++) {
        mask = seq->tpr_track[i];
        if (seq->tpr_para[i] == 8) {
            cc = 7;
        } else {
            cc = 10;
        }
        val = seq->tpr_value[i];
        for (ch = 0; ch < 16; ch++) {
            if (mask & 0x1) {
                if (cc == 7) {
                    seq->vol[ch] = val;
                } else {
                    seq->pan[ch] = val;
                }
                status = (u8) ch | 0xB0;
                SYNMidiInput(&seq->synth, &status);
            }
            mask >>= 1;
        }
    }
    seq->tpr_num = 0;
}

// Executes one pending request: set volume (bit2), fade (bit0), quick stop (bit1: 50-step fade to 0).
void seq_req_check(SND_SEQ* seq)
{
    if (seq->req_flag == 0) {
        return;
    }
    if (seq->req_flag & 0x4) {
        seq->req_flag &= ~0x4;
        seq_req_vol_set(seq);
        return;
    }
    if (seq->req_flag & 0x1) {
        seq->req_flag &= ~0x1;
        seq_req_fade_set(seq, seq->req_fade_time, seq->req_fade_end);
        return;
    }
    if (seq->req_flag & 0x2) {
        seq->req_flag &= ~0x2;
        seq_req_fade_set(seq, 50, 0);
    }
}

// Volume set at once (vol2 = vol << 8, refresh flagged).
void seq_req_vol_set(SND_SEQ* seq)
{
    seq->now_vol = seq->req_vol << 8;
    seq->update |= 0x1;
}

// Starts a fade of the 8.8 volume to `vol` in `time` steps (at least 1 per step); status 0x100.
void seq_req_fade_set(SND_SEQ* seq, s16 time, s16 vol)
{
    s16 diff;

    seq->nml_fade_end = vol << 8;
    diff = seq->nml_fade_end - seq->now_vol;
    seq->nml_fade_spd = diff / time;
    if (seq->nml_fade_spd == 0) {
        if (diff > 0) {
            seq->nml_fade_spd = 1;
        } else {
            seq->nml_fade_spd = -1;
        }
    }
    seq->be_flag |= 0x100;
}

// One fade step; when the target is reached the fade ends, and a target of 0 ends the sequence
// (returns 1).
int seq_fade_check(SND_SEQ* seq)
{
    if (seq->be_flag & 0x100) {
        if (seq->now_vol == seq->nml_fade_end) {
            seq->be_flag &= ~0x100;
            if (seq->now_vol == 0) {
                seq_play_end(seq);
                return 1;
            }
        } else {
            seq_fade_new_vol_set(seq, seq->nml_fade_spd, seq->nml_fade_end);
        }
    }
    return 0;
}

// Moves vol2 by `step` toward `target`, flags the refresh.
void seq_fade_new_vol_set(SND_SEQ* seq, s16 step, s16 target)
{
    int v;

    v = seq->now_vol + step;
    if (step > 0) {
        if (v > target) {
            v = target;
        }
    } else {
        if (v < target) {
            v = target;
        }
    }
    seq->now_vol = v;
    seq->update |= 0x1;
}

// One millisecond of sequence time: counts the delta down by `division` and plays every event
// that comes due (delta = next delta time x tempo).
void seq_one_msec(SND_CTRL* ctrl, SND_SEQ* seq)
{
    while (1) {
        if (seq->time == 0) {
            seq_one_msec_main(ctrl, seq);
            if ((seq->be_flag & 0x10) == 0) {
                return;
            }
            seq->time = Snd_seq_get_delta(seq) * seq->tempo;
        } else {
            seq->time -= seq->tpm;
            if (seq->time > 0) {
                break;
            }
            seq->time = 0;
        }
    }
}

// Reads the next 3-byte MIDI message at seq_pos and dispatches it (Snd_seq_midi_message).
void seq_one_msec_main(SND_CTRL* ctrl, SND_SEQ* seq)
{
    u8* p;

    p = seq->now_seq_ptr;
    ctrl->seq_data[0] = *p++;
    ctrl->seq_data[1] = *p++;
    ctrl->seq_data[2] = *p++;
    ctrl->code = ctrl->seq_data[0] & 0xF0;
    ctrl->track = ctrl->seq_data[0] & 0x0F;
    Snd_seq_midi_message(ctrl, seq);
}

// Ends the sequence: volume 0, note-off for every voice work it owns, status bit4 off.
void seq_play_end(SND_SEQ* seq)
{
    SND_VOICE* voice;
    int i;

    if (seq->now_vol != 0) {
        seq->now_vol = 0;
        seq->update |= 0x1;
    }
    for (i = 0; i < SND_VOICE_MAX; i++) {
        voice = &Snd_voice_work[i];
        if (voice->be_flag == 0) {
            continue;
        }
        if (voice->play_type != 2) {
            continue;
        }
        if ((s16) voice->seq_id != seq->work_id) {
            continue;
        }
        Snd_send_midi(&seq->synth, voice->track | (s8) 0x90, voice->note, 0);
        voice->be_flag = 0;
    }
    seq->be_flag &= ~0x10;
    seq->update = 0;
}

// Pushes a changed volume (flag bit0) to the synth master volume.
void seq_play_update(SND_SEQ* seq)
{
    if (seq->update & 0x1) {
        Snd_seq_work_calc_ax_vol(seq);
        SYNSetMasterVolume(&seq->synth, seq->ax_vol);
    }
    seq->update = 0;
}
