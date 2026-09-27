// game/snd_seq2: the MIDI event dispatch of the BGM sequencer in the sound driver, which forwards
// note, control and meta events to the SYN synthesizer.
#include "snd_drv.h"

// Dispatches the MIDI message in ctrl->midi_msg by status type; an unknown status is a data error
// (OSPanic).
void Snd_seq_midi_message(SND_CTRL_WORK* m, SND_SEQ_WORK* seq)
{
    switch (m->midi_type) {
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
        OSReport("SND SEQ data error : %02X %02X %02X\n", m->midi_msg[0], m->midi_msg[1], m->midi_msg[2]);
#line 60 "D:/Bio4/Prog/sound_driver/snd_seq2.cpp"
        OSPanic(__FILE__, __LINE__, "program stop");
#line 30 "snd_seq2.cpp"
    }
}

// Note on (velocity 0 = note off): takes a voice work at the channel's priority (may steal),
// remembers sequence / channel / note on it, forwards to the synth.
void seq_note_on(SND_CTRL_WORK* m, SND_SEQ_WORK* seq)
{
    SND_VOICE_WORK* voice;

    if (m->midi_msg[2] == 0) {
        seq_note_off(m, seq);
        return;
    }
    seq->seq_pos += 3;
    voice = Snd_open_voice_work_seq(seq, seq->ch_prio[m->midi_ch]);
    if (voice == NULL) {
        return;
    }
    voice->prio = seq->ch_prio[m->midi_ch];
    seq_drums_flag_ck(m, seq);
    voice->seq_no = seq->no;
    voice->seq_ch = m->midi_ch;
    voice->seq_note = m->midi_msg[1];
    Snd_seq_send_midi(m, seq);
}

// Note off: frees the note's voice work and forwards to the synth.
void seq_note_off(SND_CTRL_WORK* m, SND_SEQ_WORK* seq)
{
    SND_VOICE_WORK* voice;
    u8 note;

    seq->seq_pos += 3;
    seq_drums_flag_ck(m, seq);
    note = m->midi_msg[1];
    voice = Snd_search_voice_work_seq(seq, m->midi_ch, note);
    if (voice == NULL) {
        return;
    }
    voice->status = 0;
    Snd_seq_send_midi(m, seq);
}

// Program change: remembered per channel, forwarded.
void seq_prog_change(SND_CTRL_WORK* m, SND_SEQ_WORK* seq)
{
    seq->seq_pos += 2;
    seq->ch_prog[m->midi_ch] = m->midi_msg[1];
    Snd_seq_send_midi(m, seq);
}

// Control change: modulation / volume / pan / expression / hold / reverb / chorus are remembered
// per channel and forwarded; data entry (6 / 0x26) goes through seq_ctrl_data_entry; 0x66 sets the
// loop point, 0x67 jumps back to it, 0x68 channel priority, 0x69 drums flag (not forwarded).
void seq_ctrl_change(SND_CTRL_WORK* m, SND_SEQ_WORK* seq)
{
    u8 val;

    seq->seq_pos += 3;
    val = m->midi_msg[2];
    switch (m->midi_msg[1]) {
    case 0x01:
        seq->ch_mod[m->midi_ch] = val;
        break;
    case 0x06:
    case 0x26:
        seq_ctrl_data_entry(m, seq);
        break;
    case 0x07:
        seq->ch_vol[m->midi_ch] = val;
        break;
    case 0x0A:
        seq->ch_pan[m->midi_ch] = val;
        break;
    case 0x0B:
        seq->ch_exp[m->midi_ch] = val;
        break;
    case 0x40:
        seq->ch_hold[m->midi_ch] = val;
        break;
    case 0x5B:
        seq->ch_reverb[m->midi_ch] = val;
        break;
    case 0x5C:
        seq->ch_chorus[m->midi_ch] = val;
        break;
    case 0x66:
        seq->seq_loop = seq->seq_pos;
        return;
    case 0x67:
        seq->seq_pos = seq->seq_loop;
        return;
    case 0x68:
        seq->ch_prio[m->midi_ch] = val;
        return;
    case 0x69:
        seq->ch_flag[m->midi_ch] |= 0x1;
        return;
    default:
        OSReport("SND SEQ data error : %02X %02X %02X\n", m->midi_msg[0], m->midi_msg[1], m->midi_msg[2]);
#line 196 "D:/Bio4/Prog/sound_driver/snd_seq2.cpp"
        OSPanic(__FILE__, __LINE__, "program stop");
#line 130 "snd_seq2.cpp"
    }
    Snd_seq_send_midi(m, seq);
}

// RPN data entry: selects RPN 0 (pitch bend range) on the synth and stores the MSB / LSB per channel.
void seq_ctrl_data_entry(SND_CTRL_WORK* m, SND_SEQ_WORK* seq)
{
    int old;
    u8 status;  // the three bytes are consecutive on the stack and passed as one message
    u8 data1;
    u8 data2;

    status = m->midi_ch | 0xB0;
    data1 = 0x64;
    data2 = 0;
    old = OSDisableInterrupts();
    SYNMidiInput(&seq->synth, &status);
    OSRestoreInterrupts(old);
    status = m->midi_ch | 0xB0;
    data1 = 0x65;
    data2 = 0;
    old = OSDisableInterrupts();
    SYNMidiInput(&seq->synth, &status);
    OSRestoreInterrupts(old);
    if (m->midi_msg[1] == 6) {
        seq->ch_data_msb[m->midi_ch] = m->midi_msg[2];
    } else {
        seq->ch_data_lsb[m->midi_ch] = m->midi_msg[2];
    }
}

// Pitch bend: remembered per channel, forwarded.
void seq_pitch(SND_CTRL_WORK* m, SND_SEQ_WORK* seq)
{
    seq->seq_pos += 3;
    seq->ch_pitch_lo[m->midi_ch] = m->midi_msg[1];
    seq->ch_pitch_hi[m->midi_ch] = m->midi_msg[2];
    Snd_seq_send_midi(m, seq);
}

// Meta events: 0x2F end of track (sequence stops), 0x51 set tempo (microseconds per quarter /
// 1000 -> tempo); anything else is a data error.
void seq_event(SND_CTRL_WORK* m, SND_SEQ_WORK* seq)
{
    int tempo;
    u8 len;

    switch (m->midi_msg[1]) {
    case 0x2F:
        seq->status &= ~0x10;
        seq->flag = 0;
        break;
    case 0x51:
        seq->seq_pos += 2;
        len = Snd_seq_get_delta(seq);
        tempo = *seq->seq_pos++;
        tempo = (tempo << 8) | *seq->seq_pos++;
        tempo = (tempo << 8) | *seq->seq_pos++;
        seq->tempo = tempo / 1000;
        break;
    default:
        OSReport("SEQ data error : %02X %02X %02X\n", m->midi_msg[0], m->midi_msg[1], m->midi_msg[2]);
#line 288 "D:/Bio4/Prog/sound_driver/snd_seq2.cpp"
        OSPanic(__FILE__, __LINE__, "program end");
#line 185 "snd_seq2.cpp"
        break;
    }
}

// A channel flagged as drums (CC 0x69) is redirected to MIDI channel 9.
void seq_drums_flag_ck(SND_CTRL_WORK* m, SND_SEQ_WORK* seq)
{
    if ((seq->ch_flag[m->midi_ch] & 0x1) == 0) {
        return;
    }
    m->midi_ch = 9;
    m->midi_msg[0] &= 0xF0;
    m->midi_msg[0] |= 0x9;
}
