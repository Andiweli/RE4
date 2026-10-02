// game/snd_seq2: the MIDI event dispatch of the BGM sequencer in the sound driver, which forwards
// note, control and meta events to the SYN synthesizer.
#include "snd_drv.h"

// Dispatches the MIDI message in ctrl->midi_msg by status type; an unknown status is a data error
// (OSPanic).
void Snd_seq_midi_message(SND_CTRL* m, SND_SEQ* seq)
{
    switch (m->code) {
    case 0x80:
        seq_note_off(m, seq);
        break;
    case 0x90:
        seq_note_on(m, seq);
        break;
    case 0xB0:
        seq_ctrl_change(m, seq);
        break;
    case 0xC0:
        seq_prog_change(m, seq);
        break;
    case 0xE0:
        seq_pitch(m, seq);
        break;
    case 0xF0:
        seq_event(m, seq);
        break;
    default:
        OSReport("SND SEQ data error : %02X %02X %02X\n", m->seq_data[0], m->seq_data[1], m->seq_data[2]);
#line 60 "D:/Bio4/Prog/sound_driver/snd_seq2.cpp"
        OSPanic(__FILE__, __LINE__, "program stop");
#line 30 "snd_seq2.cpp"
    }
}

// Note on (velocity 0 = note off): takes a voice work at the channel's priority (may steal),
// remembers sequence / channel / note on it, forwards to the synth.
void seq_note_on(SND_CTRL* m, SND_SEQ* seq)
{
    SND_VOICE* voice;

    if (m->seq_data[2] == 0) {
        seq_note_off(m, seq);
        return;
    }
    seq->now_seq_ptr += 3;
    voice = Snd_open_voice_work_seq(seq, seq->prio[m->track]);
    if (voice == NULL) {
        return;
    }
    voice->vprio = seq->prio[m->track];
    seq_drums_flag_ck(m, seq);
    voice->seq_id = seq->work_id;
    voice->track = m->track;
    voice->note = m->seq_data[1];
    Snd_seq_send_midi(m, seq);
}

// Note off: frees the note's voice work and forwards to the synth.
void seq_note_off(SND_CTRL* m, SND_SEQ* seq)
{
    SND_VOICE* voice;
    u8 note;

    seq->now_seq_ptr += 3;
    seq_drums_flag_ck(m, seq);
    note = m->seq_data[1];
    voice = Snd_search_voice_work_seq(seq, m->track, note);
    if (voice == NULL) {
        return;
    }
    voice->be_flag = 0;
    Snd_seq_send_midi(m, seq);
}

// Program change: remembered per channel, forwarded.
void seq_prog_change(SND_CTRL* m, SND_SEQ* seq)
{
    seq->now_seq_ptr += 2;
    seq->prog[m->track] = m->seq_data[1];
    Snd_seq_send_midi(m, seq);
}

// Control change: modulation / volume / pan / expression / hold / reverb / chorus are remembered
// per channel and forwarded; data entry (6 / 0x26) goes through seq_ctrl_data_entry; 0x66 sets the
// loop point, 0x67 jumps back to it, 0x68 channel priority, 0x69 drums flag (not forwarded).
void seq_ctrl_change(SND_CTRL* m, SND_SEQ* seq)
{
    u8 val;

    seq->now_seq_ptr += 3;
    val = m->seq_data[2];
    switch (m->seq_data[1]) {
    case 0x01:
        seq->modulation[m->track] = val;
        break;
    case 0x06:
    case 0x26:
        seq_ctrl_data_entry(m, seq);
        break;
    case 0x07:
        seq->vol[m->track] = val;
        break;
    case 0x0A:
        seq->pan[m->track] = val;
        break;
    case 0x0B:
        seq->exp[m->track] = val;
        break;
    case 0x40:
        seq->hold[m->track] = val;
        break;
    case 0x5B:
        seq->aux_a[m->track] = val;
        break;
    case 0x5C:
        seq->aux_b[m->track] = val;
        break;
    case 0x66:
        seq->lop_seq_ptr = seq->now_seq_ptr;
        return;
    case 0x67:
        seq->now_seq_ptr = seq->lop_seq_ptr;
        return;
    case 0x68:
        seq->prio[m->track] = val;
        return;
    case 0x69:
        seq->flag[m->track] |= 0x1;
        return;
    default:
        OSReport("SND SEQ data error : %02X %02X %02X\n", m->seq_data[0], m->seq_data[1], m->seq_data[2]);
#line 196 "D:/Bio4/Prog/sound_driver/snd_seq2.cpp"
        OSPanic(__FILE__, __LINE__, "program stop");
#line 130 "snd_seq2.cpp"
    }
    Snd_seq_send_midi(m, seq);
}

// RPN data entry: selects RPN 0 (pitch bend range) on the synth and stores the MSB / LSB per channel.
void seq_ctrl_data_entry(SND_CTRL* m, SND_SEQ* seq)
{
    int old;
    u8 status;  // the three bytes are consecutive on the stack and passed as one message
    u8 data1;
    u8 data2;

    status = m->track | 0xB0;
    data1 = 0x64;
    data2 = 0;
    old = OSDisableInterrupts();
    SYNMidiInput(&seq->synth, &status);
    OSRestoreInterrupts(old);
    status = m->track | 0xB0;
    data1 = 0x65;
    data2 = 0;
    old = OSDisableInterrupts();
    SYNMidiInput(&seq->synth, &status);
    OSRestoreInterrupts(old);
    if (m->seq_data[1] == 6) {
        seq->pitch_sen_m[m->track] = m->seq_data[2];
    } else {
        seq->pitch_sen_l[m->track] = m->seq_data[2];
    }
}

// Pitch bend: remembered per channel, forwarded.
void seq_pitch(SND_CTRL* m, SND_SEQ* seq)
{
    seq->now_seq_ptr += 3;
    seq->pitch_m[m->track] = m->seq_data[1];
    seq->pitch_l[m->track] = m->seq_data[2];
    Snd_seq_send_midi(m, seq);
}

// Meta events: 0x2F end of track (sequence stops), 0x51 set tempo (microseconds per quarter /
// 1000 -> tempo); anything else is a data error.
void seq_event(SND_CTRL* m, SND_SEQ* seq)
{
    int tempo;
    u8 len;

    switch (m->seq_data[1]) {
    case 0x2F:
        seq->be_flag &= ~0x10;
        seq->update = 0;
        break;
    case 0x51:
        seq->now_seq_ptr += 2;
        len = Snd_seq_get_delta(seq);
        tempo = *seq->now_seq_ptr++;
        tempo = (tempo << 8) | *seq->now_seq_ptr++;
        tempo = (tempo << 8) | *seq->now_seq_ptr++;
        seq->tempo = tempo / 1000;
        break;
    default:
        OSReport("SEQ data error : %02X %02X %02X\n", m->seq_data[0], m->seq_data[1], m->seq_data[2]);
#line 288 "D:/Bio4/Prog/sound_driver/snd_seq2.cpp"
        OSPanic(__FILE__, __LINE__, "program end");
#line 185 "snd_seq2.cpp"
        break;
    }
}

// A channel flagged as drums (CC 0x69) is redirected to MIDI channel 9.
void seq_drums_flag_ck(SND_CTRL* m, SND_SEQ* seq)
{
    if ((seq->flag[m->track] & 0x1) == 0) {
        return;
    }
    m->track = 9;
    m->seq_data[0] &= 0xF0;
    m->seq_data[0] |= 0x9;
}
